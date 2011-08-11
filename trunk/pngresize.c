/*
    This file is part of "psdparse"
    Copyright (C) 2004-2011 Toby Thain, toby@telegraphics.com.au

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* quick hack. please don't take this too seriously */

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <unistd.h>
#include <math.h>
#include <sys/resource.h>
#include <sys/errno.h>


#include "png.h"

#ifdef HAVE_ZLIB_H
	#include "zlib.h"
#endif

#define HEADER_BYTES 8
#define MB(x) ((x) << 20)

// This was very helpful in understanding Lanczos kernel and convolution:
// http://stackoverflow.com/questions/943781/where-can-i-find-a-good-read-about-bicubic-interpolation-and-lanczos-resampling/946943#946943

int clamp(int vv){
	if(vv < 0)
		return 0;
	else if(vv > 255)
		return 255;
	return vv;
}

double sinc(double x){ return sin(x)/x; }

#define OVERSAMPLE_RATE 2.0

// build 1-D kernel
// conv: array of 2n+1 floats (-n .. 0 .. +n)
void lanczos_1d(double *conv, unsigned n, double fac){
	unsigned i;

	// fac is the rate of kernel taps.
	// at 1.0, there is a single non-zero coefficient.
	conv[n] = 1.;
	for(i = 1; i <= n; ++i){
		double x = i*M_PI / (fac*OVERSAMPLE_RATE);
		conv[n+i] = conv[n-i] = sinc(x)*sinc(x/n);
	}
	/*for(i = 0; i <= 2*n; ++i)
		printf("%.2g  ", conv[i]);
	putchar('\n');*/
}

void lanczos_decim( png_bytep *row_pointers,  // input image: array of row pointers
					int in_planes,      // bytes separating input planes;
					                    // allows for interleaved input
					int plane_index,
					int in_w,           // image columns
					int in_h,           // image rows
                    unsigned char *out, // pointer to output buffer (always one plane)
                    int out_w,          // output columns
                    int out_h,          // output rows
                    double scale )       // ratio of input to output (> 1.0)
{
	double *conv, *knl, sum, weight;
	float *t_buf, *t_row, *t_col;
	int x, y, i, j, k, KERNEL_SIZE = floor(2*scale*OVERSAMPLE_RATE);
	unsigned char *in_row, *out_row;
	
	conv = malloc((2*KERNEL_SIZE+1)*sizeof(double));

	lanczos_1d(conv, KERNEL_SIZE, scale);
	knl = conv + KERNEL_SIZE; // zero index of kernel

	// apply horizontal convolution, interpolating input to output
	// store this in a temporary buffer, with same row count as input

	t_buf = malloc(sizeof(*t_buf) * in_h * out_w);

	for(j = 0, t_row = t_buf;  j < in_h;  ++j, t_row += out_w)
	{
		in_row = row_pointers[j] + plane_index;
		for(i = 0; i < out_w; ++i){
			// find involved input pixels. multiply each by corresponding
			// kernel coefficient.

			sum = weight = 0.;
			// step over all covered input samples
			for(k = -KERNEL_SIZE; k <= KERNEL_SIZE; ++k){
				x = floor((i+.5)*scale + k/OVERSAMPLE_RATE);
				if(x >= 0 && x < in_w){
					sum += knl[k]*in_row[x*in_planes];
					weight += knl[k];
				}
			}
			t_row[i] = sum/weight;
		}
	}

	// interpolate vertically
	for(i = 0; i < out_w; ++i){
		t_col = t_buf + i;
		out_row = out + i;
		for(j = 0; j < out_h; ++j){
			// find involved input pixels. multiply each by corresponding
			// kernel coefficient.

			sum = weight = 0.;
			// step over all covered input samples
			for(k = -KERNEL_SIZE; k <= KERNEL_SIZE; ++k){
				y = floor((j+.5)*scale + k/OVERSAMPLE_RATE);
				if(y >= 0 && y < in_h){
					sum += knl[k]*t_col[y*out_w];
					weight += knl[k];
				}
			}
			out_row[0] = clamp(sum/weight + 0.5);
			out_row += out_w; // step down one row
		}
	}

	free(t_buf);
	free(conv);
}

int main(int argc, char *argv[]){
	const struct rlimit mem_limit = { MB(300), MB(300) },
						cpu_limit = { 120, 120 };
	FILE *fp;
	unsigned char header[HEADER_BYTES];
	png_structp png_ptr, wpng_ptr;
	png_infop info_ptr, end_info, winfo_ptr;
	png_bytep *row_pointers;
	png_uint_32 width, height, max_pixels, new_width, new_height, i, j;
    int bit_depth, color_type, interlace_method, compression_method,
        filter_method, channels, rowbytes,
        larger_dimension, k;
	
	if(argc <= 3){
		fprintf(stderr,
"usage:  %s source_filename dest_filename max_pixels\n\
        Writes a new PNG with height and width scaled to max_pixels.\n\
        If image is already no larger than max_pixels, makes a hard link\n\
        to original file.\n", argv[0]);
		return 2;
	}
	
	max_pixels = atoi(argv[3]);
	
	setrlimit(RLIMIT_AS, &mem_limit);
	setrlimit(RLIMIT_CPU, &cpu_limit);
	
    fp = fopen(argv[1], "rb");
    if (!fp){
        fputs("# cannot open the file\n", stderr);
        return EXIT_FAILURE;
    }
    if (fread(header, 1, HEADER_BYTES, fp) < HEADER_BYTES
        || png_sig_cmp(header, 0, HEADER_BYTES))
    {
        fputs("# not a PNG file\n", stderr);
        return EXIT_FAILURE;
    }
    
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr){
        fputs("# png_create_read_struct failed\n", stderr);
        return EXIT_FAILURE;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr){
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        fputs("# png_create_info_struct failed\n", stderr);
        return EXIT_FAILURE;
    }

    end_info = png_create_info_struct(png_ptr);
    if (!end_info){
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        fputs("# png_create_info_struct failed (end_info)\n", stderr);
        return EXIT_FAILURE;
    }
    
    if (setjmp(png_jmpbuf(png_ptr))){
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fputs("# libpng failed to read the file\n", stderr);
        return EXIT_FAILURE;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, HEADER_BYTES);

    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
    			 &color_type, &interlace_method, &compression_method, &filter_method);


    printf("width: %d  height: %d  bit_depth: %d  color_type: %d\n",
           width, height, bit_depth, color_type);
    if(width <= max_pixels && height <= max_pixels){
    	if(link(argv[1], argv[2]) == 0){
			printf("hard link created\n");
			return EXIT_SUCCESS;
		}else if(errno != EXDEV){
			fprintf(stderr, "# failed to create hard link (%d)\n", errno);
			return EXIT_FAILURE;
		}
		// otherwise fall through and write new file
    }

	// scale image and write new png

	if(interlace_method != PNG_INTERLACE_NONE){
        fputs("# interlace not supported\n", stderr);
        return EXIT_FAILURE;
	}
	
	larger_dimension = width > height ? width : height;
	new_width = max_pixels*width/larger_dimension;
	new_height = max_pixels*height/larger_dimension;
    printf("new width: %d  new height: %d\n", new_width, new_height);

    printf("file channels: %u  file rowbytes: %lu\n",
    	   png_get_channels(png_ptr, info_ptr),
    	   png_get_rowbytes(png_ptr, info_ptr));

	// define transformations
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
	else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);
    if(bit_depth == 16)
    	png_set_strip_16(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    channels = png_get_channels(png_ptr, info_ptr);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    printf("transformed channels: %d  rowbytes: %d\n", channels, rowbytes);
        
// -------------- read source image --------------
   row_pointers = calloc(height, sizeof(png_bytep));
   for(i = 0; i < height; ++i)
      row_pointers[i] = malloc(width*channels);
   png_read_image(png_ptr, row_pointers);

// -------------- filter image --------------
   unsigned char *out_plane[4];
   for(k = 0; k < channels; ++k){
		out_plane[k] = malloc(new_width*new_height);
	   lanczos_decim(row_pointers, channels, k /* plane index */, width, height,
	                 out_plane[k], new_width, new_height,
	                 (double)larger_dimension/max_pixels);
   }
	
// -------------- write output image --------------
   FILE *out_file = fopen(argv[2], "wb");

    wpng_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!wpng_ptr)
       return EXIT_FAILURE;

    winfo_ptr = png_create_info_struct(wpng_ptr);
    if (!winfo_ptr){
       png_destroy_write_struct(&wpng_ptr, (png_infopp)NULL);
       return EXIT_FAILURE;
    }

    if (setjmp(png_jmpbuf(wpng_ptr))){
        png_destroy_write_struct(&wpng_ptr, &winfo_ptr);
        fputs("# libpng failed to write the file\n", stderr);
        return EXIT_FAILURE;
    }
    
    png_init_io(wpng_ptr, out_file);
    png_set_IHDR(wpng_ptr, winfo_ptr, new_width, new_height,
       8, channels == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(wpng_ptr, winfo_ptr);
    
    unsigned char *row_pointer = malloc(channels*new_width);
    for(j = 0; j < new_height; ++j){
    	for(i = 0; i < new_width; ++i)
    		for(k = 0; k < channels; ++k)
    			row_pointer[i*channels + k] = out_plane[k][j*new_width + i];
    	png_write_row(wpng_ptr, row_pointer);
    }
    
    png_write_end(wpng_ptr, winfo_ptr);

	return EXIT_SUCCESS;
}
