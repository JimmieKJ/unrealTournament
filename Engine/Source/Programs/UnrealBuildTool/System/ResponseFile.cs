// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	public class ResponseFile
	{
		/// <summary>
		/// Creates a file from a list of strings; each string is placed on a line in the file.
		/// </summary>
		/// <param name="TempFileName">Name of response file</param>
		/// <param name="Lines">List of lines to write to the response file</param>
		public static FileReference Create(FileReference TempFileName, List<string> Lines)
		{
			FileInfo TempFileInfo = new FileInfo(TempFileName.FullName);
			// Delete the existing file if it exists
			if (TempFileInfo.Exists)
			{
				TempFileInfo.IsReadOnly = false;
				TempFileInfo.Delete();
				TempFileInfo.Refresh();
			}

			FileItem.CreateIntermediateTextFile(TempFileName, string.Join(Environment.NewLine, Lines));

			return TempFileName;
		}
	}
}
