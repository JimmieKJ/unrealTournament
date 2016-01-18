// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Tools.DotNETCommon.FileSystem
{
	public static class FileSystem
	{
		[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
		private static extern int GetShortPathName(string pathName, StringBuilder shortName, int cbShortName);

		public static string GetShortPathName(string Path)
		{
			int BufferSize = GetShortPathName(Path, null, 0);
			if (BufferSize == 0)
			{
				throw new Exception(string.Format("Unable to convert path {0} to 8.3 format", Path));
			}

			var Builder = new StringBuilder(BufferSize);
			int ConversionResult = GetShortPathName(Path, Builder, BufferSize);
			if (ConversionResult == 0)
			{
				throw new Exception(string.Format("Unable to convert path {0} to 8.3 format", Path));
			}

			return Builder.ToString();
		}
	}
}
