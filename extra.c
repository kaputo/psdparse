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

/* 'Extra data' handling. *Work in progress*
 *
 * There's guesswork and trial-and-error in here,
 * due to many errors and omissions in Adobe's documentation on this (PS6 SDK).
 * It's amazing that they would try to describe a hierarchical format
 * as a flat list of fields. Reminds me of Jasc's PSP format docs, too.
 * One must assume they don't encourage people to try and USE the info.
 */

#define BITSTR(f) ((f) ? "(1)" : "(0)")

static void ed_versdesc(psd_file_t f, int level, int printxml, struct dictentry *parent);

void entertag(psd_file_t f, int level, int printxml, struct dictentry *parent, struct dictentry *d){
	psd_bytes_t savepos = ftello(f);
	int oneline = d->tag[0] == '-';
	char *tagname = d->tag + oneline;

	if(printxml){
		// check parent's one-line-ness, because what precedes our <TAG>
		// belongs to our parent.
		fprintf(xml, "%s<%s>", parent->tag[0] == '-' ? " " : tabs(level), tagname);
		if(!oneline)
			fputc('\n', xml);
	}

	d->func(f, level+1, printxml, d); // parse contents of this datum

	if(printxml){
		fprintf(xml, "%s</%s>", oneline ? "" : tabs(level), tagname);
		// if parent's not one-line, then we can safely newline after our tag.
		fputc(parent->tag[0] == '-' ? ' ' : '\n', xml);
	}

	fseeko(f, savepos, SEEK_SET);
}

// This uses a dumb linear search. But it's efficient enough in practice.

struct dictentry *findbykey(psd_file_t f, int level, struct dictentry *parent, char *key, int printxml){
	struct dictentry *d;

	for(d = parent; d->key; ++d)
		if(!memcmp(key, d->key, 4)){
			char *tagname = d->tag + (d->tag[0] == '-');
			//fprintf(stderr, "matched tag %s\n", d->tag);
			if(d->func)
				entertag(f, level, printxml, parent, d);
			else{
				// there is no function to parse this block.
				// because tag is empty in this case, we only need to consider
				// parent's one-line-ness.
				if(printxml){
					if(parent->tag[0] == '-')
						fprintf(xml, " <%s /> <!-- not parsed --> ", tagname);
					else
						fprintf(xml, "%s<%s /> <!-- not parsed -->\n", tabs(level), tagname);
				}
			}
			return d;
		}
	return NULL;
}

static void colorspace(psd_file_t f, int level){
	// this map is taken from Colour Samplers; I'm guessing it applies generally
	static char *spaces[] = {"kDummySpace" /* = -1 */, "kRGBSpace",
		"kHSBSpace", "kCMYKSpace", "kPantoneSpace", "kFocoltoneSpace",
		"kTrumatchSpace", "kToyoSpace", "kLabSpace", "kGraySpace",
		"kWideCMYKSpace", "kHKSSpace", "kDICSpace", "kTotalInkSpace",
		"kMonitorRGBSpace", "kDuotoneSpace", "kOpacitySpace"};
	int i, space = get2B(f);

	fprintf(xml, "%s<COLOR SPACE='%d'", tabs(level), space);
	if(space >= -1 && space < (int)(sizeof(spaces)/sizeof(*spaces))-1)
		fprintf(xml, " NAME='%s'", spaces[space+1]);
	fputc('>', xml);
	for(i = 0; i < 4; ++i)
		fprintf(xml, " <C%d>%g</C%d>", i, FIXEDPT(get2Bu(f)), i);
	fputs(" </COLOR>\n", xml);
}

static void ed_typetool(psd_file_t f, int level, int printxml, struct dictentry *parent){
	int i, j, v = get2B(f), mark, type, script, facemark,
		autokern, charcount, selstart, selend, linecount, orient, align, style;
	double size, tracking, kerning, leading, baseshift, scaling, hplace, vplace;
	static char *coeff[] = {"XX","XY","YX","YY","TX","TY"}; // from CS doc
	const char *indent = tabs(level);

	if(printxml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, v);

		// read transform (6 doubles)
		fprintf(xml, "%s<TRANSFORM>", indent);
		for(i = 0; i < 6; ++i)
			fprintf(xml, " <%s>%g</%s>", coeff[i], getdoubleB(f), coeff[i]);
		fputs(" </TRANSFORM>\n", xml);

		// read font information
		v = get2B(f);
		fprintf(xml, "%s<FONTINFOVERSION>%d</FONTINFOVERSION>\n", indent, v);
		if(v <= 6){
			for(i = get2B(f); i--;){
				mark = get2B(f);
				type = get4B(f);
				fprintf(xml, "%s<FACE MARK='%d' TYPE='%d' FONTNAME='%s'", indent, mark, type, getpstr(f));
				fprintf(xml, " FONTFAMILY='%s'", getpstr(f));
				fprintf(xml, " FONTSTYLE='%s'", getpstr(f));
				script = get2B(f);
				fprintf(xml, " SCRIPT='%d'>\n", script);

				// doc is unclear, but this may work:
				fprintf(xml, "%s\t<DESIGNVECTOR>", indent);
				for(j = get4B(f); j--;)
					fprintf(xml, " <AXIS>%ld</AXIS>", get4B(f));
				fputs(" </DESIGNVECTOR>\n", xml);

				fprintf(xml, "%s</FACE>\n", indent);
			}

			j = get2B(f); // count of styles
			for(; j--;){
				mark = get2B(f);
				facemark = get2B(f);
				size = FIXEDPT(get4B(f)); // of course, which fields are fixed point is undocumented
				tracking = FIXEDPT(get4B(f));  // so I'm
				kerning = FIXEDPT(get4B(f));   // taking
				leading = FIXEDPT(get4B(f));   // a punt
				baseshift = FIXEDPT(get4B(f)); // on these
				autokern = fgetc(f);
				fprintf(xml, "%s<STYLE MARK='%d' FACEMARK='%d' SIZE='%g' TRACKING='%g' KERNING='%g' LEADING='%g' BASESHIFT='%g' AUTOKERN='%d'",
						indent, mark, facemark, size, tracking, kerning, leading, baseshift, autokern);
				if(v <= 5)
					fprintf(xml, " EXTRA='%d'", fgetc(f));
				fprintf(xml, " ROTATE='%d' />\n", fgetc(f));
			}

			type = get2B(f);
			scaling = FIXEDPT(get4B(f));
			charcount = get4B(f);
			hplace = FIXEDPT(get4B(f));
			vplace = FIXEDPT(get4B(f));
			selstart = get4B(f);
			selend = get4B(f);
			linecount = get2B(f);
			fprintf(xml, "%s<TEXT TYPE='%d' SCALING='%g' CHARCOUNT='%d' HPLACEMENT='%g' VPLACEMENT='%g' SELSTART='%d' SELEND='%d'>\n",
					indent, type, scaling, charcount, hplace, vplace, selstart, selend);
			for(i = linecount; i--;){
				char *buf;
				charcount = get4B(f);
				buf = malloc(charcount+1);
				orient = get2B(f);
				align = get2B(f);
				fprintf(xml, "%s\t<LINE ORIENTATION='%d' ALIGNMENT='%d'>\n", indent, orient, align);
				for(j = 0; j < charcount; ++j){
					wchar_t wc = get2Bu(f);
					buf[j] = wc; // FIXME: this is not the right way to get ASCII
					style = get2B(f);
					fprintf(xml, "%s\t\t<UNICODE STYLE='%d'>", indent, style);
					fputcxml(wc, xml);
					fputs("</UNICODE>\n", xml);
				}
				buf[j] = 0;
				fprintf(xml, "%s\t\t<STRING>", indent);
				fputsxml(buf, xml);
				fprintf(xml, "</STRING>\n%s\t</LINE>\n", indent);
				free(buf);
			}
			colorspace(f, level+1);
			fprintf(xml, "%s\t<ANTIALIAS>%d</ANTIALIAS>\n", indent, fgetc(f));

			fprintf(xml, "%s</TEXT>\n", indent);
		//}else if(v == 50){
		//	ed_versdesc(f, level, printxml, parent); // text
		//	ed_versdesc(f, level, printxml, parent); // warp
		}else
			fprintf(xml, "%s<!-- don't know how to parse this version -->\n", indent);
	}else
		UNQUIET("    (%s, version = %d)\n", parent->desc, v);
}

static void ed_unicodename(psd_file_t f, int level, int printxml, struct dictentry *parent){
	unsigned long len = get4B(f); // character count, not byte count

	if(len > 0 && len < 1024){ // sanity check
		if(printxml)
			while(len--)
				fputcxml(get2Bu(f), xml);
		else if(!quiet){
			fputs("    (Unicode name = '", stdout);
			while(len--)
				putwchar(get2Bu(f)); // FIXME: not working
			fputs("')\n", stdout);
		}
	}
}

static void ed_long(psd_file_t f, int level, int printxml, struct dictentry *parent){
	unsigned long id = get4B(f);
	if(printxml)
		fprintf(xml, "%lu", id);
	else
		UNQUIET("    (%s = %lu)\n", parent->desc, id);
}

static void ed_key(psd_file_t f, int level, int printxml, struct dictentry *parent){
	char *key = getkey(f);
	if(printxml)
		fprintf(xml, "%s", key);
	else
		UNQUIET("    (%s = '%s')\n", parent->desc, key);
}

static void ed_annotation(psd_file_t f, int level, int printxml, struct dictentry *parent){
	int i, j, major = get2B(f), minor = get2B(f), len, open, flags;
	char type[4], key[4];
	const char *indent = tabs(level);
	long datalen, len2, rects[8];

	if(printxml){
		fprintf(xml, "%s<VERSION MAJOR='%d' MINOR='%d' />\n", indent, major, minor);
		for(i = get4B(f); i--;){
			len = get4B(f);
			fread(type, 1, 4, f);
			open = fgetc(f);
			flags = fgetc(f);
			get2B(f); // optblocks
			// read two rectangles - icon and popup
			for(j = 0; j < 8;)
				rects[j++] = get4B(f);
			colorspace(f, level);

			if(!memcmp(type, "txtA", 4))
				fprintf(xml, "%s<TEXT", indent);
			else if(!memcmp(type, "sndA", 4))
				fprintf(xml, "%s<SOUND", indent);
			else
				fprintf(xml, "%s<UNKNOWN", indent);
			fprintf(xml, " OPEN='%d' FLAGS='%d' AUTHOR='", open, flags);
			fputsxml(getpstr2(f), xml);
			fputs("' NAME='", xml);
			fputsxml(getpstr2(f), xml);
			fputs("' MODDATE='", xml);
			fputsxml(getpstr2(f), xml);
			fprintf(xml, "' ICONT='%ld' ICONL='%ld' ICONB='%ld' ICONR='%ld'", rects[0],rects[1],rects[2],rects[3]);
			fprintf(xml, " POPUPT='%ld' POPUPL='%ld' POPUPB='%ld' POPUPR='%ld'", rects[4],rects[5],rects[6],rects[7]);

			len2 = get4B(f)-12; // remaining bytes in annotation
			fread(key, 1, 4, f);
			datalen = get4B(f); //printf(" key=%c%c%c%c datalen=%ld\n", key[0],key[1],key[2],key[3],datalen);
			if(!memcmp(key, "txtC", 4)){
				// Once again, the doc lets us down:
				// - it says "ASCII or Unicode," but doesn't say how each is distinguished;
				// - one might think it has something to do with the mysterious four bytes
				//   stuck to the beginning of the data.
				char *buf = malloc(datalen/2+1);
				fprintf(xml, ">\n%s\t<UNICODE>", indent);
				for(j = 0; j < datalen/2; ++j){
					wchar_t wc = get2Bu(f);
					buf[j] = wc; // FIXME: this is not the right way to get ASCII
					fputcxml(wc, xml);
				}
				buf[j] = 0;
				fprintf(xml, "</UNICODE>\n%s\t<STRING>", indent);
				fputsxml(buf, xml);
				fprintf(xml, "</STRING>\n%s</TEXT>\n", indent);
				len2 -= datalen; // we consumed this much from the file
				free(buf);
			}else if(!memcmp(key, "sndM", 4)){
				// Perhaps the 'length' field is actually a sampling rate?
				// Documentation says something different, natch.
				fprintf(xml, " RATE='%ld' BYTES='%ld' />\n", datalen, len2);
			}else
				fputs(" /> <!-- don't know -->\n", xml);

			fseeko(f, len2, SEEK_CUR); // skip whatever's left of this annotation's data
		}
	}else
		UNQUIET("    (%s, version = %d.%d)\n", parent->desc, major, minor);
}

static void ed_byte(psd_file_t f, int level, int printxml, struct dictentry *parent){
	int k = fgetc(f);
	if(printxml)
		fprintf(xml, "%d", k);
	else
		UNQUIET("    (%s = %d)\n", parent->desc, k);
}

static void ed_referencepoint(psd_file_t f, int level, int printxml, struct dictentry *parent){
	double x,y;

	x = getdoubleB(f);
	y = getdoubleB(f);
	if(printxml)
		fprintf(xml, " <X>%g</X> <Y>%g</Y> ", x, y);
	else
		UNQUIET("    (%s X=%g Y=%g)\n", parent->desc, x, y);
}

// CS doc
static void ed_descriptor(psd_file_t f, int level, int printxml, struct dictentry *parent){
	if(printxml)
		descriptor(f, level, printxml, parent); // TODO: pass flag to extract value data
}

// CS doc
static void ed_versdesc(psd_file_t f, int level, int printxml, struct dictentry *parent){
	if(printxml)
		fprintf(xml, "%s<DESCRIPTORVERSION>%ld</DESCRIPTORVERSION>\n", tabs(level), get4B(f));
	ed_descriptor(f, level, printxml, parent);
}

// CS doc
static void ed_objecteffects(psd_file_t f, int level, int printxml, struct dictentry *parent){
	if(printxml)
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", tabs(level), get4B(f));
	ed_versdesc(f, level, printxml, parent);
}

static int sigkeyblock(psd_file_t f, int level, int printxml, struct dictentry *dict){
	char sig[4], key[4];
	long len;
	struct dictentry *d;

	fread(sig, 1, 4, f);
	fread(key, 1, 4, f);
	len = get4B(f);
	if(!memcmp(sig, "8BIM", 4)){
		if(!printxml)
			VERBOSE("    data block: key='%c%c%c%c' length=%5ld\n",
					key[0],key[1],key[2],key[3], len);
		if(dict && (d = findbykey(f, level, dict, key, printxml)) && !d->func && !printxml)
			// there is no function to parse this block
			UNQUIET("    (data: %s)\n", d->desc);
		fseeko(f, len, SEEK_CUR);
		return len + 12; // return number of bytes consumed
	}
	return 0; // bad signature
}

/* not 'static'; these are referenced by scavenge.c */
struct dictentry bmdict[] = {
	{0, "norm", "NORMAL", "normal", NULL},
	{0, "dark", "DARKEN", "darken", NULL},
	{0, "lite", "LIGHTEN", "lighten", NULL},
	{0, "hue ", "HUE", "hue", NULL},
	{0, "sat ", "SATURATION", "saturation", NULL},
	{0, "colr", "COLOR", "color", NULL},
	{0, "lum ", "LUMINOSITY", "luminosity", NULL},
	{0, "mul ", "MULTIPLY", "multiply", NULL},
	{0, "scrn", "SCREEN", "screen", NULL},
	{0, "diss", "DISSOLVE", "dissolve", NULL},
	{0, "over", "OVERLAY", "overlay", NULL},
	{0, "hLit", "HARDLIGHT", "hard light", NULL},
	{0, "sLit", "SOFTLIGHT", "soft light", NULL},
	{0, "diff", "DIFFERENCE", "difference", NULL},
	{0, "smud", "EXCLUSION", "exclusion", NULL},
	{0, "div ", "COLORDODGE", "color dodge", NULL},
	{0, "idiv", "COLORBURN", "color burn", NULL},
	// CS
	{0, "lbrn", "LINEARBURN", "linear burn", NULL},
	{0, "lddg", "LINEARDODGE", "linear dodge", NULL},
	{0, "vLit", "VIVIDLIGHT", "vivid light", NULL},
	{0, "lLit", "LINEARLIGHT", "linear light", NULL},
	{0, "pLit", "PINLIGHT", "pin light", NULL},
	{0, "hMix", "HARDMIX", "hard mix", NULL},
	{0, NULL, NULL, NULL, NULL}
};

void layerblendmode(psd_file_t f, int level, int printxml, struct blend_mode_info *bm){
	struct dictentry *d;
	const char *indent = tabs(level);

	if(printxml && !memcmp(bm->sig, "8BIM", 4)){
		fprintf(xml, "%s<BLENDMODE OPACITY='%g' CLIPPING='%d'>\n",
				indent, bm->opacity/2.55, bm->clipping);
		findbykey(f, level+1, bmdict, bm->key, printxml);
		if(bm->flags & 1) fprintf(xml, "%s\t<TRANSPARENCYPROTECTED />\n", indent);
		if(bm->flags & 2) fprintf(xml, "%s\t<VISIBLE />\n", indent);
		if((bm->flags & (8|16)) == (8|16))  // both bits set
			fprintf(xml, "%s\t<PIXELDATAIRRELEVANT />\n", indent);
		fprintf(xml, "%s</BLENDMODE>\n", indent);
	}
	if(!printxml){
		d = findbykey(f, level+1, bmdict, bm->key, printxml);
		VERBOSE("  blending mode: sig='%c%c%c%c' key='%c%c%c%c'(%s) opacity=%d(%d%%) clipping=%d(%s)\n\
    flags=%#x(transp_prot%s visible%s bit4valid%s pixel_data_irrelevant%s)\n",
				bm->sig[0],bm->sig[1],bm->sig[2],bm->sig[3],
				bm->key[0],bm->key[1],bm->key[2],bm->key[3],
				d ? d->desc : "???",
				bm->opacity, (bm->opacity*100+127)/255,
				bm->clipping, bm->clipping ? "non-base" : "base",
				bm->flags, BITSTR(bm->flags&1), BITSTR(bm->flags&2), BITSTR(bm->flags&8), BITSTR(bm->flags&16) );
	}
}

// CS doc
static void blendmode(psd_file_t f, int level, int printxml, struct dictentry *parent){
	char sig[4], key[4];

	fread(sig, 1, 4, f);
	fread(key, 1, 4, f);
	if(printxml && !memcmp(sig, "8BIM", 4)){
		fprintf(xml, "%s<BLENDMODE>\n", tabs(level));
		findbykey(f, level+1, bmdict, key, printxml);
		fprintf(xml, "%s</BLENDMODE>\n", tabs(level));
	}
}

// CS doc
static void fx_commonstate(psd_file_t f, int level, int printxml, struct dictentry *parent){
	if(printxml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", tabs(level), get4B(f));
		fprintf(xml, "%s<VISIBLE>%d</VISIBLE>\n", tabs(level), fgetc(f));
	}
}

// CS doc
static void fx_shadow(psd_file_t f, int level, int printxml, struct dictentry *parent){
	const char *indent = tabs(level);

	if(printxml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, get4B(f));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f))); // this is fixed point, but you wouldn't know it from the doc
		fprintf(xml, "%s<INTENSITY>%g</INTENSITY>\n", indent, FIXEDPT(get4B(f))); // they're trying to make it more interesting for
		fprintf(xml, "%s<ANGLE>%g</ANGLE>\n", indent, FIXEDPT(get4B(f)));         // implementors, I guess, by setting little puzzles
		fprintf(xml, "%s<DISTANCE>%g</DISTANCE>\n", indent, FIXEDPT(get4B(f)));   // "pit yourself against our documentation!"
		colorspace(f, level);
		blendmode(f, level, printxml, parent);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<USEANGLE>%d</USEANGLE>\n", indent, fgetc(f));
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55); // doc implies this is a percentage; it's not, it's 0-255 as usual
		colorspace(f, level);
	}
}

// CS doc
static void fx_outerglow(psd_file_t f, int level, int printxml, struct dictentry *parent){
	const char *indent = tabs(level);

	if(printxml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, get4B(f));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<INTENSITY>%g</INTENSITY>\n", indent, FIXEDPT(get4B(f)));
		colorspace(f, level);
		blendmode(f, level, printxml, parent);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55);
		colorspace(f, level);
	}
}

// CS doc
static void fx_innerglow(psd_file_t f, int level, int printxml, struct dictentry *parent){
	const char *indent = tabs(level);
	long version;

	if(printxml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, version = get4B(f));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<INTENSITY>%g</INTENSITY>\n", indent, FIXEDPT(get4B(f)));
		colorspace(f, level);
		blendmode(f, level, printxml, parent);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55);
		if(version==2)
			fprintf(xml, "%s<INVERT>%d</INVERT>\n", indent, fgetc(f));
		colorspace(f, level);
	}
}

// CS doc
static void fx_bevel(psd_file_t f, int level, int printxml, struct dictentry *parent){
	const char *indent = tabs(level);
	long version;

	if(printxml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, version = get4B(f));
		fprintf(xml, "%s<ANGLE>%g</ANGLE>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<STRENGTH>%g</STRENGTH>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f)));
		blendmode(f, level, printxml, parent);
		blendmode(f, level, printxml, parent);
		colorspace(f, level);
		colorspace(f, level);
		fprintf(xml, "%s<STYLE>%d</STYLE>\n", indent, fgetc(f));
		fprintf(xml, "%s<HIGHLIGHTOPACITY>%g</HIGHLIGHTOPACITY>\n", indent, fgetc(f)/2.55);
		fprintf(xml, "%s<SHADOWOPACITY>%g</SHADOWOPACITY>\n", indent, fgetc(f)/2.55);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<USEANGLE>%d</USEANGLE>\n", indent, fgetc(f));
		fprintf(xml, "%s<UPDOWN>%d</UPDOWN>\n", indent, fgetc(f)); // heh, interpretation is undocumented
		if(version==2){
			colorspace(f, level);
			colorspace(f, level);
		}
	}
}

// CS doc
static void fx_solidfill(psd_file_t f, int level, int printxml, struct dictentry *parent){
	const char *indent = tabs(level);
	long version;

	if(printxml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, version = get4B(f));
		// blendmode is the usual 8 bytes; doc only mentions 4
		blendmode(f, level, printxml, parent);
		colorspace(f, level);
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		colorspace(f, level);
	}
}

// CS doc
static void ed_layereffects(psd_file_t f, int level, int printxml, struct dictentry *parent){
	static struct dictentry fxdict[] = {
		{0, "cmnS", "COMMONSTATE", "common state", fx_commonstate},
		{0, "dsdw", "DROPSHADOW", "drop shadow", fx_shadow},
		{0, "isdw", "INNERSHADOW", "inner shadow", fx_shadow},
		{0, "oglw", "OUTERGLOW", "outer glow", fx_outerglow},
		{0, "iglw", "INNERGLOW", "inner glow", fx_innerglow},
		{0, "bevl", "BEVEL", "bevel", fx_bevel},
		{0, "sofi", "SOLIDFILL", "solid fill", fx_solidfill}, // Photoshop 7.0
		{0, NULL, NULL, NULL, NULL}
	};
	int count;

	if(printxml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", tabs(level), get2B(f));
		for(count = get2B(f); count--;)
			if(!sigkeyblock(f, level, printxml, fxdict))
				break; // got bad signature
	}
}

static void mdblock(psd_file_t f, int level, int printxml){
	char sig[4], key[4];
	long len;
	int copy;
	const char *indent = tabs(level);

	fread(sig, 1, 4, f);
	fread(key, 1, 4, f);
	copy = fgetc(f);
	fseeko(f, 3, SEEK_CUR); // padding
	len = get4B(f);
	if(printxml){
		fprintf(xml, "%s<METADATA SIG='%c%c%c%c' KEY='%c%c%c%c'>\n", indent,
				sig[0],sig[1],sig[2],sig[3], key[0],key[1],key[2],key[3]);
		fprintf(xml, "\t%s<COPY>%d</COPY>\n", indent, copy);
		fprintf(xml, "\t%s<!-- %ld bytes of undocumented data -->\n", indent, len); // the documentation tells us that it's undocumented
		fprintf(xml, "%s</METADATA>\n", indent);
	}else
		UNQUIET("    (Metadata: sig='%c%c%c%c' key='%c%c%c%c' %ld bytes)\n",
				sig[0],sig[1],sig[2],sig[3], key[0],key[1],key[2],key[3], len);
}

// v6 doc
static void ed_metadata(psd_file_t f, int level, int printxml, struct dictentry *parent){
	long count;

	for(count = get4B(f); count--;)
		mdblock(f, level, printxml);
}

void doadditional(psd_file_t f, int level, psd_bytes_t length, int printxml){
	static struct dictentry extradict[] = {
		// v4.0
		{0, "levl", "LEVELS", "Levels", NULL},
		{0, "curv", "CURVES", "Curves", NULL},
		{0, "brit", "BRIGHTNESSCONTRAST", "Brightness/contrast", NULL},
		{0, "blnc", "COLORBALANCE", "Color balance", NULL},
		{0, "hue ", "HUESATURATION4", "Old Hue/saturation, Photoshop 4.0", NULL},
		{0, "hue2", "HUESATURATION5", "New Hue/saturation, Photoshop 5.0", NULL},
		{0, "selc", "SELECTIVECOLOR", "Selective color", NULL},
		{0, "thrs", "THRESHOLD", "Threshold", NULL},
		{0, "nvrt", "INVERT", "Invert", NULL},
		{0, "post", "POSTERIZE", "Posterize", NULL},
		// v5.0
		{0, "lrFX", "EFFECT", "Effects layer", ed_layereffects},
		{0, "tySh", "TYPETOOL5", "Type tool (5.0)", ed_typetool},
		{0, "luni", "-UNICODENAME", "Unicode layer name", ed_unicodename},
		{0, "lyid", "-LAYERID", "Layer ID", ed_long}, // '-' prefix means keep tag value on one line
		// v6.0
		{0, "lfx2", "OBJECTEFFECT", "Object based effects layer", NULL /*ed_objecteffects*/},
		{0, "Patt", "PATTERN", "Pattern", NULL},
		{0, "Pat2", "PATTERNCS", "Pattern (CS)", NULL},
		{0, "Anno", "ANNOTATION", "Annotation", ed_annotation},
		{0, "clbl", "-BLENDCLIPPING", "Blend clipping", ed_byte},
		{0, "infx", "-BLENDINTERIOR", "Blend interior", ed_byte},
		{0, "knko", "-KNOCKOUT", "Knockout", ed_byte},
		{0, "lspf", "-PROTECTED", "Protected", ed_long},
		{0, "lclr", "SHEETCOLOR", "Sheet color", NULL},
		{0, "fxrp", "-REFERENCEPOINT", "Reference point", ed_referencepoint},
		{0, "grdm", "GRADIENT", "Gradient", NULL},
		{0, "lsct", "-SECTION", "Section divider", ed_long}, // CS doc
		{0, "SoCo", "SOLIDCOLORSHEET", "Solid color sheet", NULL /*ed_versdesc*/}, // CS doc
		{0, "PtFl", "PATTERNFILL", "Pattern fill", NULL /*ed_versdesc*/}, // CS doc
		{0, "GdFl", "GRADIENTFILL", "Gradient fill", NULL /*ed_versdesc*/}, // CS doc
		{0, "vmsk", "VECTORMASK", "Vector mask", NULL}, // CS doc
		{0, "TySh", "TYPETOOL6", "Type tool (6.0)", ed_typetool}, // CS doc
		{0, "ffxi", "-FOREIGNEFFECTID", "Foreign effect ID", ed_long}, // CS doc (this is probably a key too, who knows)
		{0, "lnsr", "-LAYERNAMESOURCE", "Layer name source", ed_key}, // CS doc (who knew this was a signature? docs fail again - and what do the values mean?)
		{0, "shpa", "PATTERNDATA", "Pattern data", NULL}, // CS doc
		{0, "shmd", "METADATASETTING", "Metadata setting", ed_metadata}, // CS doc
		{0, "brst", "BLENDINGRESTRICTIONS", "Channel blending restrictions", NULL}, // CS doc
		// v7.0
		{0, "lyvr", "-LAYERVERSION", "Layer version", ed_long}, // CS doc
		{0, "tsly", "-TRANSPARENCYSHAPES", "Transparency shapes layer", ed_byte}, // CS doc
		{0, "lmgm", "-LAYERMASKASGLOBALMASK", "Layer mask as global mask", ed_byte}, // CS doc
		{0, "vmgm", "-VECTORMASKASGLOBALMASK", "Vector mask as global mask", ed_byte}, // CS doc
		// CS
		{0, "mixr", "CHANNELMIXER", "Channel mixer", NULL}, // CS doc
		{0, "phfl", "PHOTOFILTER", "Photo Filter", NULL}, // CS doc
		{0, NULL, NULL, NULL, NULL}
	};

	while(length >= 12){
		psd_bytes_t block = sigkeyblock(f, level, printxml, extradict);
		if(!block){
			warn("bad signature in layer's extra data, skipping the rest");
			break;
		}
		length -= block;
	}
}