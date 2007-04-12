/*
    This file is part of "psdparse"
    Copyright (C) 2004-7 Toby Thain, toby@telegraphics.com.au

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

#include <stdint.h>

#ifdef PSBSUPPORT
	#define _FILE_OFFSET_BITS 64
	// #define _LARGEFILE64_SOURCE

	typedef int64_t psd_bytes_t;
	#define GETPSDBYTES(f) (h->version==1 ? get4B(f) : get8B(f))
	
	// macro chooses the '%ll' version of format strings involving psd_bytes_t type
	#define LL_L(llfmt,lfmt) llfmt
#else
	typedef long psd_bytes_t;
	//typedef int psd_pixels_t;
	#define GETPSDBYTES get4B
	
	// macro chooses the '%l' version of format strings involving psd_bytes_t type
	#define LL_L(llfmt,lfmt) lfmt
#endif

typedef long psd_pixels_t;

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#ifndef PATH_MAX
	#define PATH_MAX FILENAME_MAX
#endif


enum{RAWDATA,RLECOMP};

/* Photoshop's mode constants */
#define ModeBitmap		 0
#define ModeGrayScale		 1
#define ModeIndexedColor 2
#define ModeRGBColor		 3
#define ModeCMYKColor		 4
#define ModeHSLColor		 5
#define ModeHSBColor		 6
#define ModeMultichannel 7
#define ModeDuotone		 8
#define ModeLabColor		 9
#define ModeGray16		10
#define ModeRGB48			11
#define ModeLab48			12
#define ModeCMYK64		13
#define ModeDeepMultichannel 14
#define ModeDuotone16		15

#define PAD2(x) (((x)+1) & -2) // same or next even
#define PAD4(x) (((x)+3) & -4) // same or next multiple of 4

#define VERBOSE if(verbose) printf
#define UNQUIET if(!quiet) printf

#define FIXEDPT(x) ((x)/65536.)

struct psd_header{
	char sig[4];
	short version;
	char reserved[6];
	short channels;
	long rows;
	long cols;
	short depth;
	short mode;

	// following fields are for our purposes, not actual header fields
	psd_bytes_t colormodepos;
};

struct layer_mask_info{
	psd_bytes_t size;
	long top;
	long left;
	long bottom;
	long right;
	char default_colour;
	char flags;
	//char reserved[2];
	
	// runtime data
	psd_pixels_t rows,cols;
};

struct layer_info{
	long top;
	long left;
	long bottom;
	long right;
	short channels;
	
	// runtime data (not in file)
	psd_bytes_t *chlengths; // array of channel lengths
	int *chid;       // channel ids
	int *chindex;    // lookup channel number by id (inverse of chid[])
	struct layer_mask_info mask;
	char *name;
	char *nameno; // "layerNN"
	psd_bytes_t extradatapos;
	psd_bytes_t extradatalen;
};

struct blend_mode_info{
	char sig[4];
	char key[4];
	unsigned char opacity;
	unsigned char clipping;
	unsigned char flags;
	unsigned char filler;
};

struct extra_data{
	char sig[4];
	char key[4];
	psd_bytes_t length;
	//char data[];
};

struct dictentry{
	int id;
	char *key, *tag, *desc;
	void (*func)(FILE *f, int level, int printxml, struct dictentry *dict);
};

extern char *channelsuffixes[],*mode_names[],dirsep[];
extern int verbose,quiet,makedirs;

extern FILE *xml;

void fatal(char *s);
void warn(char *fmt,...);
void alwayswarn(char *fmt,...);
void *checkmalloc(long n);
void fputcxml(char c, FILE *f);
void fputsxml(char *str, FILE *f);
char *getpstr(FILE *f);
char *getpstr2(FILE *f);
double getdoubleB(FILE *f);
long get4B(FILE *f);
int64_t get8B(FILE *f);
int get2B(FILE *f);
unsigned get2Bu(FILE *f);
const char *tabs(int n);

void entertag(FILE *f, int level, int printxml, struct dictentry *parent, struct dictentry *d);
struct dictentry *findbykey(FILE *f, int level, struct dictentry *dict, char *key, int printxml);
void doextradata(FILE *f, int level, long length, int printxml);

void descriptor(FILE *f, int level, int printxml, struct dictentry *dict);

void skipblock(FILE *f,char *desc);
void dumprow(unsigned char *b,long n,int group);
int dochannel(FILE *f,struct layer_info *li,int idx,int channels,
			  psd_pixels_t rows,psd_pixels_t cols,int depth,psd_bytes_t **rowpos,struct psd_header *h);
void doimage(FILE *f,struct layer_info *li,char *name,int channels,
			 psd_pixels_t rows,psd_pixels_t cols,struct psd_header *h);
void dolayermaskinfo(FILE *f,struct psd_header *h);
void doimageresources(FILE *f);

void setupfile(char *dstname,char *dir,char *name,char *suffix);
FILE* pngsetupwrite(FILE *psd, char *dir, char *name, psd_pixels_t width, psd_pixels_t height, 
					int channels, int color_type, struct layer_info *li, struct psd_header *h);
void pngwriteimage(FILE *png,FILE *psd, int comp[], struct layer_info *li, psd_bytes_t **rowpos,
				   int startchan, int pngchan, psd_pixels_t rows, psd_pixels_t cols, struct psd_header *h);

FILE* rawsetupwrite(FILE *psd, char *dir, char *name, psd_pixels_t width, psd_pixels_t height, 
					int channels, int color_type, struct layer_info *li, struct psd_header *h);
void rawwriteimage(FILE *png,FILE *psd, int comp[], struct layer_info *li, psd_bytes_t **rowpos,
				   int startchan, int pngchan, psd_pixels_t rows, psd_pixels_t cols, struct psd_header *h);

int unpackbits(unsigned char *outp,unsigned char *inp,psd_pixels_t rowbytes,psd_pixels_t inlen);

#ifdef WIN32
	#include <direct.h>
	#define MKDIR(name,mode) _mkdir(name) // laughable, isn't it.
#else
	#include <sys/stat.h>
	#define MKDIR mkdir
#endif

#ifdef macintosh
	int mkdir(char *s,int mode);
#endif
