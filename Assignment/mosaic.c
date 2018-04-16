#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <math.h>
#include "PPM_read_write.h"

#define USER_NAME "aca14dbt"

// function definitions
int main(int argc, char * argv[]);
int process_command_line(int argc, char *argv[]);
void print_help();
void CPU_mosaic(PPM *ppm);
void OPENMP_mosaic(PPM *ppm);
void freePPMAllocatedMemory(PPM *ppm);

// global variables
unsigned int block_size = 0;
char *input_image_name, *output_image_name;
clock_t begin, end;
double openmp_begin, openmp_end, seconds;

// default execution_mode and output_format
MODE execution_mode = CPU;
OUTPUT_FORMAT output_format = PPM_BINARY;

int main(int argc, char *argv[]) {
	if (process_command_line(argc, argv) == FAILURE)
		return 1;

	switch (execution_mode) {
	case (CPU): {

		// allocate PPM struct size in memory
		PPM *ppm;
		ppm = (PPM *)malloc(sizeof(PPM));

		// read the PPM file and store it into the struct
		if (readPPM(input_image_name, ppm)) {
			printf("Image width is %d and height is %d \n", ppm->width, ppm->height);
			// compute the cpu mosaic
			CPU_mosaic(ppm);

			// write to file
			if (!writeToFile(output_image_name, ppm, output_format, execution_mode)) fprintf(stderr, "Error: Could not write all the pixels \n");
			else printf("Info: Your %s file was successfully created \n", output_image_name);

			// free allocated memory
			freePPMAllocatedMemory(ppm);
		}
		else fprintf(stderr, "Error: Could not read all the pixels \n");

		break;
	}
	case (OPENMP): {
		// allocate PPM struct size in memory
		PPM *ppm;
		ppm = (PPM *)malloc(sizeof(PPM));

		// read the PPM file and store it into the struct
		if (readPPM(input_image_name, ppm)) {

			// compute the openmp mosaic
			OPENMP_mosaic(ppm);

			// write to file
			if (!writeToFile(output_image_name, ppm, output_format, execution_mode)) fprintf(stderr, "Error: Could not write all the pixels \n");
			else printf("Info: Your %s file was successfully created \n", output_image_name);

			// free allocated memory
			freePPMAllocatedMemory(ppm);
		}
		else fprintf(stderr, "Error: Could not read all the pixels \n");

		break;
	}
	case (CUDA): {
		printf("CUDA Implementation not required for assignment part 1\n");
		break;
	}
	case (ALL): {
		// allocate PPM struct size in memory
		PPM *ppm;
		ppm = (PPM *)malloc(sizeof(PPM));

		begin = clock();
		// read the PPM file and store it into the struct
		if (readPPM(input_image_name, ppm)) {
			/*  For the other cases the same char array was used to hold the pixels for reading and writing.
			In order to not read the file twice a special output array is used for holding the pixels
			for writing
			*/
			ppm->outputPixels = malloc(sizeof(char)*ppm->size);
			memset(ppm->outputPixels, 0, ppm->size);

			// CPU mosaic
			CPU_mosaic(ppm);

			// OPENMP mosaic
			OPENMP_mosaic(ppm);

			// write to file
			// will output the openmp computated file
			if (!writeToFile(output_image_name, ppm, output_format, execution_mode)) fprintf(stderr, "Error: Could not write all the pixels \n");
			else printf("Info: Your %s file was successfully created \n", output_image_name);

			// free allocated memory
			freePPMAllocatedMemory(ppm);
		}
		else fprintf(stderr, "Error: Could not read all the pixels \n");
		break;
	}
	}

	//save the output image file (from last executed mode)

	return 0;
}


/**
* This method is used to check if the input block_size exceeds
* the image width/height and output error and stop the program
* @param *ppm  Pointer to PPM structure
* @return void
*/
void checkBlockSize(PPM *ppm) {
	if (block_size > ppm->width && block_size > ppm->height) {
		fprintf(stderr, "Error: Specified block size is greater than the width/height \n");
		exit(1);
	}
	else if (block_size > ppm->width)
	{
		fprintf(stderr, "Error: Specified block size is greater than the width \n");
		exit(1);
	}
	else if (block_size > ppm->width)
	{
		fprintf(stderr, "Error: Specified block size is greater than the height \n");
		exit(1);
	}
}

/**
* This method is used to free any allocated memory for reading/writing the ppm image
* @param *ppm  Pointer to PPM structure
* @return void
*/
void freePPMAllocatedMemory(PPM *ppm) {
	// free the allocated memory after the image was written to the file
	if (ppm->pixels != NULL)
		free(ppm->pixels);
	if (execution_mode == ALL && ppm->outputPixels != NULL)
		free(ppm->outputPixels);
	if (ppm != NULL)
		free(ppm);
}

/**
* This method is used to compute the mosaic functionality using CPU
* @param *ppm  Pointer to PPM structure
* @return void
*/
void CPU_mosaic(PPM *ppm) {
	checkBlockSize(ppm);
	//starting CPU timing here after the file was read
	begin = clock();
	openmp_begin = omp_get_wtime();

	// global method to hold the pixel[r,g,b] values
	unsigned int globalSumR = 0, globalSumG = 0, globalSumB = 0;
	/*
	A complete width/height block is a square with sides equals to the input
	block_size.
	An incomplete width/height block is a block which does not have all sides
	equal to the input block_size.
	*/

	// calculate the total number of complete blocks based on block_size and width
	unsigned int width_blocks = ppm->width / block_size;
	/* Check wether or not we have an incomplete width block present
	and increase the number of width blocks if that is the case
	*/
	if (ppm->width % block_size != 0) width_blocks++;
	// calculate the total number of complete height blocks based on block_size and width
	unsigned height_blocks = ppm->height / block_size;
	/* Check wether or not we have an incomplete height block present
	and increase the number of height blocks if that is the case
	*/
	if (ppm->height % block_size != 0) height_blocks++;

	//iterate over the width and height blocks
	for (unsigned int height_block = 0; height_block < height_blocks; height_block++)
		for (unsigned int width_block = 0; width_block < width_blocks; width_block++)
		{
			unsigned int dynamic_block_width = block_size;
			unsigned int dynamic_block_height = block_size;
			//recompute the width of the block if it goes out of the image width
			if ((width_block*block_size + block_size) > ppm->width) {
				dynamic_block_width = ppm->width - width_block * block_size;
			}
			//recompute the height of the block if it goes out of the image width
			if ((height_block*block_size + block_size) > ppm->height) {
				dynamic_block_height = ppm->height - height_block * block_size;
			}
			// variables used to store the local sum
			int localSumR = 0, localSumG = 0, localSumB = 0;
			// iterate over block cells
			for (unsigned int block_h = 0; block_h < dynamic_block_height; block_h++)
				for (unsigned int block_w = 0; block_w < dynamic_block_width; block_w++) {
					// access the pixel within the block
					int i = height_block * block_size * ppm->width + width_block * block_size + block_w + ppm->width*block_h;

					int r = ppm->pixels[i*RGB_SIZE];
					int g = ppm->pixels[i*RGB_SIZE + 1];
					int b = ppm->pixels[i*RGB_SIZE + 2];

					// add the value to the local block sum
					localSumR += r;
					localSumG += g;
					localSumB += b;

					// add the value to the global block sum
					globalSumR += r;
					globalSumG += g;
					globalSumB += b;
				}
			// compute the number of pixels within the block
			int average_dynamic_size = dynamic_block_width * dynamic_block_height;

			char *pixels;
			// if the execution mode is ALL use the outputPixel array for saving the modifications
			if (execution_mode == ALL)
				pixels = ppm->outputPixels;
			else pixels = ppm->pixels;

			// save the average pixel[r,g,b] block value for each pixel
			for (unsigned int block_h = 0; block_h < dynamic_block_height; block_h++)
				for (unsigned int block_w = 0; block_w < dynamic_block_width; block_w++) {
					// access the pixel within the block
					int i = height_block * block_size * ppm->width + width_block * block_size + block_w + ppm->width*block_h;

					ppm->pixels[i * RGB_SIZE] = localSumR / average_dynamic_size;
					ppm->pixels[i * RGB_SIZE + 1] = localSumG / average_dynamic_size;
					ppm->pixels[i * RGB_SIZE + 2] = localSumB / average_dynamic_size;
				}
		}

	printf("CPU Average image colour red = %d, green = %d, blue = %d \n", globalSumR / ppm->pixels_count, globalSumG / ppm->pixels_count, globalSumB / ppm->pixels_count);

	//end timing here
	end = clock();
	openmp_end = omp_get_wtime();
	seconds = (end - begin) / (double)CLOCKS_PER_SEC;
	printf("CPU mode execution clock time took %.0f s and %.0f ms\n", seconds, (seconds - (int)seconds) * 1000);
	seconds = openmp_end - openmp_begin;
	printf("CPU mode execution openmp time took %.0f s and %.0f ms\n", seconds, (seconds - (int)seconds) * 1000);
}


/**
* This method is used to compute the mosaic functionality using OPENMP
* @param *ppm  Pointer to PPM structure
* @return void
*/
void OPENMP_mosaic(PPM *ppm) {
	checkBlockSize(ppm);
	//starting OPENMP timing here after the file was read
	begin = clock();
	openmp_begin = omp_get_wtime();

	// global method to hold the pixel[r,g,b] values
	float globalSumR = 0, globalSumG = 0, globalSumB = 0;
	/*
	A complete width/height block is a square with sides equals to the input
	block_size.
	An incomplete width/height block is a block which does not have all sides
	equal to the input block_size.
	*/

	// calculate the total number of complete blocks based on block_size and width
	unsigned int width_blocks = ppm->width / block_size;
	/* Check wether or not we have an incomplete width block present
	and increase the number of width blocks if that is the case
	*/
	if (ppm->width % block_size != 0) width_blocks++;
	// calculate the total number of complete height blocks based on block_size and width
	unsigned height_blocks = ppm->height / block_size;
	/* Check wether or not we have an incomplete height block present
	and increase the number of height blocks if that is the case
	*/
	if (ppm->height % block_size != 0) height_blocks++;
	// set the shared openmp loop iterators
	int width_block, height_block;
	// use nested parallel loops
#pragma omp parallel for private(width_block)
	for (height_block = 0; height_block < height_blocks; height_block++) {
		for (width_block = 0; width_block < width_blocks; width_block++)
		{
			unsigned int dynamic_block_width = block_size;
			unsigned int dynamic_block_height = block_size;
			//recompute the width of the block if it goes out of the image width
			if ((width_block*block_size + block_size) > ppm->width) {
				dynamic_block_width = ppm->width - width_block * block_size;
			}
			//recompute the height of the block if it goes out of the image height
			if ((height_block*block_size + block_size) > ppm->height) {
				dynamic_block_height = ppm->height - height_block * block_size;
			}
			// variables used to store the local sum
			int localSumR = 0, localSumG = 0, localSumB = 0;
			// iterate over block cells
			for (unsigned int block_h = 0; block_h < dynamic_block_height; block_h++)
				for (unsigned int block_w = 0; block_w < dynamic_block_width; block_w++) {
					// access the pixel within the block
					int i = height_block * block_size * ppm->width + width_block * block_size + block_w + ppm->width*block_h;

					int r = ppm->pixels[i*RGB_SIZE];
					int g = ppm->pixels[i*RGB_SIZE + 1];
					int b = ppm->pixels[i*RGB_SIZE + 2];

					// add the value to the local block sum
					localSumR += r;
					localSumG += g;
					localSumB += b;
				}
			// compute the number of pixels within the block
			int average_dynamic_size = dynamic_block_width * dynamic_block_height;
			// compute the local block average rgb values
			int average_red = localSumR / average_dynamic_size;
			int average_green = localSumG / average_dynamic_size;
			int average_blue = localSumB / average_dynamic_size;
			// calculate the percentage of how much this block represents out of the entire image
			float percentage = (float)(dynamic_block_width*dynamic_block_height) / (float)(ppm->pixels_count);
			// use the calculated percentage to add the local computed value only once per block to the
			// global average value in order to minimise global memory access from parallel threads
#pragma omp atomic
			globalSumR += average_red * percentage;
#pragma omp atomic
			globalSumG += average_green * percentage;
#pragma omp atomic
			globalSumB += average_blue * percentage;
			char *pixels;
			// if the execution mode is ALL use the outputPixel array for saving the modifications
			if (execution_mode == ALL)
				pixels = ppm->outputPixels;
			else pixels = ppm->pixels;

			// save the average pixel[r,g,b] block value for each pixel
			for (unsigned int block_h = 0; block_h < dynamic_block_height; block_h++)
				for (unsigned int block_w = 0; block_w < dynamic_block_width; block_w++) {
					// access the pixel within the block
					int i = height_block * block_size * ppm->width + width_block * block_size + block_w + ppm->width*block_h;

					pixels[i * RGB_SIZE] = average_red;
					pixels[i * RGB_SIZE + 1] = average_green;
					pixels[i * RGB_SIZE + 2] = average_blue;
				}

		}
	}

	printf("OPENMP Average image colour red = %0.0f, green = %0.0f, blue = %0.0f \n", round(globalSumR), round(globalSumG), round(globalSumB));

	//end timing here
	end = clock();
	openmp_end = omp_get_wtime();
	seconds = (end - begin) / (double)CLOCKS_PER_SEC;
	printf("OPENMP mode execution clock time took %.0f s and %.0f ms\n", seconds, (seconds - (int)seconds) * 1000);
	seconds = openmp_end - openmp_begin;
	printf("OPENMP mode execution openmp time took %.0f s and %.0f ms\n", seconds, (seconds - (int)seconds) * 1000);

}

void print_help() {
	printf("mosaic_%s C M -i input_file -o output_file [options]\n", USER_NAME);

	printf("where:\n");
	printf("\tC              Is the mosaic cell size which should be any positive\n"
		"\t               power of 2 number \n");
	printf("\tM              Is the mode with a value of either CPU, OPENMP, CUDA or\n"
		"\t               ALL. The mode specifies which version of the simulation\n"
		"\t               code should execute. ALL should execute each mode in\n"
		"\t               turn.\n");
	printf("\t-i input_file  Specifies an input image file\n");
	printf("\t-o output_file Specifies an output image file which will be used\n"
		"\t               to write the mosaic image\n");
	printf("[options]:\n");
	printf("\t-f ppm_format  PPM image output format either PPM_BINARY (default) or \n"
		"\t               PPM_PLAIN_TEXT\n ");
}

int process_command_line(int argc, char *argv[]) {
	if (argc < 7) {
		fprintf(stderr, "Error: Missing program arguments. Correct usage is...\n");
		print_help();

		return FAILURE;
	}

	//read in the non optional command line arguments
	block_size = (unsigned int)atoi(argv[1]);

	// check if block_size is greater than 0
	if (block_size < 1)
	{
		fprintf(stderr, "Error: Mosaic cell size argument 'C' must be greater than 0 \n");
		return FAILURE;
	}

	//check if block_size is a power of 2
	if ((block_size & (block_size - 1)) != 0)
	{
		fprintf(stderr, "Error: Block size has to be a power of 2 \n");
		return FAILURE;
	}
	printf("Info: Block size -> %d \n", block_size);

	//read in the mode
	if (strcmp(argv[2], "CPU") == 0)
	{
		execution_mode = CPU;
		printf("Info: Execution mode -> CPU \n");
	}
	else if (strcmp(argv[2], "OPENMP") == 0)
	{
		execution_mode = OPENMP;
		printf("Info: Execution mode -> OPENMP \n");
	}
	else if (strcmp(argv[2], "CUDA") == 0)
	{
		execution_mode = CUDA;
		printf("Info: Execution mode -> CUDA \n");
	}
	else if (strcmp(argv[2], "ALL") == 0)
	{
		execution_mode = ALL;
		printf("Info:  Execution mode -> ALL \n");
	}
	else
		fprintf(stderr, "Error: Not a recognized mode. Will use the default one -> CPU \n");

	// check if -i is present
	if (strcmp(argv[3], "-i") != 0)
	{
		fprintf(stderr, "Error: Expected -i argument followed by input image file name\n");
		return FAILURE;
	}

	//read in the input image name
	input_image_name = argv[4];
	printf("Info: Input file -> %s \n", input_image_name);

	// check if -o is present
	if (strcmp(argv[5], "-o") != 0)
	{
		fprintf(stderr, "Error: Expected -o argument followed by output image file name \n");
		return FAILURE;
	}

	//read in the output image name
	output_image_name = argv[6];
	printf("Info: Output file -> %s \n", output_image_name);

	//read in the output file format
	if (argv[7])
	{
		if (strcmp(argv[7], "-f") != 0)
		{
			fprintf(stderr, "Error: Expected -f argument followed by format type as optional arguments \n");
			return FAILURE;
		}

		if (argv[8])
		{
			if (strcmp(argv[8], "PPM_BINARY") == 0) {
				output_format = PPM_BINARY;
				printf("Info: Output format -> PPM_BINARY \n");
			}
			else if (strcmp(argv[8], "PPM_PLAIN_TEXT") == 0) {
				output_format = PPM_PLAIN_TEXT;
				printf("Info: Output format -> PPM_PLAIN_TEXT \n");
			}
			else
				fprintf(stderr, "Error: Not a recognized output format. Will use the default one -> PPM_BINARY \n");
		}
		else
		{
			fprintf(stderr, "Error: Please specify a file output format after -f \n");
			return FAILURE;
		}
	}

	return SUCCESS;
}