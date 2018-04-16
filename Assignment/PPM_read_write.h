#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FAILURE 0
#define SUCCESS !FAILURE

// Max nubmer of characters read per line from header
#define MAX_LINE	100
// The amount of numbers required to have an RGB value pixel
#define RGB_SIZE	3

// execution mode
typedef enum MODE { CPU, OPENMP, CUDA, ALL } MODE;

// Params used for reading the input
typedef enum READING_PARAMS { TAG = 1, WIDTH = 2, HEIGHT = 3, MAX_COLOR = 4, PIXELS = 5 } READING_PARAMS;

// Possible output writing formats
typedef enum OUTPUT_FORMAT { PPM_BINARY = 6, PPM_PLAIN_TEXT = 3 } OUTPUT_FORMAT;

// Delimiters used to separate line chunks of characters
static const char _delim[] = " \t\n";


// Structure used to hold the PPM file information for reading and writing
typedef struct PPM
{
	unsigned int tag, width, height, maxColor, pixels_count, size;
	unsigned char *pixels;
	unsigned char *outputPixels;
} PPM;

/**
* This method is used to read and store pixels inside
* the PPM struct for both P3 and P6 formats.
* @param *ppm  Pointer to PPM structure
* @param *f  Pointer to input file stream
* @return int This returns the number of read and stored pixels.
*/
unsigned int readPixels(PPM *ppm, FILE *f)
{
	//read all the data structure of pixels at once
	if (ppm->tag == PPM_BINARY) return fread(ppm->pixels, sizeof(char), ppm->size, f);

	// read pixels one at a time
	unsigned int i = 0;
	while (i < ppm->size)
	{
		unsigned int c = 0;
		/* output error if pixel value is not a digit or
		bigger than the allowed value  */
		if (!fscanf(f, " %d", &c) || c > ppm->maxColor) {
			fprintf(stderr, "Wrong pixel intensity value\n");
			break;
		}
		ppm->pixels[i++] = (unsigned char)c;
	}
	return i;
}

/**
* This method is used to read and store PPM information
* for both P3 and P6 formats.
* @param *fname Pointer to input file name
* @param *ppm Pointer to PPM structure
* @return int 1 if success and 0 if failure
*/
_Bool readPPM(const char *fname, PPM *ppm)
{
	// Set the reading mode to look for tag first as it should
	// be the first element present in the input file
	READING_PARAMS reading_params = TAG;
	// Open file stream
	FILE *f = fopen(fname, "rb");

	// exit and output error if the file cannot be read
	if (f == NULL) {
		fprintf(stderr, "Error: Can't open %s file for reading\n", fname);
		return FAILURE;
	}
	// define a max characters buffer line
	char line[MAX_LINE];
	char *token;

	while (reading_params && fgets(line, sizeof line, f))
	{
		if (line[0] == '#') {
			// this version of the code is based on the command line not being longer than the max buffer size
			// for now we just exit and output an error to the use saying to modify the comment length
			if (strchr(line, '\n') == NULL) {
				fprintf(stderr, "Error: Comment line is bigger than 100 characters. This is not yet handled by this program \n");
				printf("Info: Please modify comment length to be less than 100 \n");
				return FAILURE;
			}
		}
		else
		{
			// separate each chunk of characters by the specified delimiter
			token = strtok(line, _delim);
			while (reading_params && token != NULL)
			{
				switch (reading_params)
				{
					//read the tag
				case TAG:
					if (strcmp(token, "P6") == 0) ppm->tag = PPM_BINARY;
					if (strcmp(token, "P3") == 0) ppm->tag = PPM_PLAIN_TEXT;
					reading_params = ppm->tag ? WIDTH : FAILURE;
					break;
					// read the width
				case WIDTH:
					ppm->width = atoi(token);
					reading_params = ppm->width ? HEIGHT : FAILURE;
					break;
					// read the height
				case HEIGHT:
					ppm->height = atoi(token);
					reading_params = ppm->height ? MAX_COLOR : FAILURE;
					break;
					// read the max color
				case MAX_COLOR:
					ppm->maxColor = atoi(token);
					reading_params = ppm->maxColor ? PIXELS : FAILURE;
					// not knowing what pixels are stored in the buffer size we should not break the switch
					// and change the buffer line
					// but continue to the next case which is reading the pixels already there or not
					if (!reading_params) break;
					// read the pixels
				case PIXELS:
					ppm->pixels_count = ppm->width * ppm->height;
					ppm->size = ppm->width * ppm->height * RGB_SIZE;
					ppm->pixels = malloc(sizeof(char)*ppm->size);
					memset(ppm->pixels, 0, ppm->size);
					unsigned int processed_pixels = readPixels(ppm, f);
					fclose(f);
					return processed_pixels == ppm->size;
				}

				// return null if there are no more tokens present
				token = strtok(NULL, _delim);
			}
		}
	}
	fclose(f);


	return SUCCESS;
}

/**
* This method is used to write pixels to file
* either in P3 - plain text or P6 - binary formats.
* @param *ppm  Pointer to PPM structure
* @param *pixels Pointer to pixels to write
* @param *f   Pointer to input file stream
* @param output_format  The writing format of the pixels
* @return int This returns the number of written pixels.
*/
unsigned int writePixels(PPM *ppm, unsigned char *pixels, FILE *f, OUTPUT_FORMAT output_format)
{
	// if output format is plain text change write pixels one by one
	// else write all array of chars at once in binary format
	if (output_format == PPM_PLAIN_TEXT) {
		int i = 0;
		for (unsigned int p = 0; p < ppm->height * ppm->width; p++) {
			fprintf(f, "%d ", pixels[i++]);
			fprintf(f, "%d ", pixels[i++]);
			fprintf(f, "%d ", pixels[i++]);
		}
		return i;
	}
	else
		return fwrite(pixels, sizeof(char), ppm->size, f);
}

/**
* This method is used to write the PPM file
* either in P3 - plain text or P6 - binary formats.
* @param *fname  Pointer to output file name
* @param *ppm  Pointer to PPM structure
* @param output_format  The writing format of the pixels
* @param *execution_mode Execution mode - binary or plain text
* @return int This returns 1 if all the pixels were written or 0 otherwise.
*/
_Bool writeToFile(const char *fname, PPM *ppm, OUTPUT_FORMAT output_format, MODE execution_mode) {
	char writing_mode[] = "wb";
	//if output format is plain text change writing type to 'w'
	if (output_format == PPM_PLAIN_TEXT) strcpy(writing_mode, "w");

	FILE *f = fopen(fname, writing_mode);
	if (f == NULL) {
		fprintf(stderr, "Error: Can't open %s file for writing \n", fname);
		return FAILURE;
	}

	fprintf(f, "P%d\n", output_format);
	fprintf(f, "%d\n", ppm->width);
	fprintf(f, "%d\n", ppm->height);
	fprintf(f, "%d\n", ppm->maxColor);
	unsigned int processed_pixels = 0;
	if (execution_mode == ALL) processed_pixels = writePixels(ppm, ppm->outputPixels, f, output_format);
	else processed_pixels = writePixels(ppm, ppm->pixels, f, output_format);

	fclose(f);
	return processed_pixels == ppm->size;
}
