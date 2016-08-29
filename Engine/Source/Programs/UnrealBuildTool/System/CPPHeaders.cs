// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Runtime.Serialization;
using System.Linq;
using Tools.DotNETCommon.FileContentsCacheType;

namespace UnrealBuildTool
{
	/// <summary>
	/// For C++ source file items, this structure is used to cache data that will be used for include dependency scanning
	/// </summary>
	[Serializable]
	public class CPPIncludeInfo : ISerializable
	{
		/// <summary>
		/// Ordered list of include paths for the module
		/// </summary>
		public HashSet<string> IncludePaths = new HashSet<string>();

		/// <summary>
		/// The include paths where changes to contained files won't cause dependent C++ source files to
		/// be recompiled, unless BuildConfiguration.bCheckSystemHeadersForModification==true.
		/// </summary>
		public HashSet<string> SystemIncludePaths = new HashSet<string>();

		/// <summary>
		/// Contains a mapping from filename to the full path of the header in this environment.  This is used to optimized include path lookups at runtime for any given single module.
		/// </summary>
		public Dictionary<string, FileItem> IncludeFileSearchDictionary = new Dictionary<string, FileItem>();

		public CPPIncludeInfo()
		{
		}

		public CPPIncludeInfo(SerializationInfo Info, StreamingContext Context)
		{
			IncludePaths = new HashSet<string>((string[])Info.GetValue("ip", typeof(string[])));
			SystemIncludePaths = new HashSet<string>((string[])Info.GetValue("sp", typeof(string[])));
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("ip", IncludePaths.ToArray());
			Info.AddValue("sp", SystemIncludePaths.ToArray());
		}


		/// <summary>
		/// Given a C++ source file, returns a list of include paths we should search to resolve #includes for this path
		/// </summary>
		/// <param name="SourceFile">C++ source file we're going to check #includes for.</param>
		/// <returns>Ordered list of paths to search</returns>
		public List<string> GetIncludesPathsToSearch(FileItem SourceFile)
		{
			// Build a single list of include paths to search.
			List<string> IncludePathsToSearch = new List<string>();

			if (SourceFile != null)
			{
				string SourceFilesDirectory = Path.GetDirectoryName(SourceFile.AbsolutePath);
				IncludePathsToSearch.Add(SourceFilesDirectory);
			}

			IncludePathsToSearch.AddRange(IncludePaths);
			if (BuildConfiguration.bCheckSystemHeadersForModification)
			{
				IncludePathsToSearch.AddRange(SystemIncludePaths);
			}

			return IncludePathsToSearch;
		}
	}



	/// <summary>
	/// List of all files included in a file and helper class for handling circular dependencies.
	/// </summary>
	public class IncludedFilesSet : HashSet<FileItem>
	{
		/// <summary>
		/// Whether this file list has been fully initialized or not.
		/// </summary>
		public bool bIsInitialized;

		/// <summary>
		/// List of files which include this file in one of its includes.
		/// </summary>
		public List<FileItem> CircularDependencies = new List<FileItem>();
	}


	public partial class CPPEnvironment
	{
		/// <summary>
		/// Contains a cache of include dependencies (direct and indirect), one for each target we're building.
		/// </summary>
		public static readonly Dictionary<UEBuildTarget, DependencyCache> IncludeDependencyCache = new Dictionary<UEBuildTarget, DependencyCache>();

		/// <summary>
		/// Contains a cache of include dependencies (direct and indirect), one for each target we're building.
		/// </summary>
		public static readonly Dictionary<UEBuildTarget, FlatCPPIncludeDependencyCache> FlatCPPIncludeDependencyCache = new Dictionary<UEBuildTarget, FlatCPPIncludeDependencyCache>();

		public static int TotalFindIncludedFileCalls = 0;

		public static int IncludePathSearchAttempts = 0;


		/// <summary>
		/// Finds the header file that is referred to by a partial include filename.
		/// </summary>
		/// <param name="RelativeIncludePath">path relative to the project</param>
		/// <param name="bSkipExternalHeader">true to skip processing of headers in external path</param>
		/// <param name="SourceFilesDirectory">- The folder containing the source files we're generating a PCH for</param>
		public static FileItem FindIncludedFile(string RelativeIncludePath, bool bSkipExternalHeader, List<string> IncludePathsToSearch, Dictionary<string, FileItem> IncludeFileSearchDictionary)
		{
			FileItem Result = null;

			if (IncludePathsToSearch == null)
			{
				throw new BuildException("Was not expecting IncludePathsToSearch to be empty for file '{0}'!", RelativeIncludePath);
			}

			++TotalFindIncludedFileCalls;

			// Only search for the include file if the result hasn't been cached.
			string InvariantPath = RelativeIncludePath.ToLowerInvariant();
			if (!IncludeFileSearchDictionary.TryGetValue(InvariantPath, out Result))
			{
				int SearchAttempts = 0;
				if (Path.IsPathRooted(RelativeIncludePath))
				{
					FileReference Reference = new FileReference(RelativeIncludePath);
					if (DirectoryLookupCache.FileExists(Reference))
					{
						Result = FileItem.GetItemByFileReference(Reference);
					}
					++SearchAttempts;
				}
				else
				{
					// Find the first include path that the included file exists in.
					foreach (string IncludePath in IncludePathsToSearch)
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
						FileReference FullFilePath = null;
						try
						{
							FullFilePath = new FileReference(RelativeFilePath);
						}
						catch (Exception)
						{
						}
						if (FullFilePath != null && DirectoryLookupCache.FileExists(FullFilePath))
						{
							Result = FileItem.GetItemByFileReference(FullFilePath);
							break;
						}
					}
				}

				IncludePathSearchAttempts += SearchAttempts;

				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					// More than two search attempts indicates:
					//		- Include path was not relative to the directory that the including file was in
					//		- Include path was not relative to the project's base
					if (SearchAttempts > 2)
					{
						Trace.TraceInformation("   Cache miss: " + RelativeIncludePath + " found after " + SearchAttempts.ToString() + " attempts: " + (Result != null ? Result.AbsolutePath : "NOT FOUND!"));
					}
				}

				// Cache the result of the include path search.
				IncludeFileSearchDictionary.Add(InvariantPath, Result);
			}

			// @todo ubtmake: The old UBT tried to skip 'external' (STABLE) headers here.  But it didn't work.  We might want to do this though!  Skip system headers and source/thirdparty headers!

			if (Result != null)
			{
				Log.TraceVerbose("Resolved included file \"{0}\" to: {1}", RelativeIncludePath, Result.AbsolutePath);
			}
			else
			{
				Log.TraceVerbose("Couldn't resolve included file \"{0}\"", RelativeIncludePath);
			}

			return Result;
		}

		/// <summary>
		/// A cache of the list of other files that are directly or indirectly included by a C++ file.
		/// </summary>
		static Dictionary<FileItem, IncludedFilesSet> ExhaustiveIncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();

		/// <summary>
		/// A cache of all files included by a C++ file, but only has files that we knew about from a previous session, loaded from a cache at startup
		/// </summary>
		static Dictionary<FileItem, IncludedFilesSet> OnlyCachedIncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();


		public static List<FileItem> FindAndCacheAllIncludedFiles(UEBuildTarget Target, FileItem SourceFile, UEBuildPlatform BuildPlatform, CPPIncludeInfo CPPIncludeInfo, bool bOnlyCachedDependencies)
		{
			List<FileItem> Result = null;

			if (CPPIncludeInfo.IncludeFileSearchDictionary == null)
			{
				CPPIncludeInfo.IncludeFileSearchDictionary = new Dictionary<string, FileItem>();
			}

			bool bUseFlatCPPIncludeDependencyCache = BuildConfiguration.bUseUBTMakefiles && UnrealBuildTool.IsAssemblingBuild;
			if (bOnlyCachedDependencies && bUseFlatCPPIncludeDependencyCache)
			{
				Result = FlatCPPIncludeDependencyCache[Target].GetDependenciesForFile(SourceFile.Reference);
				if (Result == null)
				{
					// Nothing cached for this file!  It is new to us.  This is the expected flow when our CPPIncludeDepencencyCache is missing.
				}
			}
			else
			{
				// @todo ubtmake: HeaderParser.h is missing from the include set for Module.UnrealHeaderTool.cpp (failed to find include using:  FileItem DirectIncludeResolvedFile = CPPEnvironment.FindIncludedFile(DirectInclude.IncludeName, !BuildConfiguration.bCheckExternalHeadersForModification, IncludePathsToSearch, IncludeFileSearchDictionary );)

				// If we're doing an exhaustive include scan, make sure that we have our include dependency cache loaded and ready
				if (!bOnlyCachedDependencies)
				{
					if (!IncludeDependencyCache.ContainsKey(Target))
					{
						IncludeDependencyCache.Add(Target, DependencyCache.Create(DependencyCache.GetDependencyCachePathForTarget(Target)));
					}
				}

				Result = new List<FileItem>();

				IncludedFilesSet IncludedFileList = new IncludedFilesSet();
				CPPEnvironment.FindAndCacheAllIncludedFiles(Target, SourceFile, BuildPlatform, CPPIncludeInfo, ref IncludedFileList, bOnlyCachedDependencies: bOnlyCachedDependencies);
				foreach (FileItem IncludedFile in IncludedFileList)
				{
					Result.Add(IncludedFile);
				}

				// Update cache
				if (bUseFlatCPPIncludeDependencyCache && !bOnlyCachedDependencies)
				{
					List<FileReference> Dependencies = new List<FileReference>();
					foreach (FileItem IncludedFile in Result)
					{
						Dependencies.Add(IncludedFile.Reference);
					}
					FileReference PCHName = SourceFile.PrecompiledHeaderIncludeFilename;
					FlatCPPIncludeDependencyCache[Target].SetDependenciesForFile(SourceFile.Reference, PCHName, Dependencies);
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
		public static bool FindAndCacheAllIncludedFiles(UEBuildTarget Target, FileItem CPPFile, UEBuildPlatform BuildPlatform, CPPIncludeInfo CPPIncludeInfo, ref IncludedFilesSet Result, bool bOnlyCachedDependencies)
		{
			IncludedFilesSet IncludedFileList;
			Dictionary<FileItem, IncludedFilesSet> IncludedFilesMap = bOnlyCachedDependencies ? OnlyCachedIncludedFilesMap : ExhaustiveIncludedFilesMap;
			if (!IncludedFilesMap.TryGetValue(CPPFile, out IncludedFileList))
			{
				DateTime TimerStartTime = DateTime.UtcNow;

				IncludedFileList = new IncludedFilesSet();

				// Add an uninitialized entry for the include file to avoid infinitely recursing on include file loops.
				IncludedFilesMap.Add(CPPFile, IncludedFileList);

				// Gather a list of names of files directly included by this C++ file.
				List<DependencyInclude> DirectIncludes = GetDirectIncludeDependencies(Target, CPPFile, BuildPlatform, bOnlyCachedDependencies: bOnlyCachedDependencies);

				// Build a list of the unique set of files that are included by this file.
				HashSet<FileItem> DirectlyIncludedFiles = new HashSet<FileItem>();
				// require a for loop here because we need to keep track of the index in the list.
				for (int DirectlyIncludedFileNameIndex = 0; DirectlyIncludedFileNameIndex < DirectIncludes.Count; ++DirectlyIncludedFileNameIndex)
				{
					// Resolve the included file name to an actual file.
					DependencyInclude DirectInclude = DirectIncludes[DirectlyIncludedFileNameIndex];
					if (!DirectInclude.HasAttemptedResolve ||
						// ignore any preexisting resolve cache if we are not configured to use it.
						!BuildConfiguration.bUseIncludeDependencyResolveCache ||
						// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
						BuildConfiguration.bTestIncludeDependencyResolveCache
						)
					{
						++TotalDirectIncludeResolveCacheMisses;

						// search the include paths to resolve the file
						FileItem DirectIncludeResolvedFile = CPPEnvironment.FindIncludedFile(DirectInclude.IncludeName, !BuildConfiguration.bCheckExternalHeadersForModification, CPPIncludeInfo.GetIncludesPathsToSearch(CPPFile), CPPIncludeInfo.IncludeFileSearchDictionary);
						if (DirectIncludeResolvedFile != null)
						{
							DirectlyIncludedFiles.Add(DirectIncludeResolvedFile);
						}
						IncludeDependencyCache[Target].CacheResolvedIncludeFullPath(CPPFile, DirectlyIncludedFileNameIndex, DirectIncludeResolvedFile != null ? DirectIncludeResolvedFile.Reference : null);
					}
					else
					{
						// we might have cached an attempt to resolve the file, but couldn't actually find the file (system headers, etc).
						if (DirectInclude.IncludeResolvedNameIfSuccessful != null)
						{
							DirectlyIncludedFiles.Add(FileItem.GetItemByFileReference(DirectInclude.IncludeResolvedNameIfSuccessful));
						}
					}
				}
				TotalDirectIncludeResolves += DirectIncludes.Count;

				// Convert the dictionary of files included by this file into a list.
				foreach (FileItem DirectlyIncludedFile in DirectlyIncludedFiles)
				{
					// Add the file we're directly including
					IncludedFileList.Add(DirectlyIncludedFile);

					// Also add all of the indirectly included files!
					if (FindAndCacheAllIncludedFiles(Target, DirectlyIncludedFile, BuildPlatform, CPPIncludeInfo, ref IncludedFileList, bOnlyCachedDependencies: bOnlyCachedDependencies) == false)
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
				foreach (FileItem CircularDependency in IncludedFileList.CircularDependencies)
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

				TimeSpan TimerDuration = DateTime.UtcNow - TimerStartTime;
				TotalTimeSpentGettingIncludes += TimerDuration.TotalSeconds;
			}

			if (IncludedFileList.bIsInitialized)
			{
				// Copy the list of files included by this file into the result list.
				foreach (FileItem IncludedFile in IncludedFileList)
				{
					// If the result list doesn't contain this file yet, add the file and the files it includes.
					// NOTE: For some reason in .NET 4, Add() is over twice as fast as calling UnionWith() on the set
					Result.Add(IncludedFile);
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


		/// <summary>
		/// Regex that matches #include statements.
		/// </summary>
		static readonly Regex CPPHeaderRegex = new Regex("(([ \t]*#[ \t]*include[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		static readonly Regex MMHeaderRegex = new Regex("(([ \t]*#[ \t]*import[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex that matches C++ code with UObject declarations which we will need to generated code for.
		/// </summary>
		static readonly Regex UObjectRegex = new Regex("^\\s*U(CLASS|STRUCT|ENUM|INTERFACE|DELEGATE)\\b", RegexOptions.Compiled | RegexOptions.Multiline);

		// Maintains a cache of file contents
		private static FileContentsCacheType FileContentsCache = new FileContentsCacheType();

		// Checks if a file contains UObjects
		public static bool DoesFileContainUObjects(string Filename)
		{
			string Contents = FileContentsCache.GetContents(Filename);
			return UObjectRegex.IsMatch(Contents);
		}

		/// <summary>
		/// Finds the names of files directly included by the given C++ file, and also whether the file contains any UObjects
		/// </summary>
		public static List<DependencyInclude> GetDirectIncludeDependencies(UEBuildTarget Target, FileItem CPPFile, UEBuildPlatform BuildPlatform, bool bOnlyCachedDependencies)
		{
			// Try to fulfill request from cache first.
			List<DependencyInclude> Info = IncludeDependencyCache[Target].GetCachedDependencyInfo(CPPFile);
			if (Info != null)
			{
				return Info;
			}

			List<DependencyInclude> Result = new List<DependencyInclude>();
			if (bOnlyCachedDependencies)
			{
				return Result;
			}

			DateTime TimerStartTime = DateTime.UtcNow;
			++CPPEnvironment.TotalDirectIncludeCacheMisses;

			Result = GetUncachedDirectIncludeDependencies(CPPFile, BuildPlatform, Target.ProjectFile);

			// Populate cache with results.
			IncludeDependencyCache[Target].SetDependencyInfo(CPPFile, Result);

			CPPEnvironment.DirectIncludeCacheMissesTotalTime += (DateTime.UtcNow - TimerStartTime).TotalSeconds;

			return Result;
		}

		public static List<DependencyInclude> GetUncachedDirectIncludeDependencies(FileItem CPPFile, UEBuildPlatform BuildPlatform, FileReference ProjectFile = null)
		{
			List<DependencyInclude> Result = new List<DependencyInclude>();

			// Get the adjusted filename
			string FileToRead = CPPFile.AbsolutePath;
			if (BuildPlatform.RequiresExtraUnityCPPWriter() && Path.GetFileName(FileToRead).StartsWith("Module."))
			{
				FileToRead += ".ex";
			}

			// Read lines from the C++ file.
			string FileContents = FileContentsCache.GetContents(FileToRead);
			if (string.IsNullOrEmpty(FileContents))
			{
				return Result;
			}

			// Note: This depends on UBT executing w/ a working directory of the Engine/Source folder!
			string EngineSourceFolder = Directory.GetCurrentDirectory();
			string InstalledFolder = EngineSourceFolder;
			Int32 EngineSourceIdx = EngineSourceFolder.IndexOf("\\Engine\\Source");
			if (EngineSourceIdx != -1)
			{
				InstalledFolder = EngineSourceFolder.Substring(0, EngineSourceIdx);
			}

			if (Utils.IsRunningOnMono)
			{
				// Mono crashes when running a regex on a string longer than about 5000 characters, so we parse the file in chunks
				int StartIndex = 0;
				const int SafeTextLength = 4000;
				while (StartIndex < FileContents.Length)
				{
					int EndIndex = StartIndex + SafeTextLength < FileContents.Length ? FileContents.IndexOf("\n", StartIndex + SafeTextLength) : FileContents.Length;
					if (EndIndex == -1)
					{
						EndIndex = FileContents.Length;
					}

					Result.AddRange(CollectHeaders(ProjectFile, CPPFile, FileToRead, FileContents, InstalledFolder, StartIndex, EndIndex));

					StartIndex = EndIndex + 1;
				}
			}
			else
			{
				Result = CollectHeaders(ProjectFile, CPPFile, FileToRead, FileContents, InstalledFolder, 0, FileContents.Length);
			}

			return Result;
		}

		/// <summary>
		/// Collects all header files included in a CPPFile
		/// </summary>
		/// <param name="CPPFile"></param>
		/// <param name="FileToRead"></param>
		/// <param name="FileContents"></param>
		/// <param name="InstalledFolder"></param>
		/// <param name="StartIndex"></param>
		/// <param name="EndIndex"></param>
		private static List<DependencyInclude> CollectHeaders(FileReference ProjectFile, FileItem CPPFile, string FileToRead, string FileContents, string InstalledFolder, int StartIndex, int EndIndex)
		{
			List<DependencyInclude> Result = new List<DependencyInclude>();

			Match M = CPPHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
			CaptureCollection Captures = M.Groups["HeaderFile"].Captures;
			Result.Capacity = Result.Count;
			foreach (Capture C in Captures)
			{
				string HeaderValue = C.Value;

				if (HeaderValue.IndexOfAny(Path.GetInvalidPathChars()) != -1)
				{
					throw new BuildException("In {0}: An #include statement contains invalid characters.  You might be missing a double-quote character. (\"{1}\")", FileToRead, C.Value);
				}

				//@TODO: The intermediate exclusion is to work around autogenerated absolute paths in Module.SomeGame.cpp style files
				bool bCheckForBackwardSlashes = FileToRead.StartsWith(InstalledFolder) || ((ProjectFile != null) && new FileReference(FileToRead).IsUnderDirectory(ProjectFile.Directory));
				if (bCheckForBackwardSlashes && !FileToRead.Contains("Intermediate") && !FileToRead.Contains("ThirdParty") && HeaderValue.IndexOf('\\', 0) >= 0)
				{
					throw new BuildException("In {0}: #include \"{1}\" contains backslashes ('\\'), please use forward slashes ('/') instead.", FileToRead, C.Value);
				}
				HeaderValue = Utils.CleanDirectorySeparators(HeaderValue);
				Result.Add(new DependencyInclude(HeaderValue));
			}

			// also look for #import in objective C files
			string Ext = Path.GetExtension(CPPFile.AbsolutePath).ToUpperInvariant();
			if (Ext == ".MM" || Ext == ".M")
			{
				M = MMHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
				Captures = M.Groups["HeaderFile"].Captures;
				Result.Capacity += Captures.Count;
				foreach (Capture C in Captures)
				{
					Result.Add(new DependencyInclude(C.Value));
				}
			}

			return Result;
		}
	}
}
