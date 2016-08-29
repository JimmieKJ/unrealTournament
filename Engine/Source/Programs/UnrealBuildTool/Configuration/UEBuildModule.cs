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
			if (Binary == null)
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
		public abstract List<FileItem> Compile(UEBuildTarget Target, UEToolChain ToolChain, CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment);

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
		public override List<FileItem> Compile(UEBuildTarget Target, UEToolChain ToolChain, CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment)
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
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(UEBuildTarget Target, UEToolChain ToolChain, CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);

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

			// For an executable or a static library do not use the default RC file - 
			// If the executable wants it, it will be in their source list anyway.
			// The issue here is that when making a monolithic game, the processing
			// of the other game modules will stomp the game-specific rc file.
			if (Binary.Config.Type == UEBuildBinaryType.DynamicLinkLibrary)
			{
				// Add default PCLaunch.rc file if this module has no own resource file specified
				if (SourceFilesToBuild.RCFiles.Count <= 0)
				{
					FileReference DefRC = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Runtime", "Launch", "Resources", "Windows", "PCLaunch.rc");
					FileItem Item = FileItem.GetItemByFileReference(DefRC);
					SourceFilesToBuild.RCFiles.Add(Item);
				}

				// Always compile in the API version resource separately. This is required for the module manager to detect compatible API versions.
				FileReference ModuleVersionRC = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Runtime", "Core", "Resources", "Windows", "ModuleVersionResource.rc.inl");
				FileItem ModuleVersionItem = FileItem.GetItemByFileReference(ModuleVersionRC);
				if (!SourceFilesToBuild.RCFiles.Contains(ModuleVersionItem))
				{
					SourceFilesToBuild.RCFiles.Add(ModuleVersionItem);
				}
			}


			{
				// Process all of the header file dependencies for this module
				this.CachePCHUsageForModuleSourceFiles(Target, ModuleCompileEnvironment);

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

			// The environment with which to compile the CPP files
			CPPEnvironment CPPCompileEnvironment = ModuleCompileEnvironment;

			// Precompiled header support.
			bool bWasModuleCodeCompiled = false;
			if (BuildPlatform.ShouldUsePCHFiles(CompileEnvironment.Config.Target.Platform, CompileEnvironment.Config.Target.Configuration))
			{
				DateTime PCHGenTimerStart = DateTime.UtcNow;

				// The code below will figure out whether this module will either use a "unique PCH" (private PCH that will only be included by
				// this module's code files), or a "shared PCH" (potentially included by many code files in many modules.)  Only one or the other
				// will be used.
				FileItem SharedPCHHeaderFile = null;

				// In the case of a shared PCH, we also need to keep track of which module that PCH's header file is a member of
				string SharedPCHModuleName = String.Empty;

				if (BuildConfiguration.bUseSharedPCHs && CompileEnvironment.Config.bIsBuildingLibrary)
				{
					Log.TraceVerbose("Module '{0}' was not allowed to use Shared PCHs, because we're compiling to a library", this.Name);
				}

				bool bUseSharedPCHFiles = BuildConfiguration.bUseSharedPCHs && !CompileEnvironment.Config.bIsBuildingLibrary && GlobalCompileEnvironment.SharedPCHHeaderFiles.Count > 0;

				if (bUseSharedPCHFiles)
				{
					FileReference SharingPCHHeaderFilePath = null;
					bool bIsASharedPCHModule = bUseSharedPCHFiles && GlobalCompileEnvironment.SharedPCHHeaderFiles.Any(PCH => PCH.Module == this);
					if (bIsASharedPCHModule)
					{
						SharingPCHHeaderFilePath = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, Rules.SharedPCHHeaderFile);
					}

					// We can't use a shared PCH file when compiling a module
					// with exports, because the shared PCH can only have imports in it to work correctly.
					bool bAllowSharedPCH = (Rules.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs) ? false : true;
					bool bCanModuleUseOwnSharedPCH = bAllowSharedPCH && bIsASharedPCHModule && !Binary.Config.bAllowExports && ProcessedDependencies.UniquePCHHeaderFile.Reference == SharingPCHHeaderFilePath;
					if (bAllowSharedPCH && (!bIsASharedPCHModule || bCanModuleUseOwnSharedPCH))
					{
						// Figure out which shared PCH tier we're in
						List<UEBuildModule> ReferencedModules = new List<UEBuildModule>();
						{
							this.GetAllDependencyModules(ReferencedModules, new HashSet<UEBuildModule>(), bIncludeDynamicallyLoaded: false, bForceCircular: false, bOnlyDirectDependencies: true);
						}

						int LargestSharedPCHHeaderFileIndex = -1;
						foreach (UEBuildModule DependencyModule in ReferencedModules)
						{
							// These Shared PCHs are ordered from least complex to most complex.  We'll start at the last one and search backwards.
							for (int SharedPCHHeaderFileIndex = GlobalCompileEnvironment.SharedPCHHeaderFiles.Count - 1; SharedPCHHeaderFileIndex > LargestSharedPCHHeaderFileIndex; --SharedPCHHeaderFileIndex)
							{
								SharedPCHHeaderInfo CurSharedPCHHeaderFile = GlobalCompileEnvironment.SharedPCHHeaderFiles[SharedPCHHeaderFileIndex];

								if (DependencyModule == CurSharedPCHHeaderFile.Module ||
									(bIsASharedPCHModule && CurSharedPCHHeaderFile.Module == this))	// If we ourselves are a shared PCH module, always at least use our own module as our shared PCH header if we can't find anything better
								{
									SharedPCHModuleName = CurSharedPCHHeaderFile.Module.Name;
									SharedPCHHeaderFile = CurSharedPCHHeaderFile.PCHHeaderFile;
									LargestSharedPCHHeaderFileIndex = SharedPCHHeaderFileIndex;
									break;
								}
							}

							if (LargestSharedPCHHeaderFileIndex == GlobalCompileEnvironment.SharedPCHHeaderFiles.Count - 1)
							{
								// We've determined that the module is using our most complex PCH header, so we can early-out
								break;
							}
						}

						// Did we not find a shared PCH header that is being included by this module?  This could happen if the module is not including Core.h, even indirectly.
						if (String.IsNullOrEmpty(SharedPCHModuleName))
						{
							throw new BuildException("Module {0} doesn't use a Shared PCH!  Please add a dependency on a Shared PCH module to this module's dependency list", this.Name);
						}

						// Keep track of how many modules make use of this PCH for performance diagnostics
						SharedPCHHeaderInfo LargestSharedPCHHeader = GlobalCompileEnvironment.SharedPCHHeaderFiles[LargestSharedPCHHeaderFileIndex];
						++LargestSharedPCHHeader.NumModulesUsingThisPCH;

						// Don't allow game modules to use engine PCHs in DebugGame - the optimization settings aren't correct. 
						// @todo: we should be creating shared PCHs ahead of time, and only using them if our settings match. as it is, the first modules compiled
						// (which are currently plugins) get to call the shots for how the shared PCH gets built, and that might be a game plugin built in debug...
						if(Target.Configuration == UnrealTargetConfiguration.DebugGame && SharedPCHHeaderFile.Reference.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && !RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
						{
							SharedPCHModuleName = null;
							SharedPCHHeaderFile = null;
						}
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
				if (SharedPCHHeaderFile != null || SourceFilesToBuild.CPPFiles.Count >= MinFilesUsingPrecompiledHeader)
				{
					FileItem PCHToUse;

					if (SharedPCHHeaderFile != null)
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
						foreach (FileItem CPPFile in SourceFilesToBuild.CPPFiles)
						{
							CPPFile.PCHHeaderNameInCode = PCHToUse.AbsolutePath;
							CPPFile.PrecompiledHeaderIncludeFilename = PCHToUse.Reference;
						}
					}

					// A shared PCH was not already set up for us, so set one up.
					if (ModulePCHEnvironment == null && SourceFilesToBuild.CPPFiles.Count > 0)
					{
						FileItem PCHHeaderFile = ProcessedDependencies.UniquePCHHeaderFile;
						string PCHModuleName = this.Name;
						if (SharedPCHHeaderFile != null)
						{
							PCHHeaderFile = SharedPCHHeaderFile;
							PCHModuleName = SharedPCHModuleName;
						}
						string PCHHeaderNameInCode = SourceFilesToBuild.CPPFiles[0].PCHHeaderNameInCode;

						ModulePCHEnvironment = new PrecompileHeaderEnvironment(PCHModuleName, PCHHeaderNameInCode, PCHHeaderFile, ModuleCompileEnvironment.Config.CLRMode, ModuleCompileEnvironment.Config.OptimizeCode);

						if (SharedPCHHeaderFile != null)
						{
							// Add to list of shared PCH environments
							GlobalCompileEnvironment.SharedPCHEnvironments.Add(ModulePCHEnvironment);
							Log.TraceVerbose("Module " + Name + " uses new Shared PCH '" + ModulePCHEnvironment.PrecompiledHeaderIncludeFilename + "'");
						}
						else
						{
							Log.TraceVerbose("Module " + Name + " uses a Unique PCH '" + ModulePCHEnvironment.PrecompiledHeaderIncludeFilename + "'");
						}
					}
				}
				else
				{
					Log.TraceVerbose("Module " + Name + " doesn't use a Shared PCH, and only has " + SourceFilesToBuild.CPPFiles.Count.ToString() + " source file(s).  No Unique PCH will be generated.");
				}

				// Compile the C++ source or the unity C++ files that use a PCH environment.
				if (ModulePCHEnvironment != null)
				{
					// Setup a new compile environment for this module's source files.  It's pretty much the exact same as the
					// module's compile environment, except that it will include a PCH file.

					CPPEnvironment ModulePCHCompileEnvironment = ModuleCompileEnvironment.DeepCopy();
					ModulePCHCompileEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					ModulePCHCompileEnvironment.Config.PrecompiledHeaderIncludeFilename = ModulePCHEnvironment.PrecompiledHeaderIncludeFilename.Reference;
					ModulePCHCompileEnvironment.Config.PCHHeaderNameInCode = ModulePCHEnvironment.PCHHeaderNameInCode;

					if (SharedPCHHeaderFile != null)
					{
						// Shared PCH headers need to be force included, because we're basically forcing the module to use
						// the precompiled header that we want, instead of the "first include" in each respective .cpp file
						ModulePCHCompileEnvironment.Config.bForceIncludePrecompiledHeader = true;
					}

					List<FileItem> CPPFilesToBuild = SourceFilesToBuild.CPPFiles;
					if (bModuleUsesUnityBuild)
					{
						// unity files generated for only the set of files which share the same PCH environment
						CPPFilesToBuild = Unity.GenerateUnityCPPs(ToolChain, Target, CPPFilesToBuild, ModulePCHCompileEnvironment, Name);
					}

					// Check if there are enough unity files to warrant pch generation (and we haven't already generated the shared one)
					if (ModulePCHEnvironment.PrecompiledHeaderFile == null)
					{
						if (SharedPCHHeaderFile != null || CPPFilesToBuild.Count >= MinFilesUsingPrecompiledHeader)
						{
                            CPPOutput PCHOutput;
							if (SharedPCHHeaderFile == null)
							{
								PCHOutput = PrecompileHeaderEnvironment.GeneratePCHCreationAction(
									ToolChain,
									Target,
									CPPFilesToBuild[0].PCHHeaderNameInCode,
									ModulePCHEnvironment.PrecompiledHeaderIncludeFilename,
									ModuleCompileEnvironment,
									ModuleCompileEnvironment.Config.OutputDirectory,
									ModuleCompileEnvironment.Config.PCHOutputDirectory,
									Name,
									true);
							}
							else
							{
								UEBuildModuleCPP SharedPCHModule = (UEBuildModuleCPP)Target.FindOrCreateModuleByName(SharedPCHModuleName);

								CPPEnvironment SharedPCHCompileEnvironment = GlobalCompileEnvironment.DeepCopy();
								SharedPCHCompileEnvironment.Config.bEnableShadowVariableWarning = SharedPCHModule.Rules.bEnableShadowVariableWarnings;

								List<UEBuildModule> Modules = new List<UEBuildModule>();
								Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
								SharedPCHModule.FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);

								foreach (UEBuildModule Module in Modules)
								{
									Module.AddModuleToCompileEnvironment(
										Binary,
										ModuleToIncludePathsOnlyFlag[Module],
										SharedPCHCompileEnvironment.Config.CPPIncludeInfo.IncludePaths,
										SharedPCHCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths,
										SharedPCHCompileEnvironment.Config.Definitions,
										SharedPCHCompileEnvironment.Config.AdditionalFrameworks);
								}
                                
								PCHOutput = PrecompileHeaderEnvironment.GeneratePCHCreationAction(
									ToolChain,
									Target,
									CPPFilesToBuild[0].PCHHeaderNameInCode,
									ModulePCHEnvironment.PrecompiledHeaderIncludeFilename,
									SharedPCHCompileEnvironment,
									DirectoryReference.Combine(CompileEnvironment.Config.OutputDirectory, "SharedPCHs"),
									(CompileEnvironment.Config.PCHOutputDirectory == null)? null : DirectoryReference.Combine(CompileEnvironment.Config.PCHOutputDirectory, "SharedPCHs"),
									"Shared",
									false);
							}

							ModulePCHEnvironment.PrecompiledHeaderFile = PCHOutput.PrecompiledHeaderFile;

							ModulePCHEnvironment.OutputObjectFiles.Clear();
							ModulePCHEnvironment.OutputObjectFiles.AddRange(PCHOutput.ObjectFiles);
						}
						else if (CPPFilesToBuild.Count < MinFilesUsingPrecompiledHeader)
						{
							Log.TraceVerbose("Module " + Name + " doesn't use a Shared PCH, and only has " + CPPFilesToBuild.Count.ToString() + " unity source file(s).  No Unique PCH will be generated.");
						}
					}

					if (ModulePCHEnvironment.PrecompiledHeaderFile != null)
					{
						// Link in the object files produced by creating the precompiled header.
						LinkInputFiles.AddRange(ModulePCHEnvironment.OutputObjectFiles);

						// if pch action was generated for the environment then use pch
						ModulePCHCompileEnvironment.PrecompiledHeaderFile = ModulePCHEnvironment.PrecompiledHeaderFile;

						// Use this compile environment from now on
						CPPCompileEnvironment = ModulePCHCompileEnvironment;
					}

					LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(Target, CPPCompileEnvironment, CPPFilesToBuild, Name).ObjectFiles);
					bWasModuleCodeCompiled = true;
				}

				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					double PCHGenTime = (DateTime.UtcNow - PCHGenTimerStart).TotalSeconds;
					TotalPCHGenTime += PCHGenTime;
				}
			}

			if (!bWasModuleCodeCompiled && SourceFilesToBuild.CPPFiles.Count > 0)
			{
				List<FileItem> CPPFilesToCompile = SourceFilesToBuild.CPPFiles;
				if (bModuleUsesUnityBuild)
				{
					CPPFilesToCompile = Unity.GenerateUnityCPPs(ToolChain, Target, CPPFilesToCompile, CPPCompileEnvironment, Name);
				}
				LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(Target, CPPCompileEnvironment, CPPFilesToCompile, Name).ObjectFiles);
			}

			if (AutoGenerateCppInfo != null && AutoGenerateCppInfo.BuildInfo != null && !CPPCompileEnvironment.bHackHeaderGenerator)
			{
				string[] GeneratedFiles = Directory.GetFiles(Path.GetDirectoryName(AutoGenerateCppInfo.BuildInfo.FileWildcard), Path.GetFileName(AutoGenerateCppInfo.BuildInfo.FileWildcard));
				foreach (string GeneratedFilename in GeneratedFiles)
				{
					FileItem GeneratedCppFileItem = FileItem.GetItemByPath(GeneratedFilename);

					CachePCHUsageForModuleSourceFile(Target, CPPCompileEnvironment, GeneratedCppFileItem);

					// @todo ubtmake: Check for ALL other places where we might be injecting .cpp or .rc files for compiling without caching CachedCPPIncludeInfo first (anything platform specific?)
					LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(Target, CPPCompileEnvironment, new List<FileItem> { GeneratedCppFileItem }, Name).ObjectFiles);
				}
			}

			// Compile C files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(Target, CPPCompileEnvironment, SourceFilesToBuild.CFiles, Name).ObjectFiles);

			// Compile CC files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(Target, CPPCompileEnvironment, SourceFilesToBuild.CCFiles, Name).ObjectFiles);

			// Compile MM files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(Target, CPPCompileEnvironment, SourceFilesToBuild.MMFiles, Name).ObjectFiles);

			// Compile RC files.
			LinkInputFiles.AddRange(ToolChain.CompileRCFiles(Target, CPPCompileEnvironment, SourceFilesToBuild.RCFiles).ObjectFiles);

			return LinkInputFiles;
		}

		private PrecompileHeaderEnvironment ApplySharedPCH(CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment, CPPEnvironment ModuleCompileEnvironment, List<FileItem> CPPFiles, ref FileItem SharedPCHHeaderFile)
		{
			// Check to see if we have a PCH header already setup that we can use
			FileItem SharedPCHHeaderFileCopy = SharedPCHHeaderFile;
			PrecompileHeaderEnvironment SharedPCHEnvironment = GlobalCompileEnvironment.SharedPCHEnvironments.Find(Env => Env.PrecompiledHeaderIncludeFilename == SharedPCHHeaderFileCopy);
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
			if (Rules.bUseRTTI)
			{
				Log.TraceVerbose("Module {0} cannot use existing Shared PCH '{1}' (from module '{2}') because RTTI modes don't match", Name, SharedPCHEnvironment.PrecompiledHeaderIncludeFilename.AbsolutePath, SharedPCHEnvironment.ModuleName);
				SharedPCHHeaderFile = null;
				return null;
			}

			// Don't mix non-optimized code with optimized code (PCHs won't be compatible)
			ModuleRules.CodeOptimization SharedPCHCodeOptimization = SharedPCHEnvironment.OptimizeCode;
			ModuleRules.CodeOptimization ModuleCodeOptimization = ModuleCompileEnvironment.Config.OptimizeCode;

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
			if (!CPPFile.bExists)
			{
				throw new BuildException("Required source file not found: " + CPPFile.AbsolutePath);
			}

			DateTime PCHCacheTimerStart = DateTime.UtcNow;

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(ModuleCompileEnvironment.Config.Target.Platform);
			List<string> IncludePathsToSearch = ModuleCompileEnvironment.Config.CPPIncludeInfo.GetIncludesPathsToSearch(CPPFile);

			// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
			// for header dependencies
			CPPFile.CachedCPPIncludeInfo = ModuleCompileEnvironment.Config.CPPIncludeInfo;

			FileItem PCHFile = CachePCHUsageForCPPFile(Target, CPPFile, BuildPlatform, IncludePathsToSearch, ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludeFileSearchDictionary);

			if (BuildConfiguration.bPrintPerformanceInfo)
			{
				double PCHCacheTime = (DateTime.UtcNow - PCHCacheTimerStart).TotalSeconds;
				TotalPCHCacheTime += PCHCacheTime;
			}

			return PCHFile;
		}


		public void CachePCHUsageForModuleSourceFiles(UEBuildTarget Target, CPPEnvironment ModuleCompileEnvironment)
		{
			if (ProcessedDependencies == null)
			{
				DateTime PCHCacheTimerStart = DateTime.UtcNow;

				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(ModuleCompileEnvironment.Config.Target.Platform);

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
					FileItem PCH = UEBuildModuleCPP.CachePCHUsageForCPPFile(Target, CPPFile, BuildPlatform, IncludePathsToSearch, ModuleCompileEnvironment.Config.CPPIncludeInfo.IncludeFileSearchDictionary);
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



		private static FileItem CachePCHUsageForCPPFile(UEBuildTarget Target, FileItem CPPFile, UEBuildPlatform BuildPlatform, List<string> IncludePathsToSearch, Dictionary<string, FileItem> IncludeFileSearchDictionary)
		{
			// @todo ubtmake: We don't really need to scan every file looking for PCH headers, just need one.  The rest is just for error checking.
			// @todo ubtmake: We don't need all of the direct includes either.  We just need the first, unless we want to check for errors.
			List<DependencyInclude> DirectIncludeFilenames = CPPEnvironment.GetDirectIncludeDependencies(Target, CPPFile, BuildPlatform, bOnlyCachedDependencies: false);
			if (BuildConfiguration.bPrintDebugInfo)
			{
				Log.TraceVerbose("Found direct includes for {0}: {1}", Path.GetFileName(CPPFile.AbsolutePath), string.Join(", ", DirectIncludeFilenames.Select(F => F.IncludeName)));
			}

			if (DirectIncludeFilenames.Count == 0)
			{
				return null;
			}

			DependencyInclude FirstInclude = DirectIncludeFilenames[0];

			// The pch header should always be the first include in the source file.
			// NOTE: This is not an absolute path.  This is just the literal include string from the source file!
			CPPFile.PCHHeaderNameInCode = FirstInclude.IncludeName;

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
			FileItem PrecompiledHeaderIncludeFile = CPPEnvironment.FindIncludedFile(CPPFile.PCHHeaderNameInCode, !BuildConfiguration.bCheckExternalHeadersForModification, IncludePathsToSearch, IncludeFileSearchDictionary);
			if (PrecompiledHeaderIncludeFile == null)
			{
				throw new BuildException("The first include statement in source file '{0}' is trying to include the file '{1}' as the precompiled header, but that file could not be located in any of the module's include search paths.", CPPFile.AbsolutePath, CPPFile.PCHHeaderNameInCode);
			}

			CPPEnvironment.IncludeDependencyCache[Target].CacheResolvedIncludeFullPath(CPPFile, 0, PrecompiledHeaderIncludeFile.Reference);
			CPPFile.PrecompiledHeaderIncludeFilename = PrecompiledHeaderIncludeFile.Reference;

			return PrecompiledHeaderIncludeFile;
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

			// Override compile environment
			Result.Config.bFasterWithoutUnity = Rules.bFasterWithoutUnity;
			Result.Config.OptimizeCode = Rules.OptimizeCode;
			Result.Config.bUseRTTI = Rules.bUseRTTI;
			Result.Config.bUseAVX = Rules.bUseAVX;
			Result.Config.bEnableBufferSecurityChecks = Rules.bEnableBufferSecurityChecks;
			Result.Config.MinSourceFilesForUnityBuildOverride = Rules.MinSourceFilesForUnityBuildOverride;
			Result.Config.MinFilesUsingPrecompiledHeaderOverride = Rules.MinFilesUsingPrecompiledHeaderOverride;
			Result.Config.bBuildLocallyWithSNDBS = Rules.bBuildLocallyWithSNDBS;
			Result.Config.bEnableExceptions = Rules.bEnableExceptions;
			Result.Config.bEnableShadowVariableWarning = Rules.bEnableShadowVariableWarnings;
			Result.Config.bUseStaticCRT = (Target.Rules != null && Target.Rules.bUseStaticCRT);
			Result.Config.OutputDirectory = DirectoryReference.Combine(Binary.Config.IntermediateDirectory, Name);

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (Target.Configuration == UnrealTargetConfiguration.DebugGame)
			{
				if (!ModuleDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
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

	/// <summary>
	/// A module that is compiled from C++ CLR code.
	/// </summary>
	class UEBuildModuleCPPCLR : UEBuildModuleCPP
	{
		/// <summary>
		/// The assemblies referenced by the module's private implementation.
		/// </summary>
		HashSet<string> PrivateAssemblyReferences;

		public UEBuildModuleCPPCLR(
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
			: base(InName, InType, InModuleDirectory, InGeneratedCodeDirectory, InIntelliSenseGatherer,
			InSourceFiles, InRules,
			bInBuildSourceFiles, InRulesFile)
		{
			PrivateAssemblyReferences = HashSetFromOptionalEnumerableStringParameter(InRules.PrivateAssemblyReferences);
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(UEBuildTarget Target, UEToolChain ToolChain, CPPEnvironment GlobalCompileEnvironment, CPPEnvironment CompileEnvironment)
		{
			CPPEnvironment ModuleCLREnvironment = CompileEnvironment.DeepCopy();

			// Setup the module environment for the project CLR mode
			ModuleCLREnvironment.Config.CLRMode = CPPCLRMode.CLREnabled;

			// Add the private assembly references to the compile environment.
			foreach (string PrivateAssemblyReference in PrivateAssemblyReferences)
			{
				ModuleCLREnvironment.AddPrivateAssembly(PrivateAssemblyReference);
			}

			// Pass the CLR compilation environment to the standard C++ module compilation code.
			return base.Compile(Target, ToolChain, GlobalCompileEnvironment, ModuleCLREnvironment);
		}

		public override void SetupPrivateLinkEnvironment(
			UEBuildBinary SourceBinary,
			LinkEnvironment LinkEnvironment,
			List<UEBuildBinary> BinaryDependencies,
			HashSet<UEBuildModule> VisitedModules
			)
		{
			base.SetupPrivateLinkEnvironment(SourceBinary, LinkEnvironment, BinaryDependencies, VisitedModules);

			// Setup the link environment for linking a CLR binary.
			LinkEnvironment.Config.CLRMode = CPPCLRMode.CLREnabled;
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

	public class PrecompileHeaderEnvironment
	{
		/// <summary>
		/// The name of the module this PCH header is a member of
		/// </summary>
		public readonly string ModuleName;

		/// <summary>
		/// PCH header file name as it appears in an #include statement in source code (might include partial, or no relative path.)
		/// This is needed by some compilers to use PCH features.
		/// </summary>
		public string PCHHeaderNameInCode;

		/// <summary>
		/// The source header file that this precompiled header will be generated for
		/// </summary>
		public readonly FileItem PrecompiledHeaderIncludeFilename;

		/// <summary>
		/// Whether this precompiled header will be built with CLR features enabled.  We won't mix and match CLR PCHs with non-CLR PCHs
		/// </summary>
		public readonly CPPCLRMode CLRMode;

		/// <summary>
		/// Whether this precompiled header will be built with code optimization enabled.
		/// </summary>
		public readonly ModuleRules.CodeOptimization OptimizeCode;

		/// <summary>
		/// The PCH file we're generating
		/// </summary>
		public FileItem PrecompiledHeaderFile = null;

		/// <summary>
		/// Object files emitted from the compiler when generating this precompiled header.  These will be linked into modules that
		/// include this PCH
		/// </summary>
		public readonly List<FileItem> OutputObjectFiles = new List<FileItem>();

		public PrecompileHeaderEnvironment(string InitModuleName, string InitPCHHeaderNameInCode, FileItem InitPrecompiledHeaderIncludeFilename, CPPCLRMode InitCLRMode, ModuleRules.CodeOptimization InitOptimizeCode)
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
		public static CPPOutput GeneratePCHCreationAction(UEToolChain ToolChain, UEBuildTarget Target, string PCHHeaderNameInCode, FileItem PrecompiledHeaderIncludeFilename, CPPEnvironment ProjectCPPEnvironment, DirectoryReference OutputDirectory, DirectoryReference PCHOutputDirectory, string ModuleName, bool bAllowDLLExports)
		{
			// Find the header file to be precompiled. Don't skip external headers
			if (PrecompiledHeaderIncludeFilename.bExists)
			{
				// Create a Dummy wrapper around the PCH to avoid problems with #pragma once on clang
				string PCHGuardDefine = Path.GetFileNameWithoutExtension(PrecompiledHeaderIncludeFilename.AbsolutePath).ToUpperInvariant();
				string LocalPCHHeaderNameInCode = ToolChain.ConvertPath(PrecompiledHeaderIncludeFilename.AbsolutePath);
				string TmpPCHHeaderContents = String.Format("#ifndef __AUTO_{0}_H__\n#define __AUTO_{0}_H__\n//Last Write: {2}\n#include \"{1}\"\n#endif//__AUTO_{0}_H__", PCHGuardDefine, LocalPCHHeaderNameInCode, PrecompiledHeaderIncludeFilename.LastWriteTime);
				FileReference DummyPath = FileReference.Combine(
					ProjectCPPEnvironment.Config.OutputDirectory,
					Path.GetFileName(PrecompiledHeaderIncludeFilename.AbsolutePath));
				FileItem DummyPCH = FileItem.CreateIntermediateTextFile(DummyPath, TmpPCHHeaderContents);

				// Create a new C++ environment that is used to create the PCH.
				CPPEnvironment ProjectPCHEnvironment = ProjectCPPEnvironment.DeepCopy();
				ProjectPCHEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				ProjectPCHEnvironment.Config.PrecompiledHeaderIncludeFilename = PrecompiledHeaderIncludeFilename.Reference;
				ProjectPCHEnvironment.Config.PCHHeaderNameInCode = PCHHeaderNameInCode;
				ProjectPCHEnvironment.Config.OutputDirectory = OutputDirectory;
				ProjectPCHEnvironment.Config.PCHOutputDirectory = PCHOutputDirectory;

				if (!bAllowDLLExports)
				{
					for (int CurDefinitionIndex = 0; CurDefinitionIndex < ProjectPCHEnvironment.Config.Definitions.Count; ++CurDefinitionIndex)
					{
						// We change DLLEXPORT to DLLIMPORT for "shared" PCH headers
						string OldDefinition = ProjectPCHEnvironment.Config.Definitions[CurDefinitionIndex];
						if (OldDefinition.EndsWith("=DLLEXPORT"))
						{
							ProjectPCHEnvironment.Config.Definitions[CurDefinitionIndex] = OldDefinition.Replace("DLLEXPORT", "DLLIMPORT");
						}
					}
				}

				// Cache our CPP environment so that we can check for outdatedness quickly.  Only files that have includes need this.
				DummyPCH.CachedCPPIncludeInfo = ProjectPCHEnvironment.Config.CPPIncludeInfo;

				Log.TraceVerbose("Found PCH file \"{0}\".", PrecompiledHeaderIncludeFilename);

				// Create the action to compile the PCH file.
				return ToolChain.CompileCPPFiles(Target, ProjectPCHEnvironment, new List<FileItem>() { DummyPCH }, ModuleName);
			}
			throw new BuildException("Couldn't find PCH file \"{0}\".", PrecompiledHeaderIncludeFilename);
		}
	}
}
