// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

using Ionic.Zip;

namespace Tools.Distill
{
	public partial class Distill
	{
		/// <summary>Copy all the files in the TOC to the all the destination zips</summary>
		/// <param name="TableOfContents">Container for all the files to copy</param>
		public static void ZipFiles( TOC TableOfContents )
		{
			// Handle zips
			if( Options.ZipName.Length > 0 )
			{
				long TotalFilesZipped = TableOfContents.Entries.Count;
				long TotalBytesZipped = TableOfContents.Entries.Sum( x => x.Info.Length );

				ZipFile Zip = new ZipFile( Options.ZipName );
				Zip.CompressionLevel = Ionic.Zlib.CompressionLevel.Level9;
				Zip.UseZip64WhenSaving = Zip64Option.Always;
				Zip.BufferSize = 0x10000;

				TableOfContents.Entries.ForEach( x => Zip.UpdateFile( x.Name ) );

				Log( " ... saving zip: " + Zip.Name, ConsoleColor.Green );
				Zip.Save();

				FileInfo ZipInfo = new FileInfo( Zip.Name );
				Log( "Completed saving zip with " + TotalFilesZipped + " files to " + ZipInfo.Length + " bytes (from " + TotalBytesZipped + ")", ConsoleColor.Green );
			}
		}
	}
}
