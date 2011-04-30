/***************************************************************************
 *   Copyright (C) 2011 by amir@mohammadkhani.eu                           *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

// Compile under Linux using
// gmcs ProcessClKernelFile.cs

using System;
using System.IO;
using System.Text.RegularExpressions;

namespace luxbuild
{
    class ProcessClKernelFile
    {
        static void Main(string[] args)
        {
            if ( args.Length != 1 )
            {
                Console.WriteLine("Please supply exactly 1 argument - Path to file");
                return;
            }

            string filename = args[0];

            if ( ! File.Exists(filename) )
            {
                Console.WriteLine( "Could not find file {0}", filename );
                return;
            }

            string fileContent = File.ReadAllText( filename );

            fileContent = fileContent.Trim();
            
            // encapsulate every line with ""
            fileContent = Regex.Replace(fileContent, "^(?<lineContent>.*)$", "\"${lineContent}\\n\"", RegexOptions.Multiline);

            Console.Write( fileContent );
        }
    }
}