// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;

namespace Tools.DotNETCommon.FileContentsCacheType
{
	public class FileContentsCacheType
	{
		public string GetContents(string Filename)
		{
			string Contents;
			if (FilenameContentsMap.TryGetValue(Filename, out Contents))
			{
				return Contents;
			}

			using (var Reader = new StreamReader(Filename, System.Text.Encoding.UTF8))
			{
				Contents = Reader.ReadToEnd();
				FilenameContentsMap.Add(Filename, Contents);
			}

			return Contents;
		}

		private Dictionary<string, string> FilenameContentsMap = new Dictionary<string, string>();
	}
}
