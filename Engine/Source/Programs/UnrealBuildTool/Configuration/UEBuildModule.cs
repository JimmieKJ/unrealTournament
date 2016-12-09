// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Xml;
using Tools.DotNETCommon.CaselessDictionary;

namespace UnrealBuildTool
{
	/// <summary>
	/// Distribution level of module.
	/// Note: The name of each entry is used to search for/create folders
	/// </summary>
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


	/// <summary>
	/// A unit of code compilation and linking.
	/// </summary>
	public abstract class UEBuildModule
	{
		/// <summary>
		/// Checks what distribution level a particular path should have by checking for key folders anywhere in it
		/// </summary>
		public static UEBuildModuleDistribution GetModuleDistributionLevelBasedOnLocation(string FilePath)
		{
			// Get full path to ensure all folder separators are the same
			FilePath = Path.GetFullPath(FilePath);

			// Check from highest to lowest (don't actually need to check 'Everyone')
			for (UEBuildModuleDistribution DistributionLevel = UEBuildModuleDistribution.NoRedist; DistributionLevel > UEBuildModuleDistribution.Public; DistributionLevel--)
			{
				string DistributionFolderName = String.Format("{0}{1}{0}", Path.DirectorySeparatorChar, DistributionLevel.ToString());
				if (FilePath.IndexOf(DistributionFolderName, StringComparison.InvariantCultureIgnoreCase) >= 0)
				{
					return DistributionLevel;
				}

				if (DistributionLevel == UEBuildModuleDistribution.NotForLicensees)
				{
					// Extra checks for PS4 and XboxOne folders, which are equivalent to NotForLicensees
					string PS4FolderName = String.Format("{0}ps4{0}", Path.DirectorySeparatorChar);
					string XboxFolderName = String.Format("{0}xboxone{0}", Path.DirectorySeparatorChar);
					if (FilePath.IndexOf(PS4FolderName, StringComparison.InvariantCultureIgnoreCase) >= 0
					|| FilePath.IndexOf(XboxFolderName, StringComparison.InvariantCultureIgnoreCase) >= 0)
					{
						return UEBuildModuleDistribution.NotForLicensees;
					}
				}
			}

			return UEBuildModuleDistribution.Public;
		}

		/// <summary>
		/// Determines the distribution level of a module based on its directory and includes.
		/// </summary>
		public UEBuildModuleDistribution DetermineDistributionLevel()
		{
			List<string> PathsToCheck = new List<string>();
			PathsToCheck.Add(ModuleDirectory.FullName);
			PathsToCheck.AddRange(PublicIncludePaths);
			PathsToCheck.AddRange(PrivateIncludePaths);
			// Not sure if these two are necessary as paths will usually be in basic includes too
			PathsToCheck.AddRange(PublicSystemIncludePaths);
			PathsToCheck.AddRange(PublicLibraryPaths);

			UEBuildModuleDistribution DistributionLevel = UEBuildModuleDistribution.Public;

			if (!Rules.bOutputPubliclyDistributable)
			{
				// Keep checking as long as we haven't reached the maximum level
				for (int PathIndex = 0; PathIndex < PathsToCheck.Count && DistributionLevel != UEBuildModuleDistribution.NoRedist; ++PathIndex)
				{
					DistributionLevel = Utils.Max(DistributionLevel, UEBuildModule.GetModuleDistributionLevelBasedOnLocation(PathsToCheck[PathIndex]));
				}
			}

			return DistributionLevel;
		}

		/// <summary>
		/// Converts an optional string list parameter to a well-defined hash set.
		/// </summary>
		protected static HashSet<string> HashSetFromOptionalEnumerableStringParameter(IEnumerable<string> InEnumerableStrings)
		{
			return InEnumerableStrings == null ? new HashSet<string>() : new HashSet<string>(InEnumerableStrings);
		}

		/// <summary>
		/// The name that uniquely identifies the module.
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// The type of module being built. Used to switch between debug/development and precompiled/source configurations.
		/// </summary>
		public UHTModuleType Type;

		/// <summary>
		/// The rules for this module
		/// </summary>
		public ModuleRules Rules;

		/// <summary>
		/// Path to the module directory
		/// </summary>
		public readonly DirectoryReference ModuleDirectory;

		/// <summary>
		/// Is this module allowed to be redistributed.
		/// </summary>
		private readonly bool? IsRedistributableOverride;

		/// <summary>
		/// The name of the .Build.cs file this module was created from, if any
		/// </summary>
		public FileReference RulesFile;

		/// <summary>
		/// The binary the module will be linked into for the current target.  Only set after UEBuildBinary.BindModules is called.
		/// </summary>
		public UEBuildBinary Binary = null;

		/// <summary>
		/// Include path for this module's base directory, relative to the Engine/Source directory
		/// </summary>
		protected string NormalizedModuleIncludePath;

		/// <summary>
		/// The name of the _API define for this module
		/// </summary>
		protected readonly string ModuleApiDefine;

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

		/// <summary>
		/// Names of modules with header files that this module's public interface needs access to.
		/// </summary>
		protected List<UEBuildModule> PublicIncludePathModules;

		/// <summary>
		/// Names of modules that this module's public interface depends on.
		/// </summary>
		protected List<UEBuildModule> PublicDependencyModules;

		/// <summary>
		/// Names of DLLs that this module should delay load
		/// </summary>
		protected HashSet<string> PublicDelayLoadDLLs;

		/// <summary>
		/// Names of modules with header files that this module's private implementation needs access to.
		/// </summary>
		protected List<UEBuildModule> PrivateIncludePathModules;

		/// <summary>
		/// Names of modules that this module's private implementation depends on.
		/// </summary>
		protected List<UEBuildModule> PrivateDependencyModules;

		/// <summary>
		/// Extra modules this module may require at run time
		/// </summary>
		protected List<UEBuildModule> DynamicallyLoadedModules;

		/// <summary>
		/// Extra modules this module may require at run time, that are on behalf of another platform (i.e. shader formats and the like)
		/// </summary>
		protected List<UEBuildModule> PlatformSpecificDynamicallyLoadedModules;

		/// <summary>
		/// Files which this module depends on at runtime.
		/// </summary>
		public RuntimeDependencyList RuntimeDependencies;

		/// <summary>
		/// Returns a list of this module's immediate dependencies.
		/// </summary>
		/// <returns>An enumerable containing the dependencies of the module.</returns>
		public IEnumerable<UEBuildModule> GetDirectDependencyModules()
		{
			return PublicDependencyModules.Concat(PrivateDependencyModules).Concat(DynamicallyLoadedModules).Concat(PlatformSpecificDynamicallyLoadedModules);
		}

		public UEBuildModule(
			string InName,
			UHTModuleType InType,
			DirectoryReference InModuleDirectory,
			ModuleRules InRules,
			FileReference InRulesFile
			)
		{
			Name = InName;
			Type = InType;
			ModuleDirectory = InModuleDirectory;
			Rules = InRules;
			RulesFile = InRulesFile;

			NormalizedModuleIncludePath = Utils.CleanDirectorySeparators(ModuleDirectory.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory), '/');
			ModuleApiDefine = Name.ToUpperInvariant() + "_API";

			PublicDefinitions = HashSetFromOptionalEnumerableStringParameter(InRules.Definitions);
			PublicIncludePaths = HashSetFromOptionalEnumerableStringParameter(InRules.PublicIncludePaths);
			PublicSystemIncludePaths = HashSetFromOptionalEnumerableStringParameter(InRules.PublicSystemIncludePaths);
			PublicLibraryPaths = HashSetFromOptionalEnumerableStringParameter(InRules.PublicLibraryPaths);
			PublicAdditionalLibraries = HashSetFromOptionalEnumerableStringParameter(InRules.PublicAdditionalLibraries);
			PublicFrameworks = HashSetFromOptionalEnumerableStringParameter(InRules.PublicFrameworks);
			PublicWeakFrameworks = HashSetFromOptionalEnumerableStringParameter(InRules.PublicWeakFrameworks);
			PublicAdditionalFrameworks = InRules.PublicAdditionalFrameworks == null ? new HashSet<UEBuildFramework>() : new HashSet<UEBuildFramework>(InRules.PublicAdditionalFrameworks);
			PublicAdditionalShadowFiles = HashSetFromOptionalEnumerableStringParameter(InRules.PublicAdditionalShadowFiles);
			PublicAdditionalBundleResources = InRules.AdditionalBundleResources == null ? new HashSet<UEBuildBundleResource>() : new HashSet<UEBuildBundleResource>(InRules.AdditionalBundleResources);
			PublicDelayLoadDLLs = HashSetFromOptionalEnumerableStringParameter(InRules.PublicDelayLoadDLLs);
			PrivateIncludePaths = HashSetFromOptionalEnumerableStringParameter(InRules.PrivateIncludePaths);
			RuntimeDependencies = (InRules.RuntimeDependencies == null) ? new RuntimeDependencyList() : new RuntimeDependencyList(InRules.RuntimeDependencies);
			IsRedistributableOverride = InRules.IsRedistributableOverride;
		}

		/// <summary>
		/// Determines whether this module has a circular dependency on the given module
		/// </summary>
		public bool HasCircularDependencyOn(string ModuleName)
		{
			return Rules.CircularlyReferencedDependentModules.Contains(ModuleName);
		}

		/// <summary>
		/// Find all the modules which affect the public compile environment. Searches through 
		/// </summary>
		/// <param name="Modules"></param>
		/// <param name="bIncludePathsOnly"></param>
		/// <param name="DependencyModules"></param>
		protected void FindModulesInPublicCompileEnvironment(List<UEBuildModule> Modules, Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag)
		{
			//
			bool bModuleIncludePathsOnly;
			if (!ModuleToIncludePathsOnlyFlag.TryGetValue(this, out bModuleIncludePathsOnly))
			{
				Modules.Add(this);
			}
			else if (!bModuleIncludePathsOnly)
			{
				return;
			}

			ModuleToIncludePathsOnlyFlag[this] = false;

			foreach (UEBuildModule DependencyModule in PublicDependencyModules)
			{
				DependencyModule.FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}

			// Now add an include paths from modules with header files that we need access to, but won't necessarily be importing
			foreach (UEBuildModule IncludePathModule in PublicIncludePathModules)
			{
				IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}
		}

		/// <summary>
		/// Find all the modules which affect the public compile environment. Searches through 
		/// </summary>
		/// <param name="Modules"></param>
		/// <param name="bIncludePathsOnly"></param>
		/// <param name="DependencyModules"></param>
		protected void FindIncludePathModulesInPublicCompileEnvironment(List<UEBuildModule> Modules, Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag)
		{
			if (!ModuleToIncludePathsOnlyFlag.ContainsKey(this))
			{
				// Add this module to the list
				Modules.Add(this);
				ModuleToIncludePathsOnlyFlag.Add(this, true);

				// Include any of its public include path modules in the compile environment too
				foreach (UEBuildModule IncludePathModule in PublicIncludePathModules)
				{
					IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
				}
			}
		}

		/// <summary>
		/// Sets up the environment for compiling any module that includes the public interface of this module.
		/// </summary>
		public void AddModuleToCompileEnvironment(
			UEBuildBinary SourceBinary,
			bool bIncludePathsOnly,
			HashSet<string> IncludePaths,
			HashSet<string> SystemIncludePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks
			)
		{
			// Add this module's public include paths and definitions.
			AddIncludePathsWithChecks(IncludePaths, PublicIncludePaths);
			AddIncludePathsWithChecks(SystemIncludePaths, PublicSystemIncludePaths);
			Definitions.AddRange(PublicDefinitions);

			// If this module is being built into a DLL or EXE, set up an IMPORTS or EXPORTS definition for it.
			if(SourceBinary == null)
			{
				// No source binary means a shared PCH, so always import all symbols. It's possible that an include path module now may be a imported module for the shared PCH consumer.
				if(!bIncludePathsOnly)
				{
					if(Binary == null || !Binary.Config.bAllowExports)
					{
						Definitions.Add(ModuleApiDefine + "=");
					}
					else
					{
						Definitions.Add(ModuleApiDefine + "=DLLIMPORT");
					}
				}
			}
			else if (Binary == null)
			{
				// If we're referencing include paths for a module that's not being built, we don't actually need to import anything from it, but we need to avoid barfing when
				// the compiler encounters an _API define. We also want to avoid changing the compile environment in cases where the module happens to be compiled because it's a dependency
				// of something else, which cause a fall-through to the code below and set up an empty _API define.
				if (bIncludePathsOnly)
				{
					Log.TraceVerbose("{0}: Include paths only for {1} (no binary)", SourceBinary.Config.OutputFilePaths[0].GetFileNameWithoutExtension(), Name);
					Definitions.Add(ModuleApiDefine + "=");
				}
			}
			else
			{
				FileReference BinaryPath = Binary.Config.OutputFilePaths[0];
				FileReference SourceBinaryPath = SourceBinary.Config.OutputFilePaths[0];

				if (ProjectFileGenerator.bGenerateProjectFiles || (Binary.Config.Type == UEBuildBinaryType.StaticLibrary))
				{
					// When generating IntelliSense files, never add dllimport/dllexport specifiers as it
					// simply confuses the compiler
					Definitions.Add(ModuleApiDefine + "=");
				}
				else if (Binary == SourceBinary)
				{
					if (Binary.Config.bAllowExports)
					{
						Log.TraceVerbose("{0}: Exporting {1} from {2}", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=DLLEXPORT");
					}
					else
					{
						Log.TraceVerbose("{0}: Not importing/exporting {1} (binary: {2})", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=");
					}
				}
				else
				{
					// @todo SharedPCH: Public headers included from modules that are not importing the module of that public header, seems invalid.  
					//		Those public headers have no business having APIs in them.  OnlineSubsystem has some public headers like this.  Without changing
					//		this, we need to suppress warnings at compile time.
					if (bIncludePathsOnly)
					{
						Log.TraceVerbose("{0}: Include paths only for {1} (binary: {2})", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=");
					}
					else if (Binary.Config.bAllowExports)
					{
						Log.TraceVerbose("{0}: Importing {1} from {2}", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=DLLIMPORT");
					}
					else
					{
						Log.TraceVerbose("{0}: Not importing/exporting {1} (binary: {2})", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=");
					}
				}
			}

			// Add the module's directory to the include path, so we can root #includes to it
			IncludePaths.Add(NormalizedModuleIncludePath);

			// Add the additional frameworks so that the compiler can know about their #include paths
			AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);

			// Remember the module so we can refer to it when needed
			foreach (UEBuildFramework Framework in PublicAdditionalFrameworks)
			{
				Framework.OwningModule = this;
			}
		}

		static Regex VCMacroRegex = new Regex(@"\$\([A-Za-z0-9_]+\)");

		/// <summary>
		/// Checks if path contains a VC macro
		/// </summary>
		protected bool DoesPathContainVCMacro(string Path)
		{
			return VCMacroRegex.IsMatch(Path);
		}

		/// <summary>
		/// Adds PathsToAdd to IncludePaths, performing path normalization and ignoring duplicates.
		/// </summary>
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
				foreach (string Path in PathsToAdd)
				{
					string NormalizedPath = Path.TrimEnd('/');
					// If path doesn't exist, it may contain VC macro (which is passed directly to and expanded by compiler).
					if (Directory.Exists(NormalizedPath) || DoesPathContainVCMacro(NormalizedPath))
					{
						IncludePaths.Add(NormalizedPath);
					}
				}
			}
		}

		/// <summary>
		/// Sets up the environment for compiling this module.
		/// </summary>
		protected virtual void SetupPrivateCompileEnvironment(
			HashSet<string> IncludePaths,
			HashSet<string> SystemIncludePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks
			)
		{
			HashSet<UEBuildModule> VisitedModules = new HashSet<UEBuildModule>();

			if (this.Type.IsGameModule())
			{
				Definitions.Add("DEPRECATED_FORGAME=DEPRECATED");
			}

			// Add this module's private include paths and definitions.
			AddIncludePathsWithChecks(IncludePaths, PrivateIncludePaths);

			// Find all the modules that are part of the public compile environment for this module.
			List<UEBuildModule> Modules = new List<UEBuildModule>();
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);

			// Add in all the modules that are private dependencies
			foreach (UEBuildModule DependencyModule in PrivateDependencyModules)
			{
				DependencyModule.FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}

			// And finally add in all the modules that are include path only dependencies
			foreach (UEBuildModule IncludePathModule in PrivateIncludePathModules)
			{
				IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}

			// Now set up the compile environment for the modules in the original order that we encountered them
			foreach (UEBuildModule Module in Modules)
			{
				Module.AddModuleToCompileEnvironment(Binary, ModuleToIncludePathsOnlyFlag[Module], IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks);
			}
		}

		/// <summary>
		/// Sets up the environment for linking any module that includes the public interface of this module.
		/// </summary>
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
			HashSet<UEBuildModule> VisitedModules
			)
		{
			// There may be circular dependencies in compile dependencies, so we need to avoid reentrance.
			if (VisitedModules.Add(this))
			{
				// Add this module's binary to the binary dependencies.
				if (Binary != null
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
					List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
					AllDependencyModules.AddRange(PrivateDependencyModules);
					AllDependencyModules.AddRange(PublicDependencyModules);

					foreach (UEBuildModule DependencyModule in AllDependencyModules)
					{
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
				foreach (UEBuildFramework Framework in PublicAdditionalFrameworks)
				{
					Framework.OwningModule = this;
				}
				AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);
				AdditionalShadowFiles.AddRange(PublicAdditionalShadowFiles);
				DelayLoadDLLs.AddRange(PublicDelayLoadDLLs);
			}
		}

		/// <summary>
		/// Sets up the environment for linking this module.
		/// </summary>
		public virtual void SetupPrivateLinkEnvironment(
			UEBuildBinary SourceBinary,
			LinkEnvironment LinkEnvironment,
			List<UEBuildBinary> BinaryDependencies,
			HashSet<UEBuildModule> VisitedModules
			)
		{
			// Allow the module's public dependencies to add library paths and additional libraries to the link environment.
			SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.Config.LibraryPaths, LinkEnvironment.Config.AdditionalLibraries, LinkEnvironment.Config.Frameworks, LinkEnvironment.Config.WeakFrameworks,
				LinkEnvironment.Config.AdditionalFrameworks, LinkEnvironment.Config.AdditionalShadowFiles, LinkEnvironment.Config.AdditionalBundleResources, LinkEnvironment.Config.DelayLoadDLLs, BinaryDependencies, VisitedModules);

			// Also allow the module's public and private dependencies to modify the link environment.
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
			AllDependencyModules.AddRange(PrivateDependencyModules);
			AllDependencyModules.AddRange(PublicDependencyModules);

			foreach (UEBuildModule DependencyModule in AllDependencyModules)
			{
				DependencyModule.SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.Config.LibraryPaths, LinkEnvironment.Config.AdditionalLibraries, LinkEnvironment.Config.Frameworks, LinkEnvironment.Config.WeakFrameworks,
					LinkEnvironment.Config.AdditionalFrameworks, LinkEnvironment.Config.AdditionalShadowFiles, LinkEnvironment.Config.AdditionalBundleResources, LinkEnvironment.Config.DelayLoadDLLs, BinaryDependencies, VisitedModules);
			}
		}

		/// <summary>
		/// Compiles the module, and returns a list of files output by the compiler.
		/// </summary>
		public abstract List<FileItem> Compile(UEBuildTarget Target, UEToolChain ToolChain, CPPEnvironment CompileEnvironment, List<PrecompiledHeaderTemplate> SharedPCHModules, ActionGraph ActionGraph);

		// Object interface.
		public override string ToString()
		{
			return Name;
		}

		/// <summary>
		/// Finds the modules referenced by this module which have not yet been bound to a binary
		/// </summary>
		/// <returns>List of unbound modules</returns>
		public List<UEBuildModule> GetUnboundReferences()
		{
			List<UEBuildModule> Modules = new List<UEBuildModule>();
			Modules.AddRange(PrivateDependencyModules.Where(x => x.Binary == null));
			Modules.AddRange(PublicDependencyModules.Where(x => x.Binary == null));
			return Modules;
		}

		[DebuggerDisplay("{Index}: {Module}")]
		public class ModuleIndexPair
		{
			public UEBuildModule Module;
			public int Index;
		}

		/// <summary>
		/// Gets all of the modules referenced by this module
		/// </summary>
		/// <param name="ReferencedModules">Hash of all referenced modules with their addition index.</param>
		/// <param name="IgnoreReferencedModules">Hashset used to ignore modules which are already added to the list</param>
		/// <param name="bIncludeDynamicallyLoaded">True if dynamically loaded modules (and all of their dependent modules) should be included.</param>
		/// <param name="bForceCircular">True if circular dependencies should be processed</param>
		/// <param name="bOnlyDirectDependencies">True to return only this module's direct dependencies</param>
		public virtual void GetAllDependencyModules(List<UEBuildModule> ReferencedModules, HashSet<UEBuildModule> IgnoreReferencedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies)
		{
		}

		/// <summary>
		/// Gets all of the modules precompiled along with this module
		/// </summary>
		/// <param name="Modules">Set of all the precompiled modules</param>
		public virtual void RecursivelyAddPrecompiledModules(List<UEBuildModule> Modules)
		{
		}

		/// <summary>
		/// Creates all the modules required for this target
		/// </summary>
		public void RecursivelyCreateModules(UEBuildTarget Target)
		{
			// Create all the include path modules. These modules may not be added to the target (and we don't process their dependencies), but they need 
			// to be created to set up their compile environment.
			RecursivelyCreateIncludePathModulesByName(Target, Rules.PublicIncludePathModuleNames, ref PublicIncludePathModules);
			RecursivelyCreateIncludePathModulesByName(Target, Rules.PrivateIncludePathModuleNames, ref PrivateIncludePathModules);

			// Create all the dependency modules
			RecursivelyCreateModulesByName(Target, Rules.PublicDependencyModuleNames, ref PublicDependencyModules);
			RecursivelyCreateModulesByName(Target, Rules.PrivateDependencyModuleNames, ref PrivateDependencyModules);
			RecursivelyCreateModulesByName(Target, Rules.DynamicallyLoadedModuleNames, ref DynamicallyLoadedModules);
			RecursivelyCreateModulesByName(Target, Rules.PlatformSpecificDynamicallyLoadedModuleNames, ref PlatformSpecificDynamicallyLoadedModules);
		}

		private static void RecursivelyCreateModulesByName(UEBuildTarget Target, List<string> ModuleNames, ref List<UEBuildModule> Modules)
		{
			// Check whether the module list is already set. We set this immediately (via the ref) to avoid infinite recursion.
			if (Modules == null)
			{
				Modules = new List<UEBuildModule>();
				foreach (string ModuleName in ModuleNames)
				{
					UEBuildModule Module = Target.FindOrCreateModuleByName(ModuleName);
					if (!Modules.Contains(Module))
					{
						Module.RecursivelyCreateModules(Target);
						Modules.Add(Module);
					}
				}
			}
		}

		private static void RecursivelyCreateIncludePathModulesByName(UEBuildTarget Target, List<string> ModuleNames, ref List<UEBuildModule> Modules)
		{
			// Check whether the module list is already set. We set this immediately (via the ref) to avoid infinite recursion.
			if (Modules == null)
			{
				Modules = new List<UEBuildModule>();
				foreach (string ModuleName in ModuleNames)
				{
					UEBuildModule Module = Target.FindOrCreateModuleByName(ModuleName);
					RecursivelyCreateIncludePathModulesByName(Target, Module.Rules.PublicIncludePathModuleNames, ref Module.PublicIncludePathModules);
					Modules.Add(Module);
				}
			}
		}

		/// <summary>
		/// Write information about this binary to a JSON file
		/// </summary>
		/// <param name="Writer">Writer for this binary's data</param>
		public virtual void ExportJson(JsonWriter Writer)
		{
			Writer.WriteValue("Name", Name);
			Writer.WriteValue("Type", Type.ToString());
			Writer.WriteValue("Directory", ModuleDirectory.FullName);
			Writer.WriteValue("Rules", RulesFile.FullName);
			Writer.WriteValue("PCHUsage", Rules.PCHUsage.ToString());

			if (Rules.PrivatePCHHeaderFile != null)
			{
				Writer.WriteValue("PrivatePCH", FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile).FullName);
			}

			if (Rules.SharedPCHHeaderFile != null)
			{
				Writer.WriteValue("SharedPCH", FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile).FullName);
			}

			ExportJsonModuleArray(Writer, "PublicDependencyModules", PublicDependencyModules);
			ExportJsonModuleArray(Writer, "PublicIncludePathModules", PublicIncludePathModules);
			ExportJsonModuleArray(Writer, "PrivateDependencyModules", PrivateDependencyModules);
			ExportJsonModuleArray(Writer, "PrivateIncludePathModules", PrivateIncludePathModules);
			ExportJsonModuleArray(Writer, "DynamicallyLoadedModules", DynamicallyLoadedModules);

			Writer.WriteArrayStart("CircularlyReferencedModules");
			foreach(string ModuleName in Rules.CircularlyReferencedDependentModules)
			{
				Writer.WriteValue(ModuleName);
			}
			Writer.WriteArrayEnd();
		}

		/// <summary>
		/// Write an array of module names to a JSON writer
		/// </summary>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="ArrayName">Name of the array property</param>
		/// <param name="Modules">Sequence of modules to write. May be null.</param>
		void ExportJsonModuleArray(JsonWriter Writer, string ArrayName, IEnumerable<UEBuildModule> Modules)
		{
			Writer.WriteArrayStart(ArrayName);
			if (Modules != null)
			{
				foreach (UEBuildModule Module in Modules)
				{
					Writer.WriteValue(Module.Name);
				}
			}
			Writer.WriteArrayEnd();
		}
	};

	/// <summary>
	/// A module that is never compiled by us, and is only used to group include paths and libraries into a dependency unit.
	/// </summary>
	class UEBuildExternalModule : UEBuildModule
	{
		public UEBuildExternalModule(
			UHTModuleType InType,
			string InName,
			DirectoryReference InModuleDirectory,
			ModuleRules InRules,
			FileReference InRulesFile
			)
			: base(
				InType: InType,
				InName: InName,
				InModuleDirectory: InModuleDirectory,
				InRules: InRules,
				InRulesFile: InRulesFile
				)
		{
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(UEBuildTarget Target, UEToolChain ToolChain, CPPEnvironment CompileEnvironment, List<PrecompiledHeaderTemplate> SharedPCHs, ActionGraph ActionGraph)
		{
			return new List<FileItem>();
		}
	};

	/// <summary>
	/// A module that is compiled from C++ code.
	/// </summary>
	public class UEBuildModuleCPP : UEBuildModule
	{
		public class AutoGenerateCppInfoClass
		{
			public class BuildInfoClass
			{
				/// <summary>
				/// The wildcard of the *.generated.cpp file which was generated for the module
				/// </summary>
				public readonly string FileWildcard;

				public BuildInfoClass(string InWildcard)
				{
					Debug.Assert(InWildcard != null);

					FileWildcard = InWildcard;
				}
			}

			/// <summary>
			/// Information about how to build the .generated.cpp files. If this is null, then we're not building .generated.cpp files for this module.
			/// </summary>
			public BuildInfoClass BuildInfo;

			public AutoGenerateCppInfoClass(BuildInfoClass InBuildInfo)
			{
				BuildInfo = InBuildInfo;
			}
		}

		/// <summary>
		/// Information about the .generated.cpp file.  If this is null then this module doesn't have any UHT-produced code.
		/// </summary>
		public AutoGenerateCppInfoClass AutoGenerateCppInfo = null;

		public class SourceFilesClass
		{
			public readonly List<FileItem> MissingFiles = new List<FileItem>();
			public readonly List<FileItem> CPPFiles = new List<FileItem>();
			public readonly List<FileItem> CFiles = new List<FileItem>();
			public readonly List<FileItem> CCFiles = new List<FileItem>();
			public readonly List<FileItem> MMFiles = new List<FileItem>();
			public readonly List<FileItem> RCFiles = new List<FileItem>();
			public readonly List<FileItem> OtherFiles = new List<FileItem>();

			public int Count
			{
				get
				{
					return MissingFiles.Count +
						   CPPFiles.Count +
						   CFiles.Count +
						   CCFiles.Count +
						   MMFiles.Count +
						   RCFiles.Count +
						   OtherFiles.Count;
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

		/// <summary>
		/// Adds additional source cpp files for this module.
		/// </summary>
		/// <param name="Files">Files to add.</param>
		public void AddAdditionalCPPFiles(IEnumerable<FileItem> Files)
		{
			SourceFilesToBuild.CPPFiles.AddRange(Files);
		}

		/// <summary>
		/// A list of the absolute paths of source files to be built in this module.
		/// </summary>
		public readonly SourceFilesClass SourceFilesToBuild = new SourceFilesClass();

		/// <summary>
		/// A list of the source files that were found for the module.
		/// </summary>
		public readonly SourceFilesClass SourceFilesFound = new SourceFilesClass();

		/// <summary>
		/// The directory for this module's generated code
		/// </summary>
		public readonly DirectoryReference GeneratedCodeDirectory;

		/// <summary>
		/// The preprocessor definitions used to compile this module's private implementation.
		/// </summary>
		HashSet<string> Definitions;

		/// When set, allows this module to report compiler definitions and include paths for Intellisense
		IntelliSenseGatherer IntelliSenseGatherer;

		public List<string> IncludeSearchPaths = new List<string>();

		public class ProcessedDependenciesClass
		{
			/// <summary>
			/// The file, if any, which is used as the unique PCH for this module
			/// </summary>
			public FileItem UniquePCHHeaderFile = null;
		}

		/// <summary>
		/// The processed dependencies for the class
		/// </summary>
		public ProcessedDependenciesClass ProcessedDependencies = null;

		/// <summary>
		/// List of invalid include directives. These are buffered up and output before we start compiling.
		/// </summary>
		public List<string> InvalidIncludeDirectiveMessages;

		/// <summary>
		/// Hack to skip adding definitions to compile environment. They will be baked into source code by external code.
		/// </summary>
		public bool bSkipDefinitionsForCompileEnvironment = false;

		public int FindNumberOfGeneratedCppFiles()
		{
			return ((null == GeneratedCodeDirectory) || !GeneratedCodeDirectory.Exists()) ? 0
				 : (GeneratedCodeDirectory.EnumerateFileReferences("*.generated.*.cpp", SearchOption.AllDirectories).Count()
				  + GeneratedCodeDirectory.EnumerateFileReferences("*.generated.cpp", SearchOption.AllDirectories).Count());
		}

		/// <summary>
		/// Categorizes source files into per-extension buckets
		/// </summary>
		private static void CategorizeSourceFiles(IEnumerable<FileItem> InSourceFiles, SourceFilesClass OutSourceFiles)
		{
			foreach (FileItem SourceFile in InSourceFiles)
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

		/// <summary>
		/// List of whitelisted circular dependencies. Please do NOT add new modules here; refactor to allow the modules to be decoupled instead.
		/// </summary>
		static readonly KeyValuePair<string, string>[] WhitelistedCircularDependencies =
		{
			new KeyValuePair<string, string>("Engine", "AIModule"),
			new KeyValuePair<string, string>("Engine", "Landscape"),
			new KeyValuePair<string, string>("Engine", "UMG"),
			new KeyValuePair<string, string>("Engine", "GameplayTags"),
			new KeyValuePair<string, string>("Engine", "MaterialShaderQualitySettings"),
			new KeyValuePair<string, string>("Engine", "UnrealEd"),
			new KeyValuePair<string, string>("PacketHandler", "ReliabilityHandlerComponent"),
			new KeyValuePair<string, string>("GameplayDebugger", "AIModule"),
			new KeyValuePair<string, string>("GameplayDebugger", "GameplayTasks"),
			new KeyValuePair<string, string>("Engine", "CinematicCamera"),
			new KeyValuePair<string, string>("Engine", "CollisionAnalyzer"),
			new KeyValuePair<string, string>("Engine", "LogVisualizer"),
			new KeyValuePair<string, string>("Engine", "Kismet"),
			new KeyValuePair<string, string>("Landscape", "UnrealEd"),
			new KeyValuePair<string, string>("Landscape", "MaterialUtilities"),
			new KeyValuePair<string, string>("LocalizationDashboard", "LocalizationService"),
			new KeyValuePair<string, string>("LocalizationDashboard", "MainFrame"),
			new KeyValuePair<string, string>("LocalizationDashboard", "TranslationEditor"),
			new KeyValuePair<string, string>("Documentation", "SourceControl"),
			new KeyValuePair<string, string>("UnrealEd", "GraphEditor"),
			new KeyValuePair<string, string>("UnrealEd", "Kismet"),
			new KeyValuePair<string, string>("UnrealEd", "AudioEditor"),
			new KeyValuePair<string, string>("BlueprintGraph", "KismetCompiler"),
			new KeyValuePair<string, string>("BlueprintGraph", "UnrealEd"),
			new KeyValuePair<string, string>("BlueprintGraph", "GraphEditor"),
			new KeyValuePair<string, string>("BlueprintGraph", "Kismet"),
			new KeyValuePair<string, string>("BlueprintGraph", "CinematicCamera"),
			new KeyValuePair<string, string>("ConfigEditor", "PropertyEditor"),
			new KeyValuePair<string, string>("SourceControl", "UnrealEd"),
			new KeyValuePair<string, string>("Kismet", "BlueprintGraph"),
			new KeyValuePair<string, string>("Kismet", "UMGEditor"),
			new KeyValuePair<string, string>("MovieSceneTools", "Sequencer"),
			new KeyValuePair<string, string>("Sequencer", "MovieSceneTools"),
			new KeyValuePair<string, string>("AIModule", "AITestSuite"),
			new KeyValuePair<string, string>("GameplayTasks", "UnrealEd"),
			new KeyValuePair<string, string>("AnimGraph", "UnrealEd"),
			new KeyValuePair<string, string>("AnimGraph", "GraphEditor"),
			new KeyValuePair<string, string>("MaterialUtilities", "Landscape"),
			new KeyValuePair<string, string>("HierarchicalLODOutliner", "UnrealEd"),
			new KeyValuePair<string, string>("PixelInspectorModule", "UnrealEd"),
			new KeyValuePair<string, string>("GameplayTagsEditor", "BlueprintGraph"),
			new KeyValuePair<string, string>("GameplayAbilitiesEditor", "BlueprintGraph"),
			new KeyValuePair<string, string>("GameplayAbilitiesEditor", "GameplayTagsEditor"),
		};


		public UEBuildModuleCPP(
			string InName,
			UHTModuleType InType,
			DirectoryReference InModuleDirectory,
			DirectoryReference InGeneratedCodeDirectory,
			IntelliSenseGatherer InIntelliSenseGatherer,
			IEnumerable<FileItem> InSourceFiles,
			ModuleRules InRules,
			bool bInBuildSourceFiles,
			FileReference InRulesFile
			)
			: base(
					InName,
					InType,
					InModuleDirectory,
					InRules,
					InRulesFile
				)
		{
			GeneratedCodeDirectory = InGeneratedCodeDirectory;
			IntelliSenseGatherer = InIntelliSenseGatherer;

			CategorizeSourceFiles(InSourceFiles, SourceFilesFound);
			if (bInBuildSourceFiles)
			{
				SourceFilesToBuild.CopyFrom(SourceFilesFound);
			}

			Definitions = HashSetFromOptionalEnumerableStringParameter(InRules.Definitions);
			foreach (string Def in Definitions)
			{
				Log.TraceVerbose("Compile Env {0}: {1}", Name, Def);
			}

			foreach(string CircularlyReferencedModuleName in Rules.CircularlyReferencedDependentModules)
			{
				if(CircularlyReferencedModuleName != "BlueprintContext" && !WhitelistedCircularDependencies.Any(x => x.Key == Name && x.Value == CircularlyReferencedModuleName))
				{
					Log.TraceWarning("Found reference between '{0}' and '{1}'. Support for circular references is being phased out; please do not introduce new ones.", Name, CircularlyReferencedModuleName);
				}
			}
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(UEBuildTarget Target, UEToolChain ToolChain, CPPEnvironment CompileEnvironment, List<PrecompiledHeaderTemplate> SharedPCHs, ActionGraph ActionGraph)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Platform);

			List<FileItem> LinkInputFiles = new List<FileItem>();
			if (ProjectFileGenerator.bGenerateProjectFiles && IntelliSenseGatherer == null)
			{
				// Nothing to do for IntelliSense, bail out early
				return LinkInputFiles;
			}

			CPPEnvironment ModuleCompileEnvironment = CreateModuleCompileEnvironment(Target, CompileEnvironment);
			IncludeSearchPaths = ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludePaths.ToList();
			IncludeSearchPaths.AddRange(ModuleCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.ToList());

			if (IntelliSenseGatherer != null)
			{
				// Update project file's set of preprocessor definitions and include paths
				IntelliSenseGatherer.AddIntelliSensePreprocessorDefinitions(ModuleCompileEnvironment.Config.Definitions);
				IntelliSenseGatherer.AddInteliiSenseIncludePaths(ModuleCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths, bAddingSystemIncludes: true);
				IntelliSenseGatherer.AddInteliiSenseIncludePaths(ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludePaths, bAddingSystemIncludes: false);

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

			{
				// Process all of the header file dependencies for this module
				this.CachePCHUsageForModuleSourceFiles(Target.Headers, ModuleCompileEnvironment);

				// Make sure our RC files have cached includes.  
				foreach (FileItem RCFile in SourceFilesToBuild.RCFiles)
				{
					RCFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;
				}
			}


			// Check to see if this is an Engine module (including program or plugin modules).  That is, the module is located under the "Engine" folder
			bool IsPluginModule = ModuleDirectory.IsUnderDirectory(DirectoryReference.Combine(Target.ProjectDirectory, "Plugins"));
			bool IsGameModule = !IsPluginModule && !ModuleDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory);

			// Should we force a precompiled header to be generated for this module?  Usually, we only bother with a
			// precompiled header if there are at least several source files in the module (after combining them for unity
			// builds.)  But for game modules, it can be convenient to always have a precompiled header to single-file
			// changes to code is really quick to compile.
			int MinFilesUsingPrecompiledHeader = BuildConfiguration.MinFilesUsingPrecompiledHeader;
			if (Rules.MinFilesUsingPrecompiledHeaderOverride != 0)
			{
				MinFilesUsingPrecompiledHeader = Rules.MinFilesUsingPrecompiledHeaderOverride;
			}
			else if (IsGameModule && BuildConfiguration.bForcePrecompiledHeaderForGameModules)
			{
				// This is a game module with only a small number of source files, so go ahead and force a precompiled header
				// to be generated to make incremental changes to source files as fast as possible for small projects.
				MinFilesUsingPrecompiledHeader = 1;
			}


			// Engine modules will always use unity build mode unless MinSourceFilesForUnityBuildOverride is specified in
			// the module rules file.  By default, game modules only use unity of they have enough source files for that
			// to be worthwhile.  If you have a lot of small game modules, consider specifying MinSourceFilesForUnityBuildOverride=0
			// in the modules that you don't typically iterate on source files in very frequently.
			int MinSourceFilesForUnityBuild = 0;
			if (Rules.MinSourceFilesForUnityBuildOverride != 0)
			{
				MinSourceFilesForUnityBuild = Rules.MinSourceFilesForUnityBuildOverride;
			}
			else if (IsGameModule)
			{
				// Game modules with only a small number of source files are usually better off having faster iteration times
				// on single source file changes, so we forcibly disable unity build for those modules
				MinSourceFilesForUnityBuild = BuildConfiguration.MinGameModuleSourceFilesForUnityBuild;
			}


			// Should we use unity build mode for this module?
			bool bModuleUsesUnityBuild = false;
			if (BuildConfiguration.bUseUnityBuild || BuildConfiguration.bForceUnityBuild)
			{
				if (BuildConfiguration.bForceUnityBuild)
				{
					Log.TraceVerbose("Module '{0}' using unity build mode (bForceUnityBuild enabled for this module)", this.Name);
					bModuleUsesUnityBuild = true;
				}
				else if (Rules.bFasterWithoutUnity)
				{
					Log.TraceVerbose("Module '{0}' not using unity build mode (bFasterWithoutUnity enabled for this module)", this.Name);
					bModuleUsesUnityBuild = false;
				}
				else if (SourceFilesToBuild.CPPFiles.Count < MinSourceFilesForUnityBuild)
				{
					Log.TraceVerbose("Module '{0}' not using unity build mode (module with fewer than {1} source files)", this.Name, MinSourceFilesForUnityBuild);
					bModuleUsesUnityBuild = false;
				}
				else
				{
					Log.TraceVerbose("Module '{0}' using unity build mode (enabled in BuildConfiguration)", this.Name);
					bModuleUsesUnityBuild = true;
				}
			}
			else
			{
				Log.TraceVerbose("Module '{0}' not using unity build mode (disabled in BuildConfiguration)", this.Name);
			}

			// Set up the environment with which to compile the CPP files
			CPPEnvironment CPPCompileEnvironment = ModuleCompileEnvironment;
			if (BuildPlatform.ShouldUsePCHFiles(CompileEnvironment.Config.Platform, CompileEnvironment.Config.Configuration))
			{
				// If this module has an explicit PCH, use that
				if(Rules.PrivatePCHHeaderFile != null)
				{
					PrecompiledHeaderInstance Instance = CreatePrivatePCH(ToolChain, FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile)), CPPCompileEnvironment, ActionGraph);

					CPPCompileEnvironment = CPPCompileEnvironment.DeepCopy();
					CPPCompileEnvironment.Config.Definitions.Clear();
					CPPCompileEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					CPPCompileEnvironment.Config.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Reference;
					CPPCompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

					LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
				}

				// Try to find a suitable shared PCH for this module
				if (CPPCompileEnvironment.PrecompiledHeaderFile == null && SharedPCHs.Count > 0 && !CompileEnvironment.Config.bIsBuildingLibrary && Rules.PCHUsage != ModuleRules.PCHUsageMode.NoSharedPCHs)
				{
					// Find all the dependencies of this module
					HashSet<UEBuildModule> ReferencedModules = new HashSet<UEBuildModule>();
					GetAllDependencyModules(new List<UEBuildModule>(), ReferencedModules, bIncludeDynamicallyLoaded: false, bForceCircular: false, bOnlyDirectDependencies: true);

					// Find the first shared PCH module we can use
					PrecompiledHeaderTemplate Template = SharedPCHs.FirstOrDefault(x => ReferencedModules.Contains(x.Module));
					if(Template != null && Template.IsValidFor(CompileEnvironment))
					{
						PrecompiledHeaderInstance Instance = FindOrCreateSharedPCH(ToolChain, Template, ModuleCompileEnvironment.Config.bOptimizeCode, ActionGraph);

						FileReference PrivateDefinitionsFile = FileReference.Combine(CPPCompileEnvironment.Config.OutputDirectory, String.Format("Definitions.{0}.h", Name));
						using (StringWriter Writer = new StringWriter())
						{
							// Remove the module _API definition for cases where there are circular dependencies between the shared PCH module and modules using it
							Writer.WriteLine("#undef {0}", ModuleApiDefine);

							// Games may choose to use shared PCHs from the engine, so allow them to change the value of these macros
							if(Type.IsGameModule())
							{
								Writer.WriteLine("#undef UE_IS_ENGINE_MODULE");
								Writer.WriteLine("#undef DEPRECATED_FORGAME");
								Writer.WriteLine("#define DEPRECATED_FORGAME DEPRECATED");
							}

							WriteDefinitions(CPPCompileEnvironment.Config.Definitions, Writer);
							FileItem.CreateIntermediateTextFile(PrivateDefinitionsFile, Writer.ToString());
						}

						CPPCompileEnvironment = CPPCompileEnvironment.DeepCopy();
						CPPCompileEnvironment.Config.Definitions.Clear();
						CPPCompileEnvironment.Config.ForceIncludeFiles.Add(PrivateDefinitionsFile);
						CPPCompileEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
						CPPCompileEnvironment.Config.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Reference;
						CPPCompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

						LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
					}
				}

				// If there was one header that was included first by enough C++ files, use it as the precompiled header. Only use precompiled headers for projects with enough files to make the PCH creation worthwhile.
				if (CPPCompileEnvironment.PrecompiledHeaderFile == null && SourceFilesToBuild.CPPFiles.Count >= MinFilesUsingPrecompiledHeader && ProcessedDependencies != null)
				{
					PrecompiledHeaderInstance Instance = CreatePrivatePCH(ToolChain, ProcessedDependencies.UniquePCHHeaderFile, CPPCompileEnvironment, ActionGraph);

					CPPCompileEnvironment = CPPCompileEnvironment.DeepCopy();
					CPPCompileEnvironment.Config.Definitions.Clear();
					CPPCompileEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					CPPCompileEnvironment.Config.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Reference;
					CPPCompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

					LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
				}
			}

			// Compile CPP files
			List<FileItem> CPPFilesToCompile = SourceFilesToBuild.CPPFiles;
			if (bModuleUsesUnityBuild)
			{
				CPPFilesToCompile = Unity.GenerateUnityCPPs(ToolChain, Target, CPPFilesToCompile, CPPCompileEnvironment, Name);
				LinkInputFiles.AddRange(CompileUnityFilesWithToolChain(ToolChain, CPPCompileEnvironment, ModuleCompileEnvironment, CPPFilesToCompile, Name, ActionGraph).ObjectFiles);
			}
			else
			{
				LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CPPCompileEnvironment, CPPFilesToCompile, Name, ActionGraph).ObjectFiles);
			}

			// Compile all the generated CPP files
			if (AutoGenerateCppInfo != null && AutoGenerateCppInfo.BuildInfo != null && !CPPCompileEnvironment.bHackHeaderGenerator)
			{
				string[] GeneratedFiles = Directory.GetFiles(Path.GetDirectoryName(AutoGenerateCppInfo.BuildInfo.FileWildcard), Path.GetFileName(AutoGenerateCppInfo.BuildInfo.FileWildcard));
				if(GeneratedFiles.Length > 0)
				{
					// Create a compile environment for the generated files. We can disable creating debug info here to improve link times.
					CPPEnvironment GeneratedCPPCompileEnvironment = CPPCompileEnvironment;
					if(GeneratedCPPCompileEnvironment.Config.bCreateDebugInfo && BuildConfiguration.bDisableDebugInfoForGeneratedCode)
					{
						GeneratedCPPCompileEnvironment = GeneratedCPPCompileEnvironment.DeepCopy();
						GeneratedCPPCompileEnvironment.Config.bCreateDebugInfo = false;
					}

					// Compile all the generated files
					foreach (string GeneratedFilename in GeneratedFiles)
					{
						FileItem GeneratedCppFileItem = FileItem.GetItemByPath(GeneratedFilename);

						CachePCHUsageForModuleSourceFile(CPPCompileEnvironment, GeneratedCppFileItem);

						// @todo ubtmake: Check for ALL other places where we might be injecting .cpp or .rc files for compiling without caching CachedCPPIncludeInfo first (anything platform specific?)
						LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(GeneratedCPPCompileEnvironment, new List<FileItem> { GeneratedCppFileItem }, Name, ActionGraph).ObjectFiles);
					}
				}
			}

			// Compile C files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CPPCompileEnvironment, SourceFilesToBuild.CFiles, Name, ActionGraph).ObjectFiles);

			// Compile CC files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CPPCompileEnvironment, SourceFilesToBuild.CCFiles, Name, ActionGraph).ObjectFiles);

			// Compile MM files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CPPCompileEnvironment, SourceFilesToBuild.MMFiles, Name, ActionGraph).ObjectFiles);

			// Compile RC files.
			LinkInputFiles.AddRange(ToolChain.CompileRCFiles(ModuleCompileEnvironment, SourceFilesToBuild.RCFiles, ActionGraph).ObjectFiles);

			return LinkInputFiles;
		}

		/// <summary>
		/// Create a shared PCH template for this module, which allows constructing shared PCH instances in the future
		/// </summary>
		/// <param name="Target">The target which owns this module</param>
		/// <param name="BaseCompileEnvironment">Base compile environment for this target</param>
		/// <returns>Template for shared PCHs</returns>
		public PrecompiledHeaderTemplate CreateSharedPCHTemplate(UEBuildTarget Target, CPPEnvironment BaseCompileEnvironment)
		{
			CPPEnvironment CompileEnvironment = CreateSharedPCHCompileEnvironment(Target, BaseCompileEnvironment);
			FileItem HeaderFile = FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile));
			HeaderFile.CachedCPPIncludeInfo = CompileEnvironment.Config.CPPIncludeInfo;
			DirectoryReference OutputDir = (CompileEnvironment.Config.PCHOutputDirectory != null)? DirectoryReference.Combine(CompileEnvironment.Config.PCHOutputDirectory, Name) : CompileEnvironment.Config.OutputDirectory;
			return new PrecompiledHeaderTemplate(this, CompileEnvironment, HeaderFile, OutputDir);
		}

		/// <summary>
		/// Creates a precompiled header action to generate a new pch file 
		/// </summary>
		/// <param name="ToolChain">The toolchain to generate the PCH</param>
		/// <param name="HeaderFile"></param>
		/// <param name="ModuleCompileEnvironment"></param>
		/// <returns>The created PCH instance.</returns>
		private PrecompiledHeaderInstance CreatePrivatePCH(UEToolChain ToolChain, FileItem HeaderFile, CPPEnvironment ModuleCompileEnvironment, ActionGraph ActionGraph)
		{
			// Cache the header file include paths. This file could have been a shared PCH too, so ignore if the include paths are already set.
			if(HeaderFile.CachedCPPIncludeInfo == null)
			{
				HeaderFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;
			}

			// Create the wrapper file, which sets all the definitions needed to compile it
			FileReference WrapperLocation = FileReference.Combine(ModuleCompileEnvironment.Config.OutputDirectory, String.Format("PCH.{0}.h", Name));
			FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, ModuleCompileEnvironment.Config.Definitions, HeaderFile);

			// Create a new C++ environment that is used to create the PCH.
			CPPEnvironment CompileEnvironment = ModuleCompileEnvironment.DeepCopy();
			CompileEnvironment.Config.Definitions.Clear();
			CompileEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
			CompileEnvironment.Config.PrecompiledHeaderIncludeFilename = WrapperFile.Reference;
			CompileEnvironment.Config.OutputDirectory = ModuleCompileEnvironment.Config.OutputDirectory;
			CompileEnvironment.Config.bOptimizeCode = ModuleCompileEnvironment.Config.bOptimizeCode;

			// Create the action to compile the PCH file.
			CPPOutput Output = ToolChain.CompileCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, Name, ActionGraph);
			return new PrecompiledHeaderInstance(HeaderFile, CompileEnvironment.Config.bOptimizeCode, Output);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Template"></param>
		/// <param name="bOptimizeCode"></param>
		/// <returns></returns>
		public PrecompiledHeaderInstance FindOrCreateSharedPCH(UEToolChain ToolChain, PrecompiledHeaderTemplate Template, bool bOptimizeCode, ActionGraph ActionGraph)
		{
			PrecompiledHeaderInstance Instance = Template.Instances.Find(x => x.bOptimizeCode == bOptimizeCode);
			if(Instance == null)
			{
				// Create a suffix to distinguish this shared PCH variant from any others. Currently only optimized and non-optimized shared PCHs are supported.
				string Variant = "";
				if(bOptimizeCode != Template.BaseCompileEnvironment.Config.bOptimizeCode)
				{
					if(bOptimizeCode)
					{
						Variant += ".Optimized";
					}
					else
					{
						Variant += ".NonOptimized";
					}
				}

				// Create the wrapper file, which sets all the definitions needed to compile it
				FileReference WrapperLocation = FileReference.Combine(Template.OutputDir, String.Format("SharedPCH.{0}{1}.h", Template.Module.Name, Variant));
				FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, Template.BaseCompileEnvironment.Config.Definitions, Template.HeaderFile);

				// Create the compile environment for this PCH
				CPPEnvironment CompileEnvironment = Template.BaseCompileEnvironment.DeepCopy();
				CompileEnvironment.Config.Definitions.Clear();
				CompileEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				CompileEnvironment.Config.PrecompiledHeaderIncludeFilename = WrapperFile.Reference;
				CompileEnvironment.Config.OutputDirectory = Template.OutputDir;
				CompileEnvironment.Config.bOptimizeCode = bOptimizeCode;

				// Create the PCH
				CPPOutput Output = ToolChain.CompileCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, "Shared", ActionGraph);
				Instance = new PrecompiledHeaderInstance(WrapperFile, bOptimizeCode, Output);
				Template.Instances.Add(Instance);
			}
			return Instance;
		}

		/// <summary>
		/// Compiles the provided CPP unity files. Will
		/// </summary>
		private CPPOutput CompileUnityFilesWithToolChain(UEToolChain ToolChain, CPPEnvironment CompileEnvironment, CPPEnvironment ModuleCompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph)
		{
			List<FileItem> NormalFiles = new List<FileItem>();
			List<FileItem> AdaptiveFiles = new List<FileItem>();

			if (BuildConfiguration.bAdaptiveUnityDisablesOptimizations && !BuildConfiguration.bStressTestUnity)
			{
				foreach (FileItem File in SourceFiles)
				{
					// Basic check as to whether something in this module is/isn't a unity file...
					if (File.ToString().StartsWith(Unity.ModulePrefix))
					{
						NormalFiles.Add(File);
					}
					else
					{
						AdaptiveFiles.Add(File);
					}
				}
			}
			else
			{
				NormalFiles.AddRange(SourceFiles);
			}

			CPPOutput OutputFiles = new CPPOutput();

			if (NormalFiles.Count > 0)
			{
				OutputFiles = ToolChain.CompileCPPFiles(CompileEnvironment, NormalFiles, Name, ActionGraph);
			}

			if (AdaptiveFiles.Count > 0)
			{
				// Create an unoptmized compilation environment. Need to turn of PCH due to different
				// compiler settings
				CPPEnvironment UnoptimziedEnvironment = ModuleCompileEnvironment.DeepCopy();
				UnoptimziedEnvironment.Config.bOptimizeCode = false;
				UnoptimziedEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.None;

				// Compile the files
				CPPOutput AdaptiveOutput = ToolChain.CompileCPPFiles(UnoptimziedEnvironment, AdaptiveFiles, Name, ActionGraph);

				// Merge output
				OutputFiles.ObjectFiles.AddRange(AdaptiveOutput.ObjectFiles);
				OutputFiles.DebugDataFiles.AddRange(AdaptiveOutput.DebugDataFiles);
			}

			return OutputFiles;
		}

		/// <summary>
		/// Create a header file containing the module definitions, which also includes the PCH itself. Including through another file is necessary on Clang, since we get warnings about #pragma once otherwise, but it 
		/// also allows us to consistently define the preprocessor state on all platforms.
		/// </summary>
		/// <param name="OutputFile">The output file to create</param>
		/// <param name="Definitions">Definitions required by the PCH</param>
		/// <param name="IncludedFile">The PCH file to include</param>
		/// <returns>FileItem for the created file</returns>
		static FileItem CreatePCHWrapperFile(FileReference OutputFile, IEnumerable<string> Definitions, FileItem IncludedFile)
		{
			// Build the contents of the wrapper file
			StringBuilder WrapperContents = new StringBuilder();
			using (StringWriter Writer = new StringWriter(WrapperContents))
			{
				Writer.WriteLine("// PCH for {0}", IncludedFile.AbsolutePath);
				Writer.WriteLine("// Last updated at {0}", IncludedFile.LastWriteTime);
				WriteDefinitions(Definitions, Writer);
				Writer.WriteLine("#include \"{0}\"", IncludedFile.AbsolutePath);
			}

			// Create the item
			FileItem WrapperFile = FileItem.CreateIntermediateTextFile(OutputFile, WrapperContents.ToString());
			WrapperFile.CachedCPPIncludeInfo = IncludedFile.CachedCPPIncludeInfo;
			return WrapperFile;
		}

		/// <summary>
		/// Write a list of macro definitions to an output file
		/// </summary>
		/// <param name="Definitions">List of definitions</param>
		/// <param name="Writer">Writer to receive output</param>
		static void WriteDefinitions(IEnumerable<string> Definitions, TextWriter Writer)
		{
			foreach(string Definition in Definitions)
			{
				int EqualsIdx = Definition.IndexOf('=');
				if(EqualsIdx == -1)
				{
					Writer.WriteLine("#define {0} 1", Definition);
				}
				else
				{
					Writer.WriteLine("#define {0} {1}", Definition.Substring(0, EqualsIdx), Definition.Substring(EqualsIdx + 1));
				}
			}
		}

		public static FileItem CachePCHUsageForModuleSourceFile(CPPEnvironment ModuleCompileEnvironment, FileItem CPPFile)
		{
			if (!CPPFile.bExists)
			{
				throw new BuildException("Required source file not found: " + CPPFile.AbsolutePath);
			}

			DateTime PCHCacheTimerStart = DateTime.UtcNow;

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(ModuleCompileEnvironment.Config.Platform);
			List<string> IncludePathsToSearch = ModuleCompileEnvironment.Config.CPPIncludeInfo.GetIncludesPathsToSearch(CPPFile);

			// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
			// for header dependencies
			CPPFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;

			FileItem PCHFile = CachePCHUsageForCPPFile(ModuleCompileEnvironment.Headers, CPPFile, BuildPlatform, IncludePathsToSearch, ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludeFileSearchDictionary);

			if (BuildConfiguration.bPrintPerformanceInfo)
			{
				double PCHCacheTime = (DateTime.UtcNow - PCHCacheTimerStart).TotalSeconds;
				TotalPCHCacheTime += PCHCacheTime;
			}

			return PCHFile;
		}


		public void CachePCHUsageForModuleSourceFiles(CPPHeaders Headers, CPPEnvironment ModuleCompileEnvironment)
		{
			if(Rules == null || Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs || Rules.PrivatePCHHeaderFile != null)
			{
				if(InvalidIncludeDirectiveMessages == null)
				{
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(ModuleCompileEnvironment.Config.Platform);

					// Find all the source files in this module
					List<FileReference> ModuleFiles = SourceFileSearch.FindModuleSourceFiles(RulesFile);

					// Find headers used by the source file.
					Dictionary<string, FileReference> NameToHeaderFile = new Dictionary<string, FileReference>();
					foreach(FileReference ModuleFile in ModuleFiles)
					{
						if(ModuleFile.HasExtension(".h"))
						{
							NameToHeaderFile[ModuleFile.GetFileNameWithoutExtension()] = ModuleFile;
						}
					}

					// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking for header dependencies
					foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)
					{
						CPPFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;
					}

					// Find the directly included files for each source file, and make sure it includes the matching header if possible
					InvalidIncludeDirectiveMessages = new List<string>();
					foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)
					{
						List<DependencyInclude> DirectIncludeFilenames = Headers.GetDirectIncludeDependencies(CPPFile, BuildPlatform, bOnlyCachedDependencies: false);
						if(DirectIncludeFilenames.Count > 0)
						{
							string IncludeName = Path.GetFileNameWithoutExtension(DirectIncludeFilenames[0].IncludeName);
							string ExpectedName = CPPFile.Reference.GetFileNameWithoutExtension();
							if(String.Compare(IncludeName, ExpectedName, StringComparison.InvariantCultureIgnoreCase) != 0)
							{
								FileReference HeaderFile;
								if(NameToHeaderFile.TryGetValue(ExpectedName, out HeaderFile) && !IgnoreMismatchedHeader(ExpectedName))
								{
									InvalidIncludeDirectiveMessages.Add(String.Format("{0}(1): error: Expected {1} to be first header included.", CPPFile.Reference, HeaderFile.GetFileName()));
								}
							}
						}
					}
				}
			}
			else
			{
				if (ProcessedDependencies == null)
				{
					DateTime PCHCacheTimerStart = DateTime.UtcNow;

					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(ModuleCompileEnvironment.Config.Platform);

					bool bFoundAProblemWithPCHs = false;

					FileItem UniquePCH = null;
					foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)	// @todo ubtmake: We're not caching CPPEnvironments for .c/.mm files, etc.  Even though they don't use PCHs, they still have #includes!  This can break dependency checking!
					{
						// Build a single list of include paths to search.
						List<string> IncludePathsToSearch = ModuleCompileEnvironment.Config.CPPIncludeInfo.GetIncludesPathsToSearch(CPPFile);

						// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
						// for header dependencies
						CPPFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;

						// Find headers used by the source file.
						FileItem PCH = UEBuildModuleCPP.CachePCHUsageForCPPFile(Headers, CPPFile, BuildPlatform, IncludePathsToSearch, ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludeFileSearchDictionary);
						if (PCH == null)
						{
							throw new BuildException("Source file \"{0}\" is not including any headers.  We expect all modules to include a header file for precompiled header generation.  Please add an #include statement.", CPPFile.AbsolutePath);
						}

						if (UniquePCH == null)
						{
							UniquePCH = PCH;
						}
						else if (!UniquePCH.Info.Name.Equals(PCH.Info.Name, StringComparison.InvariantCultureIgnoreCase))		// @todo ubtmake: We do a string compare on the file name (not path) here, because sometimes the include resolver will pick an Intermediate copy of a PCH header file and throw off our comparisons
						{
							// OK, looks like we have multiple source files including a different header file first.  We'll keep track of this and print out
							// helpful information afterwards.
							bFoundAProblemWithPCHs = true;
						}
					}

					ProcessedDependencies = new ProcessedDependenciesClass { UniquePCHHeaderFile = UniquePCH };


					if (bFoundAProblemWithPCHs)
					{
						// Map from pch header string to the source files that use that PCH
						Dictionary<FileReference, List<FileItem>> UsageMapPCH = new Dictionary<FileReference, List<FileItem>>();
						foreach (FileItem CPPFile in SourceFilesToBuild.CPPFiles)
						{
							// Create a new entry if not in the pch usage map
							UsageMapPCH.GetOrAddNew(CPPFile.PrecompiledHeaderIncludeFilename).Add(CPPFile);
						}

						if (BuildConfiguration.bPrintDebugInfo)
						{
							Log.TraceVerbose("{0} PCH files for module {1}:", UsageMapPCH.Count, Name);
							int MostFilesIncluded = 0;
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH)
							{
								if (CurPCH.Value.Count > MostFilesIncluded)
								{
									MostFilesIncluded = CurPCH.Value.Count;
								}

								Log.TraceVerbose("   {0}  ({1} files including it: {2}, ...)", CurPCH.Key, CurPCH.Value.Count, CurPCH.Value[0].AbsolutePath);
							}
						}

						if (UsageMapPCH.Count > 1)
						{
							// Keep track of the PCH file that is most used within this module
							FileReference MostFilesAreIncludingPCH = null;
							int MostFilesIncluded = 0;
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH.Where(PCH => PCH.Value.Count > MostFilesIncluded))
							{
								MostFilesAreIncludingPCH = CurPCH.Key;
								MostFilesIncluded = CurPCH.Value.Count;
							}

							// Find all of the files that are not including our "best" PCH header
							StringBuilder FilesNotIncludingBestPCH = new StringBuilder();
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH.Where(PCH => PCH.Key != MostFilesAreIncludingPCH))
							{
								foreach (FileItem SourceFile in CurPCH.Value)
								{
									FilesNotIncludingBestPCH.AppendFormat("{0} (including {1})\n", SourceFile.AbsolutePath, CurPCH.Key);
								}
							}

							// Bail out and let the user know which source files may need to be fixed up
							throw new BuildException(
								"All source files in module \"{0}\" must include the same precompiled header first.  Currently \"{1}\" is included by most of the source files.  The following source files are not including \"{1}\" as their first include:\n\n{2}",
								Name,
								MostFilesAreIncludingPCH,
								FilesNotIncludingBestPCH);
						}
					}

					if (BuildConfiguration.bPrintPerformanceInfo)
					{
						double PCHCacheTime = (DateTime.UtcNow - PCHCacheTimerStart).TotalSeconds;
						TotalPCHCacheTime += PCHCacheTime;
					}
				}
			}
		}

		private bool IgnoreMismatchedHeader(string ExpectedName)
		{
			switch(ExpectedName)
			{
				case "Stats2":
				case "DynamicRHI":
				case "RHICommandList":
				case "RHIUtilities":
					return true;
			}
			switch(Name)
			{
				case "D3D11RHI":
				case "D3D12RHI":
				case "XboxOneD3D11RHI":
				case "VulkanRHI":
				case "OpenGLDrv":
				case "MetalRHI":
				case "PS4RHI":
				case "OnlineSubsystemIOS":
				case "OnlineSubsystemLive":
					return true;
			}
			return false;
		}


		private static FileItem CachePCHUsageForCPPFile(CPPHeaders Headers, FileItem CPPFile, UEBuildPlatform BuildPlatform, List<string> IncludePathsToSearch, Dictionary<string, FileItem> IncludeFileSearchDictionary)
		{
			// @todo ubtmake: We don't really need to scan every file looking for PCH headers, just need one.  The rest is just for error checking.
			// @todo ubtmake: We don't need all of the direct includes either.  We just need the first, unless we want to check for errors.
			List<DependencyInclude> DirectIncludeFilenames = Headers.GetDirectIncludeDependencies(CPPFile, BuildPlatform, bOnlyCachedDependencies: false);
			if (BuildConfiguration.bPrintDebugInfo)
			{
				Log.TraceVerbose("Found direct includes for {0}: {1}", Path.GetFileName(CPPFile.AbsolutePath), string.Join(", ", DirectIncludeFilenames.Select(F => F.IncludeName)));
			}

			if (DirectIncludeFilenames.Count == 0)
			{
				return null;
			}

			DependencyInclude FirstInclude = DirectIncludeFilenames[0];

			// Resolve the PCH header to an absolute path.
			// Check NullOrEmpty here because if the file could not be resolved we need to throw an exception
			if (FirstInclude.IncludeResolvedNameIfSuccessful != null &&
				// ignore any preexisting resolve cache if we are not configured to use it.
				BuildConfiguration.bUseIncludeDependencyResolveCache &&
				// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
				!BuildConfiguration.bTestIncludeDependencyResolveCache)
			{
				CPPFile.PrecompiledHeaderIncludeFilename = FirstInclude.IncludeResolvedNameIfSuccessful;
				return FileItem.GetItemByFileReference(CPPFile.PrecompiledHeaderIncludeFilename);
			}

			// search the include paths to resolve the file.
			FileItem PrecompiledHeaderIncludeFile = CPPHeaders.FindIncludedFile(FirstInclude.IncludeName, !BuildConfiguration.bCheckExternalHeadersForModification, IncludePathsToSearch, IncludeFileSearchDictionary);
			if (PrecompiledHeaderIncludeFile == null)
			{
				throw new BuildException("The first include statement in source file '{0}' is trying to include the file '{1}' as the precompiled header, but that file could not be located in any of the module's include search paths.", CPPFile.AbsolutePath, CPPFile.PrecompiledHeaderIncludeFilename.GetFileName());
			}

			Headers.IncludeDependencyCache.CacheResolvedIncludeFullPath(CPPFile, 0, PrecompiledHeaderIncludeFile.Reference);
			CPPFile.PrecompiledHeaderIncludeFilename = PrecompiledHeaderIncludeFile.Reference;

			return PrecompiledHeaderIncludeFile;
		}

		/// <summary>
		/// Determine whether optimization should be enabled for a given target
		/// </summary>
		/// <param name="Setting">The optimization setting from the rules file</param>
		/// <param name="Configuration">The active target configuration</param>
		/// <param name="bIsEngineModule">Whether the current module is an engine module</param>
		/// <returns>True if optimization should be enabled</returns>
		public static bool ShouldEnableOptimization(ModuleRules.CodeOptimization Setting, UnrealTargetConfiguration Configuration, bool bIsEngineModule)
		{
			switch(Setting)
			{
				case ModuleRules.CodeOptimization.Never:
					return false;
				case ModuleRules.CodeOptimization.Default:
				case ModuleRules.CodeOptimization.InNonDebugBuilds:
					return (Configuration == UnrealTargetConfiguration.Debug)? false : (Configuration != UnrealTargetConfiguration.DebugGame || bIsEngineModule);
				case ModuleRules.CodeOptimization.InShippingBuildsOnly:
					return (Configuration == UnrealTargetConfiguration.Shipping);
				default:
					return true;
			}
		}

		/// <summary>
		/// Creates a compile environment from a base environment based on the module settings.
		/// </summary>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new module compile environment.</returns>
		public CPPEnvironment CreateModuleCompileEnvironment(UEBuildTarget Target, CPPEnvironment BaseCompileEnvironment)
		{
			CPPEnvironment Result = BaseCompileEnvironment.DeepCopy();

			if (Binary == null)
			{
				// Adding this check here as otherwise the call to Binary.Config.IntermediateDirectory will give an 
				// unhandled exception
				throw new BuildException("UEBuildBinary not set up for module {0}", this.ToString());
			}

			// Check if this is an engine module
			bool bIsEngineModule = ModuleDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory);

			// Override compile environment
			Result.Config.bFasterWithoutUnity = Rules.bFasterWithoutUnity;
			Result.Config.bOptimizeCode = ShouldEnableOptimization(Rules.OptimizeCode, Target.Configuration, bIsEngineModule);
			Result.Config.bUseRTTI = Rules.bUseRTTI || UEBuildConfiguration.bForceEnableRTTI; 
			Result.Config.bUseAVX = Rules.bUseAVX;
			Result.Config.bEnableBufferSecurityChecks = Rules.bEnableBufferSecurityChecks;
			Result.Config.MinSourceFilesForUnityBuildOverride = Rules.MinSourceFilesForUnityBuildOverride;
			Result.Config.MinFilesUsingPrecompiledHeaderOverride = Rules.MinFilesUsingPrecompiledHeaderOverride;
			Result.Config.bBuildLocallyWithSNDBS = Rules.bBuildLocallyWithSNDBS;
			Result.Config.bEnableExceptions = Rules.bEnableExceptions;
			Result.Config.bEnableShadowVariableWarning = Rules.bEnableShadowVariableWarnings;
			Result.Config.bUseStaticCRT = (Target.Rules != null && Target.Rules.bUseStaticCRT);
			Result.Config.OutputDirectory = DirectoryReference.Combine(Binary.Config.IntermediateDirectory, Name);
			Result.Config.PCHOutputDirectory = (Result.Config.PCHOutputDirectory == null)? null : DirectoryReference.Combine(Result.Config.PCHOutputDirectory, Name);

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			if (bIsEngineModule)
			{
				Result.Config.Definitions.Add("UE_IS_ENGINE_MODULE=1");
			}

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (Target.Configuration == UnrealTargetConfiguration.DebugGame && !bIsEngineModule)
			{
				Result.Config.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=1");
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

		/// <summary>
		/// Creates a compile environment for a shared PCH from a base environment based on the module settings.
		/// </summary>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new shared PCH compile environment.</returns>
		public CPPEnvironment CreateSharedPCHCompileEnvironment(UEBuildTarget Target, CPPEnvironment BaseCompileEnvironment)
		{
			CPPEnvironment Result = BaseCompileEnvironment.DeepCopy();

			// Use the default optimization setting for 
			bool bIsEngineModule = ModuleDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory);
			Result.Config.bOptimizeCode = ShouldEnableOptimization(ModuleRules.CodeOptimization.Default, Target.Configuration, bIsEngineModule);

			// Override compile environment
			Result.Config.bIsBuildingDLL = !Target.ShouldCompileMonolithic();
			Result.Config.bIsBuildingLibrary = false;
			Result.Config.bUseStaticCRT = (Target.Rules != null && Target.Rules.bUseStaticCRT);
			Result.Config.OutputDirectory = DirectoryReference.Combine(Binary.Config.IntermediateDirectory, Name);
			Result.Config.PCHOutputDirectory = (Result.Config.PCHOutputDirectory == null)? null : DirectoryReference.Combine(Result.Config.PCHOutputDirectory, Name);

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			if (bIsEngineModule)
			{
				Result.Config.Definitions.Add("UE_IS_ENGINE_MODULE=1");
			}

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (Target.Configuration == UnrealTargetConfiguration.DebugGame && !bIsEngineModule)
			{
				Result.Config.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=1");
			}

			// Add the module's private definitions.
			Result.Config.Definitions.AddRange(Definitions);

			// Find all the modules that are part of the public compile environment for this module.
			List<UEBuildModule> Modules = new List<UEBuildModule>();
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);

			// Now set up the compile environment for the modules in the original order that we encountered them
			foreach (UEBuildModule Module in Modules)
			{
				Module.AddModuleToCompileEnvironment(null, ModuleToIncludePathsOnlyFlag[Module], Result.Config.CPPIncludeInfo.IncludePaths, Result.Config.CPPIncludeInfo.SystemIncludePaths, Result.Config.Definitions, Result.Config.AdditionalFrameworks);
			}
			return Result;
		}

		public class UHTModuleInfoCacheType
		{
			public UHTModuleInfoCacheType(IEnumerable<string> InHeaderFilenames, UHTModuleInfo InInfo)
			{
				HeaderFilenames = InHeaderFilenames;
				Info = InInfo;
			}

			public IEnumerable<string> HeaderFilenames = null;
			public UHTModuleInfo Info = null;
		}

		private UHTModuleInfoCacheType UHTModuleInfoCache = null;

		/// Total time spent generating PCHs for modules (not actually compiling, but generating the PCH's input data)
		public static double TotalPCHGenTime = 0.0;

		/// Time spent caching which PCH header is included by each module and source file
		public static double TotalPCHCacheTime = 0.0;


		/// <summary>
		/// If any of this module's source files contain UObject definitions, this will return those header files back to the caller
		/// </summary>
		/// <returns>
		public UHTModuleInfoCacheType GetCachedUHTModuleInfo(EGeneratedCodeVersion GeneratedCodeVersion)
		{
			if (UHTModuleInfoCache == null)
			{
				IEnumerable<string> HeaderFilenames = Directory.GetFiles(ModuleDirectory.FullName, "*.h", SearchOption.AllDirectories);
				UHTModuleInfo Info = ExternalExecution.CreateUHTModuleInfo(HeaderFilenames, Name, ModuleDirectory, Type, GeneratedCodeVersion);
				UHTModuleInfoCache = new UHTModuleInfoCacheType(Info.PublicUObjectHeaders.Concat(Info.PublicUObjectClassesHeaders).Concat(Info.PrivateUObjectHeaders).Select(x => x.AbsolutePath).ToList(), Info);
			}

			return UHTModuleInfoCache;
		}

		public override void GetAllDependencyModules(List<UEBuildModule> ReferencedModules, HashSet<UEBuildModule> IgnoreReferencedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies)
		{
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
			AllDependencyModules.AddRange(PrivateDependencyModules);
			AllDependencyModules.AddRange(PublicDependencyModules);
			if (bIncludeDynamicallyLoaded)
			{
				AllDependencyModules.AddRange(DynamicallyLoadedModules);
				AllDependencyModules.AddRange(PlatformSpecificDynamicallyLoadedModules);
			}

			foreach (UEBuildModule DependencyModule in AllDependencyModules)
			{
				if (!IgnoreReferencedModules.Contains(DependencyModule))
				{
					// Don't follow circular back-references!
					bool bIsCircular = HasCircularDependencyOn(DependencyModule.Name);
					if (bForceCircular || !bIsCircular)
					{
						IgnoreReferencedModules.Add(DependencyModule);

						if (!bOnlyDirectDependencies)
						{
							// Recurse into dependent modules first
							DependencyModule.GetAllDependencyModules(ReferencedModules, IgnoreReferencedModules, bIncludeDynamicallyLoaded, bForceCircular, bOnlyDirectDependencies);
						}

						ReferencedModules.Add(DependencyModule);
					}
				}
			}
		}

		public override void RecursivelyAddPrecompiledModules(List<UEBuildModule> Modules)
		{
			if (!Modules.Contains(this))
			{
				Modules.Add(this);

				// Get the dependent modules
				List<UEBuildModule> DependentModules = new List<UEBuildModule>();
				if (PrivateDependencyModules != null)
				{
					DependentModules.AddRange(PrivateDependencyModules);
				}
				if (PublicDependencyModules != null)
				{
					DependentModules.AddRange(PublicDependencyModules);
				}
				if (DynamicallyLoadedModules != null)
				{
					DependentModules.AddRange(DynamicallyLoadedModules);
				}
				if (PlatformSpecificDynamicallyLoadedModules != null)
				{
					DependentModules.AddRange(PlatformSpecificDynamicallyLoadedModules);
				}

				// Find modules for each of them, and add their dependencies too
				foreach (UEBuildModule DependentModule in DependentModules)
				{
					DependentModule.RecursivelyAddPrecompiledModules(Modules);
				}
			}
		}
	}

	public class UEBuildFramework
	{
		public UEBuildFramework(string InFrameworkName)
		{
			FrameworkName = InFrameworkName;
		}

		public UEBuildFramework(string InFrameworkName, string InFrameworkZipPath)
		{
			FrameworkName = InFrameworkName;
			FrameworkZipPath = InFrameworkZipPath;
		}

		public UEBuildFramework(string InFrameworkName, string InFrameworkZipPath, string InCopyBundledAssets)
		{
			FrameworkName = InFrameworkName;
			FrameworkZipPath = InFrameworkZipPath;
			CopyBundledAssets = InCopyBundledAssets;
		}

		public UEBuildModule OwningModule = null;
		public string FrameworkName = null;
		public string FrameworkZipPath = null;
		public string CopyBundledAssets = null;
	}

	public class UEBuildBundleResource
	{
		public UEBuildBundleResource(string InResourcePath, string InBundleContentsSubdir = "Resources", bool bInShouldLog = true)
		{
			ResourcePath = InResourcePath;
			BundleContentsSubdir = InBundleContentsSubdir;
			bShouldLog = bInShouldLog;
		}

		public string ResourcePath = null;
		public string BundleContentsSubdir = null;
		public bool bShouldLog = true;
	}

	/// <summary>
	/// Information about a PCH instance
	/// </summary>
	public class PrecompiledHeaderInstance
	{
		/// <summary>
		/// The file to include to use this shared PCH
		/// </summary>
		public FileItem HeaderFile;

		/// <summary>
		/// Whether optimization is enabled
		/// </summary>
		public bool bOptimizeCode;

		/// <summary>
		/// The output files for the shared PCH
		/// </summary>
		public CPPOutput Output;

		/// <summary>
		/// Constructor
		/// </summary>
		public PrecompiledHeaderInstance(FileItem HeaderFile, bool bOptimizeCode, CPPOutput Output)
		{
			this.HeaderFile = HeaderFile;
			this.bOptimizeCode = bOptimizeCode;
			this.Output = Output;
		}

		/// <summary>
		/// Return a string representation of this object for debugging
		/// </summary>
		/// <returns>String representation of the object</returns>
		public override string ToString()
		{
			return String.Format("{0} (Optimized={1})", HeaderFile.Reference.GetFileName(), bOptimizeCode);
		}
	}

	/// <summary>
	/// A template for creating a shared PCH. Instances of it are created depending on the configurations required.
	/// </summary>
	public class PrecompiledHeaderTemplate
	{
		/// <summary>
		/// Module providing this PCH.
		/// </summary>
		public UEBuildModuleCPP Module;

		/// <summary>
		/// The base compile environment, including all the public compile environment that all consuming modules inherit.
		/// </summary>
		public CPPEnvironment BaseCompileEnvironment;

		/// <summary>
		/// The header file 
		/// </summary>
		public FileItem HeaderFile;

		/// <summary>
		/// Output directory for instances of this PCH.
		/// </summary>
		public DirectoryReference OutputDir;

		/// <summary>
		/// Instances of this PCH
		/// </summary>
		public List<PrecompiledHeaderInstance> Instances = new List<PrecompiledHeaderInstance>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Module">The module with a valid shared PCH</param>
		public PrecompiledHeaderTemplate(UEBuildModuleCPP Module, CPPEnvironment BaseCompileEnvironment, FileItem HeaderFile, DirectoryReference OutputDir)
		{
			this.Module = Module;
			this.BaseCompileEnvironment = BaseCompileEnvironment;
			this.HeaderFile = HeaderFile;
			this.OutputDir = OutputDir;
		}

		/// <summary>
		/// Checks whether this template is valid for the given compile environment
		/// </summary>
		/// <param name="CompileEnvironment">Compile environment to check with</param>
		/// <returns>True if the template is compatible with the given compile environment</returns>
		public bool IsValidFor(CPPEnvironment CompileEnvironment)
		{
			if(CompileEnvironment.Config.bIsBuildingDLL != BaseCompileEnvironment.Config.bIsBuildingDLL)
			{
				return false;
			}
			if(CompileEnvironment.Config.bIsBuildingLibrary != BaseCompileEnvironment.Config.bIsBuildingLibrary)
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Return a string representation of this object for debugging
		/// </summary>
		/// <returns>String representation of the object</returns>
		public override string ToString()
		{
			return HeaderFile.AbsolutePath;
		}
	}
}
