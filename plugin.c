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

#include "psdparse.h"

#ifdef WIN32
	#define fsread_large FSRead
#endif

int pl_fgetc(psd_file_t f){
	// FIXME: replace with buffered version, this will be SLOW
	unsigned char c;
	FILECOUNT count = 1;
	return fsread_large(f, &count, &c) ? EOF : c;
}

size_t pl_fread(void *ptr, size_t s, size_t n, psd_file_t f){
	FILECOUNT count = s*n;
	return fsread_large(f, &count, ptr) ? 0 : n;
}

int pl_fseeko(psd_file_t f, off_t pos, int wh){
	int err;
	FILEPOS newpos;

	switch(wh){
	case SEEK_SET: err = setfpos_large(f, fsFromStart, pos); break;
	case SEEK_CUR: err = setfpos_large(f, fsFromMark, pos); break;
	case SEEK_END: err = setfpos_large(f, fsFromLEOF, pos); break;
	default: return -1;
	}
	getfpos_large(f,&newpos);
	//printf("pl_fseeko(%lld, %d) ... pos now %lld\n",pos,wh,newpos);
	return err ? -1 : 0;
}

off_t pl_ftello(psd_file_t f){
	FILEPOS pos;
	return getfpos_large(f, &pos) ? -1 : pos;
}

int pl_feof(psd_file_t f){
	FILEPOS eof;
	return !geteof_large(f, &eof) && pl_ftello(f) >= eof;
}

void pl_fatal(char *s){
	// TODO
}

// stubs to keep linker happy.

FILE* pngsetupwrite(psd_file_t psd, char *dir, char *name, psd_pixels_t width, psd_pixels_t height, 
					int channels, int color_type, struct layer_info *li, struct psd_header *h)
{
	return NULL;
}

void pngwriteimage(FILE *png,psd_file_t psd, int comp[], struct layer_info *li, psd_bytes_t **rowpos,
				   int startchan, int pngchan, psd_pixels_t rows, psd_pixels_t cols, struct psd_header *h)
{
}

FILE* rawsetupwrite(psd_file_t psd, char *dir, char *name, psd_pixels_t width, psd_pixels_t height, 
					int channels, int color_type, struct layer_info *li, struct psd_header *h)
{
	return NULL;
}

void rawwriteimage(FILE *png,psd_file_t psd, int comp[], struct layer_info *li, psd_bytes_t **rowpos,
				   int startchan, int pngchan, psd_pixels_t rows, psd_pixels_t cols, struct psd_header *h)
{
}
