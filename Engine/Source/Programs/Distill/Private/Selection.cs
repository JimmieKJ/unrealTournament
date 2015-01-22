// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace Tools.Distill
{
	public partial class Distill
	{
		/// <summary>Get the tagset to use to define the set of files to operate on</summary>
		/// <param name="Settings">The contents of the configuration file</param>
		/// <returns>The tagset that defines which files to operate on</returns>
		public static TagSet GetTagSet( DistillSettings Settings )
		{
			TagSet Tags =
			(
				from TagSetDetail in Settings.TagSets
				where TagSetDetail.Name == Options.TagSet
				select TagSetDetail
			).FirstOrDefault();

			if( Tags == null )
			{
				Error( "Failed to find tagset: " + Options.TagSet );
				return null;
			}

			Log( " ... using tagset " + Tags.Name + " with " + Tags.Tags.Length + " tags.", ConsoleColor.Cyan );
			return Tags;
		}

		/// <summary>Gets the list of filesets with macros from the configuration file</summary>
		/// <param name="Settings">The contents of the configuration file</param>
		/// <param name="Set">The tagset to look for</param>
		/// <returns>A list of filesets including macros</returns>
		public static List<FileSet> GetFileSets( DistillSettings Settings, TagSet Set )
		{
			IEnumerable<FileSet[]> BaseSets =
			(
				from Group in Settings.FileGroups
				where Set.Tags.Contains( Group.Tag )
				where Group.Platform == null || Group.Platform.Contains( Options.Platform )
				select Group.Files
			);

			List<FileSet> Sets = new List<FileSet>();
			foreach( FileSet[] FileSet in BaseSets )
			{
				Sets.AddRange( FileSet );
			}

			return Sets;
		}

		private static string[] ExpandFilterMacros( string[] OldFilters )
		{
			if( OldFilters == null )
			{
				return null;
			}

			List<string> NewFilters = new List<string>();

			foreach( string OldFilter in OldFilters )
			{
				string NewFilter = OldFilter;
				NewFilter = NewFilter.Replace( "%GAME%", Options.Game );
				NewFilter = NewFilter.Replace( "%PLATFORM%", Options.Platform );
				NewFilter = NewFilter.Replace( "%LANGUAGE%", Options.Region );

				if( NewFilter.Contains( "%NOTGAME%" ) )
				{
					NewFilters.AddRange( Options.NotGames.Select( x => NewFilter.Replace( "%NOTGAME%", x ) ) );
				}
				else if( NewFilter.Contains( "%NOTPLATFORM%" ) )
				{
					NewFilters.AddRange( Options.NotPlatforms.Select( x => NewFilter.Replace( "%NOTPLATFORM%", x ) ) );

					// Always keep "Windows" folder is specified platform was a Windows platform
					if( Options.Platform == "Win64" || Options.Platform == "Win32" )
					{
						NewFilters.Remove( "Windows" );
					}
				}
				else if( NewFilter.Contains( "%NOTLANGUAGE%" ) )
				{
					NewFilters.AddRange( Options.NotLanguages.Select( x => NewFilter.Replace( "%NOTLANGUAGE%", x ) ) );
				}
				else
				{
					NewFilters.Add( NewFilter );
				}
			}

			return NewFilters.ToArray();
		}

		/// <summary>Generate a new list of filesets with all macros (e.g. %GAME%) expanded to their real values</summary>
		/// <param name="FileSets">List of filesets potentially containing macros</param>
		/// <param name="Settings">The settings used to fill in the macros</param>
		/// <returns>List of filesets with evaluated macros</returns>
		public static List<FileSet> ExpandMacros( List<FileSet> FileSets, DistillSettings Settings )
		{
			List<FileSet> NewFileSets = new List<FileSet>();

			Options.NotPlatforms = new List<string>( Settings.KnownPlatforms.Where( x => x.Name != Options.Platform && !Options.IncPlatforms.Contains(x.Name) ).Select( x => x.Name ) );
			Options.NotLanguages = new List<string>( Settings.KnownLanguages.Where( x => x != Options.Region ) );

			foreach( FileSet OldFileSet in FileSets )
			{
				FileSet NewFileSet = new FileSet();

				NewFileSet.bIsRecursive = OldFileSet.bIsRecursive;
				NewFileSet.Path = OldFileSet.Path;

				NewFileSet.Path = NewFileSet.Path.Replace( "%GAME%", Options.Game );
				NewFileSet.Path = NewFileSet.Path.Replace( "%PLATFORM%", Options.Platform );
				NewFileSet.Path = NewFileSet.Path.Replace( "%LANGUAGE%", Options.Region );
				NewFileSet.Path = NewFileSet.Path.Replace( "\\", "/" );

				NewFileSet.FilterOutFiles = ExpandFilterMacros(OldFileSet.FilterOutFiles);
				NewFileSet.FilterOutFolders = ExpandFilterMacros(OldFileSet.FilterOutFolders);

				if (NewFileSet.FilterOutFolders != null)
				{
					List<string> NewFolders = new List<string>();
					foreach (string FilterOutFolder in NewFileSet.FilterOutFolders)
					{
						string NewFilter = "/" + FilterOutFolder + "/";
						NewFolders.Add(NewFilter);
					}
					NewFileSet.FilterOutFolders = NewFolders.ToArray();
				}

				NewFileSets.Add( NewFileSet );
			}

			Log( " ... found " + NewFileSets.Count + " file sets.", ConsoleColor.Cyan );
			return NewFileSets;
		}

		private static List<FileInfo> GetFiles( string DirectoryName, string FileName, bool bIsRecursive )
		{
			List<FileInfo> Files = new List<FileInfo>();

			DetailLog( "Checking: " + Path.Combine( DirectoryName, FileName ) + ( bIsRecursive ? " (recursively)" : "" ), ConsoleColor.DarkCyan );

			DirectoryInfo DirInfo = new DirectoryInfo( DirectoryName );
			if( DirInfo.Exists )
			{
				Files.AddRange( DirInfo.GetFiles( FileName ).ToList() );
				DetailLog( " ... added " + Files.Count + " files.", ConsoleColor.DarkCyan );

				if( bIsRecursive )
				{
					foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories() )
					{
						Files.AddRange( GetFiles( SubDirInfo.FullName, FileName, true ) );
					}
				}
			}

			return Files;
		}

		private static List<Regex> ConvertToRegularExpressions( string[] Filters )
		{
			List<Regex> RegExpFilters = new List<Regex>();

			if( Filters != null )
			{
				foreach( string Filter in Filters )
				{
					string Escaped = Regex.Escape( Filter );
					Escaped = Escaped.Replace( "\\*", ".*" );
					Escaped = Escaped.Replace( "\\?", "." );

					Regex RegExp = new Regex( Escaped, RegexOptions.Compiled | RegexOptions.IgnoreCase );
					RegExpFilters.Add( RegExp );
				}
			}

			return RegExpFilters;
		}

		private static bool CheckFolder(FileInfo Info, List<Regex> FilterOutFolders)
		{
			string InfoDirectory = Info.Directory.ToString();
			string RelativePath = InfoDirectory.Substring(Environment.CurrentDirectory.Length + 1);
			string[] Directories = RelativePath.Split( "\\/".ToCharArray() );
			
			bool Check =
			(
				from Dir in Directories
				from Filter in FilterOutFolders
				where Filter.IsMatch("/" + Dir + "/")
				select Dir
			).Any();

			return !Check;
		}

		private static bool CheckFile(FileInfo Info, List<Regex> FilterOutFiles)
		{
			bool Check =
			(
				from Filter in FilterOutFiles
				where Filter.IsMatch( Info.Name )
				select Filter
			).Any();

			return !Check;
		}

		private static List<FileInfo> FilterFiles(string[] FilterOutFiles, string[] FilterOutFolders, List<FileInfo> PotentialFiles)
		{
			List<FileInfo> GoodFiles = new List<FileInfo>();

			if (FilterOutFiles != null || FilterOutFolders != null || Options.bWhitelistSignatures)
			{
				List<Regex> FileRegExpFilters = ConvertToRegularExpressions(FilterOutFiles);
				List<Regex> FolderRegExpFilters = ConvertToRegularExpressions(FilterOutFolders);

				DetailLog( " ... using " + FileRegExpFilters.Count + " file, and " + FolderRegExpFilters.Count + " folder filters", ConsoleColor.DarkCyan );

				foreach( FileInfo Info in PotentialFiles )
				{
					DetailLog( "Filtering: " + Info.FullName, ConsoleColor.DarkCyan );
					if( !CheckFolder( Info, FolderRegExpFilters ) )
					{
						DetailLog( " ... filtered by folder", ConsoleColor.DarkCyan );
						continue;
					}

					if( !CheckFile( Info, FileRegExpFilters ) )
					{
						DetailLog( " ... filtered by file", ConsoleColor.DarkCyan );
						continue;
					}

					if( Options.bWhitelistSignatures )
					{
						if( Info.Extension.ToLower() == ".dll" || Info.Extension.ToLower() == ".exe" )
						{
							if( !Crypto.ValidateDigitalSignature( Info ) )
							{
								DetailLog( " ... filtered by signature", ConsoleColor.DarkCyan );
								continue;
							}
						}
					}

					GoodFiles.Add( Info );
				}
			}
			else
			{
				DetailLog( " ... unfiltered", ConsoleColor.DarkCyan );
				return PotentialFiles;
			}

			return GoodFiles;
		}

		/// <summary>Find all the local files that match the criteria defined in the list of filesets</summary>
		/// <param name="FileSets">A list of filesets that define the files to look for</param>
		/// <returns>A list of FileInfos of all the found files</returns>
		public static List<FileInfo> GetFiles( List<FileSet> FileSets )
		{
			List<FileInfo> Files = new List<FileInfo>();

			foreach( FileSet SourceSet in FileSets )
			{
				string DirectoryName = Path.Combine( Options.Source, Path.GetDirectoryName( SourceSet.Path ) );
				string FileName = Path.GetFileName( SourceSet.Path );
				List<FileInfo> PotentialFiles = GetFiles( DirectoryName, FileName, SourceSet.bIsRecursive );
				if( PotentialFiles.Count > 0 )
				{
					List<FileInfo> FilteredFiles = FilterFiles(SourceSet.FilterOutFiles, SourceSet.FilterOutFolders, PotentialFiles);
					Files.AddRange( FilteredFiles );
				}
			}

			Log( " ... found " + Files.Count + " files.", ConsoleColor.Cyan );
			return Files;
		}
	}
}
