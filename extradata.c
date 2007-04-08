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

#include <wchar.h>

#include "psdparse.h"

/* 'Extra data' handling. *Work in progress*
 * 
 * There's guesswork and trial-and-error in here,
 * due to many errors and omissions in Adobe's documentation on this (PS6 SDK).
 * It's amazing that they would try to describe a hierarchical format
 * as a flat list of fields. Reminds me of Jasc's PSP format docs, too.
 * One assumes they don't really encourage people to try and USE the info.
 */

// fetch Pascal string (length byte followed by text)
char *getpstr(FILE *f){
	static char pstr[0x100];
	int len = fgetc(f) & 0xff;
	fread(pstr, 1, len, f);
	pstr[len] = 0;
	return pstr;
}

// Pascal string, aligned to 2 byte
char *getpstr2(FILE *f){
	static char pstr[0x100];
	int len = fgetc(f) & 0xff;
	fread(pstr, 1, len + !(len & 1), f); // if length is even, read an extra byte
	pstr[len] = 0;
	return pstr;
}

#define FIXEDPT(x) ((x)/65536.)

void ed_typetool(FILE *f, FILE *xmlfile, int printxml){
	int i, j, v = get2B(f), mark, type, script, facemark,
		autokern, charcount, selstart, selend, linecount, orient, align, style;
	double size, tracking, kerning, leading, baseshift, scaling, hplace, vplace;

	if(printxml){
		fprintf(xmlfile, "\n\t\t\t<VERSION>%d</VERSION>\n", v);
		
		
		// read transform (6 doubles)
		fputs("\t\t\t<TRANSFORM>\n", xmlfile);
		for(i = 6; i--;)
			fprintf(xmlfile, "\t\t\t\t<COEFF>%g</COEFF>\n", getdoubleB(f));
		fputs("\t\t\t</TRANSFORM>\n", xmlfile);
		
		// read font information
		v = get2B(f);
		fprintf(xmlfile, "\t\t\t<FONTINFO VERSION='%d'", v);
		if(v <= 6){
			fputs(">\n", xmlfile);
			for(i = get2B(f); i--;){
				mark = get2B(f);
				type = get4B(f);
				fprintf(xmlfile, "\t\t\t\t<FACE MARK='%d' TYPE='%d' FONTNAME='%s'", mark, type, getpstr(f));
				fprintf(xmlfile, " FONTFAMILY='%s'", getpstr(f));
				fprintf(xmlfile, " FONTSTYLE='%s'", getpstr(f));
				script = get2B(f);
				fprintf(xmlfile, " SCRIPT='%d'>\n", script);
				
				// doc is unclear, but this may work:
				fputs("\t\t\t\t\t<DESIGNVECTOR>", xmlfile);
				for(j = get4B(f); j--;)
					fprintf(xmlfile, " <AXIS>%ld</AXIS>", get4B(f));
				fputs(" </DESIGNVECTOR>\n", xmlfile);

				fprintf(xmlfile, "\t\t\t\t</FACE>\n");
			}
			fputs("\t\t\t</FONTINFO>\n", xmlfile);
			fputs("\t\t\t<STYLEINFO>\n", xmlfile);
			j = get2B(f); printf("%d\n",j);//exit(1);
			for(; j--;){
				mark = get2B(f);
				facemark = get2B(f);
				size = FIXEDPT(get4B(f)); // of course, this representation is undocumented
				tracking = FIXEDPT(get4B(f));
				kerning = FIXEDPT(get4B(f));
				leading = FIXEDPT(get4B(f));
				baseshift = FIXEDPT(get4B(f));
				autokern = fgetc(f);
				fprintf(xmlfile, "\t\t\t\t<STYLE MARK='%d' FACEMARK='%d' SIZE='%g' TRACKING='%g' KERNING='%g' LEADING='%g' BASESHIFT='%g' AUTOKERN='%d'",
						mark, facemark, size, tracking, kerning, leading, baseshift, autokern);
				if(v <= 5)
					fprintf(xmlfile, " EXTRA='%d'", fgetc(f));
				fprintf(xmlfile, " ROTATE='%d' />\n", fgetc(f));
			}
			fputs("\t\t\t</STYLEINFO>\n", xmlfile);

			type = get2B(f);
			scaling = FIXEDPT(get4B(f));
			charcount = get4B(f);
			hplace = FIXEDPT(get4B(f));
			vplace = FIXEDPT(get4B(f));
			selstart = get4B(f);
			selend = get4B(f);
			linecount = get2B(f);
			fprintf(xmlfile, "\t\t\t<TEXT TYPE='%d' SCALING='%g' CHARCOUNT='%d' HPLACEMENT='%g' VPLACEMENT='%g' SELSTART='%d' SELEND='%d'>\n",
					type, scaling, charcount, hplace, vplace, selstart, selend);
			for(i = linecount; i--;){
				charcount = get4B(f);
				orient = get2B(f);
				align = get2B(f);
				fprintf(xmlfile, "\t\t\t\t<LINE ORIENTATION='%d' ALIGNMENT='%d'>\n", orient, align);
				for(j = charcount; j--;){
					wchar_t wc = get2B(f);
					style = get2B(f);
					fprintf(xmlfile, "\t\t\t\t\t<UNICODE STYLE='%d'>%04x</UNICODE>", style, wc); // FIXME
					if(isprint(wc))
						fprintf(xmlfile, " <!--%c-->", wc);
					fputc('\n', xmlfile);
				}
				fputs("\t\t\t\t</LINE>\n", xmlfile);
			}
			fputs("\t\t\t</TEXT>\n", xmlfile);
		}else
			fputs(" /> <!-- don't know how to parse this version -->\n", xmlfile);
		fputs("\t\t", xmlfile);
	}else
		UNQUIET("    (Type tool, version = %d)\n", v);
}

void ed_unicodename(FILE *f, FILE *xmlfile, int printxml){
	unsigned long len = get4B(f);

	if(len > 0 && len < 1024){ // sanity check
		if(printxml) // FIXME: what's the right way to represent a Unicode string in XML? UTF-8?
			while(len--)
				fprintf(xmlfile,"%04x",get2B(f));
		else if(!quiet){
			fputs("    (Unicode name = '", stdout);
			while(len--)
				fputwc(get2B(f), stdout); // FIXME: not working
			fputs("')\n", stdout);
		}
	}
}

void ed_layerid(FILE *f, FILE *xmlfile, int printxml){
	unsigned long id = get4B(f);
	if(printxml)
		fprintf(xmlfile, "%lu", id);
	else
		UNQUIET("    (Layer ID = %lu)\n", id);
}

void ed_annotation(FILE *f, FILE *xmlfile, int printxml){
	int i, j, major = get2B(f), minor = get2B(f), len, open, flags;
	char type[4], key[4];
	long datalen, len2;

	if(printxml){
		fprintf(xmlfile, "\n\t\t\t<VERSION MAJOR='%d' MINOR='%d' />\n", major, minor);
		for(i = get4B(f); i--;){
			len = get4B(f);
			fread(type, 1, 4, f);
			if(!memcmp(type, "txtA", 4))
				fprintf(xmlfile, "\t\t\t<TEXT");
			else if(!memcmp(type, "sndA", 4))
				fprintf(xmlfile, "\t\t\t<SOUND");
			else
				fprintf(xmlfile, "\t\t\t<UNKNOWN");
			open = fgetc(f);
			flags = fgetc(f);
			//optblocks = get2B(f);
			//icont = get4B(f);  iconl = get4B(f);  iconb = get4B(f);  iconr = get4B(f);
			//popupt = get4B(f); popupl = get4B(f); popupb = get4B(f); popupr = get4B(f);
			fseek(f, 2+16+16+10, SEEK_CUR); // skip
			fprintf(xmlfile, " OPEN='%d' FLAGS='%d' AUTHOR='", open, flags);
			fputsxml(getpstr2(f), xmlfile);
			fputs("' NAME='", xmlfile);
			fputsxml(getpstr2(f), xmlfile);
			fputs("' MODDATE='", xmlfile);
			fputsxml(getpstr2(f), xmlfile);
			fputc('\'', xmlfile);

			len2 = get4B(f); //printf(" len2=%d\n", len2);
			fread(key, 1, 4, f);
			datalen = get4B(f); //printf(" key=%c%c%c%c datalen=%d\n", key[0],key[1],key[2],key[3],datalen);
			if(!memcmp(key, "txtC", 4)){
				char *buf = malloc(datalen/2+1);
				fputs(">\n\t\t\t\t<UNICODE>", xmlfile);
				for(j = 0; j < datalen/2; ++j){
					wchar_t wc = get2B(f);
					fprintf(xmlfile, "%04x", wc);
					buf[j] = wc;
				}
				buf[j] = 0;
				free(buf);
				fputs("</UNICODE>\n\t\t\t\t<ASCII>", xmlfile);
				fputsxml(buf, xmlfile);
				fputs("</ASCII>\n\t\t\t</TEXT>\n", xmlfile);
				// doc says this is 4-byte padded, but it is lying; don't skip.
				//fseek(f, 4 - (datalen & 3), SEEK_CUR);
			}else if(!memcmp(key, "sndM", 4)){
				// Perhaps the 'length' field is actually a sampling rate?
				// Documentation says something different, natch.
				fprintf(xmlfile, " RATE='%d' BYTES='%d' />\n", datalen, len2-12);
				fseek(f, PAD4(len2-12), SEEK_CUR);
			}else
				fputs(" /> <!-- don't know -->\n", xmlfile);
		}
		fputs("\t\t", xmlfile);
	}else
		UNQUIET("    (Annotation, version = %d.%d)\n", major, minor);
}

void ed_knockout(FILE *f, FILE *xmlfile, int printxml){
	int k = fgetc(f);
	if(printxml)
		fprintf(xmlfile, "%d", k);
	else
		UNQUIET("    (Knockout = %d)\n", k);
}

void ed_protected(FILE *f, FILE *xmlfile, int printxml){
	int k = get4B(f);
	if(printxml)
		fprintf(xmlfile, "%d", k);
	else
		UNQUIET("    (Protected = %d)\n", k);
}

void ed_referencepoint(FILE *f, FILE *xmlfile, int printxml){
	double x,y;

	x = getdoubleB(f);
	y = getdoubleB(f);
	if(printxml)
		fprintf(xmlfile, " <X>%g</X> <Y>%g</Y> ", x, y);
	else
		UNQUIET("    (Reference point X=%g Y=%g)\n", x, y);
}

void doextradata(FILE *f, long length, int printxml){
	struct extra_data extra;
	static struct dictentry{
			char *key,*tag,*desc;
			void (*func)(FILE *f, FILE *xmlf, int printxml);
		} extradict[] = {
			// v4.0
			{"levl", "LEVELS", "Levels", NULL},
			{"curv", "CURVES", "Curves", NULL},
			{"brit", "BRIGHTNESSCONTRAST", "Brightness/contrast", NULL},
			{"blnc", "COLORBALANCE", "Color balance", NULL},
			{"hue ", "HUESATURATION4", "Old Hue/saturation, Photoshop 4.0", NULL},
			{"hue2", "HUESATURATION5", "New Hue/saturation, Photoshop 5.0", NULL},
			{"selc", "SELECTIVECOLOR", "Selective color", NULL},
			{"thrs", "THRESHOLD", "Threshold", NULL},
			{"nvrt", "INVERT", "Invert", NULL},
			{"post", "POSTERIZE", "Posterize", NULL},
			// v5.0
			{"lrFX", "EFFECT", "Effects layer", NULL},
			{"tySh", "TYPETOOL6", "Type tool (6.0)", ed_typetool},
			{"TySh", "TYPETOOL", "Type tool", ed_typetool},
			{"luni", "UNICODENAME", "Unicode layer name", ed_unicodename},
			{"lyid", "LAYERID", "Layer ID", ed_layerid},
			// v6.0
			{"lfx2", "OBJECTEFFECT", "Object based effects layer", NULL},
			{"Patt", "PATTERN", "Pattern", NULL},
			{"Anno", "ANNOTATION", "Annotation", ed_annotation},
			{"clbl", "BLENDCLIPPING", "Blend clipping", NULL},
			{"infx", "BLENDINTERIOR", "Blend interior", NULL},
			{"knko", "KNOCKOUT", "Knockout", ed_knockout},
			{"lspf", "PROTECTED", "Protected", ed_protected},
			{"lclr", "SHEETCOLOR", "Sheet color", NULL},
			{"fxrp", "REFERENCEPOINT", "Reference point", ed_referencepoint},
			{"grdm", "GRADIENT", "Gradient", NULL},
			{NULL, NULL, NULL, NULL}
		};
	struct dictentry *d;

	while(length > 0){
		fread(extra.sig, 1, 4, f);
		fread(extra.key, 1, 4, f);
		extra.length = get4B(f);
		length -= 12 + extra.length;
		if(!memcmp(extra.sig, "8BIM", 4)){
			VERBOSE("    extra data: sig='%c%c%c%c' key='%c%c%c%c' length=%5lu\n",
					extra.sig[0],extra.sig[1],extra.sig[2],extra.sig[3],
					extra.key[0],extra.key[1],extra.key[2],extra.key[3],
					extra.length);
			for(d = extradict; d->key; ++d)
				if(!memcmp(extra.key, d->key, 4)){
					if(d->func){
						long savepos = ftell(f);
						if(printxml) fprintf(xmlfile, "\t\t<%s>", d->tag);
						d->func(f, xmlfile, printxml);
						if(printxml) fprintf(xmlfile, "</%s>\n", d->tag);
						fseek(f, savepos, SEEK_SET);
					}else{
						// there is no function to parse this block
						if(printxml)
							fprintf(xmlfile, "\t\t<%s /> <!-- not parsed -->\n", d->tag);
						else
							UNQUIET("    (%s data)\n", d->desc);
					}
					break;
				}
			
			fseek(f, extra.length, SEEK_CUR);
		}else{
			warn("bad signature in layer's extra data, skipping the rest");
			break;
		}
	}
}