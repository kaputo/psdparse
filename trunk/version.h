/*
    This file is part of "psdparse"
    Copyright (C) 2004-9 Toby Thain, toby@telegraphics.com.au

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

/* version history:
  15-Oct-2004: 0.1 commenced
  19-Oct-2004: 0.1b1 released
  26-Mar-2005: 1.0b1 significant bugs in layer handling fixed
  01-Apr-2005: began code to create PNGs of layers
  05-Apr-2005: 1.1 last version of dump-only tool
  05-Apr-2005: 1.2 merge PNG extraction code
  10-Apr-2005: 1.3b1 ported to Win32 console app
  17-Apr-2006: 1.4b1 many cleanups
  20-Apr-2006: 1.5b1,2 improve handling of extra channels, uncommon modes, and corrupt files
  02-May-2006: 1.6b1 add ability to skip channels if data format error
  03-May-2006: 1.6b2,3,4 major improvements to handling corrupt RLE data (thanks Torsten Will)
  04-May-2006: 1.7b1 now extract layer masks, and use channel ids to identify masks, alphas
  04-Jun-2006: 1.7b2 close PNG file after writing; add option to use
                     generic names for PNG layer files (thanks Tankko Omaskio)
  30-Jan-2007: 1.8b1 for 32 bit PSD, don't try to write PNG - but write raw file instead
  26-Mar-2007: 1.9b1 write XML description
  07-Apr-2007: 2.0b1 add some support for parsing v4+ 'extra data' (non-image layers)
  11-Apr-2007: 2.0b2 disable buggy descriptor parsing
  11-Apr-2007: 2.1b1 initial PSB support
  14-Apr-2007: 2.2b1,2 more metadata support (effects layers, etc)
  16-Apr-2007: 2.3b1 bugs fixed, refactoring
  21-Apr-2007: 2.4b1 dump ICC profile to XML (for Leo Revzin)
  22-Jul-2007: 2.4b2 fix encoding of Unicode characters in XML
  19-Apr-2008: 2.5b1 write UTF-8 XML
  13-Sep-2008: 2.5b2 fix Layer/Mask Info bug found by John Sauter
  12-Oct-2008: 2.5b3 #ifdef out the UTF-8 locale code, this broke build on most platforms :(
  14-May-2009: 2.6b1 add 'scavenge' mode that does not need header, but searches exhaustively for layer info
                     (this was used to recover some damaged header-less PSDs for lotusflower)
  15-May-2009: 2.6b2
  25-May-2009: 2.6b3 port scavenge to Win32
  19-Sep-2009: 2.7b1,2 new scavenge option --scavengerle, to search for layer data patterns
  15-Nov-2009: 2.8b1 fix descriptor parsing
  17-Nov-2009: 2.8b2 some XML structure changes
  23-Nov-2009: 2.9b1 fix XML/UTF-8 generation
  24-Nov-2009: 2.9b2 add rudimentary parsing for embedded PDF (Type Tool 6)
  25-Nov-2009: 2.9b3 parse some structure out of embedded PDF
  26-Nov-2009: 3.0b1 enable descriptor based structures including object effects, and fill layers
  30-Nov-2009: 3.1b1 add some resource types. Various debugging and refactoring.
  05-Dec-2009: 3.1b2,3 various refactoring. fix segfault introduced in r284 (reported by Tobias Jakobs)
  08-Dec-2009: 3.2b1 overhaul image data handling (simplify and prepare for ZIP layer data)
  09-Dec-2009: 3.2b2 process 'nested' Lr16 layers (thanks to Rino for headsup and test data)
*/

#define VERSION_STR "3.2b3"  // <-- remember to change in configure.ac!
#define VERSION_NUM 3,0x20,beta,3
#define VERS_RSRC \
	VERSION_NUM,\
	verAustralia,\
	VERSION_STR,\
	VERSION_STR ", Copyright (C) Toby Thain 2004-9 http://www.telegraphics.com.au/"

/* formatted for Win32 VERSIONINFO resource */
// development = 0x20, alpha = 0x40, beta = 0x60, final = 0x80
#define VI_VERS_NUM 3,2,0x60,3
#define VI_FLAGS	VS_FF_PRERELEASE /* 0 for final, or any of VS_FF_DEBUG,VS_FF_PATCHED,VS_FF_PRERELEASE,VS_FF_PRIVATEBUILD,VS_FF_SPECIALBUILD */
#define VI_COMMENTS	"Beta.\r\n\r\nPlease contact support@telegraphics.com.au with any bug reports, suggestions or comments.\0"	/* null terminated Comments field */

/* wildcard signature in resources */
#define ANY '    '
