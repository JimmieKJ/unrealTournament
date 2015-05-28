// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace UnrealBuildTool
{
	/** For C++ source file items, this structure is used to cache data that will be used for include dependency scanning */
	[Serializable]
	public class CPPIncludeInfo
	{
		/** Ordered list of include paths for the module  */
		public HashSet<string> IncludePaths = new HashSet<string>();

		/**
		 * The include paths where changes to contained files won't cause dependent C++ source files to
		 * be recompiled, unless BuildConfiguration.bCheckSystemHeadersForModification==true.
		 */
		public HashSet<string> SystemIncludePaths = new HashSet<string>();

		/** Contains a mapping from filename to the full path of the header in this environment.  This is used to optimized include path lookups at runtime for any given single module. */
		[NonSerialized]
		public Dictionary<string, FileItem> IncludeFileSearchDictionary = new Dictionary<string, FileItem>();


		/// <summary>
		/// Given a C++ source file, returns a list of include paths we should search to resolve #includes for this path
		/// </summary>
		/// <param name="SourceFile">C++ source file we're going to check #includes for.</param>
		/// <returns>Ordered list of paths to search</returns>
		public List<string> GetIncludesPathsToSearch( FileItem SourceFile )
		{
			// Build a single list of include paths to search.
			var IncludePathsToSearch = new List<string>();
			
			if( SourceFile != null )
			{ 
				string SourceFilesDirectory = Path.GetDirectoryName( SourceFile.AbsolutePath );
				IncludePathsToSearch.Add( SourceFilesDirectory );
			}

			IncludePathsToSearch.AddRange( IncludePaths );
			if( BuildConfiguration.bCheckSystemHeadersForModification )
			{
				IncludePathsToSearch.AddRange( SystemIncludePaths );
			}
			
			return IncludePathsToSearch;
		}
	}



	/// <summary>
	/// List of all files included in a file and helper class for handling circular dependencies.
	/// </summary>
	public class IncludedFilesSet : HashSet<FileItem>
	{
		/** Whether this file list has been fully initialized or not. */
		public bool bIsInitialized;

		/** List of files which include this file in one of its includes. */
		public List<FileItem> CircularDependencies = new List<FileItem>();
	}


	public partial class CPPEnvironment
	{
		/** Contains a cache of include dependencies (direct and indirect), one for each target we're building. */
		public static readonly Dictionary<UEBuildTarget, DependencyCache> IncludeDependencyCache = new Dictionary<UEBuildTarget, DependencyCache>();

		/** Contains a cache of include dependencies (direct and indirect), one for each target we're building. */
		public static readonly Dictionary<UEBuildTarget, FlatCPPIncludeDependencyCache> FlatCPPIncludeDependencyCache = new Dictionary<UEBuildTarget, FlatCPPIncludeDependencyCache>();

		public static int TotalFindIncludedFileCalls = 0;

		public static int IncludePathSearchAttempts = 0;


		/** 
		 * Finds the header file that is referred to by a partial include filename. 
		 * @param RelativeIncludePath path relative to the project
		 * @param bSkipExternalHeader true to skip processing of headers in external path
		 * @param SourceFilesDirectory - The folder containing the source files we're generating a PCH for
		 */
		public static FileItem FindIncludedFile( string RelativeIncludePath, bool bSkipExternalHeader, List<string> IncludePathsToSearch, Dictionary<string, FileItem> IncludeFileSearchDictionary )
		{
			FileItem Result = null;

			if( IncludePathsToSearch == null )
			{
				throw new BuildException( "Was not expecting IncludePathsToSearch to be empty for file '{0}'!", RelativeIncludePath );
			}

			++TotalFindIncludedFileCalls;

			// Only search for the include file if the result hasn't been cached.
			string InvariantPath = RelativeIncludePath.ToLowerInvariant();
			if( !IncludeFileSearchDictionary.TryGetValue( InvariantPath, out Result ) )
			{
				int SearchAttempts = 0;
				if( Path.IsPathRooted( RelativeIncludePath ) )
				{
					if( DirectoryLookupCache.FileExists( RelativeIncludePath ) )
					{
						Result = FileItem.GetItemByFullPath( RelativeIncludePath );
					}
					++SearchAttempts;
				}
				else
				{
					// Find the first include path that the included file exists in.
					foreach( string IncludePath in IncludePathsToSearch )
					{
						++SearchAttempts;
						string RelativeFilePath = "";
						try
						{
							RelativeFilePath = Path.Combine(IncludePath, RelativeIncludePath);
						}
						catch (ArgumentException Exception)
						{
							throw new BuildException(Exception, "Failed to combine null or invalid include paths.");
						}
						string FullFilePath = Path.GetFullPath( RelativeFilePath );
						if( DirectoryLookupCache.FileExists( FullFilePath ) )
						{
							Result = FileItem.GetItemByFullPath( FullFilePath );
							break;
						}
					}
				}

				IncludePathSearchAttempts += SearchAttempts;

				if( BuildConfiguration.bPrintPerformanceInfo )
				{
					// More than two search attempts indicates:
					//		- Include path was not relative to the directory that the including file was in
					//		- Include path was not relative to the project's base
					if( SearchAttempts > 2 )
					{
						Trace.TraceInformation( "   Cache miss: " + RelativeIncludePath + " found after " + SearchAttempts.ToString() + " attempts: " + ( Result != null ? Result.AbsolutePath : "NOT FOUND!" ) );
					}
				}

				// Cache the result of the include path search.
				IncludeFileSearchDictionary.Add( InvariantPath, Result );
			}

			// @todo ubtmake: The old UBT tried to skip 'external' (STABLE) headers here.  But it didn't work.  We might want to do this though!  Skip system headers and source/thirdparty headers!

			if( Result != null )
			{
				Log.TraceVerbose("Resolved included file \"{0}\" to: {1}", RelativeIncludePath, Result.AbsolutePath);
			}
			else
			{
				Log.TraceVerbose("Couldn't resolve included file \"{0}\"", RelativeIncludePath);
			}

			return Result;
		}

		/** A cache of the list of other files that are directly or indirectly included by a C++ file. */
		static Dictionary<FileItem, IncludedFilesSet> ExhaustiveIncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();

		/** A cache of all files included by a C++ file, but only has files that we knew about from a previous session, loaded from a cache at startup */
		static Dictionary<FileItem, IncludedFilesSet> OnlyCachedIncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();


		public static List<FileItem> FindAndCacheAllIncludedFiles( UEBuildTarget Target, FileItem SourceFile, IUEBuildPlatform BuildPlatform, CPPIncludeInfo CPPIncludeInfo, bool bOnlyCachedDependencies )
		{
			List<FileItem> Result = null;
			
			if( CPPIncludeInfo.IncludeFileSearchDictionary == null )
			{
				CPPIncludeInfo.IncludeFileSearchDictionary = new Dictionary<string,FileItem>();
			}

			bool bUseFlatCPPIncludeDependencyCache = BuildConfiguration.bUseUBTMakefiles && UnrealBuildTool.IsAssemblingBuild;
			if( bOnlyCachedDependencies && bUseFlatCPPIncludeDependencyCache )
			{ 
				Result = FlatCPPIncludeDependencyCache[Target].GetDependenciesForFile( SourceFile.AbsolutePath );
				if( Result == null )
				{
					// Nothing cached for this file!  It is new to us.  This is the expected flow when our CPPIncludeDepencencyCache is missing.
				}
			}
			else
			{
				// @todo ubtmake: HeaderParser.h is missing from the include set for Module.UnrealHeaderTool.cpp (failed to find include using:  FileItem DirectIncludeResolvedFile = CPPEnvironment.FindIncludedFile(DirectInclude.IncludeName, !BuildConfiguration.bCheckExternalHeadersForModification, IncludePathsToSearch, IncludeFileSearchDictionary );)

				// If we're doing an exhaustive include scan, make sure that we have our include dependency cache loaded and ready
				if( !bOnlyCachedDependencies )
				{ 
					if( !IncludeDependencyCache.ContainsKey( Target ) )
					{
						IncludeDependencyCache.Add( Target, DependencyCache.Create( DependencyCache.GetDependencyCachePathForTarget( Target ) ) );
					}
				}

				Result = new List<FileItem>();

				var IncludedFileList = new IncludedFilesSet();
				CPPEnvironment.FindAndCacheAllIncludedFiles( Target, SourceFile, BuildPlatform, CPPIncludeInfo, ref IncludedFileList, bOnlyCachedDependencies:bOnlyCachedDependencies );
				foreach( FileItem IncludedFile in IncludedFileList )
				{
					Result.Add( IncludedFile );
				}
			
				// Update cache
				if( bUseFlatCPPIncludeDependencyCache && !bOnlyCachedDependencies )
				{ 
					var Dependencies = new List<string>();
					foreach( var IncludedFile in Result )
					{
						Dependencies.Add( IncludedFile.AbsolutePath );
					}
					string PCHName = SourceFile.PrecompiledHeaderIncludeFilename;
					FlatCPPIncludeDependencyCache[Target].SetDependenciesForFile( SourceFile.AbsolutePath, PCHName, Dependencies );
				}
			}

			return Result;
		}


		/// <summary>
		/// Finds the files directly or indirectly included by the given C++ file.
		/// </summary>
		/// <param name="CPPFile">C++ file to get the dependencies for.</param>
		/// <param name="Result">List of CPPFile dependencies.</param>
		/// <returns>false if CPPFile is still being processed further down the callstack, true otherwise.</returns>
		public static bool FindAndCacheAllIncludedFiles(UEBuildTarget Target, FileItem CPPFile, IUEBuildPlatform BuildPlatform, CPPIncludeInfo CPPIncludeInfo, ref IncludedFilesSet Result, bool bOnlyCachedDependencies)
		{
			IncludedFilesSet IncludedFileList;
			var IncludedFilesMap = bOnlyCachedDependencies ? OnlyCachedIncludedFilesMap : ExhaustiveIncludedFilesMap;
			if( !IncludedFilesMap.TryGetValue( CPPFile, out IncludedFileList ) )
			{
				var TimerStartTime = DateTime.UtcNow;

				IncludedFileList = new IncludedFilesSet();

				// Add an uninitialized entry for the include file to avoid infinitely recursing on include file loops.
				IncludedFilesMap.Add(CPPFile, IncludedFileList);

				// Gather a list of names of files directly included by this C++ file.
				bool HasUObjects;
				List<DependencyInclude> DirectIncludes = GetDirectIncludeDependencies( Target, CPPFile, BuildPlatform, bOnlyCachedDependencies:bOnlyCachedDependencies, HasUObjects:out HasUObjects );

				// Build a list of the unique set of files that are included by this file.
				var DirectlyIncludedFiles = new HashSet<FileItem>();
				// require a for loop here because we need to keep track of the index in the list.
				for (int DirectlyIncludedFileNameIndex = 0; DirectlyIncludedFileNameIndex < DirectIncludes.Count; ++DirectlyIncludedFileNameIndex)
				{
					// Resolve the included file name to an actual file.
					DependencyInclude DirectInclude = DirectIncludes[DirectlyIncludedFileNameIndex];
					if (DirectInclude.IncludeResolvedName == null || 
						// ignore any preexisting resolve cache if we are not configured to use it.
						!BuildConfiguration.bUseIncludeDependencyResolveCache ||
						// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
						BuildConfiguration.bTestIncludeDependencyResolveCache
						)
					{
						++TotalDirectIncludeResolveCacheMisses;

						// search the include paths to resolve the file
						FileItem DirectIncludeResolvedFile = CPPEnvironment.FindIncludedFile(DirectInclude.IncludeName, !BuildConfiguration.bCheckExternalHeadersForModification, CPPIncludeInfo.GetIncludesPathsToSearch( CPPFile ), CPPIncludeInfo.IncludeFileSearchDictionary );
						if (DirectIncludeResolvedFile != null)
						{
							DirectlyIncludedFiles.Add(DirectIncludeResolvedFile);
						}
						IncludeDependencyCache[Target].CacheResolvedIncludeFullPath(CPPFile, DirectlyIncludedFileNameIndex, DirectIncludeResolvedFile != null ? DirectIncludeResolvedFile.AbsolutePath : "");
					}
					else
					{
						// we might have cached an attempt to resolve the file, but couldn't actually find the file (system headers, etc).
						if (DirectInclude.IncludeResolvedName != string.Empty)
						{
							DirectlyIncludedFiles.Add(FileItem.GetItemByFullPath(DirectInclude.IncludeResolvedName));
						}
					}
				}
				TotalDirectIncludeResolves += DirectIncludes.Count;

				// Convert the dictionary of files included by this file into a list.
				foreach( var DirectlyIncludedFile in DirectlyIncludedFiles )
				{
					// Add the file we're directly including
					IncludedFileList.Add( DirectlyIncludedFile );

					// Also add all of the indirectly included files!
					if (FindAndCacheAllIncludedFiles(Target, DirectlyIncludedFile, BuildPlatform, CPPIncludeInfo, ref IncludedFileList, bOnlyCachedDependencies:bOnlyCachedDependencies) == false)
					{
						// DirectlyIncludedFile is a circular dependency which is still being processed
						// further down the callstack. Add this file to its circular dependencies list 
						// so that it can update its dependencies later.
						IncludedFilesSet DirectlyIncludedFileIncludedFileList;
						if (IncludedFilesMap.TryGetValue(DirectlyIncludedFile, out DirectlyIncludedFileIncludedFileList))
						{
							DirectlyIncludedFileIncludedFileList.CircularDependencies.Add(CPPFile);
						}
					}
				}

				// All dependencies have been processed by now so update all circular dependencies
				// with the full list.
				foreach (var CircularDependency in IncludedFileList.CircularDependencies)
				{
					IncludedFilesSet CircularDependencyIncludedFiles = IncludedFilesMap[CircularDependency];
					foreach (FileItem IncludedFile in IncludedFileList)
					{
						CircularDependencyIncludedFiles.Add(IncludedFile);
					}
				}
				// No need to keep this around anymore.
				IncludedFileList.CircularDependencies.Clear();

				// Done collecting files.
				IncludedFileList.bIsInitialized = true;

				var TimerDuration = DateTime.UtcNow - TimerStartTime;
				TotalTimeSpentGettingIncludes += TimerDuration.TotalSeconds;
			}

			if (IncludedFileList.bIsInitialized)
			{
				// Copy the list of files included by this file into the result list.
				foreach( FileItem IncludedFile in IncludedFileList )
				{
					// If the result list doesn't contain this file yet, add the file and the files it includes.
					// NOTE: For some reason in .NET 4, Add() is over twice as fast as calling UnionWith() on the set
					Result.Add( IncludedFile );
				}

				return true;
			}
			else
			{
				// The IncludedFileList.bIsInitialized was false because we added a dummy entry further down the call stack.  We're already processing
				// the include list for this header elsewhere in the stack frame, so we don't need to add anything here.
				return false;
			}
		}

		public static double TotalTimeSpentGettingIncludes = 0.0;
		public static int TotalIncludesRequested = 0;
		public static double DirectIncludeCacheMissesTotalTime = 0.0;
		public static int TotalDirectIncludeCacheMisses = 0;
		public static int TotalDirectIncludeResolveCacheMisses = 0;
		public static int TotalDirectIncludeResolves = 0;


		/** Regex that matches #include statements. */
		static readonly Regex CPPHeaderRegex = new Regex("(([ \t]*#[ \t]*include[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture );

		static readonly Regex MMHeaderRegex = new Regex("(([ \t]*#[ \t]*import[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture );

		/** Regex that matches C++ code with UObject declarations which we will need to generated code for. */
		static readonly Regex UObjectRegex = new Regex("^\\s*U(CLASS|STRUCT|ENUM|INTERFACE|DELEGATE)\\b", RegexOptions.Compiled | RegexOptions.Multiline );

		/** Finds the names of files directly included by the given C++ file. */
		public static List<DependencyInclude> GetDirectIncludeDependencies( UEBuildTarget Target, FileItem CPPFile, IUEBuildPlatform BuildPlatform, bool bOnlyCachedDependencies, out bool HasUObjects )
		{
			// Try to fulfill request from cache first.
			List<DependencyInclude> Result;
			if( IncludeDependencyCache[Target].GetCachedDirectDependencies( CPPFile, out Result, out HasUObjects ) )
			{
				return Result;
			}
			else if( bOnlyCachedDependencies )
			{
				return new List<DependencyInclude>();
			}

			var TimerStartTime = DateTime.UtcNow;
			++CPPEnvironment.TotalDirectIncludeCacheMisses;

			// Get the adjusted filename
			string FileToRead = CPPFile.AbsolutePath;

			if (BuildPlatform.RequiresExtraUnityCPPWriter() == true &&
				Path.GetFileName(FileToRead).StartsWith("Module."))
			{
				FileToRead += ".ex";
			}

			// Read lines from the C++ file.
			string FileContents = Utils.ReadAllText( FileToRead );
            if (string.IsNullOrEmpty(FileContents))
            {
                return new List<DependencyInclude>();
            }

			HasUObjects = CPPEnvironment.UObjectRegex.IsMatch(FileContents);

			// Note: This depends on UBT executing w/ a working directory of the Engine/Source folder!
			string EngineSourceFolder = Directory.GetCurrentDirectory();
			string InstalledFolder = EngineSourceFolder;
			Int32 EngineSourceIdx = EngineSourceFolder.IndexOf("\\Engine\\Source");
			if (EngineSourceIdx != -1)
			{
				InstalledFolder = EngineSourceFolder.Substring(0, EngineSourceIdx);
			}

			Result = new List<DependencyInclude>();
			if (Utils.IsRunningOnMono)
			{
				// Mono crashes when running a regex on a string longer than about 5000 characters, so we parse the file in chunks
				int StartIndex = 0;
				const int SafeTextLength = 4000;
				while( StartIndex < FileContents.Length )
				{
					int EndIndex = StartIndex + SafeTextLength < FileContents.Length ? FileContents.IndexOf( "\n", StartIndex + SafeTextLength ) : FileContents.Length;
					if( EndIndex == -1 )
					{
						EndIndex = FileContents.Length;
					}

					CollectHeaders(CPPFile, Result, FileToRead, FileContents, InstalledFolder, StartIndex, EndIndex);

					StartIndex = EndIndex + 1;
				}
			}
			else
			{
				CollectHeaders(CPPFile, Result, FileToRead, FileContents, InstalledFolder, 0, FileContents.Length);
			}

			// Populate cache with results.
			IncludeDependencyCache[Target].SetDependencyInfo( CPPFile, Result, HasUObjects );

			CPPEnvironment.DirectIncludeCacheMissesTotalTime += ( DateTime.UtcNow - TimerStartTime ).TotalSeconds;

			return Result;
		}

		/// <summary>
		/// Collects all header files included in a CPPFile
		/// </summary>
		/// <param name="CPPFile"></param>
		/// <param name="Result"></param>
		/// <param name="FileToRead"></param>
		/// <param name="FileContents"></param>
		/// <param name="InstalledFolder"></param>
		/// <param name="StartIndex"></param>
		/// <param name="EndIndex"></param>
		private static void CollectHeaders(FileItem CPPFile, List<DependencyInclude> Result, string FileToRead, string FileContents, string InstalledFolder, int StartIndex, int EndIndex)
		{
			Match M = CPPHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
			CaptureCollection CC = M.Groups["HeaderFile"].Captures;
			Result.Capacity = Math.Max(Result.Count + CC.Count, Result.Capacity);
			foreach (Capture C in CC)
			{
				string HeaderValue = C.Value;

				if( HeaderValue.IndexOfAny( Path.GetInvalidPathChars() ) != -1 )
				{
					throw new BuildException( "In {0}: An #include statement contains invalid characters.  You might be missing a double-quote character. (\"{1}\")", FileToRead, C.Value );
				}

				//@TODO: The intermediate exclusion is to work around autogenerated absolute paths in Module.SomeGame.cpp style files
				bool bIsIntermediateOrThirdParty = FileToRead.Contains("Intermediate") || FileToRead.Contains("ThirdParty");
				bool bCheckForBackwardSlashes = FileToRead.StartsWith(InstalledFolder);
				if (UnrealBuildTool.HasUProjectFile())
				{
					bCheckForBackwardSlashes |= Utils.IsFileUnderDirectory(FileToRead, UnrealBuildTool.GetUProjectPath());
				}
				if (bCheckForBackwardSlashes && !bIsIntermediateOrThirdParty)
				{
					if (HeaderValue.IndexOf('\\', 0) >= 0)
					{
						throw new BuildException("In {0}: #include \"{1}\" contains backslashes ('\\'), please use forward slashes ('/') instead.", FileToRead, C.Value);
					}
				}
				HeaderValue = Utils.CleanDirectorySeparators(HeaderValue);
				Result.Add(new DependencyInclude(HeaderValue));
			}

			// also look for #import in objective C files
			string Ext = Path.GetExtension(CPPFile.AbsolutePath).ToUpperInvariant();
			if (Ext == ".MM" || Ext == ".M")
			{
				M = MMHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
				CC = M.Groups["HeaderFile"].Captures;
				Result.Capacity += CC.Count;
				foreach (Capture C in CC)
				{
					Result.Add(new DependencyInclude(C.Value));
				}
			}
		}
	}
}
