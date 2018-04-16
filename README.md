# C image processing

This program is capable of taking an input PPM image and a block size and 
averaging each block size in order to output a new blurred PPM image in either plain text or binary format.
It can either run only on the master thread - CPU or using all available threads - OPENMP.

For example to process a 1920x1280 image with a block size of 8 and to output it
in plain text format we could use the following options:

myapp.exe 8 OPENMP -i 1920x1280.ppm -o out.ppm -f PPM_PLAIN_TEXT


