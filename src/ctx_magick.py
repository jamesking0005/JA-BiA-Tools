"""
Created on February 6, 2012

@author: sbobovyc
"""
"""   
    Copyright (C) 2011 Stanislav Bobovych

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

import argparse
import os
from ctx_file import CTX_file

parser = argparse.ArgumentParser(description='Tool that can unpack Jagged Alliance: BiA ctx files.')

parser.add_argument('file', nargs='?', help='Input file')
parser.add_argument('outdir', nargs='?', help='Output directory')
parser.add_argument('-i', '--info', default=False, action='store_true', help='Output information about ctx file')
parser.add_argument('-d', '--debug', default=False, action='store_true', help='Show debug messages.')


args = parser.parse_args()
file = args.file
outdir = args.outdir
info = args.info
debug = args.debug

if file != None and info != False:
    info_filepath = os.path.abspath(file)
    print "Not implemented yet."
    
    
elif file != None:            
    ctx_filepath = os.path.abspath(file)
    print "Unpacking %s" % ctx_filepath
    ctx_file = CTX_file(filepath=ctx_filepath)
    ctx_file.open()
    ctx_file.unpack(verbose=debug)

    if outdir != None:
        output_filepath = os.path.abspath(outdir)
        ctx_file.dump2file(outdir)
    else:
        ctx_file.dump2file()
        
else:
    print "Nothing happened"
    parser.print_help()
        