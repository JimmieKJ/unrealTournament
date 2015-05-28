// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores an ordered list of header files
	/// </summary>
	[Serializable]
	class FlatCPPIncludeDependencyInfo
	{
		/// The PCH header this file is dependent on.
		public string PCHName;

		/// List of files this file includes, excluding headers that were included from a PCH.
		public List<string> Includes;

		/// Transient cached list of FileItems for all of the includes for a specific files.  This just saves a bunch of string
		/// hash lookups as we locate FileItems for files that we've already requested dependencies for
		[NonSerialized]
		public List<FileItem> IncludeFileItems;
	}


	/// <summary>
	/// For a given target, caches all of the C++ source files and the files they are including
	/// </summary>
	[Serializable]
	public class FlatCPPIncludeDependencyCache
	{
		/// <summary>
		/// Creates the cache object
		/// </summary>
		/// <param name="Target">The target to create the cache for</param>
		/// <returns>The new instance</returns>
		public static FlatCPPIncludeDependencyCache Create( UEBuildTarget Target )
		{
			string CachePath = FlatCPPIncludeDependencyCache.GetDependencyCachePathForTarget( Target );;

			// See whether the cache file exists.
			FileItem Cache = FileItem.GetItemByPath(CachePath);
			if (Cache.bExists)
			{
				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					Log.TraceInformation("Loading existing FlatCPPIncludeDependencyCache: " + Cache.AbsolutePath);
				}

				var TimerStartTime = DateTime.UtcNow;

				// Deserialize cache from disk if there is one.
				FlatCPPIncludeDependencyCache Result = Load(Cache);
				if (Result != null)
				{
					var TimerDuration = DateTime.UtcNow - TimerStartTime;
					if (BuildConfiguration.bPrintPerformanceInfo)
					{
						Log.TraceInformation("Loading FlatCPPIncludeDependencyCache took " + TimerDuration.TotalSeconds + "s");
					}
					return Result;
				}
			}

			bool bIsBuilding = (ProjectFileGenerator.bGenerateProjectFiles == false ) && (BuildConfiguration.bXGEExport == false) && (UEBuildConfiguration.bGenerateManifest == false) && (UEBuildConfiguration.bGenerateExternalFileList == false) && (UEBuildConfiguration.bCleanProject == false);
			if( bIsBuilding && !UnrealBuildTool.bNeedsFullCPPIncludeRescan )
			{ 
				UnrealBuildTool.bNeedsFullCPPIncludeRescan = true;
				Log.TraceInformation( "Performing full C++ include scan (no include cache file)" );
			}

			// Fall back to a clean cache on error or non-existence.
			return new FlatCPPIncludeDependencyCache( Cache );
		}

		/// <summary>
		/// Loads the cache from disk
		/// </summary>
		/// <param name="Cache">The file to load</param>
		/// <returns>The loaded instance</returns>
		public static FlatCPPIncludeDependencyCache Load(FileItem Cache)
		{
			FlatCPPIncludeDependencyCache Result = null;
			try
			{
				using (FileStream Stream = new FileStream(Cache.AbsolutePath, FileMode.Open, FileAccess.Read))
				{	
					// @todo ubtmake: We can store the cache in a cheaper/smaller way using hash file names and indices into included headers, but it might actually slow down load times
					// @todo ubtmake: If we can index PCHs here, we can avoid storing all of the PCH's included headers (PCH's action should have been invalidated, so we shouldn't even have to report the PCH's includes as our indirect includes)
					BinaryFormatter Formatter = new BinaryFormatter();
					Result = Formatter.Deserialize(Stream) as FlatCPPIncludeDependencyCache;
					Result.CacheFileItem = Cache;
					Result.bIsDirty = false;
				}
			}
			catch (Exception Ex)
			{
				// Don't bother failing if the file format has changed, simply abort the cache load
				if (Ex.Message.Contains( "cannot be converted to type" ))	// To catch serialization differences added when we added the DependencyInfo struct
				{
					Console.Error.WriteLine("Failed to read FlatCPPIncludeDependencyCache: {0}", Ex.Message);
				}
			}
			return Result;
		}


		/// <summary>
		/// Constructs a fresh cache, storing the file name that it should be saved as later on
		/// </summary>
		/// <param name="Cache">File name for this cache, usually unique per context (e.g. target)</param>
		protected FlatCPPIncludeDependencyCache(FileItem Cache)
		{
			CacheFileItem = Cache;
			DependencyMap = new Dictionary<string, FlatCPPIncludeDependencyInfo>();
		}


		/// <summary>
		/// Saves out the cache
		/// </summary>
		public void Save()
		{
			// Only save if we've made changes to it since load.
			if (bIsDirty)
			{
				var TimerStartTime = DateTime.UtcNow;

				// Serialize the cache to disk.
				try
				{
					Directory.CreateDirectory(Path.GetDirectoryName(CacheFileItem.AbsolutePath));
					using (FileStream Stream = new FileStream(CacheFileItem.AbsolutePath, FileMode.Create, FileAccess.Write))
					{
						BinaryFormatter Formatter = new BinaryFormatter();
						Formatter.Serialize(Stream, this);
					}
				}
				catch (Exception Ex)
				{
					Console.Error.WriteLine("Failed to write FlatCPPIncludeDependencyCache: {0}", Ex.Message);
				}

				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					var TimerDuration = DateTime.UtcNow - TimerStartTime;
					Log.TraceInformation("Saving FlatCPPIncludeDependencyCache took " + TimerDuration.TotalSeconds + "s");
				}
			}
			else
			{
				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					Log.TraceInformation("FlatCPPIncludeDependencyCache did not need to be saved (bIsDirty=false)");
				}
			}
		}


		/// <summary>
		/// Sets the new dependencies for the specified file
		/// </summary>
		/// <param name="AbsoluteFilePath">File to set</param>
		/// <param name="PCHName">The PCH for this file</param>
		/// <param name="Dependencies">List of source dependencies</param>
		public void SetDependenciesForFile( string AbsoluteFilePath, string PCHName, List<string> Dependencies )
		{
			var DependencyInfo = new FlatCPPIncludeDependencyInfo();
			DependencyInfo.PCHName = PCHName;	// @todo ubtmake: Not actually used yet.  The idea is to use this to reduce the number of indirect includes we need to store in the cache.
			DependencyInfo.Includes = Dependencies;
			DependencyInfo.IncludeFileItems = null;

			// @todo ubtmake: We could shrink this file by not storing absolute paths (project and engine relative paths only, except for system headers.)  May affect load times.
			DependencyMap[ AbsoluteFilePath.ToLowerInvariant() ] = DependencyInfo;
			bIsDirty = true;
		}


		/// <summary>
		/// Gets everything that this file includes from our cache (direct and indirect!)
		/// </summary>
		/// <param name="AbsoluteFilePath">Path to the file</param>
		/// <returns>The list of includes</returns>
		public List<FileItem> GetDependenciesForFile( string AbsoluteFilePath )
		{
			FlatCPPIncludeDependencyInfo DependencyInfo;
			if( DependencyMap.TryGetValue( AbsoluteFilePath.ToLowerInvariant(), out DependencyInfo ) )
			{
				// Update our transient cache of FileItems for each of the included files
				if( DependencyInfo.IncludeFileItems == null )
				{
					DependencyInfo.IncludeFileItems = new List<FileItem>( DependencyInfo.Includes.Count );
					foreach( string Dependency in DependencyInfo.Includes )
					{
						DependencyInfo.IncludeFileItems.Add( FileItem.GetItemByFullPath( Dependency ) );
					}
				}
				return DependencyInfo.IncludeFileItems;
			}

			return null;
		}

		/// <summary>
		/// Gets the dependency cache path and filename for the specified target.
		/// </summary>
		/// <param name="Target">Current build target</param>
		/// <returns>Cache Path</returns>
		public static string GetDependencyCachePathForTarget(UEBuildTarget Target)
		{
			string PlatformIntermediatePath = BuildConfiguration.PlatformIntermediatePath;
			if (UnrealBuildTool.HasUProjectFile())
			{
				PlatformIntermediatePath = Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.PlatformIntermediateFolder);
			}
			string CachePath = Path.Combine(PlatformIntermediatePath, Target.GetTargetName(), "FlatCPPIncludes.bin" );
			return CachePath;
		}


		/// File name of this cache, should be unique for every includes context (e.g. target)
		[NonSerialized]
		private FileItem CacheFileItem;

		/// True if the cache needs to be saved
		[NonSerialized]
		private bool bIsDirty = false;

		/// Dependency lists, keyed (case-insensitively) on file's absolute path.
		private Dictionary<string, FlatCPPIncludeDependencyInfo> DependencyMap;


	}
}
