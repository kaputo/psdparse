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

static void desc_class(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_reference(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_list(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_double(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_unitfloat(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_unicodestr(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_enumerated(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_integer(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_boolean(psd_file_t f, int level, int printxml, struct dictentry *parent);
static void desc_alias(psd_file_t f, int level, int printxml, struct dictentry *parent);

static void ascii_string(psd_file_t f, long count){
	fputs(" <STRING>", xml);
	while(count--)
		fputcxml(fgetc(f), xml);
	fputs("</STRING>", xml);
}

/* PDF data. This has embedded literal strings in UTF-16, e.g.:

00000820  74 6f 72 0a 09 09 3c 3c  0a 09 09 09 2f 54 65 78  |tor...<<..../Tex|
00000830  74 20 28 fe ff ac f5 ac  1c 00 20 d3 ec d1 a0 c5  |t (....... .....|
00000840  68 bc 94 00 20 d5 04 b8  5c 5c ad f8 b7 a8 c7 78  |h... ...\\.....x|
00000850  00 20 b9 e5 c2 a4 d3 98  c7 74 d3 7c c7 58 00 20  |. .......t.|.X. |
00000860  d3 ec d1 a0 c0 f5 00 20  d3 0c c7 7c 00 20 cd 9c  |....... ...|. ..|
00000870  b8 25 c7 44 00 20 c7 04  d5 5c 5c 00 2c 00 20 00  |.%.D. ...\\.,. .|
00000880  50 00 53 00 44 d3 0c c7  7c c7 58 00 20 b0 b4 bd  |P.S.D...|.X. ...|
00000890  80 00 20 ad 6c c8 70 00  20 bd 84 c1 1d d3 0c c7  |.. .l.p. .......|
000008a0  7c c7 85 b2 c8 b2 e4 00  2e 00 0d 00 0d 00 0d 29  ||..............)|

From PDF Reference 1.7, 3.8.1 Text String Type

The text string type is used for character strings that are encoded in either PDFDocEncoding
or the UTF-16BE Unicode character encoding scheme. PDFDocEncoding
can encode all of the ISO Latin 1 character set and is documented in Appendix D. ...

For text strings encoded in Unicode, the first two bytes must be 254 followed by 255.
These two bytes represent the Unicode byte order marker, U+FEFF,
indicating that the string is encoded in the UTF-16BE (big-endian) encoding scheme ...

Note: Applications that process PDF files containing Unicode text strings
should be prepared to handle supplementary characters;
that is, characters requiring more than two bytes to represent.
*/

// TODO: re-encode the embedded Unicode text strings as UTF-8;
//       perhaps parse out keys from PDF and emit some corresponding XML structure.

static void desc_pdf(psd_file_t f, int level, int printxml, struct dictentry *parent){
	long count = get4B(f);
	unsigned char *buf = malloc(count), *p;

	fputs("<![CDATA[", xml);
	if(buf){
		size_t inb = fread(buf, 1, count, f);
/*
		paren = 0;
		for(p = buf, n = inb; n--;){
			c = *p++;
			if(c == '('){
				++paren;
				if(p[0] == 0xfe && p[1] == 0xff)
					;
			}
		}
*/
		fwrite(buf, 1, inb, xml);
		free(buf);
	}
	fputs("]]>\n", xml);
}

static void stringorid(psd_file_t f, int level, char *tag){
	long count = get4B(f);
	fprintf(xml, "%s<%s>", tabs(level), tag);
	if(count)
		ascii_string(f, count);
	else{
		fputs(" <ID>", xml);
		fputsxml(getkey(f), xml);
		fputs("</ID>", xml);
	}
	fprintf(xml, " </%s>\n", tag);
}

static void ref_property(psd_file_t f, int level, int printxml, struct dictentry *parent){
	desc_class(f, level, printxml, parent);
	stringorid(f, level, "KEY");
}

static void ref_enumref(psd_file_t f, int level, int printxml, struct dictentry *parent){
	desc_class(f, level, printxml, parent);
	desc_enumerated(f, level, printxml, parent);
}

static void ref_offset(psd_file_t f, int level, int printxml, struct dictentry *parent){
	desc_class(f, level, printxml, parent);
	desc_integer(f, level, printxml, parent);
}

static void desc_class(psd_file_t f, int level, int printxml, struct dictentry *parent){
	desc_unicodestr(f, level, printxml, parent);
	stringorid(f, level, "CLASS");
}

static void desc_reference(psd_file_t f, int level, int printxml, struct dictentry *parent){
	static struct dictentry refdict[] = {
		// all functions must be present, for this to parse correctly
		{0, "prop", "PROPERTY", "Property", ref_property},
		{0, "Clss", "CLASS", "Class", desc_class},
		{0, "Enmr", "ENUMREF", "Enumerated Reference", ref_enumref},
		{0, "rele", "-OFFSET", "Offset", ref_offset}, // '-' prefix means keep tag value on one line
		{0, "Idnt", "IDENTIFIER", "Identifier", NULL}, // doc is missing?!
		{0, "indx", "INDEX", "Index", NULL}, // doc is missing?!
		{0, "name", "NAME", "Name", NULL}, // doc is missing?!
		{0, NULL, NULL, NULL, NULL}
	};
	long count = get4B(f);
	while(count-- && findbykey(f, level, refdict, getkey(f), printxml, 0))
		;
}

struct dictentry *item(psd_file_t f, int level){
	static struct dictentry itemdict[] = {
		{0, "obj ", "REFERENCE", "Reference", desc_reference},
		{0, "Objc", "DESCRIPTOR", "Descriptor", descriptor}, // doc missing?!
		{0, "list", "LIST", "List", desc_list}, // not documented?
		{0, "VlLs", "LIST", "List", desc_list},
		{0, "doub", "-DOUBLE", "Double", desc_double}, // '-' prefix means keep tag value on one line
		{0, "UntF", "-UNITFLOAT", "Unit float", desc_unitfloat},
		{0, "TEXT", "-STRING", "String", desc_unicodestr},
		{0, "enum", "ENUMERATED", "Enumerated", desc_enumerated}, // Enmr? (see v6 rel2)
		{0, "long", "-INTEGER", "Integer", desc_integer},
		{0, "bool", "-BOOLEAN", "Boolean", desc_boolean},
		{0, "GlbO", "GLOBALOBJECT", "GlobalObject same as Descriptor", descriptor},
		{0, "type", "CLASS", "Class", desc_class},  // doc missing?! - Clss? (see v6 rel2)
		{0, "GlbC", "CLASS", "Class", desc_class}, // doc missing?!
		{0, "alis", "-ALIAS", "Alias", desc_alias},
		{0, "tdta", "ENGINEDATA", "Engine Data", desc_pdf}, // undocumented, apparently PDF syntax data
		{0, NULL, NULL, NULL, NULL}
	};
	char *k;
	struct dictentry *p;

	stringorid(f, level, "KEY");
	p = findbykey(f, level, itemdict, k = getkey(f), 1, 0);

	if(!p){
		fprintf(stderr, "### item(): unknown key '%s'; file offset %#lx\n",
				k, (unsigned long)ftell(f));
		exit(1);
	}
	return p;
}

static void desc_list(psd_file_t f, int level, int printxml, struct dictentry *parent){
	long count = get4B(f);
	while(count--)
		item(f, level);
}

void descriptor(psd_file_t f, int level, int printxml, struct dictentry *parent){
	long count;

	desc_class(f, level, printxml, parent);
	count = get4B(f);
	fprintf(xml, "%s<!--count:%ld-->\n", tabs(level), count);
	while(count--)
		item(f, level);
}

static void desc_double(psd_file_t f, int level, int printxml, struct dictentry *parent){
	fprintf(xml, "%g", getdoubleB(f));
};

static void desc_unitfloat(psd_file_t f, int level, int printxml, struct dictentry *parent){
	static struct dictentry ufdict[] = {
		{0, "#Ang", "-ANGLE", "angle: base degrees", desc_double},
		{0, "#Rsl", "-DENSITY", "density: base per inch", desc_double},
		{0, "#Rlt", "-DISTANCE", "distance: base 72ppi", desc_double},
		{0, "#Nne", "-NONE", "none: coerced", desc_double},
		{0, "#Prc", "-PERCENT", "percent: tagged unit value", desc_double},
		{0, "#Pxl", "-PIXELS", "pixels: tagged unit value", desc_double},
		{0, NULL, NULL, NULL, NULL}
	};

	findbykey(f, level, ufdict, getkey(f), 1, 0); // FIXME: check for NULL return
}

static void desc_unicodestr(psd_file_t f, int level, int printxml, struct dictentry *parent){
	long count = get4B(f);
	fprintf(xml, "%s<UNICODE>", parent->tag[0] == '-' ? " " : tabs(level));
	while(count--)
		fputcxml(get2Bu(f), xml);
	fprintf(xml, "</UNICODE>%c", parent->tag[0] == '-' ? ' ' : '\n');
}

static void desc_enumerated(psd_file_t f, int level, int printxml, struct dictentry *parent){
	stringorid(f, level, "TYPE");
	stringorid(f, level, "ENUM");
}

static void desc_integer(psd_file_t f, int level, int printxml, struct dictentry *parent){
	fprintf(xml, " <INTEGER>%ld</INTEGER> ", get4B(f));
}

static void desc_boolean(psd_file_t f, int level, int printxml, struct dictentry *parent){
	fprintf(xml, " <BOOLEAN>%d</BOOLEAN> ", fgetc(f));
}

static void desc_alias(psd_file_t f, int level, int printxml, struct dictentry *parent){
	psd_bytes_t count = get4B(f);
	fprintf(xml, " <!-- %lu bytes alias data --> ", (unsigned long)count);
	fseeko(f, count, SEEK_CUR); // skip over
}
