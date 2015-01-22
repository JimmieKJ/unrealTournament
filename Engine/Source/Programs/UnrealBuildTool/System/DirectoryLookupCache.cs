// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;

namespace UnrealBuildTool
{
	class DirectoryCache
	{
		public DirectoryCache( string BaseDirectory )
		{
			if( Directory.Exists( BaseDirectory ) )
			{
				BaseDirectory = Utils.CleanDirectorySeparators(BaseDirectory);
				var FoundFiles = Directory.GetFiles( BaseDirectory );
				foreach( var File in FoundFiles )
				{
					Files.Add(Utils.CleanDirectorySeparators(File).ToLowerInvariant());
				}
			}
		}

		public bool HasFile( string File )
		{
			return Files.Contains(Utils.CleanDirectorySeparators(File).ToLowerInvariant());
		}

		HashSet<string> Files = new HashSet<string>();
	}


	public static class DirectoryLookupCache
	{
		static public bool FileExists( string FileFullPath )
		{
			string FileDirectory = Utils.CleanDirectorySeparators(Path.GetDirectoryName(FileFullPath));
			string DirectoriesKey = FileDirectory.ToLowerInvariant();

			DirectoryCache FoundDirectoryCache;
			if( !Directories.TryGetValue( DirectoriesKey, out FoundDirectoryCache ) )
			{
				// First time we've seen this directory.  Gather information about files.
				FoundDirectoryCache = new DirectoryCache( FileDirectory );
				Directories.Add( DirectoriesKey, FoundDirectoryCache );
			}

			// Try to find the file in this directory
			return FoundDirectoryCache.HasFile( FileFullPath );
		}

		static Dictionary<string, DirectoryCache> Directories = new Dictionary<string, DirectoryCache>();
	}
}
