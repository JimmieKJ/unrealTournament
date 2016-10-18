// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	public class ResponseFile
	{
		[FlagsAttribute]
		public enum CreateOptions
		{
			None                 = 0x0,
			WriteEvenIfUnchanged = 0x1
		}

		/// <summary>
		/// Creates a file from a list of strings; each string is placed on a line in the file.
		/// </summary>
		/// <param name="TempFileName">Name of response file</param>
		/// <param name="Lines">List of lines to write to the response file</param>
		public static FileReference Create(FileReference TempFileName, List<string> Lines, CreateOptions Options = CreateOptions.None)
		{
			FileInfo TempFileInfo = new FileInfo(TempFileName.FullName);
			if (TempFileInfo.Exists)
			{
				if ((Options & CreateOptions.WriteEvenIfUnchanged) != CreateOptions.WriteEvenIfUnchanged)
				{
					string Body = string.Join(Environment.NewLine, Lines);
					// Reuse the existing response file if it remains unchanged
					string OriginalBody = File.ReadAllText(TempFileName.FullName);
					if (string.Equals(OriginalBody, Body, StringComparison.Ordinal))
					{
						return TempFileName;
					}
				}
				// Delete the existing file if it exists and requires modification
				TempFileInfo.IsReadOnly = false;
				TempFileInfo.Delete();
				TempFileInfo.Refresh();
			}

			FileItem.CreateIntermediateTextFile(TempFileName, string.Join(Environment.NewLine, Lines));

			return TempFileName;
		}
	}
}
