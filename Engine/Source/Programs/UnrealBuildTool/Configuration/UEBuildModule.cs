// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Xml;

namespace UnrealBuildTool
{
	/** Type of module. Mirrored in UHT as EBuildModuleType */
	public enum UEBuildModuleType
	{
		Unknown,
		Runtime,
		Developer,
		Editor,
		ThirdParty,
		Program,
		Game,
	}
	public static class UEBuildModuleTypeExtensions
	{
		public static bool IsEngineModule(this UEBuildModuleType ModuleType)
		{
			return ModuleType != UEBuildModuleType.Game;
		}
	}
	/**
	 * Distribution level of module.
	 * Note: The name of each entry is used to search for/create folders
	 */
	public enum UEBuildModuleDistribution
	{
		/// Binaries can be distributed to everyone
		Public,

		/// Can be used by UE4 but not required
		CarefullyRedist,

		/// Epic Employees and Contractors
		NotForLicensees,

		/// Epic Employees only
		NoRedist,
	}

	
	/** A unit of code compilation and linking. */
	public abstract class UEBuildModule
	{
		/** Known module folders */
		public static readonly string RuntimeFolder = String.Format("{0}Runtime{0}", Path.DirectorySeparatorChar);
		public static readonly string DeveloperFolder = String.Format("{0}Developer{0}", Path.DirectorySeparatorChar);
		public static readonly string EditorFolder = String.Format("{0}Editor{0}", Path.DirectorySeparatorChar);
		public static readonly string ProgramsFolder = String.Format("{0}Programs{0}", Path.DirectorySeparatorChar);

		public static UEBuildModuleType GetModuleTypeFromDescriptor(ModuleDescriptor Module)
		{
			switch (Module.Type)
			{
				case ModuleHostType.Developer:
					return UEBuildModuleType.Developer;
				case ModuleHostType.Editor:
				case ModuleHostType.EditorNoCommandlet:
					return UEBuildModuleType.Editor;
				case ModuleHostType.Program:
					return UEBuildModuleType.Program;
				case ModuleHostType.Runtime:
				case ModuleHostType.RuntimeNoCommandlet:
					return UEBuildModuleType.Runtime;
				default:
					throw new BuildException("Unhandled module type {0}", Module.Type.ToString());
			}
		}

		public static UEBuildModuleType GetEngineModuleTypeBasedOnLocation(string ModuleName, UEBuildModuleType ModuleType, string ModuleFileRelativeToEngineDirectory)
		{
			if (ModuleFileRelativeToEngineDirectory.IndexOf(UEBuildModule.RuntimeFolder, StringComparison.InvariantCultureIgnoreCase) >= 0)
			{
				ModuleType = UEBuildModuleType.Runtime;
			}
			else if (ModuleFileRelativeToEngineDirectory.IndexOf(UEBuildModule.DeveloperFolder, StringComparison.InvariantCultureIgnoreCase) >= 0)
			{
				ModuleType = UEBuildModuleType.Developer;
			}
			else if (ModuleFileRelativeToEngineDirectory.IndexOf(UEBuildModule.EditorFolder, StringComparison.InvariantCultureIgnoreCase) >= 0)
			{
				ModuleType = UEBuildModuleType.Editor;
			}
			else if (ModuleFileRelativeToEngineDirectory.IndexOf(UEBuildModule.ProgramsFolder, StringComparison.InvariantCultureIgnoreCase) >= 0)
			{
				ModuleType = UEBuildModuleType.Program;
			}
			return ModuleType;
		}

		/**
		 * Checks what distribution level a particular path should have by checking for key folders anywhere in it
		 */
		public static UEBuildModuleDistribution GetModuleDistributionLevelBasedOnLocation(string FilePath)
		{
			// Get full path to ensure all folder separators are the same
			FilePath = Path.GetFullPath(FilePath);

			// Check from highest to lowest (don't actually need to check 'Everyone')
			for (var DistributionLevel = UEBuildModuleDistribution.NoRedist; DistributionLevel > UEBuildModuleDistribution.Public; DistributionLevel--)
			{
				var DistributionFolderName = String.Format("{0}{1}{0}", Path.DirectorySeparatorChar, DistributionLevel.ToString());
				if (FilePath.IndexOf(DistributionFolderName, StringComparison.InvariantCultureIgnoreCase) >= 0)
				{
					return DistributionLevel;
				}

				if (DistributionLevel == UEBuildModuleDistribution.NotForLicensees)
				{
					// Extra checks for PS4 and XboxOne folders, which are equivalent to NotForLicensees
					var PS4FolderName = String.Format("{0}ps4{0}", Path.DirectorySeparatorChar);
					var XboxFolderName = String.Format("{0}xboxone{0}", Path.DirectorySeparatorChar);
					if (FilePath.IndexOf(PS4FolderName, StringComparison.InvariantCultureIgnoreCase) >= 0
					|| FilePath.IndexOf(XboxFolderName, StringComparison.InvariantCultureIgnoreCase) >= 0)
					{
						return UEBuildModuleDistribution.NotForLicensees;
					}
				}
			}

			return UEBuildModuleDistribution.Public;
		}

		/**
		 * Determines the distribution level of a module based on its directory and includes.
		 */
		private void SetupModuleDistributionLevel()
		{
			List<string> PathsToCheck = new List<string>();
			PathsToCheck.Add(ModuleDirectory);
			PathsToCheck.AddRange(PublicIncludePaths);
			PathsToCheck.AddRange(PrivateIncludePaths);
			// Not sure if these two are necessary as paths will usually be in basic includes too
			PathsToCheck.AddRange(PublicSystemIncludePaths);
			PathsToCheck.AddRange(PublicLibraryPaths);

			DistributionLevel = UEBuildModuleDistribution.Public;

			// Keep checking as long as we haven't reached the maximum level
			for (int PathIndex = 0; PathIndex < PathsToCheck.Count && DistributionLevel != UEBuildModuleDistribution.NoRedist; ++PathIndex)
			{
				DistributionLevel = Utils.Max(DistributionLevel, UEBuildModule.GetModuleDistributionLevelBasedOnLocation(PathsToCheck[PathIndex]));
			}
		}

		/** Converts an optional string list parameter to a well-defined hash set. */
		protected static HashSet<string> HashSetFromOptionalEnumerableStringParameter(IEnumerable<string> InEnumerableStrings)
		{
			return InEnumerableStrings == null ? new HashSet<string>() : new HashSet<string>(InEnumerableStrings);
		}

		/** The target which owns this module. */
		public readonly UEBuildTarget Target;
	
		/** The name that uniquely identifies the module. */
		public readonly string Name;

		/** The type of module being built. Used to switch between debug/development and precompiled/source configurations. */
		public UEBuildModuleType Type;

		/** The distribution level of the module being built. Used to check where build products are placed. */
		public UEBuildModuleDistribution DistributionLevel;

		/** Path to the module directory */
		public readonly string ModuleDirectory;

		/** Is this module allowed to be redistributed. */
		private readonly bool? IsRedistributableOverride;

		/** The name of the .Build.cs file this module was created from, if any */
		private readonly string BuildCsFilenameField;
		public string BuildCsFilename { get { return BuildCsFilenameField; } }

		/**
		 * Tells if this module can be redistributed.
		 * 
		 * @returns True if this module can be redistributed. False otherwise.
		 */
		public bool IsRedistributable()
		{
			return IsRedistributableOverride.HasValue
				? IsRedistributableOverride.Value
				: (Type != UEBuildModuleType.Developer && Type != UEBuildModuleType.Editor);
		}

		/** The binary the module will be linked into for the current target.  Only set after UEBuildBinary.BindModules is called. */
		public UEBuildBinary Binary = null;

		/** Whether this module is included in the current target.  Only set after UEBuildBinary.BindModules is called. */
		public bool bIncludedInTarget = false;

		protected readonly HashSet<string> PublicDefinitions;
		protected readonly HashSet<string> PublicIncludePaths;
		protected readonly HashSet<string> PrivateIncludePaths;
		protected readonly HashSet<string> PublicSystemIncludePaths;
		protected readonly HashSet<string> PublicLibraryPaths;
		protected readonly HashSet<string> PublicAdditionalLibraries;
		protected readonly HashSet<string> PublicFrameworks;
		protected readonly HashSet<string> PublicWeakFrameworks;
		protected readonly HashSet<UEBuildFramework> PublicAdditionalFrameworks;
		protected readonly HashSet<string> PublicAdditionalShadowFiles;
		protected readonly HashSet<UEBuildBundleResource> PublicAdditionalBundleResources;

		/** Names of modules with header files that this module's public interface needs access to. */
		protected HashSet<string> PublicIncludePathModuleNames;

		/** Names of modules that this module's public interface depends on. */
		protected HashSet<string> PublicDependencyModuleNames;

		/** Names of DLLs that this module should delay load */
		protected HashSet<string> PublicDelayLoadDLLs;

		/** Names of modules with header files that this module's private implementation needs access to. */
		protected HashSet<string> PrivateIncludePathModuleNames;
	
		/** Names of modules that this module's private implementation depends on. */
		protected HashSet<string> PrivateDependencyModuleNames;

		/** If any of this module's dependent modules circularly reference this module, then they must be added to this list */
		public HashSet<string> CircularlyReferencedDependentModules
		{
			get;
			protected set;
		}

		/** Extra modules this module may require at run time */
		protected HashSet<string> DynamicallyLoadedModuleNames;

        /** Extra modules this module may require at run time, that are on behalf of another platform (i.e. shader formats and the like) */
		protected HashSet<string> PlatformSpecificDynamicallyLoadedModuleNames;

		/** Files which this module depends on at runtime. */
		public List<RuntimeDependency> RuntimeDependencies;

		public UEBuildModule(
			UEBuildTarget InTarget,
			string InName,
			UEBuildModuleType InType,
			string InModuleDirectory,
			bool? InIsRedistributableOverride,
			IEnumerable<string> InPublicDefinitions,
			IEnumerable<string> InPublicIncludePaths,
			IEnumerable<string> InPublicSystemIncludePaths,
			IEnumerable<string> InPublicLibraryPaths,
			IEnumerable<string> InPublicAdditionalLibraries,
			IEnumerable<string> InPublicFrameworks,
			IEnumerable<string> InPublicWeakFrameworks,
			IEnumerable<UEBuildFramework> InPublicAdditionalFrameworks,
			IEnumerable<string> InPublicAdditionalShadowFiles,
			IEnumerable<UEBuildBundleResource> InPublicAdditionalBundleResources,
			IEnumerable<string> InPublicIncludePathModuleNames,
			IEnumerable<string> InPublicDependencyModuleNames,
			IEnumerable<string> InPublicDelayLoadDLLs,
			IEnumerable<string> InPrivateIncludePaths,
			IEnumerable<string> InPrivateIncludePathModuleNames,
			IEnumerable<string> InPrivateDependencyModuleNames,
			IEnumerable<string> InCircularlyReferencedDependentModules,
			IEnumerable<string> InDynamicallyLoadedModuleNames,
            IEnumerable<string> InPlatformSpecificDynamicallyLoadedModuleNames,
			IEnumerable<RuntimeDependency> InRuntimeDependencies,
			string InBuildCsFilename
			)
		{
			Target = InTarget;
			Name = InName;
			Type = InType;
			ModuleDirectory = InModuleDirectory;
			PublicDefinitions = HashSetFromOptionalEnumerableStringParameter(InPublicDefinitions);
			PublicIncludePaths = HashSetFromOptionalEnumerableStringParameter(InPublicIncludePaths);
			PublicSystemIncludePaths = HashSetFromOptionalEnumerableStringParameter(InPublicSystemIncludePaths);
			PublicLibraryPaths = HashSetFromOptionalEnumerableStringParameter(InPublicLibraryPaths);
			PublicAdditionalLibraries = HashSetFromOptionalEnumerableStringParameter(InPublicAdditionalLibraries);
			PublicFrameworks = HashSetFromOptionalEnumerableStringParameter(InPublicFrameworks);
			PublicWeakFrameworks = HashSetFromOptionalEnumerableStringParameter(InPublicWeakFrameworks);
			PublicAdditionalFrameworks = InPublicAdditionalFrameworks == null ? new HashSet<UEBuildFramework>() : new HashSet<UEBuildFramework>(InPublicAdditionalFrameworks);
			PublicAdditionalShadowFiles = HashSetFromOptionalEnumerableStringParameter(InPublicAdditionalShadowFiles);
			PublicAdditionalBundleResources = InPublicAdditionalBundleResources == null ? new HashSet<UEBuildBundleResource>() : new HashSet<UEBuildBundleResource>(InPublicAdditionalBundleResources);
			PublicIncludePathModuleNames = HashSetFromOptionalEnumerableStringParameter( InPublicIncludePathModuleNames );
			PublicDependencyModuleNames = HashSetFromOptionalEnumerableStringParameter(InPublicDependencyModuleNames);
			PublicDelayLoadDLLs = HashSetFromOptionalEnumerableStringParameter(InPublicDelayLoadDLLs);
			PrivateIncludePaths = HashSetFromOptionalEnumerableStringParameter(InPrivateIncludePaths);
			PrivateIncludePathModuleNames = HashSetFromOptionalEnumerableStringParameter( InPrivateIncludePathModuleNames );
			PrivateDependencyModuleNames = HashSetFromOptionalEnumerableStringParameter( InPrivateDependencyModuleNames );
			CircularlyReferencedDependentModules = new HashSet<string>( HashSetFromOptionalEnumerableStringParameter( InCircularlyReferencedDependentModules ) );
			DynamicallyLoadedModuleNames = HashSetFromOptionalEnumerableStringParameter( InDynamicallyLoadedModuleNames );
            PlatformSpecificDynamicallyLoadedModuleNames = HashSetFromOptionalEnumerableStringParameter(InPlatformSpecificDynamicallyLoadedModuleNames);
			RuntimeDependencies = (InRuntimeDependencies == null)? new List<RuntimeDependency>() : new List<RuntimeDependency>(InRuntimeDependencies);
			IsRedistributableOverride = InIsRedistributableOverride;

			Debug.Assert(InBuildCsFilename == null || InBuildCsFilename.EndsWith(".Build.cs", StringComparison.InvariantCultureIgnoreCase));
			BuildCsFilenameField = InBuildCsFilename;

			SetupModuleDistributionLevel();

			Target.RegisterModule(this);
		}

		/** Adds a public definition */
		public virtual void AddPublicDefinition(string Definition)
		{
			PublicDefinitions.Add(Definition);
		}

		/** Adds a public include path. */
		public virtual void AddPublicIncludePath(string PathName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicIncludePaths.Contains(PathName))
				{
					return;
				}
			}
			PublicIncludePaths.Add(PathName);
		}

		/** Adds a public library path */
		public virtual void AddPublicLibraryPath(string PathName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicLibraryPaths.Contains(PathName))
				{
					return;
				}
			}
			PublicLibraryPaths.Add(PathName);
		}

		/** Adds a public additional library */
		public virtual void AddPublicAdditionalLibrary(string LibraryName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicAdditionalLibraries.Contains(LibraryName))
				{
					return;
				}
			}
			PublicAdditionalLibraries.Add(LibraryName);
		}

		/** Adds a public framework */
		public virtual void AddPublicFramework(string LibraryName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicFrameworks.Contains(LibraryName))
				{
					return;
				}
			}
			PublicFrameworks.Add(LibraryName);
		}

		/** Adds a public weak framework */
		public virtual void AddPublicWeakFramework(string LibraryName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicWeakFrameworks.Contains(LibraryName))
				{
					return;
				}
			}
			PublicWeakFrameworks.Add(LibraryName);
		}

		/** Adds a public additional framework */
		public virtual void AddPublicAdditionalFramework(UEBuildFramework Framework, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if ( PublicAdditionalFrameworks.Contains( Framework ) )
				{
					return;
				}
			}
			PublicAdditionalFrameworks.Add( Framework );
		}

		/** Adds a file that will be synced to a remote machine if a remote build is being executed */
		public virtual void AddPublicAdditionalShadowFile(string FileName)
		{
			PublicAdditionalShadowFiles.Add(FileName);
		}

		/** Adds a file or folder that will be copied to Mac or iOS app bundle */
		public virtual void AddAdditionalBundleResource(UEBuildBundleResource Resource, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if ( PublicAdditionalBundleResources.Contains( Resource ) )
				{
					return;
				}
			}
			PublicAdditionalBundleResources.Add( Resource );
		}

		/** Adds a public include path module. */
		public virtual void AddPublicIncludePathModule(string ModuleName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicIncludePathModuleNames.Contains(ModuleName))
				{
					return;
				}
			}
			PublicIncludePathModuleNames.Add(ModuleName);
		}

		/** Adds a public dependency module. */
		public virtual void AddPublicDependencyModule(string ModuleName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicDependencyModuleNames.Contains(ModuleName))
				{
					return;
				}
			}
			PublicDependencyModuleNames.Add(ModuleName);
		}
		
		/** Adds a dynamically loaded module. */
		public virtual void AddDynamicallyLoadedModule(string ModuleName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (DynamicallyLoadedModuleNames.Contains(ModuleName))
				{
					return;
				}
			}
			DynamicallyLoadedModuleNames.Add(ModuleName);
		}
        /** Adds a dynamically loaded module platform specifc. */
		public virtual void AddPlatformSpecificDynamicallyLoadedModule(string ModuleName, bool bCheckForDuplicates = true)
        {
            if (bCheckForDuplicates == true)
            {
                if (PlatformSpecificDynamicallyLoadedModuleNames.Contains(ModuleName))
                {
                    return;
                }
            }
            PlatformSpecificDynamicallyLoadedModuleNames.Add(ModuleName);
        }
		/** Adds a public delay load DLL */
		public virtual void AddPublicDelayLoadDLLs(string DelayLoadDLLName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PublicDelayLoadDLLs.Contains(DelayLoadDLLName))
				{
					return;
				}
			}
			PublicDelayLoadDLLs.Add(DelayLoadDLLName);
		}

		/** Adds a private include path. */
		public virtual void AddPrivateIncludePath(string PathName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PrivateIncludePaths.Contains(PathName))
				{
					return;
				}
			}
			PrivateIncludePaths.Add(PathName);
		}

		/** Adds a private include path module. */
		public virtual void AddPrivateIncludePathModule(string ModuleName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PrivateIncludePathModuleNames.Contains(ModuleName))
				{
					return;
				}
			}
			PrivateIncludePathModuleNames.Add(ModuleName);
		}

		/** Adds a dependency module. */
		public virtual void AddPrivateDependencyModule(string ModuleName, bool bCheckForDuplicates = true)
		{
			if (bCheckForDuplicates == true)
			{
				if (PrivateDependencyModuleNames.Contains(ModuleName))
				{
					return;
				}
			}
			PrivateDependencyModuleNames.Add(ModuleName);
		}

		/** Tells the module that a specific dependency is circularly reference */
		public virtual void AddCircularlyReferencedDependentModule(string ModuleName)
		{
			CircularlyReferencedDependentModules.Add( ModuleName );
		}

		/** Adds an extra module name */
		public virtual void AddExtraModuleName(string ModuleName)
		{
			DynamicallyLoadedModuleNames.Add( ModuleName );
		}

		/**
		 * Clears all public include paths.
		 */
		internal void ClearPublicIncludePaths()
		{
			PublicIncludePaths.Clear();
		}

		/** 
		 * Clears all public library paths.
		 * This is to allow a given platform the opportunity to remove set libraries.
		 */
		internal void ClearPublicLibraryPaths()
		{
			PublicLibraryPaths.Clear();
		}

		/** 
		 * Clears all public additional libraries from the module.
		 * This is to allow a given platform the opportunity to remove set libraries.
		 */
		internal void ClearPublicAdditionalLibraries()
		{
			PublicAdditionalLibraries.Clear();
		}

		/// <summary>
		/// If present, remove the given library from the PublicAdditionalLibraries array
		/// </summary>
		/// <param name="InLibrary">The library to remove from the list</param>
		internal void RemovePublicAdditionalLibrary(string InLibrary)
		{
			PublicAdditionalLibraries.Remove(InLibrary);
		}

		/**
		 * If found, remove the given entry from the definitions of the module.
		 */
		internal void RemovePublicDefinition(string InDefintion)
		{
			PublicDefinitions.Remove(InDefintion);
		}

		/** Sets up the environment for compiling any module that includes the public interface of this module. */
		protected virtual void SetupPublicCompileEnvironment(
			UEBuildBinary SourceBinary,
			bool bIncludePathsOnly,
			HashSet<string> IncludePaths,
			HashSet<string> SystemIncludePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks,
			Dictionary<UEBuildModule, bool> VisitedModules
			)
		{
			// There may be circular dependencies in compile dependencies, so we need to avoid reentrance.
			if(!VisitedModules.ContainsKey(this))
			{
				VisitedModules.Add(this,true);

				// Add this module's public include paths and definitions.
				AddIncludePathsWithChecks(IncludePaths, PublicIncludePaths);
				AddIncludePathsWithChecks(SystemIncludePaths, PublicSystemIncludePaths);
				Definitions.AddRange(PublicDefinitions);
				
				// If this module is being built into a DLL or EXE, set up an IMPORTS or EXPORTS definition for it.
				if (Binary != null)
				{
					string BinaryPath = Binary.Config.OutputFilePaths[0];
					string SourceBinaryPath = SourceBinary.Config.OutputFilePaths[0];

					if (ProjectFileGenerator.bGenerateProjectFiles || (Binary.Config.Type == UEBuildBinaryType.StaticLibrary))
					{
						// When generating IntelliSense files, never add dllimport/dllexport specifiers as it
						// simply confuses the compiler
						Definitions.Add( Name.ToUpperInvariant() + "_API=" );
					}
					else if( Binary == SourceBinary )
					{
						if( Binary.Config.bAllowExports )
						{
							Log.TraceVerbose( "{0}: Exporting {1} from {2}", Path.GetFileNameWithoutExtension( SourceBinaryPath ), Name, Path.GetFileNameWithoutExtension( BinaryPath ) );
							Definitions.Add( Name.ToUpperInvariant() + "_API=DLLEXPORT" );
						}
						else
						{
							Log.TraceVerbose( "{0}: Not importing/exporting {1} (binary: {2})", Path.GetFileNameWithoutExtension( SourceBinaryPath ), Name, Path.GetFileNameWithoutExtension( BinaryPath ) );
							Definitions.Add( Name.ToUpperInvariant() + "_API=" );
						}
					}
					else
					{
						// @todo SharedPCH: Public headers included from modules that are not importing the module of that public header, seems invalid.  
						//		Those public headers have no business having APIs in them.  OnlineSubsystem has some public headers like this.  Without changing
						//		this, we need to suppress warnings at compile time.
						if( bIncludePathsOnly )
						{
							Log.TraceVerbose( "{0}: Include paths only for {1} (binary: {2})", Path.GetFileNameWithoutExtension( SourceBinaryPath ), Name, Path.GetFileNameWithoutExtension( BinaryPath ) );
							Definitions.Add( Name.ToUpperInvariant() + "_API=" );
						}
						else if (Binary.Config.bAllowExports)
						{
							Log.TraceVerbose( "{0}: Importing {1} from {2}", Path.GetFileNameWithoutExtension( SourceBinaryPath ), Name, Path.GetFileNameWithoutExtension( BinaryPath ) );
							Definitions.Add( Name.ToUpperInvariant() + "_API=DLLIMPORT" );
						}
						else
						{
							Log.TraceVerbose( "{0}: Not importing/exporting {1} (binary: {2})", Path.GetFileNameWithoutExtension( SourceBinaryPath ), Name, Path.GetFileNameWithoutExtension( BinaryPath ));
							Definitions.Add( Name.ToUpperInvariant() + "_API=" );
						}
					}
				}

				if( !bIncludePathsOnly )
				{
					// Recurse on this module's public dependencies.
					foreach(var DependencyName in PublicDependencyModuleNames)
					{
						var DependencyModule = Target.GetModuleByName(DependencyName);
						DependencyModule.SetupPublicCompileEnvironment(SourceBinary,bIncludePathsOnly, IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks, VisitedModules);
					}
				}

				// Now add an include paths from modules with header files that we need access to, but won't necessarily be importing
				foreach( var IncludePathModuleName in PublicIncludePathModuleNames )
				{
					bool bInnerIncludePathsOnly = true;
					var IncludePathModule = Target.GetModuleByName(IncludePathModuleName);
					IncludePathModule.SetupPublicCompileEnvironment( SourceBinary, bInnerIncludePathsOnly, IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks, VisitedModules );
				}

				// Add the module's directory to the include path, so we can root #includes to it
				IncludePaths.Add(Utils.CleanDirectorySeparators(Utils.MakePathRelativeTo(ModuleDirectory, Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source")), '/'));

				// Add the additional frameworks so that the compiler can know about their #include paths
				AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);
				
				// Remember the module so we can refer to it when needed
				foreach (var Framework in PublicAdditionalFrameworks)
				{
					Framework.OwningModule = this;
				}
			}
		}

		static Regex VCMacroRegex = new Regex(@"\$\([A-Za-z0-9_]+\)");

		/** Checks if path contains a VC macro */
		protected bool DoesPathContainVCMacro(string Path)
		{
			return VCMacroRegex.IsMatch(Path);
		}

		/** Adds PathsToAdd to IncludePaths, performing path normalization and ignoring duplicates. */
		protected void AddIncludePathsWithChecks(HashSet<string> IncludePaths, HashSet<string> PathsToAdd)
		{
			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				// Extra checks are switched off for IntelliSense generation as they provide
				// no additional value and cause performance impact.
				IncludePaths.UnionWith(PathsToAdd);
			}
			else
			{
				foreach (var Path in PathsToAdd)
				{
					var NormalizedPath = Path.TrimEnd('/');
					// If path doesn't exist, it may contain VC macro (which is passed directly to and expanded by compiler).
					if (Directory.Exists(NormalizedPath) || DoesPathContainVCMacro(NormalizedPath))
					{
						IncludePaths.Add(NormalizedPath);
					}
				}
			}
		}

		/** Sets up the environment for compiling this module. */
		protected virtual void SetupPrivateCompileEnvironment(
			HashSet<string> IncludePaths,
			HashSet<string> SystemIncludePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks
			)
		{
			var VisitedModules = new Dictionary<UEBuildModule, bool>();

			if (this.Type == UEBuildModuleType.Game)
			{
				Definitions.Add("DEPRECATED_FORGAME=DEPRECATED");
			}

			// Add this module's private include paths and definitions.
			AddIncludePathsWithChecks(IncludePaths, PrivateIncludePaths);

			// Allow the module's public dependencies to modify the compile environment.
			bool bIncludePathsOnly = false;
			SetupPublicCompileEnvironment(Binary,bIncludePathsOnly, IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks, VisitedModules);

			// Also allow the module's private dependencies to modify the compile environment.
			foreach(var DependencyName in PrivateDependencyModuleNames)
			{
				var DependencyModule = Target.GetModuleByName(DependencyName);
				DependencyModule.SetupPublicCompileEnvironment(Binary,bIncludePathsOnly, IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks, VisitedModules);
			}

			// Add include paths from modules with header files that our private files need access to, but won't necessarily be importing
			foreach( var IncludePathModuleName in PrivateIncludePathModuleNames )
			{
				bool bInnerIncludePathsOnly = true;
				var IncludePathModule = Target.GetModuleByName(IncludePathModuleName);
				IncludePathModule.SetupPublicCompileEnvironment(Binary, bInnerIncludePathsOnly, IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks, VisitedModules);
			}
		}

		/** Sets up the environment for linking any module that includes the public interface of this module. */ 
		protected virtual void SetupPublicLinkEnvironment(
			UEBuildBinary SourceBinary,
			List<string> LibraryPaths,
			List<string> AdditionalLibraries,
			List<string> Frameworks,
			List<string> WeakFrameworks,
			List<UEBuildFramework> AdditionalFrameworks,
			List<string> AdditionalShadowFiles,
			List<UEBuildBundleResource> AdditionalBundleResources,
			List<string> DelayLoadDLLs,
			List<UEBuildBinary> BinaryDependencies,
			Dictionary<UEBuildModule, bool> VisitedModules
			)
		{
			// There may be circular dependencies in compile dependencies, so we need to avoid reentrance.
			if(!VisitedModules.ContainsKey(this))
			{
				VisitedModules.Add(this,true);

				// Only process modules that are included in the current target.
				if(bIncludedInTarget)
				{
					// Add this module's binary to the binary dependencies.
					if(Binary != null
					&& Binary != SourceBinary
					&& !BinaryDependencies.Contains(Binary))
					{
						BinaryDependencies.Add(Binary);
					}

					// If this module belongs to a static library that we are not currently building, recursively add the link environment settings for all of its dependencies too.
					// Keep doing this until we reach a module that is not part of a static library (or external module, since they have no associated binary).
					// Static libraries do not contain the symbols for their dependencies, so we need to recursively gather them to be linked into other binary types.
					bool bIsBuildingAStaticLibrary = (SourceBinary != null && SourceBinary.Config.Type == UEBuildBinaryType.StaticLibrary);
					bool bIsModuleBinaryAStaticLibrary = (Binary != null && Binary.Config.Type == UEBuildBinaryType.StaticLibrary);
					if (!bIsBuildingAStaticLibrary && bIsModuleBinaryAStaticLibrary)
					{
						// Gather all dependencies and recursively call SetupPublicLinkEnvironmnet
						List<string> AllDependencyModuleNames = new List<string>(PrivateDependencyModuleNames);
						AllDependencyModuleNames.AddRange(PublicDependencyModuleNames);

						foreach (var DependencyName in AllDependencyModuleNames)
						{
							var DependencyModule = Target.GetModuleByName(DependencyName);
							bool bIsExternalModule = (DependencyModule as UEBuildExternalModule != null);
							bool bIsInStaticLibrary = (DependencyModule.Binary != null && DependencyModule.Binary.Config.Type == UEBuildBinaryType.StaticLibrary);
							if (bIsExternalModule || bIsInStaticLibrary)
							{
								DependencyModule.SetupPublicLinkEnvironment(SourceBinary, LibraryPaths, AdditionalLibraries, Frameworks, WeakFrameworks,
									AdditionalFrameworks, AdditionalShadowFiles, AdditionalBundleResources, DelayLoadDLLs, BinaryDependencies, VisitedModules);
							}
						}
					}

					// Add this module's public include library paths and additional libraries.
					LibraryPaths.AddRange(PublicLibraryPaths);
					AdditionalLibraries.AddRange(PublicAdditionalLibraries);
					Frameworks.AddRange(PublicFrameworks);
					WeakFrameworks.AddRange(PublicWeakFrameworks);
					AdditionalBundleResources.AddRange(PublicAdditionalBundleResources);
					// Remember the module so we can refer to it when needed
					foreach ( var Framework in PublicAdditionalFrameworks )
					{
						Framework.OwningModule = this;
					}
					AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);
					AdditionalShadowFiles.AddRange(PublicAdditionalShadowFiles);
					DelayLoadDLLs.AddRange(PublicDelayLoadDLLs);
				}
			}
		}

		/** Sets up the environment for linking this module. */
		public virtual void SetupPrivateLinkEnvironment(
			UEBuildBinary SourceBinary,
			LinkEnvironment LinkEnvironment,
			List<UEBuildBinary> BinaryDependencies,
			Dictionary<UEBuildModule, bool> VisitedModules
			)
		{
			// Allow the module's public dependencies to add library paths and additional libraries to the link environment.
			SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.Config.LibraryPaths, LinkEnvironment.Config.AdditionalLibraries, LinkEnvironment.Config.Frameworks, LinkEnvironment.Config.WeakFrameworks,
				LinkEnvironment.Config.AdditionalFrameworks,LinkEnvironment.Config.AdditionalShadowFiles, LinkEnvironment.Config.AdditionalBundleResources, LinkEnvironment.Config.DelayLoadDLLs, BinaryDependencies, VisitedModules);

			// Also allow the module's public and private dependencies to modify the link environment.
			List<string> AllDependencyModuleNames = new List<string>(PrivateDependencyModuleNames);
			AllDependencyModuleNames.AddRange(PublicDependencyModuleNames);

			foreach (var DependencyName in AllDependencyModuleNames)
			{
				var DependencyModule = Target.GetModuleByName(DependencyName);
				DependencyModule.SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.Config.LibraryPaths, LinkEnvironment.Config.AdditionalLibraries, LinkEnvironment.Config.Frameworks, LinkEnvironment.Config.WeakFrameworks,
					LinkEnvironment.Config.AdditionalFrameworks, LinkEnvironment.Config.AdditionalShadowFiles, LinkEnvironment.Config.AdditionalBundleResources, LinkEnvironment.Config.DelayLoadDLLs, BinaryDependencies, VisitedModules);
			}
		}

		/** Compiles the module, and returns a list of files output by the compiler. */
		public abstract List<FileItem> Compile( CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment );
		
		// Object interface.
		public override string ToString()
		{
			return Name;
		}


		/**
		 * Gets all of the modules referenced by this module
		 * 
		 * @param	ReferencedModules	Hash of all referenced modules
		 * @param	OrderedModules		Ordered list of the referenced modules
		 * @param	bIncludeDynamicallyLoaded	True if dynamically loaded modules (and all of their dependent modules) should be included.
		 * @param	bForceCircular	True if circular dependencies should be processed
		 * @param	bOnlyDirectDependencies	True to return only this module's direct dependencies
		 */
		public virtual void GetAllDependencyModules(Dictionary<string, UEBuildModule> ReferencedModules, List<UEBuildModule> OrderedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies)
		{
		}

		/**
		 * Gets all of the modules precompiled along with this module
		 * 
		 * @param	Modules	Set of all the precompiled modules
		 */
		public virtual void RecursivelyAddPrecompiledModules(List<UEBuildModule> Modules)
		{
		}

		/**
		 * Gathers and binds binaries for this module and all of it's dependent modules
		 */
		public virtual void RecursivelyProcessUnboundModules()
		{
		}

		/**
		 * Recursively adds modules that are referenced only for include paths (not actual dependencies).
		 * 
		 * @param	Target	The target we are currently building
		 * @param	bPublicIncludesOnly	True to only add modules for public includes, false to add all include path modules.  Regardless of what you pass here, recursive adds will only add public include path modules.
		 */
		public virtual void RecursivelyAddIncludePathModules( UEBuildTarget Target, bool bPublicIncludesOnly )
		{
		}
	};

	/** A module that is never compiled by us, and is only used to group include paths and libraries into a dependency unit. */
	class UEBuildExternalModule : UEBuildModule
	{
		public UEBuildExternalModule(
			UEBuildTarget InTarget,
			UEBuildModuleType InType,
			string InName,
			string InModuleDirectory,
			bool? InIsRedistributableOverride,
			IEnumerable<string> InPublicDefinitions,
			IEnumerable<string> InPublicIncludePaths,
			IEnumerable<string> InPublicSystemIncludePaths,
			IEnumerable<string> InPublicLibraryPaths,
			IEnumerable<string> InPublicAdditionalLibraries,
			IEnumerable<string> InPublicFrameworks,
			IEnumerable<string> InPublicWeakFrameworks,
			IEnumerable<UEBuildFramework> InPublicAdditionalFrameworks,
			IEnumerable<string> InPublicAdditionalShadowFiles,
			IEnumerable<UEBuildBundleResource> InPublicAdditionalBundleResources,
			IEnumerable<string> InPublicDependencyModuleNames,
			IEnumerable<string> InPublicDelayLoadDLLs,		// Delay loaded DLLs that should be setup when including this module
			IEnumerable<RuntimeDependency> InRuntimeDependencies,
			string InBuildCsFilename
			)
		: base(
			InTarget:										InTarget,
			InType:											InType,
			InName:											InName,
			InModuleDirectory:								InModuleDirectory,
			InIsRedistributableOverride:					InIsRedistributableOverride,
			InPublicDefinitions:							InPublicDefinitions,
			InPublicIncludePaths:							InPublicIncludePaths,
			InPublicSystemIncludePaths:						InPublicSystemIncludePaths,
			InPublicLibraryPaths:							InPublicLibraryPaths,
			InPublicAdditionalLibraries:					InPublicAdditionalLibraries,
			InPublicFrameworks:								InPublicFrameworks,
			InPublicWeakFrameworks:							InPublicWeakFrameworks,
			InPublicAdditionalFrameworks:					InPublicAdditionalFrameworks,
			InPublicAdditionalShadowFiles:					InPublicAdditionalShadowFiles,
			InPublicAdditionalBundleResources:				InPublicAdditionalBundleResources,
			InPublicIncludePathModuleNames:					null,
			InPublicDependencyModuleNames:					InPublicDependencyModuleNames,
			InPublicDelayLoadDLLs:							InPublicDelayLoadDLLs,
			InPrivateIncludePaths:							null,
			InPrivateIncludePathModuleNames:				null,
			InPrivateDependencyModuleNames:					null,
			InCircularlyReferencedDependentModules:			null,
			InDynamicallyLoadedModuleNames:					null,
			InPlatformSpecificDynamicallyLoadedModuleNames: null,
			InRuntimeDependencies:							InRuntimeDependencies,
			InBuildCsFilename:								InBuildCsFilename
			)
		{
			bIncludedInTarget = true;
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment)
		{
			return new List<FileItem>();
		}
	};

	/** A module that is compiled from C++ code. */	
	public class UEBuildModuleCPP : UEBuildModule
	{
		public class AutoGenerateCppInfoClass
		{
			public class BuildInfoClass
			{
				/** The wildcard of the *.generated.cpp file which was generated for the module */
				public readonly string FileWildcard;

				public BuildInfoClass(string InWildcard)
				{
					Debug.Assert(InWildcard != null);

					FileWildcard = InWildcard;
				}
			}

			/** Information about how to build the .generated.cpp files. If this is null, then we're not building .generated.cpp files for this module. */
			public BuildInfoClass BuildInfo;

			public AutoGenerateCppInfoClass(BuildInfoClass InBuildInfo)
			{
				BuildInfo = InBuildInfo;
			}
		}

		/** Information about the .generated.cpp file.  If this is null then this module doesn't have any UHT-produced code. */
		public AutoGenerateCppInfoClass AutoGenerateCppInfo = null;

		public class SourceFilesClass
		{
			public readonly List<FileItem> MissingFiles = new List<FileItem>();
			public readonly List<FileItem> CPPFiles     = new List<FileItem>();
			public readonly List<FileItem> CFiles       = new List<FileItem>();
			public readonly List<FileItem> CCFiles      = new List<FileItem>();
			public readonly List<FileItem> MMFiles      = new List<FileItem>();
			public readonly List<FileItem> RCFiles      = new List<FileItem>();
			public readonly List<FileItem> OtherFiles   = new List<FileItem>();

			public int Count
			{
				get
				{
					return MissingFiles.Count +
					       CPPFiles    .Count +
					       CFiles      .Count +
					       CCFiles     .Count +
					       MMFiles     .Count +
					       RCFiles     .Count +
					       OtherFiles  .Count;
				}
			}

			/// <summary>
			/// Copy from list to list helper.
			/// </summary>
			/// <param name="From">Source list.</param>
			/// <param name="To">Destination list.</param>
			private static void CopyFromListToList(List<FileItem> From, List<FileItem> To)
			{
				To.Clear();
				To.AddRange(From);
			}

			/// <summary>
			/// Copies file lists from other SourceFilesClass to this.
			/// </summary>
			/// <param name="Other">Source object.</param>
			public void CopyFrom(SourceFilesClass Other)
			{
				CopyFromListToList(Other.MissingFiles, MissingFiles);
				CopyFromListToList(Other.CPPFiles, CPPFiles);
				CopyFromListToList(Other.CFiles, CFiles);
				CopyFromListToList(Other.CCFiles, CCFiles);
				CopyFromListToList(Other.MMFiles, MMFiles);
				CopyFromListToList(Other.RCFiles, RCFiles);
				CopyFromListToList(Other.OtherFiles, OtherFiles);
			}
		}

		/**
		 * Adds additional source cpp files for this module.
		 *
		 * @param Files Files to add.
		 */
		public void AddAdditionalCPPFiles(IEnumerable<FileItem> Files)
		{
			SourceFilesToBuild.CPPFiles.AddRange(Files);
		}

		/** A list of the absolute paths of source files to be built in this module. */
		public readonly SourceFilesClass SourceFilesToBuild = new SourceFilesClass();

		/** A list of the source files that were found for the module. */
		public readonly SourceFilesClass SourceFilesFound = new SourceFilesClass();

		/** The directory for this module's generated code */
		public readonly string GeneratedCodeDirectory;

		/** The preprocessor definitions used to compile this module's private implementation. */
		HashSet<string> Definitions;

		/// When set, allows this module to report compiler definitions and include paths for Intellisense
		IntelliSenseGatherer IntelliSenseGatherer;

		/** Whether this module contains performance critical code. */
		ModuleRules.CodeOptimization OptimizeCode = ModuleRules.CodeOptimization.Default;

		/** Whether this module should use a "shared PCH" header if possible.  Disable this for modules that have lots of global includes
			in a private PCH header (e.g. game modules) */
		public bool bAllowSharedPCH = true;

		/** Header file name for a shared PCH provided by this module.  Must be a valid relative path to a public C++ header file.
			This should only be set for header files that are included by a significant number of other C++ modules. */
		public string SharedPCHHeaderFile = String.Empty;

		/** Use run time type information */
		public bool bUseRTTI = false;

		/** Enable buffer security checks. This should usually be enabled as it prevents severe security risks. */
		public bool bEnableBufferSecurityChecks = true;

		/** If true and unity builds are enabled, this module will build without unity. */
		public bool bFasterWithoutUnity = false;

		/** Overrides BuildConfiguration.MinFilesUsingPrecompiledHeader if non-zero. */
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/** Enable exception handling */
		public bool bEnableExceptions = false;

		/** Enable warnings for shadowed variables */
		public bool bEnableShadowVariableWarnings;

		public List<string> IncludeSearchPaths = new List<string>();

		public class ProcessedDependenciesClass
		{
			/** The file, if any, which is used as the unique PCH for this module */
			public FileItem UniquePCHHeaderFile = null;
		}

		/** The processed dependencies for the class */
		public ProcessedDependenciesClass ProcessedDependencies = null;

		/** @hack to skip adding definitions to compile environment. They will be baked into source code by external code. */
		public bool bSkipDefinitionsForCompileEnvironment = false;

		/** Categorizes source files into per-extension buckets */
		private static void CategorizeSourceFiles(IEnumerable<FileItem> InSourceFiles, SourceFilesClass OutSourceFiles)
		{
			foreach (var SourceFile in InSourceFiles)
			{
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();
				if (!SourceFile.bExists)
				{
					OutSourceFiles.MissingFiles.Add(SourceFile);
				}
				else if (Extension == ".CPP")
				{
					OutSourceFiles.CPPFiles.Add(SourceFile);
				}
				else if (Extension == ".C")
				{
					OutSourceFiles.CFiles.Add(SourceFile);
				}
				else if (Extension == ".CC")
				{
					OutSourceFiles.CCFiles.Add(SourceFile);
				}
				else if (Extension == ".MM" || Extension == ".M")
				{
					OutSourceFiles.MMFiles.Add(SourceFile);
				}
				else if (Extension == ".RC")
				{
					OutSourceFiles.RCFiles.Add(SourceFile);
				}
				else
				{
					OutSourceFiles.OtherFiles.Add(SourceFile);
				}
			}
		}

		public UEBuildModuleCPP(
			UEBuildTarget InTarget,
			string InName,
			UEBuildModuleType InType,
			string InModuleDirectory,
			string InGeneratedCodeDirectory,
			bool? InIsRedistributableOverride,
			IntelliSenseGatherer InIntelliSenseGatherer,
			IEnumerable<FileItem> InSourceFiles,
			IEnumerable<string> InPublicIncludePaths,
			IEnumerable<string> InPublicSystemIncludePaths,
            IEnumerable<string> InPublicLibraryPaths,
			IEnumerable<string> InDefinitions,
			IEnumerable<string> InPublicIncludePathModuleNames,
			IEnumerable<string> InPublicDependencyModuleNames,
			IEnumerable<string> InPublicDelayLoadDLLs,
			IEnumerable<string> InPublicAdditionalLibraries,
			IEnumerable<string> InPublicFrameworks,
			IEnumerable<string> InPublicWeakFrameworks,
			IEnumerable<UEBuildFramework> InPublicAdditionalFrameworks,
			IEnumerable<string> InPublicAdditionalShadowFiles,
			IEnumerable<UEBuildBundleResource> InPublicAdditionalBundleResources,
			IEnumerable<string> InPrivateIncludePaths,
			IEnumerable<string> InPrivateIncludePathModuleNames,
			IEnumerable<string> InPrivateDependencyModuleNames,
			IEnumerable<string> InCircularlyReferencedDependentModules,
			IEnumerable<string> InDynamicallyLoadedModuleNames,
            IEnumerable<string> InPlatformSpecificDynamicallyLoadedModuleNames,
			IEnumerable<RuntimeDependency> InRuntimeDependencies,
            ModuleRules.CodeOptimization InOptimizeCode,
			bool InAllowSharedPCH,
			string InSharedPCHHeaderFile,
			bool InUseRTTI,
			bool InEnableBufferSecurityChecks,
			bool InFasterWithoutUnity,
			int InMinFilesUsingPrecompiledHeaderOverride,
			bool InEnableExceptions,
			bool InEnableShadowVariableWarnings,
			bool bInBuildSourceFiles,
			string InBuildCsFilename
			)
			: base(	InTarget,
					InName, 
					InType,
					InModuleDirectory,
					InIsRedistributableOverride,
					InDefinitions, 
					InPublicIncludePaths,
					InPublicSystemIncludePaths, 
					InPublicLibraryPaths, 
					InPublicAdditionalLibraries,
					InPublicFrameworks,
					InPublicWeakFrameworks,
					InPublicAdditionalFrameworks,
					InPublicAdditionalShadowFiles,
					InPublicAdditionalBundleResources,
					InPublicIncludePathModuleNames,
					InPublicDependencyModuleNames,
					InPublicDelayLoadDLLs,
					InPrivateIncludePaths, 
					InPrivateIncludePathModuleNames,
					InPrivateDependencyModuleNames, 
					InCircularlyReferencedDependentModules,
					InDynamicallyLoadedModuleNames,
                    InPlatformSpecificDynamicallyLoadedModuleNames,
					InRuntimeDependencies,
					InBuildCsFilename
				)
		{
			GeneratedCodeDirectory = InGeneratedCodeDirectory;
			IntelliSenseGatherer = InIntelliSenseGatherer;

			CategorizeSourceFiles(InSourceFiles, SourceFilesFound);
			if (bInBuildSourceFiles)
			{
				SourceFilesToBuild.CopyFrom(SourceFilesFound);
			}

			Definitions = HashSetFromOptionalEnumerableStringParameter(InDefinitions);
			foreach (var Def in Definitions)
			{
				Log.TraceVerbose("Compile Env {0}: {1}", Name, Def);
			}
			OptimizeCode                           = InOptimizeCode;
			bAllowSharedPCH                        = InAllowSharedPCH;
			SharedPCHHeaderFile                    = InSharedPCHHeaderFile;
			bUseRTTI                               = InUseRTTI;
			bEnableBufferSecurityChecks 		   = InEnableBufferSecurityChecks;
			bFasterWithoutUnity                    = InFasterWithoutUnity;
			MinFilesUsingPrecompiledHeaderOverride = InMinFilesUsingPrecompiledHeaderOverride;
			bEnableExceptions                      = InEnableExceptions;
			bEnableShadowVariableWarnings          = InEnableShadowVariableWarnings;
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment)
		{
			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);

			var LinkInputFiles = new List<FileItem>();
			if( ProjectFileGenerator.bGenerateProjectFiles && IntelliSenseGatherer == null)
			{
				// Nothing to do for IntelliSense, bail out early
				return LinkInputFiles;
			}

			var ModuleCompileEnvironment = CreateModuleCompileEnvironment(CompileEnvironment);
			IncludeSearchPaths = ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludePaths.ToList();
			IncludeSearchPaths.AddRange(ModuleCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.ToList());

			if( IntelliSenseGatherer != null )
			{
				// Update project file's set of preprocessor definitions and include paths
				IntelliSenseGatherer.AddIntelliSensePreprocessorDefinitions( ModuleCompileEnvironment.Config.Definitions );
				IntelliSenseGatherer.AddInteliiSenseIncludePaths( ModuleCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths, bAddingSystemIncludes: true );
				IntelliSenseGatherer.AddInteliiSenseIncludePaths( ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludePaths, bAddingSystemIncludes: false );

				// Bail out.  We don't need to actually compile anything while generating project files.
				return LinkInputFiles;
			}

			// Throw an error if the module's source file list referenced any non-existent files.
			if (SourceFilesToBuild.MissingFiles.Count > 0)
			{
				throw new BuildException(
					"UBT ERROR: Module \"{0}\" references non-existent files:\n{1} (perhaps a file was added to the project but not checked in)",
					Name,
					string.Join("\n", SourceFilesToBuild.MissingFiles.Select(M => M.AbsolutePath))
				);
			}

			// For an executable or a static library do not use the default RC file - 
			// If the executable wants it, it will be in their source list anyway.
			// The issue here is that when making a monolithic game, the processing
			// of the other game modules will stomp the game-specific rc file.
			if (Binary.Config.Type == UEBuildBinaryType.DynamicLinkLibrary)
			{
				// Add default PCLaunch.rc file if this module has no own resource file specified
				if (SourceFilesToBuild.RCFiles.Count <= 0)
				{
					string DefRC = Utils.CleanDirectorySeparators( Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), "Runtime/Launch/Resources/Windows/PCLaunch.rc")) );
					FileItem Item = FileItem.GetItemByFullPath(DefRC);
					SourceFilesToBuild.RCFiles.Add(Item);
				}

				// Always compile in the API version resource separately. This is required for the module manager to detect compatible API versions.
				string ModuleVersionRC = Utils.CleanDirectorySeparators( Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), "Runtime/Core/Resources/Windows/ModuleVersionResource.rc.inl")) );
				FileItem ModuleVersionItem = FileItem.GetItemByFullPath(ModuleVersionRC);
				if( !SourceFilesToBuild.RCFiles.Contains(ModuleVersionItem) )
				{
					SourceFilesToBuild.RCFiles.Add(ModuleVersionItem);
				}
			}


			{ 
				// Process all of the header file dependencies for this module
				this.CachePCHUsageForModuleSourceFiles( ModuleCompileEnvironment );

				// Make sure our RC files have cached includes.  
				foreach( var RCFile in SourceFilesToBuild.RCFiles )
				{ 
					RCFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;
				}
			}


			// Check to see if this is an Engine module (including program or plugin modules).  That is, the module is located under the "Engine" folder
			var IsGameModule = !Utils.IsFileUnderDirectory( this.ModuleDirectory, Path.Combine( ProjectFileGenerator.EngineRelativePath ) );

			// Should we force a precompiled header to be generated for this module?  Usually, we only bother with a
			// precompiled header if there are at least several source files in the module (after combining them for unity
			// builds.)  But for game modules, it can be convenient to always have a precompiled header to single-file
			// changes to code is really quick to compile.
			int MinFilesUsingPrecompiledHeader = BuildConfiguration.MinFilesUsingPrecompiledHeader;
			if( MinFilesUsingPrecompiledHeaderOverride != 0 )
			{
				MinFilesUsingPrecompiledHeader = MinFilesUsingPrecompiledHeaderOverride;
			}
			else if( IsGameModule && BuildConfiguration.bForcePrecompiledHeaderForGameModules )
			{
				// This is a game module with only a small number of source files, so go ahead and force a precompiled header
				// to be generated to make incremental changes to source files as fast as possible for small projects.
				MinFilesUsingPrecompiledHeader = 1;
			}


			// Should we use unity build mode for this module?
			bool bModuleUsesUnityBuild = BuildConfiguration.bUseUnityBuild || BuildConfiguration.bForceUnityBuild;
			if (!BuildConfiguration.bForceUnityBuild)
			{
				if (bFasterWithoutUnity)
				{
					bModuleUsesUnityBuild = false;
				}
				else if (IsGameModule && SourceFilesToBuild.CPPFiles.Count < BuildConfiguration.MinGameModuleSourceFilesForUnityBuild)
				{
					// Game modules with only a small number of source files are usually better off having faster iteration times
					// on single source file changes, so we forcibly disable unity build for those modules
					bModuleUsesUnityBuild = false;
				}
			}

			// The environment with which to compile the CPP files
			var CPPCompileEnvironment = ModuleCompileEnvironment;

			// Precompiled header support.
			bool bWasModuleCodeCompiled = false;
			if (BuildPlatform.ShouldUsePCHFiles(CompileEnvironment.Config.Target.Platform, CompileEnvironment.Config.Target.Configuration))
			{
				var PCHGenTimerStart = DateTime.UtcNow;

				// The code below will figure out whether this module will either use a "unique PCH" (private PCH that will only be included by
				// this module's code files), or a "shared PCH" (potentially included by many code files in many modules.)  Only one or the other
				// will be used.
				FileItem SharedPCHHeaderFile = null;

				// In the case of a shared PCH, we also need to keep track of which module that PCH's header file is a member of
				string SharedPCHModuleName = String.Empty;

				if( BuildConfiguration.bUseSharedPCHs && CompileEnvironment.Config.bIsBuildingLibrary )
				{
					Log.TraceVerbose("Module '{0}' was not allowed to use Shared PCHs, because we're compiling to a library", this.Name );
				}

				bool bUseSharedPCHFiles  = BuildConfiguration.bUseSharedPCHs && !CompileEnvironment.Config.bIsBuildingLibrary && GlobalCompileEnvironment.SharedPCHHeaderFiles.Count > 0;

				if( bUseSharedPCHFiles )
				{
					string SharingPCHHeaderFilePath = null;
					bool bIsASharedPCHModule = bUseSharedPCHFiles && GlobalCompileEnvironment.SharedPCHHeaderFiles.Any( PCH => PCH.Module == this );
					if( bIsASharedPCHModule )
					{
						SharingPCHHeaderFilePath = Path.GetFullPath( Path.Combine( ProjectFileGenerator.RootRelativePath, "Engine", "Source", this.SharedPCHHeaderFile ) );
					}

					// We can't use a shared PCH file when compiling a module
					// with exports, because the shared PCH can only have imports in it to work correctly.
					bool bCanModuleUseOwnSharedPCH = bAllowSharedPCH && bIsASharedPCHModule && !Binary.Config.bAllowExports && ProcessedDependencies.UniquePCHHeaderFile.AbsolutePath.Equals( SharingPCHHeaderFilePath, StringComparison.InvariantCultureIgnoreCase );
					if( bAllowSharedPCH && ( !bIsASharedPCHModule || bCanModuleUseOwnSharedPCH ) )
					{
						// Figure out which shared PCH tier we're in
						var DirectDependencyModules = new List<UEBuildModule>();
						{ 
							var ReferencedModules = new Dictionary<string, UEBuildModule>( StringComparer.InvariantCultureIgnoreCase );
							this.GetAllDependencyModules( ReferencedModules, DirectDependencyModules, bIncludeDynamicallyLoaded:false, bForceCircular:false, bOnlyDirectDependencies:true );
						}

						int LargestSharedPCHHeaderFileIndex = -1;
						foreach( var DependencyModule in DirectDependencyModules )
						{
							// These Shared PCHs are ordered from least complex to most complex.  We'll start at the last one and search backwards.
							for( var SharedPCHHeaderFileIndex = GlobalCompileEnvironment.SharedPCHHeaderFiles.Count - 1; SharedPCHHeaderFileIndex > LargestSharedPCHHeaderFileIndex; --SharedPCHHeaderFileIndex )
							{
								var CurSharedPCHHeaderFile = GlobalCompileEnvironment.SharedPCHHeaderFiles[ SharedPCHHeaderFileIndex ];

								if( DependencyModule == CurSharedPCHHeaderFile.Module || 
									( bIsASharedPCHModule && CurSharedPCHHeaderFile.Module == this ) )	// If we ourselves are a shared PCH module, always at least use our own module as our shared PCH header if we can't find anything better
								{
									SharedPCHModuleName = CurSharedPCHHeaderFile.Module.Name;
									SharedPCHHeaderFile = CurSharedPCHHeaderFile.PCHHeaderFile;
									LargestSharedPCHHeaderFileIndex = SharedPCHHeaderFileIndex;
									break;
								}
							}

							if( LargestSharedPCHHeaderFileIndex == GlobalCompileEnvironment.SharedPCHHeaderFiles.Count - 1 )
							{
								// We've determined that the module is using our most complex PCH header, so we can early-out
								break;
							}
						}

						// Did we not find a shared PCH header that is being included by this module?  This could happen if the module is not including Core.h, even indirectly.
						if( String.IsNullOrEmpty( SharedPCHModuleName ) )
						{
							throw new BuildException( "Module {0} doesn't use a Shared PCH!  Please add a dependency on a Shared PCH module to this module's dependency list", this.Name);
						}

						// Keep track of how many modules make use of this PCH for performance diagnostics
						var LargestSharedPCHHeader = GlobalCompileEnvironment.SharedPCHHeaderFiles[ LargestSharedPCHHeaderFileIndex ];
						++LargestSharedPCHHeader.NumModulesUsingThisPCH;

					}
					else
					{
						Log.TraceVerbose("Module '{0}' cannot create or use Shared PCHs, because it needs its own private PCH", this.Name);
					}
				}


				// The precompiled header environment for all source files in this module that use a precompiled header, if we even need one
				PrecompileHeaderEnvironment ModulePCHEnvironment = null;

				// If there was one header that was included first by enough C++ files, use it as the precompiled header.
				// Only use precompiled headers for projects with enough files to make the PCH creation worthwhile.
				if( SharedPCHHeaderFile != null || SourceFilesToBuild.CPPFiles.Count >= MinFilesUsingPrecompiledHeader )
				{
					FileItem PCHToUse;

					if( SharedPCHHeaderFile != null )
					{
						ModulePCHEnvironment = ApplySharedPCH(GlobalCompileEnvironment, CompileEnvironment, ModuleCompileEnvironment, SourceFilesToBuild.CPPFiles, ref SharedPCHHeaderFile);
						if (ModulePCHEnvironment != null)
						{
							// @todo SharedPCH: Ideally we would exhaustively check for a compatible compile environment (definitions, imports/exports, etc)
							//    Currently, it's possible for the shared PCH to be compiled differently depending on which module UBT happened to have
							//    include it first during the build phase.  This could create problems with deterministic builds, or turn up compile
							//    errors unexpectedly due to compile environment differences.
							Log.TraceVerbose("Module " + Name + " uses existing Shared PCH '" + ModulePCHEnvironment.PrecompiledHeaderIncludeFilename + "' (from module " + ModulePCHEnvironment.ModuleName + ")");
						}

						PCHToUse = SharedPCHHeaderFile;
					}
					else
					{
						PCHToUse = ProcessedDependencies.UniquePCHHeaderFile;
					}

					if (PCHToUse != null)
					{
						// Update all CPPFiles to point to the PCH
						foreach (var CPPFile in SourceFilesToBuild.CPPFiles)
						{
							CPPFile.PCHHeaderNameInCode              = PCHToUse.AbsolutePath;
							CPPFile.PrecompiledHeaderIncludeFilename = PCHToUse.AbsolutePath;
						}
					}

					// A shared PCH was not already set up for us, so set one up.
					if( ModulePCHEnvironment == null )
					{
						var PCHHeaderFile = ProcessedDependencies.UniquePCHHeaderFile;
						var PCHModuleName = this.Name;
						if( SharedPCHHeaderFile != null )
						{
							PCHHeaderFile = SharedPCHHeaderFile;
							PCHModuleName = SharedPCHModuleName;
						}
						var PCHHeaderNameInCode = SourceFilesToBuild.CPPFiles[ 0 ].PCHHeaderNameInCode;

						ModulePCHEnvironment = new PrecompileHeaderEnvironment( PCHModuleName, PCHHeaderNameInCode, PCHHeaderFile, ModuleCompileEnvironment.Config.CLRMode, ModuleCompileEnvironment.Config.OptimizeCode );

						if( SharedPCHHeaderFile != null )
						{
							// Add to list of shared PCH environments
							GlobalCompileEnvironment.SharedPCHEnvironments.Add( ModulePCHEnvironment );
							Log.TraceVerbose( "Module " + Name + " uses new Shared PCH '" + ModulePCHEnvironment.PrecompiledHeaderIncludeFilename + "'" );
						}
						else
						{
							Log.TraceVerbose( "Module " + Name + " uses a Unique PCH '" + ModulePCHEnvironment.PrecompiledHeaderIncludeFilename + "'" );
						}
					}
				}
				else
				{
					Log.TraceVerbose( "Module " + Name + " doesn't use a Shared PCH, and only has " + SourceFilesToBuild.CPPFiles.Count.ToString() + " source file(s).  No Unique PCH will be generated." );
				}

				// Compile the C++ source or the unity C++ files that use a PCH environment.
				if( ModulePCHEnvironment != null )
				{
					// Setup a new compile environment for this module's source files.  It's pretty much the exact same as the
					// module's compile environment, except that it will include a PCH file.

					var ModulePCHCompileEnvironment = ModuleCompileEnvironment.DeepCopy();
					ModulePCHCompileEnvironment.Config.PrecompiledHeaderAction          = PrecompiledHeaderAction.Include;
					ModulePCHCompileEnvironment.Config.PrecompiledHeaderIncludeFilename = ModulePCHEnvironment.PrecompiledHeaderIncludeFilename.AbsolutePath;
					ModulePCHCompileEnvironment.Config.PCHHeaderNameInCode              = ModulePCHEnvironment.PCHHeaderNameInCode;

					if( SharedPCHHeaderFile != null )
					{
						// Shared PCH headers need to be force included, because we're basically forcing the module to use
						// the precompiled header that we want, instead of the "first include" in each respective .cpp file
						ModulePCHCompileEnvironment.Config.bForceIncludePrecompiledHeader = true;
					}

					var CPPFilesToBuild = SourceFilesToBuild.CPPFiles;
					if (bModuleUsesUnityBuild)
					{
						// unity files generated for only the set of files which share the same PCH environment
						CPPFilesToBuild = Unity.GenerateUnityCPPs( Target, CPPFilesToBuild, ModulePCHCompileEnvironment, Name );
					}

					// Check if there are enough unity files to warrant pch generation (and we haven't already generated the shared one)
					if( ModulePCHEnvironment.PrecompiledHeaderFile == null )
					{
						if( SharedPCHHeaderFile != null || CPPFilesToBuild.Count >= MinFilesUsingPrecompiledHeader )
						{
							bool bAllowDLLExports = true;
							var PCHOutputDirectory = ModuleCompileEnvironment.Config.OutputDirectory;
							var PCHModuleName = this.Name;

							if( SharedPCHHeaderFile != null )
							{
								// Disallow DLLExports when generating shared PCHs.  These headers aren't able to export anything, because they're potentially shared between many modules.
								bAllowDLLExports = false;

								// Save shared PCHs to a specific folder
								PCHOutputDirectory = Path.Combine( CompileEnvironment.Config.OutputDirectory, "SharedPCHs" );

								// Use a fake module name for "shared" PCHs.  It may be used by many modules, so we don't want to use this module's name.
								PCHModuleName = "Shared";
							}

							var PCHOutput = PrecompileHeaderEnvironment.GeneratePCHCreationAction( 
								Target,
								CPPFilesToBuild[0].PCHHeaderNameInCode,
								ModulePCHEnvironment.PrecompiledHeaderIncludeFilename,
								ModuleCompileEnvironment, 
								PCHOutputDirectory,
								PCHModuleName, 
								bAllowDLLExports );
							ModulePCHEnvironment.PrecompiledHeaderFile = PCHOutput.PrecompiledHeaderFile;
							
							ModulePCHEnvironment.OutputObjectFiles.Clear();
							ModulePCHEnvironment.OutputObjectFiles.AddRange( PCHOutput.ObjectFiles );
						}
						else if( CPPFilesToBuild.Count < MinFilesUsingPrecompiledHeader )
						{
							Log.TraceVerbose( "Module " + Name + " doesn't use a Shared PCH, and only has " + CPPFilesToBuild.Count.ToString() + " unity source file(s).  No Unique PCH will be generated." );
						}
					}

					if( ModulePCHEnvironment.PrecompiledHeaderFile != null )
					{
						// Link in the object files produced by creating the precompiled header.
						LinkInputFiles.AddRange( ModulePCHEnvironment.OutputObjectFiles );

						// if pch action was generated for the environment then use pch
						ModulePCHCompileEnvironment.PrecompiledHeaderFile = ModulePCHEnvironment.PrecompiledHeaderFile;

						// Use this compile environment from now on
						CPPCompileEnvironment = ModulePCHCompileEnvironment;
					}

					LinkInputFiles.AddRange( CPPCompileEnvironment.CompileFiles( Target, CPPFilesToBuild, Name ).ObjectFiles );
					bWasModuleCodeCompiled = true;
				}

				if( BuildConfiguration.bPrintPerformanceInfo )
				{
					var PCHGenTime = ( DateTime.UtcNow - PCHGenTimerStart ).TotalSeconds;
					TotalPCHGenTime += PCHGenTime;
				}
			}

			if( !bWasModuleCodeCompiled && SourceFilesToBuild.CPPFiles.Count > 0 )
			{
				var CPPFilesToCompile = SourceFilesToBuild.CPPFiles;
				if (bModuleUsesUnityBuild)
				{
					CPPFilesToCompile = Unity.GenerateUnityCPPs( Target, CPPFilesToCompile, CPPCompileEnvironment, Name );
				}
				LinkInputFiles.AddRange( CPPCompileEnvironment.CompileFiles( Target, CPPFilesToCompile, Name ).ObjectFiles );
			}

			if (AutoGenerateCppInfo != null && AutoGenerateCppInfo.BuildInfo != null && !CPPCompileEnvironment.bHackHeaderGenerator)
			{
				string[] GeneratedFiles = Directory.GetFiles(Path.GetDirectoryName(AutoGenerateCppInfo.BuildInfo.FileWildcard), Path.GetFileName(AutoGenerateCppInfo.BuildInfo.FileWildcard));
				foreach (string GeneratedFilename in GeneratedFiles)
				{
					var GeneratedCppFileItem = FileItem.GetItemByPath(GeneratedFilename);

					CachePCHUsageForModuleSourceFile(this.Target, CPPCompileEnvironment, GeneratedCppFileItem);

					// @todo ubtmake: Check for ALL other places where we might be injecting .cpp or .rc files for compiling without caching CachedCPPIncludeInfo first (anything platform specific?)
					LinkInputFiles.AddRange(CPPCompileEnvironment.CompileFiles(Target, new List<FileItem> { GeneratedCppFileItem }, Name).ObjectFiles);
				}
			}

			// Compile C files directly.
			LinkInputFiles.AddRange(CPPCompileEnvironment.CompileFiles( Target, SourceFilesToBuild.CFiles, Name).ObjectFiles);

			// Compile CC files directly.
			LinkInputFiles.AddRange(CPPCompileEnvironment.CompileFiles( Target, SourceFilesToBuild.CCFiles, Name).ObjectFiles);

			// Compile MM files directly.
			LinkInputFiles.AddRange(CPPCompileEnvironment.CompileFiles( Target, SourceFilesToBuild.MMFiles, Name).ObjectFiles);

			// Compile RC files.
			LinkInputFiles.AddRange(CPPCompileEnvironment.CompileRCFiles(Target, SourceFilesToBuild.RCFiles).ObjectFiles);

			return LinkInputFiles;
		}

		private PrecompileHeaderEnvironment ApplySharedPCH(CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment, CPPEnvironment ModuleCompileEnvironment, List<FileItem> CPPFiles, ref FileItem SharedPCHHeaderFile)
		{
			// Check to see if we have a PCH header already setup that we can use
			var SharedPCHHeaderFileCopy = SharedPCHHeaderFile;
			var SharedPCHEnvironment = GlobalCompileEnvironment.SharedPCHEnvironments.Find(Env => Env.PrecompiledHeaderIncludeFilename == SharedPCHHeaderFileCopy);
			if (SharedPCHEnvironment == null)
			{ 
				return null;
			}

			// Don't mix CLR modes
			if (SharedPCHEnvironment.CLRMode != ModuleCompileEnvironment.Config.CLRMode)
			{
				Log.TraceVerbose("Module {0} cannot use existing Shared PCH '{1}' (from module '{2}') because CLR modes don't match", Name, SharedPCHEnvironment.PrecompiledHeaderIncludeFilename.AbsolutePath, SharedPCHEnvironment.ModuleName);
				SharedPCHHeaderFile = null;
				return null;
			}
			// Don't mix RTTI modes
			if (bUseRTTI)
			{
				Log.TraceVerbose("Module {0} cannot use existing Shared PCH '{1}' (from module '{2}') because RTTI modes don't match", Name, SharedPCHEnvironment.PrecompiledHeaderIncludeFilename.AbsolutePath, SharedPCHEnvironment.ModuleName);
				SharedPCHHeaderFile = null;
				return null;
			}

			// Don't mix non-optimized code with optimized code (PCHs won't be compatible)
			var SharedPCHCodeOptimization = SharedPCHEnvironment.OptimizeCode;
			var ModuleCodeOptimization    = ModuleCompileEnvironment.Config.OptimizeCode;

			if (CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Debug)
			{
				if (SharedPCHCodeOptimization == ModuleRules.CodeOptimization.InNonDebugBuilds)
				{
					SharedPCHCodeOptimization = ModuleRules.CodeOptimization.Always;
				}

				if (ModuleCodeOptimization == ModuleRules.CodeOptimization.InNonDebugBuilds)
				{
					ModuleCodeOptimization = ModuleRules.CodeOptimization.Always;
				}
			}

			if (SharedPCHCodeOptimization != ModuleCodeOptimization)
			{
				Log.TraceVerbose("Module {0} cannot use existing Shared PCH '{1}' (from module '{2}') because optimization levels don't match", Name, SharedPCHEnvironment.PrecompiledHeaderIncludeFilename.AbsolutePath, SharedPCHEnvironment.ModuleName);
				SharedPCHHeaderFile = null;
				return null;
			}

			return SharedPCHEnvironment;
		}

		public static FileItem CachePCHUsageForModuleSourceFile(UEBuildTarget Target, CPPEnvironment ModuleCompileEnvironment, FileItem CPPFile)
		{
			if( !CPPFile.bExists )
			{
				throw new BuildException( "Required source file not found: " + CPPFile.AbsolutePath );
			}

			var PCHCacheTimerStart = DateTime.UtcNow;

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform( ModuleCompileEnvironment.Config.Target.Platform );
			var IncludePathsToSearch = ModuleCompileEnvironment.Config.CPPIncludeInfo.GetIncludesPathsToSearch( CPPFile );

			// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
			// for header dependencies
			CPPFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;

			var PCHFile = CachePCHUsageForCPPFile( Target, CPPFile, BuildPlatform, IncludePathsToSearch, ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludeFileSearchDictionary );

			if( BuildConfiguration.bPrintPerformanceInfo )
			{
				var PCHCacheTime = ( DateTime.UtcNow - PCHCacheTimerStart ).TotalSeconds;
				TotalPCHCacheTime += PCHCacheTime;
			}
			
			return PCHFile;
		}


		public void CachePCHUsageForModuleSourceFiles(CPPEnvironment ModuleCompileEnvironment)
		{
			if( ProcessedDependencies == null )
			{
				var PCHCacheTimerStart = DateTime.UtcNow;

				var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform( ModuleCompileEnvironment.Config.Target.Platform );

				bool bFoundAProblemWithPCHs = false;

				FileItem UniquePCH = null;
				foreach( var CPPFile in SourceFilesFound.CPPFiles )	// @todo ubtmake: We're not caching CPPEnvironments for .c/.mm files, etc.  Even though they don't use PCHs, they still have #includes!  This can break dependency checking!
				{
					// Build a single list of include paths to search.
					var IncludePathsToSearch = ModuleCompileEnvironment.Config.CPPIncludeInfo.GetIncludesPathsToSearch( CPPFile );

					// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
					// for header dependencies
					CPPFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;

					// Find headers used by the source file.
					var PCH = UEBuildModuleCPP.CachePCHUsageForCPPFile(Target, CPPFile, BuildPlatform, IncludePathsToSearch, ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludeFileSearchDictionary);
					if( PCH == null )
					{
						throw new BuildException( "Source file \"{0}\" is not including any headers.  We expect all modules to include a header file for precompiled header generation.  Please add an #include statement.", CPPFile.AbsolutePath );
					}

					if( UniquePCH == null )
					{
						UniquePCH = PCH;
					}
					else if( !UniquePCH.Info.Name.Equals( PCH.Info.Name, StringComparison.InvariantCultureIgnoreCase ) )		// @todo ubtmake: We do a string compare on the file name (not path) here, because sometimes the include resolver will pick an Intermediate copy of a PCH header file and throw off our comparisons
					{
						// OK, looks like we have multiple source files including a different header file first.  We'll keep track of this and print out
						// helpful information afterwards.
						bFoundAProblemWithPCHs = true;
					}
				}

				ProcessedDependencies = new ProcessedDependenciesClass{ UniquePCHHeaderFile = UniquePCH };


				if( bFoundAProblemWithPCHs )
				{
					// Map from pch header string to the source files that use that PCH
					var UsageMapPCH = new Dictionary<string, List<FileItem>>( StringComparer.InvariantCultureIgnoreCase );
					foreach( var CPPFile in SourceFilesToBuild.CPPFiles )
					{
						// Create a new entry if not in the pch usage map
						UsageMapPCH.GetOrAddNew( CPPFile.PrecompiledHeaderIncludeFilename ).Add( CPPFile );
					}

					if( BuildConfiguration.bPrintDebugInfo )
					{
						Log.TraceVerbose( "{0} PCH files for module {1}:", UsageMapPCH.Count, Name );
						int MostFilesIncluded = 0;
						foreach( var CurPCH in UsageMapPCH )
						{
							if( CurPCH.Value.Count > MostFilesIncluded )
							{
								MostFilesIncluded = CurPCH.Value.Count;
							}

							Log.TraceVerbose("   {0}  ({1} files including it: {2}, ...)", CurPCH.Key, CurPCH.Value.Count, CurPCH.Value[0].AbsolutePath);
						}
					}

					if( UsageMapPCH.Count > 1 )
					{
						// Keep track of the PCH file that is most used within this module
						string MostFilesAreIncludingPCH = string.Empty;
						int MostFilesIncluded = 0;
						foreach( var CurPCH in UsageMapPCH.Where( PCH => PCH.Value.Count > MostFilesIncluded ) )
						{
							MostFilesAreIncludingPCH = CurPCH.Key;
							MostFilesIncluded        = CurPCH.Value.Count;
						}

						// Find all of the files that are not including our "best" PCH header
						var FilesNotIncludingBestPCH = new StringBuilder();
						foreach( var CurPCH in UsageMapPCH.Where( PCH => PCH.Key != MostFilesAreIncludingPCH ) )
						{
							foreach( var SourceFile in CurPCH.Value )
							{
								FilesNotIncludingBestPCH.AppendFormat( "{0} (including {1})\n", SourceFile.AbsolutePath, CurPCH.Key );
							}
						}

						// Bail out and let the user know which source files may need to be fixed up
						throw new BuildException(
							"All source files in module \"{0}\" must include the same precompiled header first.  Currently \"{1}\" is included by most of the source files.  The following source files are not including \"{1}\" as their first include:\n\n{2}",
							Name,
							MostFilesAreIncludingPCH,
							FilesNotIncludingBestPCH );
					}
				}
				
				if( BuildConfiguration.bPrintPerformanceInfo )
				{
					var PCHCacheTime = ( DateTime.UtcNow - PCHCacheTimerStart ).TotalSeconds;
					TotalPCHCacheTime += PCHCacheTime;
				}
			}
		}



		private static FileItem CachePCHUsageForCPPFile(UEBuildTarget Target, FileItem CPPFile, IUEBuildPlatform BuildPlatform, List<string> IncludePathsToSearch, Dictionary<string, FileItem> IncludeFileSearchDictionary)
		{
			// @todo ubtmake: We don't really need to scan every file looking for PCH headers, just need one.  The rest is just for error checking.
			// @todo ubtmake: We don't need all of the direct includes either.  We just need the first, unless we want to check for errors.
			bool HasUObjects;
			List<DependencyInclude> DirectIncludeFilenames = CPPEnvironment.GetDirectIncludeDependencies(Target, CPPFile, BuildPlatform, bOnlyCachedDependencies:false, HasUObjects:out HasUObjects);
			if (BuildConfiguration.bPrintDebugInfo)
			{
				Log.TraceVerbose("Found direct includes for {0}: {1}", Path.GetFileName(CPPFile.AbsolutePath), string.Join(", ", DirectIncludeFilenames.Select(F => F.IncludeName)));
			}

			if (DirectIncludeFilenames.Count == 0)
			{ 
				return null;
			}

			var FirstInclude = DirectIncludeFilenames[0];

			// The pch header should always be the first include in the source file.
			// NOTE: This is not an absolute path.  This is just the literal include string from the source file!
			CPPFile.PCHHeaderNameInCode = FirstInclude.IncludeName;

			// Resolve the PCH header to an absolute path.
			// Check NullOrEmpty here because if the file could not be resolved we need to throw an exception
			if (!string.IsNullOrEmpty(FirstInclude.IncludeResolvedName) &&
				// ignore any preexisting resolve cache if we are not configured to use it.
				BuildConfiguration.bUseIncludeDependencyResolveCache &&
				// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
				!BuildConfiguration.bTestIncludeDependencyResolveCache)
			{
				CPPFile.PrecompiledHeaderIncludeFilename = FirstInclude.IncludeResolvedName;
				return FileItem.GetItemByFullPath(CPPFile.PrecompiledHeaderIncludeFilename);
			}

			// search the include paths to resolve the file.
			FileItem PrecompiledHeaderIncludeFile = CPPEnvironment.FindIncludedFile(CPPFile.PCHHeaderNameInCode, !BuildConfiguration.bCheckExternalHeadersForModification, IncludePathsToSearch, IncludeFileSearchDictionary );
			if (PrecompiledHeaderIncludeFile == null)
			{ 
				throw new BuildException("The first include statement in source file '{0}' is trying to include the file '{1}' as the precompiled header, but that file could not be located in any of the module's include search paths.", CPPFile.AbsolutePath, CPPFile.PCHHeaderNameInCode);
			}

			CPPEnvironment.IncludeDependencyCache[Target].CacheResolvedIncludeFullPath(CPPFile, 0, PrecompiledHeaderIncludeFile.AbsolutePath);
			CPPFile.PrecompiledHeaderIncludeFilename = PrecompiledHeaderIncludeFile.AbsolutePath;

			return PrecompiledHeaderIncludeFile;
		}

		/// <summary>
		/// Creates a compile environment from a base environment based on the module settings.
		/// </summary>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new module compile environment.</returns>
		public CPPEnvironment CreateModuleCompileEnvironment(CPPEnvironment BaseCompileEnvironment)
		{
			var Result = BaseCompileEnvironment.DeepCopy();

			// Override compile environment
			Result.Config.bFasterWithoutUnity                    = bFasterWithoutUnity;
			Result.Config.OptimizeCode                           = OptimizeCode;
			Result.Config.bUseRTTI                               = bUseRTTI;
			Result.Config.bEnableBufferSecurityChecks            = bEnableBufferSecurityChecks;
			Result.Config.bFasterWithoutUnity                    = bFasterWithoutUnity;
			Result.Config.MinFilesUsingPrecompiledHeaderOverride = MinFilesUsingPrecompiledHeaderOverride;
			Result.Config.bEnableExceptions                      = bEnableExceptions;
			Result.Config.bEnableShadowVariableWarning          = bEnableShadowVariableWarnings;
			Result.Config.bUseStaticCRT							 = (Target.Rules != null && Target.Rules.bUseStaticCRT);
			Result.Config.OutputDirectory                        = Path.Combine(Binary.Config.IntermediateDirectory, Name);

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (Target.Configuration == UnrealTargetConfiguration.DebugGame)
			{
				if(!Utils.IsFileUnderDirectory(ModuleDirectory, BuildConfiguration.RelativeEnginePath))
				{
					Result.Config.Target.Configuration = CPPTargetConfiguration.Debug;
					Result.Config.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=1");
				}
			}

			// Add the module's private definitions.
			Result.Config.Definitions.AddRange(Definitions);

			// Setup the compile environment for the module.
			SetupPrivateCompileEnvironment(Result.Config.CPPIncludeInfo.IncludePaths, Result.Config.CPPIncludeInfo.SystemIncludePaths, Result.Config.Definitions, Result.Config.AdditionalFrameworks);

			// @hack to skip adding definitions to compile environment, they will be baked into source code files
			if (bSkipDefinitionsForCompileEnvironment)
			{
				Result.Config.Definitions.Clear();
				Result.Config.CPPIncludeInfo.IncludePaths = new HashSet<string>(BaseCompileEnvironment.Config.CPPIncludeInfo.IncludePaths);
			}

			return Result;
		}

		public HashSet<FileItem> _AllClassesHeaders     = new HashSet<FileItem>();
		public HashSet<FileItem> _PublicUObjectHeaders  = new HashSet<FileItem>();
		public HashSet<FileItem> _PrivateUObjectHeaders = new HashSet<FileItem>();

		private UHTModuleInfo CachedModuleUHTInfo = null;

		/// Total time spent generating PCHs for modules (not actually compiling, but generating the PCH's input data)
		public static double TotalPCHGenTime = 0.0;

		/// Time spent caching which PCH header is included by each module and source file
		public static double TotalPCHCacheTime = 0.0;



		/// <summary>
		/// If any of this module's source files contain UObject definitions, this will return those header files back to the caller
		/// </summary>
		/// <param name="PublicUObjectClassesHeaders">All UObjects headers in the module's Classes folder (legacy)</param>
		/// <param name="PublicUObjectHeaders">Dependent UObject headers in Public source folders</param>
		/// <param name="PrivateUObjectHeaders">Dependent UObject headers not in Public source folders</param>
		/// <returns>
		public UHTModuleInfo GetUHTModuleInfo()
		{
			if (CachedModuleUHTInfo != null)
			{
				return CachedModuleUHTInfo;
			}

			var ClassesFolder = Path.Combine(this.ModuleDirectory, "Classes");
			var PublicFolder  = Path.Combine(this.ModuleDirectory, "Public");

			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Target.Platform);

			var ClassesFiles = Directory.GetFiles( this.ModuleDirectory, "*.h", SearchOption.AllDirectories );
			foreach (var ClassHeader in ClassesFiles)
			{
				// Check to see if we know anything about this file.  If we have up-to-date cached information about whether it has
				// UObjects or not, we can skip doing a test here.
				var UObjectHeaderFileItem = FileItem.GetExistingItemByPath( ClassHeader );

				bool HasUObjects;
				var DirectIncludes = CPPEnvironment.GetDirectIncludeDependencies( Target, UObjectHeaderFileItem, BuildPlatform, bOnlyCachedDependencies:false, HasUObjects:out HasUObjects );
				Debug.Assert( DirectIncludes != null );
				
				if (HasUObjects)
				{ 
					if (UObjectHeaderFileItem.AbsolutePath.StartsWith(ClassesFolder))
					{
						_AllClassesHeaders.Add(UObjectHeaderFileItem);
					}
					else if (UObjectHeaderFileItem.AbsolutePath.StartsWith(PublicFolder))
					{
						_PublicUObjectHeaders.Add(UObjectHeaderFileItem);
					}
					else
					{
						_PrivateUObjectHeaders.Add(UObjectHeaderFileItem);
					}
				}
			}

			CachedModuleUHTInfo = new UHTModuleInfo 
			{
				ModuleName                  = this.Name,
				ModuleDirectory             = this.ModuleDirectory,
				ModuleType					= this.Type.ToString(),
				PublicUObjectClassesHeaders = _AllClassesHeaders    .ToList(),
				PublicUObjectHeaders        = _PublicUObjectHeaders .ToList(),
				PrivateUObjectHeaders       = _PrivateUObjectHeaders.ToList(),
				GeneratedCodeVersion = this.Target.Rules.GetGeneratedCodeVersion()
			};

			return CachedModuleUHTInfo;
		}
	
		public override void GetAllDependencyModules( Dictionary<string, UEBuildModule> ReferencedModules, List<UEBuildModule> OrderedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies )
		{
			var AllModuleNames = new List<string>();
			AllModuleNames.AddRange(PrivateDependencyModuleNames);
			AllModuleNames.AddRange(PublicDependencyModuleNames);
			if( bIncludeDynamicallyLoaded )
			{
				AllModuleNames.AddRange(DynamicallyLoadedModuleNames);
                AllModuleNames.AddRange(PlatformSpecificDynamicallyLoadedModuleNames);
            }

			foreach (var DependencyName in AllModuleNames)
			{
				if (!ReferencedModules.ContainsKey(DependencyName))
				{
					// Don't follow circular back-references!
					bool bIsCircular = CircularlyReferencedDependentModules.Contains( DependencyName );
					if (bForceCircular || !bIsCircular)
					{
						var Module = Target.GetModuleByName( DependencyName );
						ReferencedModules[ DependencyName ] = Module;

						if( !bOnlyDirectDependencies )
						{ 
							// Recurse into dependent modules first
							Module.GetAllDependencyModules(ReferencedModules, OrderedModules, bIncludeDynamicallyLoaded, bForceCircular, bOnlyDirectDependencies);
						}

						OrderedModules.Add( Module );
					}
				}
			}
		}

		public override void RecursivelyAddPrecompiledModules(List<UEBuildModule> Modules)
		{
			if(!Modules.Contains(this))
			{
				Modules.Add(this);

				// Get the dependent modules
				List<string> DependentModuleNames = new List<string>();
				DependentModuleNames.AddRange(PrivateDependencyModuleNames);
				DependentModuleNames.AddRange(PublicDependencyModuleNames);
				DependentModuleNames.AddRange(DynamicallyLoadedModuleNames);
				DependentModuleNames.AddRange(PlatformSpecificDynamicallyLoadedModuleNames);

				// Find modules for each of them, and add their dependencies too
				foreach(string DependentModuleName in DependentModuleNames)
				{
					UEBuildModule DependentModule = Target.FindOrCreateModuleByName(DependentModuleName);
					DependentModule.RecursivelyAddPrecompiledModules(Modules);
				}
			}
		}

		public override void RecursivelyAddIncludePathModules( UEBuildTarget Target, bool bPublicIncludesOnly )
		{
			var AllIncludePathModuleNames = new List<string>();
			AllIncludePathModuleNames.AddRange( PublicIncludePathModuleNames );
			if( !bPublicIncludesOnly )
			{
				AllIncludePathModuleNames.AddRange( PrivateIncludePathModuleNames );
			}
			foreach( var IncludePathModuleName in AllIncludePathModuleNames )
			{
				var IncludePathModule = Target.FindOrCreateModuleByName( IncludePathModuleName );

				// No need to do anything here.  We just want to make sure that we've instantiated our representation of the
				// module so that we can find it later when processing include path module names.  Unlike actual dependency
				// modules, these include path modules may never be "bound" (have Binary or bIncludedInTarget member variables set)

				// We'll also need to make sure we know about all dependent public include path modules, too!
				IncludePathModule.RecursivelyAddIncludePathModules( Target, bPublicIncludesOnly:true );
			}
		}

		public override void RecursivelyProcessUnboundModules()
		{
			try
			{
				// Make sure this module is bound to a binary
				if( !bIncludedInTarget )
				{
					throw new BuildException( "Module '{0}' should already have been bound to a binary!", Name );
				}

				var AllModuleNames = new List<string>();
				AllModuleNames.AddRange( PrivateDependencyModuleNames );
				AllModuleNames.AddRange( PublicDependencyModuleNames );
				AllModuleNames.AddRange( DynamicallyLoadedModuleNames );
				AllModuleNames.AddRange( PlatformSpecificDynamicallyLoadedModuleNames );

				foreach( var DependencyName in AllModuleNames )
				{
					var DependencyModule = Target.FindOrCreateModuleByName(DependencyName);

					// Skip modules that are included with the target (externals)
					if( !DependencyModule.bIncludedInTarget )
					{
						bool bIsCrossTarget = PlatformSpecificDynamicallyLoadedModuleNames.Contains(DependencyName) && !DynamicallyLoadedModuleNames.Contains(DependencyName);

						// Get the binary that this module should be bound to
						UEBuildBinary BinaryToBindTo = Target.FindOrAddBinaryForModule(DependencyName, bIsCrossTarget);

						// Bind this module
						DependencyModule.Binary = BinaryToBindTo;
						DependencyModule.bIncludedInTarget = true;

						// Also add binaries for this module's dependencies
						DependencyModule.RecursivelyProcessUnboundModules();
					}

					if (Target.ShouldCompileMonolithic() == false)
					{
						// Check to see if there is a circular relationship between the module and it's referencer
						if( DependencyModule.Binary != null )
						{
							if( CircularlyReferencedDependentModules.Contains( DependencyName ) )
							{
								DependencyModule.Binary.SetCreateImportLibrarySeparately( true );
							}
						}
					}
				}

				// Also make sure module entries are created for any module that is pulled in as an "include path" module.
				// These modules are never linked in unless they were referenced as an actual dependency of a different module,
				// but we still need to keep track of them so that we can find their include paths when setting up our
				// module's include paths.
				RecursivelyAddIncludePathModules( Target, bPublicIncludesOnly:false );
			}
			catch (System.Exception ex)
			{
				throw new ModuleProcessingException(this, ex);
			}
		}
	}

	/** A module that is compiled from C++ CLR code. */
	class UEBuildModuleCPPCLR : UEBuildModuleCPP
	{
		/** The assemblies referenced by the module's private implementation. */
		HashSet<string> PrivateAssemblyReferences;

		public UEBuildModuleCPPCLR(
			UEBuildTarget InTarget,
			string InName,
			UEBuildModuleType InType,
			string InModuleDirectory,
			string InGeneratedCodeDirectory,
			bool? InIsRedistributableOverride,
			IntelliSenseGatherer InIntelliSenseGatherer,
			IEnumerable<FileItem> InSourceFiles,
			IEnumerable<string> InPublicIncludePaths,
			IEnumerable<string> InPublicSystemIncludePaths,
			IEnumerable<string> InDefinitions,
			IEnumerable<string> InPrivateAssemblyReferences,
			IEnumerable<string> InPublicIncludePathModuleNames,
			IEnumerable<string> InPublicDependencyModuleNames,
			IEnumerable<string> InPublicDelayLoadDLLs,
			IEnumerable<string> InPublicAdditionalLibraries,
			IEnumerable<string> InPublicFrameworks,
			IEnumerable<string> InPublicWeakFrameworks,
			IEnumerable<UEBuildFramework> InPublicAdditionalFrameworks,
			IEnumerable<string> InPublicAdditionalShadowFiles,
			IEnumerable<UEBuildBundleResource> InPublicAdditionalBundleResources,
			IEnumerable<string> InPrivateIncludePaths,
			IEnumerable<string> InPrivateIncludePathModuleNames,
			IEnumerable<string> InPrivateDependencyModuleNames,
			IEnumerable<string> InCircularlyReferencedDependentModules,
			IEnumerable<string> InDynamicallyLoadedModuleNames,
            IEnumerable<string> InPlatformSpecificDynamicallyLoadedModuleNames,
			IEnumerable<RuntimeDependency> InRuntimeDependencies,
            ModuleRules.CodeOptimization InOptimizeCode,
			bool InAllowSharedPCH,
			string InSharedPCHHeaderFile,
			bool InUseRTTI,
			bool InEnableBufferSecurityChecks,
			bool InFasterWithoutUnity,
			int InMinFilesUsingPrecompiledHeaderOverride,
			bool InEnableExceptions,
			bool InEnableShadowVariableWarnings,
			bool bInBuildSourceFiles,
			string InBuildCsFilename
			)
			: base(InTarget,InName,InType,InModuleDirectory,InGeneratedCodeDirectory,InIsRedistributableOverride,InIntelliSenseGatherer,
			InSourceFiles,InPublicIncludePaths,InPublicSystemIncludePaths,null,InDefinitions,
			InPublicIncludePathModuleNames,InPublicDependencyModuleNames,InPublicDelayLoadDLLs,InPublicAdditionalLibraries,InPublicFrameworks,InPublicWeakFrameworks,InPublicAdditionalFrameworks,InPublicAdditionalShadowFiles,InPublicAdditionalBundleResources,
			InPrivateIncludePaths,InPrivateIncludePathModuleNames,InPrivateDependencyModuleNames,
            InCircularlyReferencedDependentModules, InDynamicallyLoadedModuleNames, InPlatformSpecificDynamicallyLoadedModuleNames, InRuntimeDependencies, InOptimizeCode,
			InAllowSharedPCH, InSharedPCHHeaderFile, InUseRTTI, InEnableBufferSecurityChecks, InFasterWithoutUnity, InMinFilesUsingPrecompiledHeaderOverride,
			InEnableExceptions, InEnableShadowVariableWarnings, bInBuildSourceFiles, InBuildCsFilename)
		{
			PrivateAssemblyReferences = HashSetFromOptionalEnumerableStringParameter(InPrivateAssemblyReferences);
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile( CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment )
		{
			var ModuleCLREnvironment = CompileEnvironment.DeepCopy();

			// Setup the module environment for the project CLR mode
			ModuleCLREnvironment.Config.CLRMode = CPPCLRMode.CLREnabled;

			// Add the private assembly references to the compile environment.
			foreach(var PrivateAssemblyReference in PrivateAssemblyReferences)
			{
				ModuleCLREnvironment.AddPrivateAssembly(PrivateAssemblyReference);
			}

			// Pass the CLR compilation environment to the standard C++ module compilation code.
			return base.Compile(GlobalCompileEnvironment, ModuleCLREnvironment );
		}

		public override void SetupPrivateLinkEnvironment(
			UEBuildBinary SourceBinary,
			LinkEnvironment LinkEnvironment,
			List<UEBuildBinary> BinaryDependencies,
			Dictionary<UEBuildModule, bool> VisitedModules
			)
		{
			base.SetupPrivateLinkEnvironment(SourceBinary, LinkEnvironment, BinaryDependencies, VisitedModules);

			// Setup the link environment for linking a CLR binary.
			LinkEnvironment.Config.CLRMode = CPPCLRMode.CLREnabled;
		}
	}

	public class UEBuildFramework
	{
		public UEBuildFramework( string InFrameworkName )
		{
			FrameworkName = InFrameworkName;
		}

        public UEBuildFramework(string InFrameworkName, string InFrameworkZipPath)
        {
            FrameworkName = InFrameworkName;
            FrameworkZipPath = InFrameworkZipPath;
        }

		public UEBuildFramework( string InFrameworkName, string InFrameworkZipPath, string InCopyBundledAssets )
		{
			FrameworkName		= InFrameworkName;
			FrameworkZipPath	= InFrameworkZipPath;
			CopyBundledAssets	= InCopyBundledAssets;
		}

		public UEBuildModule	OwningModule		= null;
		public string			FrameworkName		= null;
		public string			FrameworkZipPath	= null;
		public string			CopyBundledAssets	= null;
	}

	public class UEBuildBundleResource
	{
		public UEBuildBundleResource(string InResourcePath, string InBundleContentsSubdir = "Resources", bool bInShouldLog = true)
		{
			ResourcePath = InResourcePath;
			BundleContentsSubdir = InBundleContentsSubdir;
			bShouldLog = bInShouldLog;
		}

		public string ResourcePath			= null;
		public string BundleContentsSubdir	= null;
		public bool bShouldLog				= true;
	}

	public class PrecompileHeaderEnvironment
	{
		/** The name of the module this PCH header is a member of */
		public readonly string ModuleName;

		/** PCH header file name as it appears in an #include statement in source code (might include partial, or no relative path.)
			This is needed by some compilers to use PCH features. */
		public string PCHHeaderNameInCode;

		/** The source header file that this precompiled header will be generated for */
		public readonly FileItem PrecompiledHeaderIncludeFilename;

		/** Whether this precompiled header will be built with CLR features enabled.  We won't mix and match CLR PCHs with non-CLR PCHs */
		public readonly CPPCLRMode CLRMode;

		/** Whether this precompiled header will be built with code optimization enabled. */
		public readonly ModuleRules.CodeOptimization OptimizeCode;

		/** The PCH file we're generating */
		public FileItem PrecompiledHeaderFile = null;

		/** Object files emitted from the compiler when generating this precompiled header.  These will be linked into modules that
			include this PCH */
		public readonly List<FileItem> OutputObjectFiles = new List<FileItem>();

		public PrecompileHeaderEnvironment( string InitModuleName, string InitPCHHeaderNameInCode, FileItem InitPrecompiledHeaderIncludeFilename, CPPCLRMode InitCLRMode, ModuleRules.CodeOptimization InitOptimizeCode )
		{
			ModuleName = InitModuleName;
			PCHHeaderNameInCode = InitPCHHeaderNameInCode;
			PrecompiledHeaderIncludeFilename = InitPrecompiledHeaderIncludeFilename;
			CLRMode = InitCLRMode;
			OptimizeCode = InitOptimizeCode;
		}

		/// <summary>
		/// Creates a precompiled header action to generate a new pch file 
		/// </summary>
		/// <param name="PCHHeaderNameInCode">The precompiled header name as it appeared in an #include statement</param>
		/// <param name="PrecompiledHeaderIncludeFilename">Name of the header used for pch.</param>
		/// <param name="ProjectCPPEnvironment">The environment the C/C++ files in the project are compiled with.</param>
		/// <param name="OutputDirectory">The folder to save the generated PCH file to</param>
		/// <param name="ModuleName">Name of the module this PCH is being generated for</param>
		/// <param name="bAllowDLLExports">True if we should allow DLLEXPORT definitions for this PCH</param>
		/// <returns>the compilation output result of the created pch.</returns>
		public static CPPOutput GeneratePCHCreationAction(UEBuildTarget Target, string PCHHeaderNameInCode, FileItem PrecompiledHeaderIncludeFilename, CPPEnvironment ProjectCPPEnvironment, string OutputDirectory, string ModuleName, bool bAllowDLLExports )
		{
			// Find the header file to be precompiled. Don't skip external headers
			if (PrecompiledHeaderIncludeFilename.bExists)
			{
				// Create a Dummy wrapper around the PCH to avoid problems with #pragma once on clang
				var ToolChain = UEToolChain.GetPlatformToolChain(ProjectCPPEnvironment.Config.Target.Platform);
				string PCHGuardDefine = Path.GetFileNameWithoutExtension(PrecompiledHeaderIncludeFilename.AbsolutePath).ToUpper();
				string LocalPCHHeaderNameInCode = ToolChain.ConvertPath(PrecompiledHeaderIncludeFilename.AbsolutePath);
				string TmpPCHHeaderContents = String.Format("#ifndef __AUTO_{0}_H__\n#define __AUTO_{0}_H__\n//Last Write: {2}\n#include \"{1}\"\n#endif//__AUTO_{0}_H__", PCHGuardDefine, LocalPCHHeaderNameInCode, PrecompiledHeaderIncludeFilename.LastWriteTime);
				string DummyPath = Path.Combine(
					ProjectCPPEnvironment.Config.OutputDirectory,
					Path.GetFileName(PrecompiledHeaderIncludeFilename.AbsolutePath));
				FileItem DummyPCH = FileItem.CreateIntermediateTextFile(DummyPath, TmpPCHHeaderContents);

				// Create a new C++ environment that is used to create the PCH.
				var ProjectPCHEnvironment = ProjectCPPEnvironment.DeepCopy();
				ProjectPCHEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				ProjectPCHEnvironment.Config.PrecompiledHeaderIncludeFilename = PrecompiledHeaderIncludeFilename.AbsolutePath;
				ProjectPCHEnvironment.Config.PCHHeaderNameInCode = PCHHeaderNameInCode;
				ProjectPCHEnvironment.Config.OutputDirectory = OutputDirectory;

				if( !bAllowDLLExports )
				{
					for( var CurDefinitionIndex = 0; CurDefinitionIndex < ProjectPCHEnvironment.Config.Definitions.Count; ++CurDefinitionIndex )
					{
						// We change DLLEXPORT to DLLIMPORT for "shared" PCH headers
						var OldDefinition = ProjectPCHEnvironment.Config.Definitions[ CurDefinitionIndex ];
						if( OldDefinition.EndsWith( "=DLLEXPORT" ) )
						{
							ProjectPCHEnvironment.Config.Definitions[ CurDefinitionIndex ] = OldDefinition.Replace( "DLLEXPORT", "DLLIMPORT" );
						}
					}
				}

				// Cache our CPP environment so that we can check for outdatedness quickly.  Only files that have includes need this.
				DummyPCH.CachedCPPIncludeInfo = ProjectPCHEnvironment.Config.CPPIncludeInfo;

				Log.TraceVerbose( "Found PCH file \"{0}\".", PrecompiledHeaderIncludeFilename );

				// Create the action to compile the PCH file.
				return ProjectPCHEnvironment.CompileFiles(Target, new List<FileItem>() { DummyPCH }, ModuleName);
			}
			throw new BuildException( "Couldn't find PCH file \"{0}\".", PrecompiledHeaderIncludeFilename );
		}
	}
}
