# This file is part of "psdparse"
# Copyright (C) 2004 Toby Thain, toby@telegraphics.com.au

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by  
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License  
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

OBJ    = psdparse.c.x

LIB    =  �
				  "{SharedLibraries}InterfaceLib" �
				  "{SharedLibraries}StdCLib" �
				  "{SharedLibraries}MathLib" �
				  "{PPCLibraries}StdCRuntime.o" �
				  "{PPCLibraries}PPCCRuntime.o" �
				  "{PPCLibraries}PPCToolLibs.o"

CFLAGS = -d MAC_ENV -w 2

.c.x  �  .c
	{PPCC} {depDir}{default}.c -o {targDir}{default}.c.x {CFLAGS}

psdparse  ��  {OBJ} {LIB}
	PPCLink -o {Targ} {OBJ} {LIB} -d -t 'MPST' -c 'MPS '