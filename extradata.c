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

char *getpstr(FILE *f){
	static char pstr[0x100];

	int len = fgetc(f) & 0xff;
	fread(pstr, 1, len, f);
	pstr[len] = 0;
	return pstr;
}

#define FIXEDPT(x) ((x)/65536.)

void ed_typetool(FILE *f, FILE *xmlfile, int printxml, struct extra_data *hdr){
	int i, j, v = get2B(f), mark, type, script, count, dvector, facemark,
		autokern, rotate, charcount, selstart, selend, linecount, orient, align, style;
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
					fprintf(xmlfile, " <AXIS>%d</AXIS>", get4B(f));
				fputs(" </DESIGNVECTOR>\n", xmlfile);

				fprintf(xmlfile, "\t\t\t\t</FACE>\n");
			}
			fputs("\t\t\t</FONTINFO>\n", xmlfile);
			fputs("\t\t\t<STYLEINFO>\n", xmlfile);
			j = get2B(f); printf("%d\n",j);//exit(1);
			for(; j--;){
				mark = get2B(f);
				facemark = get2B(f);
				size = FIXEDPT(get4B(f));
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
					fprintf(xmlfile, "\t\t\t\t\t<UNICODE STYLE='%d'>%04x</UNICODE>", style, wc);
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

void ed_unicodename(FILE *f, FILE *xmlfile, int printxml, struct extra_data *hdr){
	unsigned long len = get4B(f);
	unsigned i;

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

void ed_layerid(FILE *f, FILE *xmlfile, int printxml, struct extra_data *hdr){
	unsigned long id = get4B(f);
	if(printxml)
		fprintf(xmlfile, "%lu", id);
	else
		UNQUIET("    (Layer ID = %lu)\n", id);
}

void ed_annotation(FILE *f, FILE *xmlfile, int printxml, struct extra_data *hdr){
	int major = get2B(f), minor = get2B(f);
	if(printxml)
		fprintf(xmlfile, "<VERSION MAJOR='%d' MINOR='%d' />\n", major, minor);
	else
		UNQUIET("    (Annotation, version = %d.%d)\n", major, minor);
}

void ed_knockout(FILE *f, FILE *xmlfile, int printxml, struct extra_data *hdr){
	int k = fgetc(f);
	if(printxml)
		fprintf(xmlfile, "%d", k);
	else
		UNQUIET("    (Knockout = %d)\n", k);
}

void ed_protected(FILE *f, FILE *xmlfile, int printxml, struct extra_data *hdr){
	int k = get4B(f);
	if(printxml)
		fprintf(xmlfile, "%d", k);
	else
		UNQUIET("    (Protected = %d)\n", k);
}

void ed_referencepoint(FILE *f, FILE *xmlfile, int printxml, struct extra_data *hdr){
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
			void (*func)(FILE *f, FILE *xmlf, int printxml, struct extra_data *hdr);
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
			if(!printxml)
				VERBOSE("    extra data: sig='%c%c%c%c' key='%c%c%c%c' length=%5lu\n",
						extra.sig[0],extra.sig[1],extra.sig[2],extra.sig[3],
						extra.key[0],extra.key[1],extra.key[2],extra.key[3],
						extra.length);
			for(d = extradict; d->key; ++d)
				if(!memcmp(extra.key, d->key, 4)){
					if(d->func){
						long savepos = ftell(f);
						if(printxml) fprintf(xmlfile, "\t\t<%s>", d->tag);
						d->func(f, xmlfile, printxml, &extra);
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
