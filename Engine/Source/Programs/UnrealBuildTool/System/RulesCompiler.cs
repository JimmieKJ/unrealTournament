// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Diagnostics;
using System.Runtime.Serialization;

namespace UnrealBuildTool
{
	/// <summary>
	/// Information about a target, passed along when creating a module descriptor
	/// </summary>
	[Serializable]
	public class TargetInfo : ISerializable
	{
		/// Target platform
		public readonly UnrealTargetPlatform Platform;

		/// Target architecture (or empty string if not needed)
		public readonly string Architecture;

		/// Target build configuration
		public readonly UnrealTargetConfiguration Configuration;

		/// Target type (if known)
		public readonly TargetRules.TargetType? Type;

		/// Whether the target is monolithic (if known)
		public readonly bool? bIsMonolithic;

		public TargetInfo(SerializationInfo Info, StreamingContext Context)
		{
			Platform = (UnrealTargetPlatform)Info.GetInt32("pl");
			Architecture = Info.GetString("ar");
			Configuration = (UnrealTargetConfiguration)Info.GetInt32("co");
			if (Info.GetBoolean("t?"))
			{
				Type = (TargetRules.TargetType)Info.GetInt32("tt");
			}
			if (Info.GetBoolean("m?"))
			{
				bIsMonolithic = Info.GetBoolean("mo");
			}
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("pl", (int)Platform);
			Info.AddValue("ar", Architecture);
			Info.AddValue("co", (int)Configuration);
			Info.AddValue("t?", Type.HasValue);
			if (Type.HasValue)
			{
				Info.AddValue("tt", (int)Type.Value);
			}
			Info.AddValue("m?", bIsMonolithic.HasValue);
			if (bIsMonolithic.HasValue)
			{
				Info.AddValue("mo", bIsMonolithic);
			}
		}

		/// <summary>
		/// Constructs a TargetInfo
		/// </summary>
		/// <param name="InitPlatform">Target platform</param>
		/// <param name="InitConfiguration">Target build configuration</param>
		public TargetInfo(UnrealTargetPlatform InitPlatform, UnrealTargetConfiguration InitConfiguration, string InitArchitecture)
		{
			Platform = InitPlatform;
			Configuration = InitConfiguration;
			Architecture = InitArchitecture;
		}

		/// <summary>
		/// Constructs a TargetInfo
		/// </summary>
		/// <param name="InitPlatform">Target platform</param>
		/// <param name="InitConfiguration">Target build configuration</param>
		/// <param name="InitType">Target type</param>
		/// <param name="bInitIsMonolithic">Whether the target is monolithic</param>
		public TargetInfo(UnrealTargetPlatform InitPlatform, UnrealTargetConfiguration InitConfiguration, string InitArchitecture, TargetRules.TargetType InitType, bool bInitIsMonolithic)
			: this(InitPlatform, InitConfiguration, InitArchitecture)
		{
			Type = InitType;
			bIsMonolithic = bInitIsMonolithic;
		}

		/// <summary>
		/// True if the target type is a cooked game.
		/// </summary>
		public bool IsCooked
		{
			get
			{
				if (!Type.HasValue)
				{
					throw new BuildException("Trying to access TargetInfo.IsCooked when TargetInfo.Type is not set. Make sure IsCooked is used only in ModuleRules.");
				}
				return Type == TargetRules.TargetType.Client ||
					 Type == TargetRules.TargetType.Game ||
					 Type == TargetRules.TargetType.Server;
			}
		}

		/// <summary>
		/// True if the target type is a monolithic binary
		/// </summary>
		public bool IsMonolithic
		{
			get
			{
				if (!bIsMonolithic.HasValue)
				{
					throw new BuildException("Trying to access TargetInfo.IsMonolithic when bIsMonolithic is not set. Make sure IsMonolithic is used only in ModuleRules.");
				}
				return bIsMonolithic.Value;
			}
		}
	}


	/// <summary>
	/// ModuleRules is a data structure that contains the rules for defining a module
	/// </summary>
	public class ModuleRules
	{
		/// <summary>
		/// Type of module
		/// </summary>
		public enum ModuleType
		{
			/// <summary>
			/// C++
			/// <summary>
			CPlusPlus,

			/// <summary>
			/// CLR module (mixed C++ and C++/CLI)
			/// </summary>
			CPlusPlusCLR,

			/// <summary>
			/// External (third-party)
			/// </summary>
			External,
		}

		/// <summary>
		/// Code optimization settings
		/// </summary>
		public enum CodeOptimization
		{
			/// <summary>
			/// Code should never be optimized if possible.
			/// </summary>
			Never,

			/// <summary>
			/// Code should only be optimized in non-debug builds (not in Debug).
			/// </summary>
			InNonDebugBuilds,

			/// <summary>
			/// Code should only be optimized in shipping builds (not in Debug, DebugGame, Development)
			/// </summary>
			InShippingBuildsOnly,

			/// <summary>
			/// Code should always be optimized if possible.
			/// </summary>
			Always,

			/// <summary>
			/// Default: 'InNonDebugBuilds' for game modules, 'Always' otherwise.
			/// </summary>
			Default,
		}

		/// <summary>
		/// What type of PCH to use for this module.
		/// </summary>
		public enum PCHUsageMode
		{
			/// <summary>
			/// Default: Engine modules use shared PCHs, game modules do not
			/// </summary>
			Default,

			/// <summary>
			/// Never use shared PCHs.  Always generate a unique PCH for this module if appropriate
			/// </summary>
			NoSharedPCHs,

			/// <summary>
			/// Shared PCHs are OK!
			/// </summary>
			UseSharedPCHs
		}

		/// <summary>
		/// Which type of targets this module should be precompiled for
		/// </summary>
		public enum PrecompileTargetsType
		{
			/// <summary>
			/// Never precompile this module.
			/// </summary>
			None,

			/// <summary>
			/// Inferred from the module's directory. Engine modules under Engine/Source/Runtime will be compiled for games, those under Engine/Source/Editor will be compiled for the editor, etc...
			/// </summary>
			Default,

			/// <summary>
			/// Any game targets.
			/// </summary>
			Game,

			/// <summary>
			/// Any editor targets.
			/// </summary>
			Editor,

			/// <summary>
			/// Any targets.
			/// </summary>
			Any,
		}

		/// <summary>
		/// Type of module
		/// </summary>
		public ModuleType Type = ModuleType.CPlusPlus;

		/// <summary>
		/// Subfolder of Binaries/PLATFORM folder to put this module in when building DLLs. This should only be used by modules that are found via searching like the
		/// TargetPlatform or ShaderFormat modules. If FindModules is not used to track them down, the modules will not be found.
		/// </summary>
		public string BinariesSubFolder = "";

		/// <summary>
		/// When this module's code should be optimized.
		/// </summary>
		public CodeOptimization OptimizeCode = CodeOptimization.Default;

		/// <summary>
		/// Header file name for a shared PCH provided by this module.  Must be a valid relative path to a public C++ header file.
		/// This should only be set for header files that are included by a significant number of other C++ modules.
		/// </summary>
		public string SharedPCHHeaderFile = String.Empty;

		/// <summary>
		/// Precompiled header usage for this module
		/// </summary>
		public PCHUsageMode PCHUsage = PCHUsageMode.Default;

		/// <summary>
		/// Use run time type information
		/// </summary>
		public bool bUseRTTI = false;

		/// <summary>
		/// Use AVX instructions
		/// </summary>
		public bool bUseAVX = false;

		/// <summary>
		/// Enable buffer security checks.  This should usually be enabled as it prevents severe security risks.
		/// </summary>
		public bool bEnableBufferSecurityChecks = true;

		/// <summary>
		/// Enable exception handling
		/// </summary>
		public bool bEnableExceptions = false;

		/// <summary>
		/// Enable warnings for shadowed variables
		/// </summary>
		public bool bEnableShadowVariableWarnings = true;

		/// <summary>
		/// If true and unity builds are enabled, this module will build without unity.
		/// </summary>
		public bool bFasterWithoutUnity = false;

		/// <summary>
		/// The number of source files in this module before unity build will be activated for that module.  If set to
		/// anything besides -1, will override the default setting which is controlled by MinGameModuleSourceFilesForUnityBuild
		/// </summary>
		public int MinSourceFilesForUnityBuildOverride = 0;

		/// <summary>
		/// Overrides BuildConfiguration.MinFilesUsingPrecompiledHeader if non-zero.
		/// </summary>
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/// <summary>
		/// Module uses a #import so must be built locally when compiling with SN-DBS
		/// </summary>
		public bool bBuildLocallyWithSNDBS = false;

		/// <summary>
		/// Redistribution override flag for this module.
		/// </summary>
		public bool? IsRedistributableOverride = null;

		/// <summary>
		/// Whether the output from this module can be publicly distributed, even if it has code/
		/// dependencies on modules that are not (i.e. CarefullyRedist, NotForLicensees, NoRedist).
		/// This should be used when you plan to release binaries but not source.
		/// </summary>
		public bool bOutputPubliclyDistributable = false;

		/// <summary>
        /// List of modules names (no path needed) with header files that our module's public headers needs access to, but we don't need to "import" or link against.
		/// </summary>
		public List<string> PublicIncludePathModuleNames = new List<string>();

		/// <summary>
        /// List of public dependency module names (no path needed) (automatically does the private/public include). These are modules that are required by our public source files.
		/// </summary>
		public List<string> PublicDependencyModuleNames = new List<string>();

		/// <summary>
        /// List of modules name (no path needed) with header files that our module's private code files needs access to, but we don't need to "import" or link against.
		/// </summary>
		public List<string> PrivateIncludePathModuleNames = new List<string>();

		/// <summary>
		/// List of private dependency module names.  These are modules that our private code depends on but nothing in our public
		/// include files depend on.
		/// </summary>
		public List<string> PrivateDependencyModuleNames = new List<string>();

		/// <summary>
        /// Only for legacy reason, should not be used in new code. List of module dependencies that should be treated as circular references.  This modules must have already been added to
        /// either the public or private dependent module list.
		/// </summary>
		public List<string> CircularlyReferencedDependentModules = new List<string>();

		/// <summary>
        /// List of system/library include paths - typically used for External (third party) modules.  These are public stable header file directories that are not checked when resolving header dependencies.
		/// </summary>
		public List<string> PublicSystemIncludePaths = new List<string>();

		/// <summary>
        /// (This setting is currently not need as we discover all files from the 'Public' folder) List of all paths to include files that are exposed to other modules
		/// </summary>
		public List<string> PublicIncludePaths = new List<string>();

		/// <summary>
        /// List of all paths to this module's internal include files, not exposed to other modules (at least one include to the 'Private' path, more if we want to avoid relative paths)
		/// </summary>
		public List<string> PrivateIncludePaths = new List<string>();

		/// <summary>
        /// List of system/library paths (directory of .lib files) - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicLibraryPaths = new List<string>();

		/// <summary>
        /// List of additional libraries (names of the .lib files including extension) - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicAdditionalLibraries = new List<string>();

		/// <summary>
        // List of XCode frameworks (iOS and MacOS)
		/// </summary>
		public List<string> PublicFrameworks = new List<string>();

		/// <summary>
		// List of weak frameworks (for OS version transitions)
		/// </summary>
		public List<string> PublicWeakFrameworks = new List<string>();

		/// <summary>
		/// List of addition frameworks - typically used for External (third party) modules on Mac and iOS
		/// </summary>
		public List<UEBuildFramework> PublicAdditionalFrameworks = new List<UEBuildFramework>();

		/// <summary>
		/// List of addition resources that should be copied to the app bundle for Mac or iOS
		/// </summary>
		public List<UEBuildBundleResource> AdditionalBundleResources = new List<UEBuildBundleResource>();

		/// <summary>
		/// For builds that execute on a remote machine (e.g. iOS), this list contains additional files that
		/// need to be copied over in order for the app to link successfully.  Source/header files and PCHs are
		/// automatically copied.  Usually this is simply a list of precompiled third party library dependencies.
		/// </summary>
		public List<string> PublicAdditionalShadowFiles = new List<string>();

		/// <summary>
		/// List of delay load DLLs - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicDelayLoadDLLs = new List<string>();

		/// <summary>
		/// Additional compiler definitions for this module
		/// </summary>
		public List<string> Definitions = new List<string>();

		/// <summary>
		/// CLR modules only: The assemblies referenced by the module's private implementation.
		/// </summary>
		public List<string> PrivateAssemblyReferences = new List<string>();

		/// <summary>
		/// Addition modules this module may require at run-time 
		/// </summary>
		public List<string> DynamicallyLoadedModuleNames = new List<string>();

		/// <summary>
		/// Extra modules this module may require at run time, that are on behalf of another platform (i.e. shader formats and the like)
		/// </summary>
		public List<string> PlatformSpecificDynamicallyLoadedModuleNames = new List<string>();

		/// <summary>
		/// List of files which this module depends on at runtime. These files will be staged along with the target.
		/// </summary>
		public RuntimeDependencyList RuntimeDependencies = new RuntimeDependencyList();

		/// <summary>
		/// List of additional properties to be added to the build receipt
		/// </summary>
		public List<ReceiptProperty> AdditionalPropertiesForReceipt = new List<ReceiptProperty>();

		/// <summary>
		/// Which targets this module should be precompiled for
		/// </summary>
		public PrecompileTargetsType PrecompileForTargets = PrecompileTargetsType.Default;

		/// <summary>
		/// Property for the directory containing this module. Useful for adding paths to third party dependencies.
		/// </summary>
		public string ModuleDirectory
		{
			get
			{
				return Path.GetDirectoryName(RulesCompiler.GetFileNameFromType(GetType()));
			}
		}

		/// <summary>
		/// Made Obsolete so that we can more clearly show that this should be used for third party modules within the Engine directory
		/// </summary>
		/// <param name="ModuleNames">The names of the modules to add</param>
		[Obsolete("Use AddEngineThirdPartyPrivateStaticDependencies to add dependencies on ThirdParty modules within the Engine Directory")]
		public void AddThirdPartyPrivateStaticDependencies(TargetInfo Target, params string[] InModuleNames)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, InModuleNames);
		}

		/// <summary>
		/// Add the given Engine ThirdParty modules as static private dependencies
		///	Statically linked to this module, meaning they utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicStaticDependencies function.
		/// </summary>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddEngineThirdPartyPrivateStaticDependencies(TargetInfo Target, params string[] InModuleNames)
		{
			if (!UnrealBuildTool.IsEngineInstalled() || Target.IsMonolithic)
			{
				PrivateDependencyModuleNames.AddRange(InModuleNames);
			}
		}

		/// <summary>
		/// Made Obsolete so that we can more clearly show that this should be used for third party modules within the Engine directory
		/// </summary>
		/// <param name="ModuleNames">The names of the modules to add</param>
		[Obsolete("Use AddEngineThirdPartyPrivateDynamicDependencies to add dependencies on ThirdParty modules within the Engine Directory")]
		public void AddThirdPartyPrivateDynamicDependencies(TargetInfo Target, params string[] InModuleNames)
		{
			AddEngineThirdPartyPrivateDynamicDependencies(Target, InModuleNames);
		}

		/// <summary>
		/// Add the given Engine ThirdParty modules as dynamic private dependencies
		///	Dynamically linked to this module, meaning they do not utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicDynamicDependencies function.
		/// </summary>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddEngineThirdPartyPrivateDynamicDependencies(TargetInfo Target, params string[] InModuleNames)
		{
			if (!UnrealBuildTool.IsEngineInstalled() || Target.IsMonolithic)
			{
				PrivateIncludePathModuleNames.AddRange(InModuleNames);
				DynamicallyLoadedModuleNames.AddRange(InModuleNames);
			}
		}

		/// <summary>
		/// Setup this module for PhysX/APEX support (based on the settings in UEBuildConfiguration)
		/// </summary>
		public void SetupModulePhysXAPEXSupport(TargetInfo Target)
		{
			if (UEBuildConfiguration.bCompilePhysX == true)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "PhysX");
				Definitions.Add("WITH_PHYSX=1");
				if (UEBuildConfiguration.bCompileAPEX == true)
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "APEX");
					Definitions.Add("WITH_APEX=1");
				}
				else
				{
					Definitions.Add("WITH_APEX=0");
				}
			}
			else
			{
				Definitions.Add("WITH_PHYSX=0");
				Definitions.Add("WITH_APEX=0");
			}

			if (UEBuildConfiguration.bRuntimePhysicsCooking == true)
			{
				Definitions.Add("WITH_RUNTIME_PHYSICS_COOKING");
			}
		}

		/// <summary>
		/// Setup this module for Box2D support (based on the settings in UEBuildConfiguration)
		/// </summary>
		public void SetupModuleBox2DSupport(TargetInfo Target)
		{
			//@TODO: This need to be kept in sync with RulesCompiler.cs for now
			bool bSupported = false;
			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
			{
				bSupported = true;
			}

			bSupported = bSupported && UEBuildConfiguration.bCompileBox2D;

			if (bSupported)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Box2D");
			}

			// Box2D included define (required because pointer types may be in public exported structures)
			Definitions.Add(string.Format("WITH_BOX2D={0}", bSupported ? 1 : 0));
		}
	}

	/// <summary>
	/// TargetRules is a data structure that contains the rules for defining a target (application/executable)
	/// </summary>
	public abstract class TargetRules
	{
		/// <summary>
		/// The type of target
		/// </summary>
		[Serializable]
		public enum TargetType
		{
			/// <summary>
			/// Cooked monolithic game executable (GameName.exe).  Also used for a game-agnostic engine executable (UE4Game.exe or RocketGame.exe)
			/// </summary>
			Game,

			/// <summary>
			/// Uncooked modular editor executable and DLLs (UE4Editor.exe, UE4Editor*.dll, GameName*.dll)
			/// </summary>
			Editor,

			/// <summary>
			/// Cooked monolithic game client executable (GameNameClient.exe, but no server code)
			/// </summary>
			Client,

			/// <summary>
			/// Cooked monolithic game server executable (GameNameServer.exe, but no client code)
			/// </summary>
			Server,

			/// <summary>
			/// Program (standalone program, e.g. ShaderCompileWorker.exe, can be modular or monolithic depending on the program)
			/// </summary>
			Program,
		}

		/// <summary>
		/// The name of the game, this is set up by the rules compiler after it compiles and constructs this
		/// </summary>
		public string TargetName = null;

		/// <summary>
		/// Whether the target uses Steam (todo: substitute with more generic functionality)
		/// </summary>
		public bool bUsesSteam;

		/// <summary>
		/// Whether the target uses CEF3
		/// </summary>
		public bool bUsesCEF3;

		/// <summary>
		/// Whether the project uses visual Slate UI (as opposed to the low level windowing/messaging which is always used)
		/// </summary>
		public bool bUsesSlate = true;

		/// <summary>
		/// Hack for legacy game styling isses.  No new project should ever set this to true
		/// Whether the project uses the Slate editor style in game.  
		/// </summary>
		public bool bUsesSlateEditorStyle = false;

		/// <summary>
		/// Forces linking against the static CRT. This is not supported across the engine due to the need for allocator implementations to be shared (for example), and TPS 
		/// libraries to be consistent with each other, but can be used for utility programs.
		/// </summary>
		public bool bUseStaticCRT = false;

		/// <summary>
		/// By default we use the Release C++ Runtime (CRT), even when compiling Debug builds.  This is because the Debug C++
		/// Runtime isn't very useful when debugging Unreal Engine projects, and linking against the Debug CRT libraries forces
		/// our third party library dependencies to also be compiled using the Debug CRT (and often perform more slowly.)  Often
		/// it can be inconvenient to require a separate copy of the debug versions of third party static libraries simply
		/// so that you can debug your program's code.
		/// </summary>
		public bool bDebugBuildsActuallyUseDebugCRT = false;

		/// <summary>
		/// Whether the output from this target can be publicly distributed, even if it has
		/// dependencies on modules that are not (i.e. CarefullyRedist, NotForLicensees, NoRedist).
		/// This should be used when you plan to release binaries but not source.
		/// </summary>
		public bool bOutputPubliclyDistributable = false;

		/// <summary>
		/// Specifies the configuration whose binaries do not require a "-Platform-Configuration" suffix.
		/// </summary>
		public UnrealTargetConfiguration UndecoratedConfiguration = UnrealTargetConfiguration.Development;

		/// <summary>
		/// Build all the plugins that we can find, even if they're not enabled. This is particularly useful for content-only projects, 
		/// where you're building the UE4Editor target but running it with a game that enables a plugin.
		/// </summary>
		public bool bBuildAllPlugins = false;

		/// <summary>
		/// A list of additional plugins which need to be included in this target. This allows referencing non-optional plugin modules
		/// which cannot be disabled, and allows building against specific modules in program targets which do not fit the categories
		/// in ModuleHostType.
		/// </summary>
		public List<string> AdditionalPlugins = new List<string>();

		/// <summary>
		/// Path to the set of pak signing keys to embed in the executable
		/// </summary>
		public string PakSigningKeysFile = "";

		/// <summary>
		/// Is the given type a 'game' type (Game/Editor/Server) wrt building?
		/// </summary>
		/// <param name="InType">The target type of interest</param>
		/// <returns>true if it *is* a 'game' type</returns>
		static public bool IsGameType(TargetType InType)
		{
			return (
				(InType == TargetType.Game) ||
				(InType == TargetType.Editor) ||
				(InType == TargetType.Client) ||
				(InType == TargetType.Server)
				);
		}

		/// <summary>
		/// Is the given type a game?
		/// </summary>
		/// <param name="InType">The target type of interest</param>
		/// <returns>true if it *is* a game</returns>
		static public bool IsAGame(TargetType InType)
		{
			return (
				(InType == TargetType.Game) ||
				(InType == TargetType.Client)
				);
		}

		/// <summary>
		/// Is the given type an 'editor' type with regard to building?
		/// </summary>
		/// <param name="InType">The target type of interest</param>
		/// <returns>true if it *is* an 'editor' type</returns>
		static public bool IsEditorType(TargetType InType)
		{
			return (InType == TargetType.Editor);
		}


		/// <summary>
		/// Type of target
		/// </summary>
		public TargetType Type = TargetType.Game;

		/// <summary>
		/// The name of this target's 'configuration' within the development IDE.  No project may have more than one target with the same configuration name.
		/// If no configuration name is set, then it defaults to the TargetType name
		/// </summary>
		public string ConfigurationName
		{
			get
			{
				if (String.IsNullOrEmpty(ConfigurationNameVar))
				{
					return Type.ToString();
				}
				else
				{
					return ConfigurationNameVar;
				}
			}
			set
			{
				ConfigurationNameVar = value;
			}
		}
		private string ConfigurationNameVar = String.Empty;


		/// <summary>
		/// Allows a Program Target to specify it's own solution folder path
		/// </summary>
		public string SolutionDirectory = String.Empty;

		/// <summary>
		/// If true, the built target goes into the Engine/Binaries/<PLATFORM> folder
		/// </summary>
		public bool bOutputToEngineBinaries = false;

		/// <summary>
		/// Sub folder where the built target goes: Engine/Binaries/<PLATFORM>/<SUBDIR>
		/// </summary>
		public string ExeBinariesSubFolder = String.Empty;

		/// <summary>
		/// Whether this target should be compiled in monolithic mode
		/// </summary>
		/// <param name="InPlatform">The platform being built</param>
		/// <param name="InConfiguration">The configuration being built</param>
		/// <returns>true if it should, false if not</returns>
		public virtual bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// By default, only Editor types compile non-monolithic
			if (IsEditorType(Type) == false)
			{
				// You can build a modular game/program/server via passing '-modular' to UBT
				if (UnrealBuildTool.CommandLineContains("-modular") == false)
				{
					return true;
				}
			}

			if (UnrealBuildTool.CommandLineContains("-monolithic"))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Give the target an opportunity to override toolchain settings
		/// </summary>
		/// <param name="Target">The target currently being setup</param>
		/// <returns>true if successful, false if not</returns>
		public virtual bool ConfigureToolchain(TargetInfo Target)
		{
			return true;
		}

		/// <summary>
		/// Get the supported platforms for this target
		/// </summary>
		/// <param name="OutPlatforms">The list of platforms supported</param>
		/// <returns>true if successful, false if not</returns>
		public virtual bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
		{
			if (Type == TargetType.Program)
			{
				// By default, all programs are desktop only.
				return UnrealBuildTool.GetAllDesktopPlatforms(ref OutPlatforms, false);
			}
			else if (IsEditorType(Type))
			{
				return UnrealBuildTool.GetAllEditorPlatforms(ref OutPlatforms, false);
			}
			else if (TargetRules.IsGameType(Type))
			{
				// By default all games support all platforms
				return UnrealBuildTool.GetAllPlatforms(ref OutPlatforms, false);
			}

			return false;
		}

		public bool SupportsPlatform(UnrealTargetPlatform InPlatform)
		{
			List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();
			if (GetSupportedPlatforms(ref SupportedPlatforms) == true)
			{
				return SupportedPlatforms.Contains(InPlatform);
			}
			return false;
		}

		/// <summary>
		/// Get the supported configurations for this target
		/// </summary>
		/// <param name="OutConfigurations">The list of configurations supported</param>
		/// <returns>true if successful, false if not</returns>
		public virtual bool GetSupportedConfigurations(ref List<UnrealTargetConfiguration> OutConfigurations, bool bIncludeTestAndShippingConfigs)
		{
			if (Type == TargetType.Program)
			{
				// By default, programs are Debug and Development only.
				OutConfigurations.Add(UnrealTargetConfiguration.Debug);
				OutConfigurations.Add(UnrealTargetConfiguration.Development);
			}
			else
			{
				// By default all games support all configurations
				foreach (UnrealTargetConfiguration Config in Enum.GetValues(typeof(UnrealTargetConfiguration)))
				{
					if (Config != UnrealTargetConfiguration.Unknown)
					{
						// Some configurations just don't make sense for the editor
						if (IsEditorType(Type) &&
							(Config == UnrealTargetConfiguration.Shipping || Config == UnrealTargetConfiguration.Test))
						{
							// We don't currently support a "shipping" editor config
						}
						else if (!bIncludeTestAndShippingConfigs &&
							(Config == UnrealTargetConfiguration.Shipping || Config == UnrealTargetConfiguration.Test))
						{
							// User doesn't want 'Test' or 'Shipping' configs in their project files
						}
						else
						{
							OutConfigurations.Add(Config);
						}
					}
				}
			}

			return (OutConfigurations.Count > 0) ? true : false;
		}

		/// <summary>
		/// Setup the binaries associated with this target.
		/// </summary>
		/// <param name="Target">The target information - such as platform and configuration</param>
		/// <param name="OutBuildBinaryConfigurations">Output list of binaries to generated</param>
		/// <param name="OutExtraModuleNames">Output list of extra modules that this target could utilize</param>
		public virtual void SetupBinaries(
			TargetInfo Target,
			ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
			ref List<string> OutExtraModuleNames
			)
		{
		}

		/// <summary>
		/// Setup the global environment for building this target
		/// IMPORTANT: Game targets will *not* have this function called if they use the shared build environment.
		/// See ShouldUseSharedBuildEnvironment().
		/// </summary>
		/// <param name="Target">The target information - such as platform and configuration</param>
		/// <param name="OutLinkEnvironmentConfiguration">Output link environment settings</param>
		/// <param name="OutCPPEnvironmentConfiguration">Output compile environment settings</param>
		public virtual void SetupGlobalEnvironment(
			TargetInfo Target,
			ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
			ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
			)
		{
		}

		/// <summary>
		/// Allows a target to choose whether to use the shared build environment for a given configuration. Using
		/// the shared build environment allows binaries to be reused between targets, but prevents customizing the
		/// compile environment through SetupGlobalEnvironment().
		/// </summary>
		/// <param name="Target">Information about the target</param>
		/// <returns>True if the target should use the shared build environment</returns>
		public virtual bool ShouldUseSharedBuildEnvironment(TargetInfo Target)
		{
			return UnrealBuildTool.IsEngineInstalled() || (Target.Type != TargetType.Program && !Target.IsMonolithic);
		}

		/// <summary>
		/// Allows the target to specify modules which can be precompiled with the -Precompile/-UsePrecompiled arguments to UBT. 
		/// All dependencies of the specified modules will be included.
		/// </summary>
		/// <param name="Target">The target information, such as platform and configuration</param>
		/// <param name="ModuleNames">List which receives module names to precompile</param>
		[Obsolete("GetModulesToPrecompile() is deprecated in the 4.11 release. The -precompile option to UBT now automatically compiles all engine modules compatible with the current target.")]
		public virtual void GetModulesToPrecompile(TargetInfo Target, List<string> ModuleNames)
		{
		}

		/// <summary>
		/// Return true if this target should always be built with the base editor. Usually programs like shadercompilerworker.
		/// </summary>
		/// <returns>true if this target should always be built with the base editor.</returns>
		public virtual bool GUBP_AlwaysBuildWithBaseEditor()
		{
			return false;
		}
		/// <summary>
		/// Return true if this target should always be built with the tools. Usually programs like unrealpak.
		/// <param name="SeparateNode">If this is set to true, the program will get its own node</param>
		/// </summary>
		/// <returns>true if this target should always be built with the base editor.</returns>
		[Obsolete]
		public virtual bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode)
		{
			bInternalToolOnly = false;
			SeparateNode = false;
			return false;
		}
		/// <summary>
		/// Return true if this target should always be built with the tools. Usually programs like unrealpak.
		/// <param name="SeparateNode">If this is set to true, the program will get its own node</param>
		/// </summary>
		/// <returns>true if this target should always be built with the base editor.</returns>        
		public virtual bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode, out bool CrossCompile)
		{
			bInternalToolOnly = false;
			SeparateNode = false;
			CrossCompile = false;
			return false;
		}
		/// <summary>
		/// Return a list of platforms to build a tool for
		/// </summary>
		/// <returns>a list of platforms to build a tool for</returns>
		public virtual List<UnrealTargetPlatform> GUBP_ToolPlatforms(UnrealTargetPlatform InHostPlatform)
		{
			return new List<UnrealTargetPlatform> { InHostPlatform };
		}
		/// <summary>
		/// Return a list of configs to build a tool for
		/// </summary>
		/// <returns>a list of configs to build a tool for</returns>
		public virtual List<UnrealTargetConfiguration> GUBP_ToolConfigs(UnrealTargetPlatform InHostPlatform)
		{
			return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Development };
		}
		/// <summary>
		/// Return true if target should include a NonUnity test
		/// </summary>
		/// <returns>true if this target should include a NonUnity test
		public virtual bool GUBP_IncludeNonUnityToolTest()
		{
			return false;
		}
		/// <summary>
		/// Return true if this target should use a platform specific pass
		/// </summary>
		/// <returns>true if this target should use a platform specific pass
		public virtual bool GUBP_NeedsPlatformSpecificDLLs()
		{
			return false;
		}

		///<summary>
		///Returns true if XP monolithics are required for a game
		/// </summary>
		/// <returns>true if this target needs to be compiled for Windows XP</returns>
		public virtual bool GUBP_BuildWindowsXPMonolithics()
		{
			return false;
		}

		/// <summary>
		/// Return a list of target platforms for the monolithic
		/// </summary>
		/// <returns>a list of target platforms for the monolithic</returns>        
		public virtual List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
		{
			List<UnrealTargetPlatform> Result = new List<UnrealTargetPlatform> { HostPlatform };
			// hack to set up the templates without adding anything to their .targets.cs files
			if (!String.IsNullOrEmpty(TargetName) && TargetName.StartsWith("TP_"))
			{
				if (HostPlatform == UnrealTargetPlatform.Win64)
				{
					Result.Add(UnrealTargetPlatform.IOS);
					Result.Add(UnrealTargetPlatform.TVOS);
					Result.Add(UnrealTargetPlatform.Android);
				}
				else if (HostPlatform == UnrealTargetPlatform.Mac)
				{
					Result.Add(UnrealTargetPlatform.IOS);
					Result.Add(UnrealTargetPlatform.TVOS);
				}
			}
			return Result;
		}
		/// <summary>
		/// Return a list of target platforms for the monolithic without cook
		/// </summary>
		/// <returns>a list of target platforms for the monolithic without cook</returns>        
		public virtual List<UnrealTargetPlatform> GUBP_GetBuildOnlyPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
		{
			List<UnrealTargetPlatform> Result = new List<UnrealTargetPlatform> { };
			return Result;
		}
		/// <summary>
		/// Return a list of configs for target platforms for the monolithic
		/// </summary>
		/// <returns>a list of configs for a target platforms for the monolithic</returns>        
		public virtual List<UnrealTargetConfiguration> GUBP_GetConfigs_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
		{
			return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Development };
		}
		/// <summary>
		/// Return a list of configs which are precompiled for the given target platform
		/// </summary>
		/// <returns>a list of configs for a target platforms for the monolithic</returns>        
		public virtual List<UnrealTargetConfiguration> GUBP_GetConfigsForPrecompiledBuilds_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
		{
			return new List<UnrealTargetConfiguration>();
		}
		/// <summary>
		/// Return a list of configs for target platforms for formal builds
		/// </summary>
		/// <returns>a list of configs for a target platforms for the monolithic</returns>        
		[Obsolete]
		public virtual List<UnrealTargetConfiguration> GUBP_GetConfigsForFormalBuilds_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
		{
			return new List<UnrealTargetConfiguration>();
		}

		public class GUBPFormalBuild
		{
			public UnrealTargetPlatform TargetPlatform = UnrealTargetPlatform.Unknown;
			public UnrealTargetConfiguration TargetConfig = UnrealTargetConfiguration.Unknown;
			public bool bTest = false;
			public bool bBeforeTrigger = false;
			public GUBPFormalBuild(UnrealTargetPlatform InTargetPlatform, UnrealTargetConfiguration InTargetConfig, bool bInTest = false, bool bInBeforeTrigger = false)
			{
				TargetPlatform = InTargetPlatform;
				TargetConfig = InTargetConfig;
				bTest = bInTest;
				bBeforeTrigger = bInBeforeTrigger;
			}
		}
		/// <summary>
		/// Return a list of formal builds
		/// </summary>
		/// <returns>a list of formal builds</returns>        
		public virtual List<GUBPFormalBuild> GUBP_GetConfigsForFormalBuilds_MonolithicOnly(UnrealTargetPlatform HostPlatform)
		{
			return new List<GUBPFormalBuild>();
		}


		/// <summary>
		/// Return true if this target should be included in a promotion and indicate shared or not
		/// </summary>
		/// <returns>if this target should be included in a promotion.</returns>
		public class GUBPProjectOptions
		{
			public bool bIsPromotable = false;
			public bool bBuildAnyway = false;
			public bool bSeparateGamePromotion = false;
			public bool bTestWithShared = false;
			public bool bIsMassive = false;
			public bool bCustomWorkflowForPromotion = false;
			public bool bIsNonCode = false;
			public bool bPromoteEditorOnly = true;
			public string GroupName = null;
		}
		public virtual GUBPProjectOptions GUBP_IncludeProjectInPromotedBuild_EditorTypeOnly(UnrealTargetPlatform HostPlatform)
		{
			GUBPProjectOptions Result = new GUBPProjectOptions();
			// hack to set up the templates without adding anything to their .targets.cs files
			// tweaked to include FP_ folders too - which are temporary
			if (!String.IsNullOrEmpty(TargetName) && (TargetName.StartsWith("TP_") || TargetName.StartsWith("FP_")))
			{
				Result.bTestWithShared = true;
				Result.GroupName = "Templates";
			}
			return Result;
		}
		/// <summary>
		/// Return a list of the non-code projects to test
		/// </summary>
		/// <returns>a list of the non-code projects to build cook and test</returns>
		public virtual Dictionary<string, List<UnrealTargetPlatform>> GUBP_NonCodeProjects_BaseEditorTypeOnly(UnrealTargetPlatform HostPlatform)
		{
			return new Dictionary<string, List<UnrealTargetPlatform>>();
		}
		/// <summary>
		/// Return a list of the non-code projects to make formal builds for
		/// </summary>
		/// <returns>a list of the non-code projects to build cook and test</returns>
		[Obsolete]
		public virtual Dictionary<string, List<KeyValuePair<UnrealTargetPlatform, UnrealTargetConfiguration>>> GUBP_NonCodeFormalBuilds_BaseEditorTypeOnly()
		{
			return new Dictionary<string, List<KeyValuePair<UnrealTargetPlatform, UnrealTargetConfiguration>>>();
		}
		/// <summary>
		/// Return a list of the non-code projects to make formal builds for
		/// </summary>
		/// <returns>a list of the non-code projects to build cook and test</returns>
		public virtual Dictionary<string, List<GUBPFormalBuild>> GUBP_GetNonCodeFormalBuilds_BaseEditorTypeOnly()
		{
			return new Dictionary<string, List<GUBPFormalBuild>>();
		}

		/// <summary>
		/// Return a list of "test name", "UAT command" pairs for testing the editor
		/// </summary>
		public virtual Dictionary<string, string> GUBP_GetEditorTests_EditorTypeOnly(UnrealTargetPlatform HostPlatform)
		{
			string MacOption = HostPlatform == UnrealTargetPlatform.Mac ? " -Mac" : "";
			Dictionary<string, string> Result = new Dictionary<string, string>();
			Result.Add("EditorTest", "BuildCookRun -run -editortest -unattended -nullrhi -NoP4" + MacOption);
			Result.Add("GameTest", "BuildCookRun -run -unattended -nullrhi -NoP4" + MacOption);
			Result.Add("EditorAutomationTest", "BuildCookRun -run -editortest -RunAutomationTests -unattended -nullrhi -NoP4" + MacOption);
			Result.Add("GameAutomationTest", "BuildCookRun -run -RunAutomationTests -unattended -nullrhi -NoP4" + MacOption);
			return Result;
		}
		/// <summary>
		/// Allow the platform to setup emails for the GUBP for folks that care about node failures relating to this platform
		/// Obsolete. Included to avoid breaking existing projects.
		/// </summary>
		/// <param name="Branch">p4 root of the branch we are running</param>
		[Obsolete]
		public virtual string GUBP_GetGameFailureEMails_EditorTypeOnly(string Branch)
		{
			return "";
		}
		/// <summary>
		/// Allow the Game to set up emails for Promotable and Promotion
		/// Obsolete. Included to avoid breaking existing projects.
		/// </summary>
		[Obsolete]
		public virtual string GUBP_GetPromotionEMails_EditorTypeOnly(string Branch)
		{
			return "";
		}

		/// <summary>
		/// Return a list of "test name", "UAT command" pairs for testing a monolithic
		/// </summary>
		public virtual Dictionary<string, string> GUBP_GetGameTests_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform AltHostPlatform, UnrealTargetPlatform Platform)
		{
			Dictionary<string, string> Result = new Dictionary<string, string>();
			if ((Platform == HostPlatform || Platform == AltHostPlatform) && Type == TargetType.Game)  // for now, we will only run these for the dev config of the host platform
			{
				Result.Add("CookedGameTest", "BuildCookRun -run -skipcook -stage -pak -deploy -unattended -nullrhi -NoP4 -platform=" + Platform.ToString());
				Result.Add("CookedGameAutomationTest", "BuildCookRun -run -skipcook -stage -pak -deploy -RunAutomationTests -unattended -nullrhi -NoP4 -platform=" + Platform.ToString());
			}
			return Result;
		}
		/// <summary>
		/// Return a list of "test name", "UAT command" pairs for testing a monolithic
		/// </summary>
		public virtual Dictionary<string, string> GUBP_GetClientServerTests_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform AltHostPlatform, UnrealTargetPlatform ServerPlatform, UnrealTargetPlatform ClientPlatform)
		{
			Dictionary<string, string> Result = new Dictionary<string, string>();
#if false // needs work
            if ((ServerPlatform == HostPlatform || ServerPlatform == AltHostPlatform) &&
                (ClientPlatform == HostPlatform || ClientPlatform == AltHostPlatform) && 
                Type == TargetType.Game)  // for now, we will only run these for the dev config of the host platform and only the game executable, not sure how to deal with a client only executable
            {
                Result.Add("CookedNetTest", "BuildCookRun -run -skipcook -stage -pak -deploy -unattended -server -nullrhi -NoP4  -addcmdline=\"-nosteam\" -platform=" + ClientPlatform.ToString() + " -serverplatform=" + ServerPlatform.ToString());
            }
#endif
			return Result;
		}
		/// <summary>
		/// Return additional parameters to cook commandlet
		/// </summary>
		public virtual string GUBP_AdditionalCookParameters(UnrealTargetPlatform HostPlatform, string Platform)
		{
			return "";
		}

		/// <summary>
		/// Return additional parameters to package commandlet
		/// </summary>
		public virtual string GUBP_AdditionalPackageParameters(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
		{
			return "";
		}

		/// <summary>
		/// Allow Cook Platform Override from a target file
		/// </summary>
		public virtual string GUBP_AlternateCookPlatform(UnrealTargetPlatform HostPlatform, string Platform)
		{
			return "";
		}

		/// <summary>
		/// Allow target module to override UHT code generation version.
		/// </summary>
		public virtual EGeneratedCodeVersion GetGeneratedCodeVersion()
		{
			return EGeneratedCodeVersion.None;
		}
	}

	public class RulesAssembly
	{
		/// <summary>
		/// The compiled assembly
		/// </summary>
		private Assembly CompiledAssembly;

		/// <summary>
		/// All the plugins included in this assembly
		/// </summary>
		private IReadOnlyList<PluginInfo> Plugins;

		/// <summary>
		/// Maps module names to their actual xxx.Module.cs file on disk
		/// </summary>
		private Dictionary<string, FileReference> ModuleNameToModuleFile = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Maps target names to their actual xxx.Target.cs file on disk
		/// </summary>
		private Dictionary<string, FileReference> TargetNameToTargetFile = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping from module file to its plugin info.
		/// </summary>
		private Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo;

		/// <summary>
		/// Cache for whether a module has source code
		/// </summary>
		private Dictionary<FileReference, bool> ModuleHasSource = new Dictionary<FileReference, bool>();

		/// <summary>
		/// The parent rules assembly that this assembly inherits. Game assemblies inherit the engine assembly, and the engine assembly inherits nothing.
		/// </summary>
		private RulesAssembly Parent;

		/// <summary>
		/// Constructor. Compiles a rules assembly from the given source files.
		/// </summary>
		/// <param name="Plugins">All the plugins included in this assembly</param>
		/// <param name="ModuleFiles">List of module files to compile</param>
		/// <param name="TargetFiles">List of target files to compile</param>
		/// <param name="ModuleFileToPluginInfo">Mapping of module file to the plugin that contains it</param>
		/// <param name="AssemblyFileName">The output path for the compiled assembly</param>
		/// <param name="Parent">The parent rules assembly</param>
		public RulesAssembly(IReadOnlyList<PluginInfo> Plugins, List<FileReference> ModuleFiles, List<FileReference> TargetFiles, Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo, FileReference AssemblyFileName, RulesAssembly Parent)
		{
			this.Plugins = Plugins;
			this.ModuleFileToPluginInfo = ModuleFileToPluginInfo;
			this.Parent = Parent;

			// Find all the source files
			List<FileReference> AssemblySourceFiles = new List<FileReference>();
			AssemblySourceFiles.AddRange(ModuleFiles);
			AssemblySourceFiles.AddRange(TargetFiles);

			// Compile the assembly
			if (AssemblySourceFiles.Count > 0)
			{
				CompiledAssembly = DynamicCompilation.CompileAndLoadAssembly(AssemblyFileName, AssemblySourceFiles);
			}

			// Setup the module map
			foreach (FileReference ModuleFile in ModuleFiles)
			{
				string ModuleName = ModuleFile.GetFileNameWithoutAnyExtensions();
				if (!ModuleNameToModuleFile.ContainsKey(ModuleName))
				{
					ModuleNameToModuleFile.Add(ModuleName, ModuleFile);
				}
			}

			// Setup the target map
			foreach (FileReference TargetFile in TargetFiles)
			{
				string TargetName = TargetFile.GetFileNameWithoutAnyExtensions();
				if (!TargetNameToTargetFile.ContainsKey(TargetName))
				{
					TargetNameToTargetFile.Add(TargetName, TargetFile);
				}
			}
		}

		/// <summary>
		/// Fills a list with all the module names in this assembly (or its parent)
		/// </summary>
		/// <param name="ModuleNames">List to receive the module names</param>
		public void GetAllModuleNames(List<string> ModuleNames)
		{
			if(Parent != null)
			{
				Parent.GetAllModuleNames(ModuleNames);
			}
			ModuleNames.AddRange(ModuleNameToModuleFile.Keys);
		}

		/// <summary>
		/// Tries to get the filename that declared the given type
		/// </summary>
		/// <param name="ExistingType"></param>
		/// <param name="FileName"></param>
		/// <returns>True if the type was found, false otherwise</returns>
		public bool TryGetFileNameFromType(Type ExistingType, out FileReference File)
		{
			if (ExistingType.Assembly == CompiledAssembly)
			{
				string Name = ExistingType.Name;
				if (ModuleNameToModuleFile.TryGetValue(Name, out File))
				{
					return true;
				}
				if (TargetNameToTargetFile.TryGetValue(Name, out File))
				{
					return true;
				}
			}
			else
			{
				if (Parent != null && Parent.TryGetFileNameFromType(ExistingType, out File))
				{
					return true;
				}
			}

			File = null;
			return false;
		}

		/// <summary>
		/// Gets the source file containing rules for the given module
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <returns>The filename containing rules for this module, or an empty string if not found</returns>
		public FileReference GetModuleFileName(string ModuleName)
		{
			FileReference ModuleFile;
			if (ModuleNameToModuleFile.TryGetValue(ModuleName, out ModuleFile))
			{
				return ModuleFile;
			}
			else
			{
				return (Parent == null) ? null : Parent.GetModuleFileName(ModuleName);
			}
		}

		/// <summary>
		/// Gets the source file containing rules for the given target
		/// </summary>
		/// <param name="TargetName">The name of the target</param>
		/// <returns>The filename containing rules for this target, or an empty string if not found</returns>
		public FileReference GetTargetFileName(string TargetName)
		{
			FileReference TargetFile;
			if (TargetNameToTargetFile.TryGetValue(TargetName, out TargetFile))
			{
				return TargetFile;
			}
			else
			{
				return (Parent == null) ? null : Parent.GetTargetFileName(TargetName);
			}
		}

		/// <summary>
		/// Creates an instance of a module rules descriptor object for the specified module name
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="Target">Information about the target associated with this module</param>
		/// <returns>Compiled module rule info</returns>
		public ModuleRules CreateModuleRules(string ModuleName, TargetInfo Target)
		{
			FileReference ModuleFileName;
			return CreateModuleRules(ModuleName, Target, out ModuleFileName);
		}

		/// <summary>
		/// Creates an instance of a module rules descriptor object for the specified module name
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="Target">Information about the target associated with this module</param>
		/// <param name="ModuleFileName">The original source file name for the Module.cs file for this module</param>
		/// <returns>Compiled module rule info</returns>
		public ModuleRules CreateModuleRules(string ModuleName, TargetInfo Target, out FileReference ModuleFileName)
		{
			// Currently, we expect the user's rules object type name to be the same as the module name
			string ModuleTypeName = ModuleName;

			// Make sure the module file is known to us
			if (!ModuleNameToModuleFile.TryGetValue(ModuleName, out ModuleFileName))
			{
				if (Parent == null)
				{
					throw new MissingModuleException(ModuleName);
				}
				else
				{
					return Parent.CreateModuleRules(ModuleName, Target, out ModuleFileName);
				}
			}

			UnrealTargetPlatform LocalPlatform = Target.Platform;
			UnrealTargetConfiguration LocalConfiguration = Target.Configuration;
			TargetInfo LocalTarget = new TargetInfo(LocalPlatform, LocalConfiguration, Target.Architecture, Target.Type.Value, Target.bIsMonolithic.Value);

			// The build module must define a type named 'Rules' that derives from our 'ModuleRules' type.  
			Type RulesObjectType = CompiledAssembly.GetType(ModuleName);

			if (RulesObjectType == null)
			{
				// Temporary hack to avoid System namespace collisions
				// @todo projectfiles: Make rules assemblies require namespaces.
				RulesObjectType = CompiledAssembly.GetType("UnrealBuildTool.Rules." + ModuleName);
			}

			if (RulesObjectType == null)
			{
				throw new BuildException("Expecting to find a type to be declared in a module rules named '{0}' in {1}.  This type must derive from the 'ModuleRules' type defined by Unreal Build Tool.", ModuleTypeName, CompiledAssembly.FullName);
			}

			// Create an instance of the module's rules object
			ModuleRules RulesObject;
			try
			{
				RulesObject = (ModuleRules)Activator.CreateInstance(RulesObjectType, LocalTarget);
			}
			catch (Exception Ex)
			{
				throw new BuildException(Ex, "Unable to instantiate instance of '{0}' object type from compiled assembly '{1}'.  Unreal Build Tool creates an instance of your module's 'Rules' object in order to find out about your module's requirements.  The CLR exception details may provide more information:  {2}", ModuleTypeName, CompiledAssembly.FullName, Ex.ToString());
			}

			// Update the run-time dependencies path to remove $(PluginDir) and replace with a full path. When the receipt is saved it'll be converted to a $(ProjectDir) or $(EngineDir) equivalent.
			foreach (RuntimeDependency Dependency in RulesObject.RuntimeDependencies)
			{
				const string PluginDirVariable = "$(PluginDir)";
				if (Dependency.Path.StartsWith(PluginDirVariable, StringComparison.InvariantCultureIgnoreCase))
				{
					PluginInfo Plugin;
					if (ModuleFileToPluginInfo.TryGetValue(ModuleFileName, out Plugin))
					{
						Dependency.Path = Plugin.Directory + Dependency.Path.Substring(PluginDirVariable.Length);
					}
				}
			}

			return RulesObject;
		}

		/// <summary>
		/// Determines whether the given module name is a game module (as opposed to an engine module)
		/// </summary>
		public bool IsGameModule(string InModuleName)
		{
			FileReference ModuleFileName = GetModuleFileName(InModuleName);
			return (ModuleFileName != null && !ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory));
		}

		protected bool GetTargetTypeAndRulesInstance(string InTargetName, TargetInfo InTarget, out System.Type OutRulesObjectType, out TargetRules OutRulesObject)
		{
			// The build module must define a type named '<TargetName>Target' that derives from our 'TargetRules' type.  
			OutRulesObjectType = CompiledAssembly.GetType(InTargetName);
			if (OutRulesObjectType == null)
			{
				throw new BuildException(
					"Expecting to find a type to be declared in a target rules named '{0}'.  This type must derive from the 'TargetRules' type defined by Unreal Build Tool.",
					InTargetName);
			}

			// Create an instance of the module's rules object
			try
			{
				OutRulesObject = (TargetRules)Activator.CreateInstance(OutRulesObjectType, InTarget);
			}
			catch (Exception Ex)
			{
				string AssemblyFileName = Path.GetFileNameWithoutExtension(CompiledAssembly.Location);
				throw new BuildException(Ex,
					"Unable to instantiate instance of '{0}' object type from compiled assembly '{1}'.  Unreal Build Tool creates an instance of your module's 'Rules' object in order to find out about your module's requirements.  The CLR exception details may provide more information:  {2}",
					InTargetName, AssemblyFileName, Ex.ToString());
			}

			OutRulesObject.TargetName = InTargetName;

			return true;
		}

		/// <summary>
		/// Creates a target rules object for the specified target name.
		/// </summary>
		/// <param name="TargetName">Name of the target</param>
		/// <param name="Target">Information about the target associated with this target</param>
		/// <returns>The build target rules for the specified target</returns>
		public TargetRules CreateTargetRules(string TargetName, TargetInfo Target, bool bInEditorRecompile)
		{
			FileReference TargetFileName;
			return CreateTargetRules(TargetName, Target, bInEditorRecompile, out TargetFileName);
		}

		/// <summary>
		/// Creates a target rules object for the specified target name.
		/// </summary>
		/// <param name="TargetName">Name of the target</param>
		/// <param name="Target">Information about the target associated with this target</param>
		/// <param name="TargetFileName">The original source file name of the Target.cs file for this target</param>
		/// <returns>The build target rules for the specified target</returns>
		public TargetRules CreateTargetRules(string TargetName, TargetInfo Target, bool bInEditorRecompile, out FileReference TargetFileName)
		{
			// Make sure the target file is known to us
			bool bFoundTargetName = TargetNameToTargetFile.ContainsKey(TargetName);
			if (bFoundTargetName == false)
			{
				if (Parent == null)
				{
					//				throw new BuildException("Couldn't find target rules file for target '{0}' in rules assembly '{1}'.", TargetName, RulesAssembly.FullName);
					string ExceptionMessage = "Couldn't find target rules file for target '";
					ExceptionMessage += TargetName;
					ExceptionMessage += "' in rules assembly '";
					ExceptionMessage += CompiledAssembly.FullName;
					ExceptionMessage += "'." + Environment.NewLine;

					ExceptionMessage += "Location: " + CompiledAssembly.Location + Environment.NewLine;

					ExceptionMessage += "Target rules found:" + Environment.NewLine;
					foreach (KeyValuePair<string, FileReference> entry in TargetNameToTargetFile)
					{
						ExceptionMessage += "\t" + entry.Key + " - " + entry.Value + Environment.NewLine;
					}

					throw new BuildException(ExceptionMessage);
				}
				else
				{
					return Parent.CreateTargetRules(TargetName, Target, bInEditorRecompile, out TargetFileName);
				}
			}

			// Return the target file name to the caller
			TargetFileName = TargetNameToTargetFile[TargetName];

			// Currently, we expect the user's rules object type name to be the same as the module name + 'Target'
			string TargetTypeName = TargetName + "Target";

			// The build module must define a type named '<TargetName>Target' that derives from our 'TargetRules' type.  
			System.Type RulesObjectType;
			TargetRules RulesObject;
			GetTargetTypeAndRulesInstance(TargetTypeName, Target, out RulesObjectType, out RulesObject);
			if (bInEditorRecompile)
			{
				// Make sure this is an editor module.
				if (RulesObject != null)
				{
					if (RulesObject.Type != TargetRules.TargetType.Editor)
					{
						// Not the editor... determine the editor project
						string TargetSourceFolderString = TargetFileName.FullName;
						Int32 SourceFolderIndex = -1;
						if (Utils.IsRunningOnMono)
						{
							TargetSourceFolderString = TargetSourceFolderString.Replace("\\", "/");
							SourceFolderIndex = TargetSourceFolderString.LastIndexOf("/Source/", StringComparison.InvariantCultureIgnoreCase);
						}
						else
						{
							TargetSourceFolderString = TargetSourceFolderString.Replace("/", "\\");
							SourceFolderIndex = TargetSourceFolderString.LastIndexOf("\\Source\\", StringComparison.InvariantCultureIgnoreCase);
						}
						if (SourceFolderIndex != -1)
						{
							DirectoryReference TargetSourceFolder = new DirectoryReference(TargetSourceFolderString.Substring(0, SourceFolderIndex + 7));
							foreach (KeyValuePair<string, FileReference> CheckEntry in TargetNameToTargetFile)
							{
								if (CheckEntry.Value.IsUnderDirectory(TargetSourceFolder))
								{
									if (CheckEntry.Key.Equals(TargetName, StringComparison.InvariantCultureIgnoreCase) == false)
									{
										// We have found a target in the same source folder that is not the original target found.
										// See if it is the editor project
										string CheckTargetTypeName = CheckEntry.Key + "Target";
										System.Type CheckRulesObjectType;
										TargetRules CheckRulesObject;
										GetTargetTypeAndRulesInstance(CheckTargetTypeName, Target, out CheckRulesObjectType, out CheckRulesObject);
										if (CheckRulesObject != null)
										{
											if (CheckRulesObject.Type == TargetRules.TargetType.Editor)
											{
												// Found it
												// NOTE: This prevents multiple Editor targets from co-existing...
												RulesObject = CheckRulesObject;
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			return RulesObject;
		}

		/// <summary>
		/// Enumerates all the plugins that are available
		/// </summary>
		/// <returns></returns>
		public IEnumerable<PluginInfo> EnumeratePlugins()
		{
			return global::UnrealBuildTool.Plugins.FilterPlugins(EnumeratePluginsInternal());
		}

		/// <summary>
		/// Enumerates all the plugins that are available
		/// </summary>
		/// <returns></returns>
		protected IEnumerable<PluginInfo> EnumeratePluginsInternal()
		{
			if (Parent == null)
			{
				return Plugins;
			}
			else
			{
				return Plugins.Concat(Parent.EnumeratePluginsInternal());
			}
		}

		/// <summary>
		/// Tries to find the PluginInfo associated with a given module file
		/// </summary>
		/// <param name="ModuleFile">The module to search for</param>
		/// <param name="Plugin">The matching plugin info, or null.</param>
		/// <returns>True if the module belongs to a plugin</returns>
		public bool TryGetPluginForModule(FileReference ModuleFile, out PluginInfo Plugin)
		{
			if (ModuleFileToPluginInfo.TryGetValue(ModuleFile, out Plugin))
			{
				return true;
			}
			else
			{
				return (Parent == null) ? false : Parent.TryGetPluginForModule(ModuleFile, out Plugin);
			}
		}

		/// <summary>
		/// Determines if a module in this rules assembly has source code.
		/// </summary>
		/// <param name="ModuleName">Name of the module to check</param>
		/// <returns>True if the module has source files, false if the module was not found, or does not have source files.</returns>
		public bool DoesModuleHaveSource(string ModuleName)
		{
			FileReference ModuleFile;
			if (ModuleNameToModuleFile.TryGetValue(ModuleName, out ModuleFile))
			{
				bool HasSource;
				if (!ModuleHasSource.TryGetValue(ModuleFile, out HasSource))
				{
					foreach (string FileName in Directory.EnumerateFiles(ModuleFile.Directory.FullName, "*.cpp", SearchOption.AllDirectories))
					{
						HasSource = true;
						break;
					}
					ModuleHasSource.Add(ModuleFile, HasSource);
				}
				return HasSource;
			}
			return (Parent == null) ? false : Parent.DoesModuleHaveSource(ModuleName);
		}
	}

	public class RulesCompiler
	{
		/// <summary>
		/// Enum for types of rules files. Should match extensions in RulesFileExtensions.
		/// </summary>
		public enum RulesFileType
		{
			Module,
			Target,
			Automation,
			AutomationModule
		}

		/// <summary>
		/// Cached list of rules files in each directory of each type
		/// </summary>
		class RulesFileCache
		{
			public List<FileReference> ModuleRules = new List<FileReference>();
			public List<FileReference> TargetRules = new List<FileReference>();
			public List<FileReference> AutomationModules = new List<FileReference>();
		}

		/// Map of root folders to a cached list of all UBT-related source files in that folder or any of its sub-folders.
		/// We cache these file names so we can avoid searching for them later on.
		static Dictionary<DirectoryReference, RulesFileCache> RootFolderToRulesFileCache = new Dictionary<DirectoryReference, RulesFileCache>();
		
		public static List<FileReference> FindAllRulesSourceFiles(RulesFileType RulesFileType, List<DirectoryReference> GameFolders, List<FileReference> ForeignPlugins, List<DirectoryReference> AdditionalSearchPaths, bool bIncludeEngine = true)
		{
			List<DirectoryReference> Folders = new List<DirectoryReference>();

			// Add all engine source (including third party source)
			if (bIncludeEngine)
			{
				Folders.Add(UnrealBuildTool.EngineSourceDirectory);
			}

			// @todo plugin: Disallow modules from including plugin modules as dependency modules? (except when the module is part of that plugin)

			// Get all the root folders for plugins
			List<DirectoryReference> RootFolders = new List<DirectoryReference>();
			if (bIncludeEngine)
			{
				RootFolders.Add(UnrealBuildTool.EngineDirectory);
			}
			if (GameFolders != null)
			{
				RootFolders.AddRange(GameFolders);
			}

			// Find all the plugin source directories
			foreach (DirectoryReference RootFolder in RootFolders)
			{
				DirectoryReference PluginsFolder = DirectoryReference.Combine(RootFolder, "Plugins");
				foreach (FileReference PluginFile in Plugins.EnumeratePlugins(PluginsFolder))
				{
					Folders.Add(DirectoryReference.Combine(PluginFile.Directory, "Source"));
				}
			}

			// Add all the extra plugin folders
			if (ForeignPlugins != null)
			{
				foreach (FileReference ForeignPlugin in ForeignPlugins)
				{
					Folders.Add(DirectoryReference.Combine(ForeignPlugin.Directory, "Source"));
				}
			}

			// Add in the game folders to search
			if (GameFolders != null)
			{
				foreach (DirectoryReference GameFolder in GameFolders)
				{
					DirectoryReference GameSourceFolder = DirectoryReference.Combine(GameFolder, "Source");
					Folders.Add(GameSourceFolder);
					DirectoryReference GameIntermediateSourceFolder = DirectoryReference.Combine(GameFolder, "Intermediate", "Source");
					Folders.Add(GameIntermediateSourceFolder);
				}
			}

			// Process the additional search path, if sent in
			if (AdditionalSearchPaths != null)
			{
				foreach (DirectoryReference AdditionalSearchPath in AdditionalSearchPaths)
				{
					if (AdditionalSearchPath != null)
					{
						if (AdditionalSearchPath.Exists())
						{
							Folders.Add(AdditionalSearchPath);
						}
						else
						{
							throw new BuildException("Couldn't find AdditionalSearchPath for rules source files '{0}'", AdditionalSearchPath);
						}
					}
				}
			}

			// Iterate over all the folders to check
			List<FileReference> SourceFiles = new List<FileReference>();
			HashSet<FileReference> UniqueSourceFiles = new HashSet<FileReference>();
			foreach (DirectoryReference Folder in Folders)
			{
				IReadOnlyList<FileReference> SourceFilesForFolder = FindAllRulesFiles(Folder, RulesFileType);
				foreach (FileReference SourceFile in SourceFilesForFolder)
				{
					if (UniqueSourceFiles.Add(SourceFile))
					{
						SourceFiles.Add(SourceFile);
					}
				}
			}
			return SourceFiles;
		}

        public static void InvalidateRulesFileCache(string DirectoryPath)
        {
            DirectoryReference Directory = new DirectoryReference(DirectoryPath);
            RootFolderToRulesFileCache.Remove(Directory);
            DirectoryLookupCache.InvalidateCachedDirectory(Directory);
        }

		private static IReadOnlyList<FileReference> FindAllRulesFiles(DirectoryReference Directory, RulesFileType Type)
		{
			// Check to see if we've already cached source files for this folder
			RulesFileCache Cache;
			if (!RootFolderToRulesFileCache.TryGetValue(Directory, out Cache))
			{
				Cache = new RulesFileCache();
				FindAllRulesFilesRecursively(Directory, Cache);
					RootFolderToRulesFileCache[Directory] = Cache;
				}

			// Get the list of files of the type we're looking for
			if (Type == RulesCompiler.RulesFileType.Module)
			{
				return Cache.ModuleRules;
			}
			else if (Type == RulesCompiler.RulesFileType.Target)
			{
				return Cache.TargetRules;
			}
			else if (Type == RulesCompiler.RulesFileType.AutomationModule)
			{
				return Cache.AutomationModules;
			}
			else
			{
				throw new BuildException("Unhandled rules type: {0}", Type);
			}
		}

		private static void FindAllRulesFilesRecursively(DirectoryReference Directory, RulesFileCache Cache)
		{
			// Scan all the files in this directory
			bool bSearchSubFolders = true;
			foreach (FileReference File in DirectoryLookupCache.EnumerateFiles(Directory))
			{
				if (File.HasExtension(".build.cs"))
				{
					Cache.ModuleRules.Add(File);
					bSearchSubFolders = false;
				}
				else if (File.HasExtension(".target.cs"))
				{
					Cache.TargetRules.Add(File);
				}
				else if (File.HasExtension(".automation.csproj"))
				{
					Cache.AutomationModules.Add(File);
					bSearchSubFolders = false;
				}
			}

			// If we didn't find anything to stop the search, search all the subdirectories too
			if (bSearchSubFolders)
			{
				foreach (DirectoryReference SubDirectory in DirectoryLookupCache.EnumerateDirectories(Directory))
				{
					FindAllRulesFilesRecursively(SubDirectory, Cache);
				}
			}
		}

		/// <summary>
		/// The cached rules assembly for engine modules and targets.
		/// </summary>
		private static RulesAssembly EngineRulesAssembly;

		/// Map of assembly names we've already compiled and loaded to their Assembly and list of game folders.  This is used to prevent
		/// trying to recompile the same assembly when ping-ponging between different types of targets
		private static Dictionary<FileReference, RulesAssembly> LoadedAssemblyMap = new Dictionary<FileReference, RulesAssembly>();

		/// <summary>
		/// Creates the engine rules assembly
		/// </summary>
		/// <param name="ForeignPlugins">List of plugins to include in this assembly</param>
		/// <returns>New rules assembly</returns>
		public static RulesAssembly CreateEngineRulesAssembly()
		{
			if (EngineRulesAssembly == null)
			{
				// Find all the rules files
				List<FileReference> ModuleFiles = new List<FileReference>(FindAllRulesFiles(UnrealBuildTool.EngineSourceDirectory, RulesFileType.Module));
				List<FileReference> TargetFiles = new List<FileReference>(FindAllRulesFiles(UnrealBuildTool.EngineSourceDirectory, RulesFileType.Target));

				// Add all the plugin modules too
				IReadOnlyList<PluginInfo> EnginePlugins = Plugins.ReadEnginePlugins(UnrealBuildTool.EngineDirectory);
				Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo = new Dictionary<FileReference, PluginInfo>();
				FindModuleRulesForPlugins(EnginePlugins, ModuleFiles, ModuleFileToPluginInfo);

				// Create a path to the assembly that we'll either load or compile
				FileReference AssemblyFileName = FileReference.Combine(UnrealBuildTool.EngineDirectory, BuildConfiguration.BaseIntermediateFolder, "BuildRules", "UE4Rules.dll");
				EngineRulesAssembly = new RulesAssembly(EnginePlugins, ModuleFiles, TargetFiles, ModuleFileToPluginInfo, AssemblyFileName, null);
			}
			return EngineRulesAssembly;
		}

		/// <summary>
		/// Creates a rules assembly with the given parameters.
		/// </summary>
		/// <param name="ProjectFileName">The project file to create rules for. Null for the engine.</param>
		/// <param name="ForeignPlugins">List of foreign plugin folders to include in the assembly. May be null.</param>
		public static RulesAssembly CreateProjectRulesAssembly(FileReference ProjectFileName)
		{
			// Check if there's an existing assembly for this project
			RulesAssembly ProjectRulesAssembly;
			if (!LoadedAssemblyMap.TryGetValue(ProjectFileName, out ProjectRulesAssembly))
			{
				// Create the engine rules assembly
				RulesAssembly Parent = CreateEngineRulesAssembly();

				// Find all the rules under the project source directory
				DirectoryReference ProjectDirectory = ProjectFileName.Directory;
				DirectoryReference ProjectSourceDirectory = DirectoryReference.Combine(ProjectDirectory, "Source");
				List<FileReference> ModuleFiles = new List<FileReference>(FindAllRulesFiles(ProjectSourceDirectory, RulesFileType.Module));
				List<FileReference> TargetFiles = new List<FileReference>(FindAllRulesFiles(ProjectSourceDirectory, RulesFileType.Target));

				// Find all the project plugins
				IReadOnlyList<PluginInfo> ProjectPlugins = Plugins.ReadProjectPlugins(ProjectFileName.Directory);
				Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo = new Dictionary<FileReference, PluginInfo>();
				FindModuleRulesForPlugins(ProjectPlugins, ModuleFiles, ModuleFileToPluginInfo);

				// Add the games project's intermediate source folder
				DirectoryReference ProjectIntermediateSourceDirectory = DirectoryReference.Combine(ProjectDirectory, "Intermediate", "Source");
				if (ProjectIntermediateSourceDirectory.Exists())
				{
					TargetFiles.AddRange(FindAllRulesFiles(ProjectIntermediateSourceDirectory, RulesFileType.Target));
				}

				// Compile the assembly
				FileReference AssemblyFileName = FileReference.Combine(ProjectDirectory, BuildConfiguration.BaseIntermediateFolder, "BuildRules", ProjectFileName.GetFileNameWithoutExtension() + "ModuleRules.dll");
				ProjectRulesAssembly = new RulesAssembly(ProjectPlugins, ModuleFiles, TargetFiles, ModuleFileToPluginInfo, AssemblyFileName, Parent);
				LoadedAssemblyMap.Add(ProjectFileName, ProjectRulesAssembly);
			}
			return ProjectRulesAssembly;
		}

		/// <summary>
		/// Creates a rules assembly with the given parameters.
		/// </summary>
		/// <param name="ProjectFileName">The project file to create rules for. Null for the engine.</param>
		/// <param name="ForeignPlugins">List of foreign plugin folders to include in the assembly. May be null.</param>
		public static RulesAssembly CreatePluginRulesAssembly(FileReference PluginFileName, RulesAssembly Parent)
		{
			// Check if there's an existing assembly for this project
			RulesAssembly PluginRulesAssembly;
			if (!LoadedAssemblyMap.TryGetValue(PluginFileName, out PluginRulesAssembly))
			{
				// Find all the rules source files
				List<FileReference> ModuleFiles = new List<FileReference>();
				List<FileReference> TargetFiles = new List<FileReference>();

				// Create a list of plugins for this assembly. If it already exists in the parent assembly, just create an empty assembly.
				List<PluginInfo> ForeignPlugins = new List<PluginInfo>();
				if (Parent == null || !Parent.EnumeratePlugins().Any(x => x.File == PluginFileName))
				{
					ForeignPlugins.Add(new PluginInfo(PluginFileName, PluginLoadedFrom.GameProject));
				}

				// Find all the modules
				Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo = new Dictionary<FileReference, PluginInfo>();
				FindModuleRulesForPlugins(ForeignPlugins, ModuleFiles, ModuleFileToPluginInfo);

				// Compile the assembly
				FileReference AssemblyFileName = FileReference.Combine(PluginFileName.Directory, BuildConfiguration.BaseIntermediateFolder, "BuildRules", Path.GetFileNameWithoutExtension(PluginFileName.FullName) + "ModuleRules.dll");
				PluginRulesAssembly = new RulesAssembly(ForeignPlugins, ModuleFiles, TargetFiles, ModuleFileToPluginInfo, AssemblyFileName, Parent);
				LoadedAssemblyMap.Add(PluginFileName, PluginRulesAssembly);
			}
			return PluginRulesAssembly;
		}

		/// <summary>
		/// Finds all the module rules for plugins under the given directory.
		/// </summary>
		/// <param name="PluginsDirectory">The directory to search</param>
		/// <param name="ModuleFiles">List of module files to be populated</param>
		/// <param name="ModuleFileToPluginFile">Dictionary which is filled with mappings from the module file to its corresponding plugin file</param>
		private static void FindModuleRulesForPlugins(IReadOnlyList<PluginInfo> Plugins, List<FileReference> ModuleFiles, Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo)
		{
			foreach (PluginInfo Plugin in Plugins)
			{
				IReadOnlyList<FileReference> PluginModuleFiles = FindAllRulesFiles(DirectoryReference.Combine(Plugin.Directory, "Source"), RulesFileType.Module);
				foreach (FileReference ModuleFile in PluginModuleFiles)
				{
					ModuleFiles.Add(ModuleFile);
					ModuleFileToPluginInfo[ModuleFile] = Plugin;
				}
			}
		}

		/// <summary>
		/// Gets the filename that declares the given type.
		/// </summary>
		/// <param name="ExistingType">The type to search for.</param>
		/// <returns>The filename that declared the given type, or null</returns>
		public static string GetFileNameFromType(Type ExistingType)
		{
			FileReference FileName;
			if (EngineRulesAssembly != null && EngineRulesAssembly.TryGetFileNameFromType(ExistingType, out FileName))
			{
				return FileName.FullName;
			}
			foreach (RulesAssembly RulesAssembly in LoadedAssemblyMap.Values)
			{
				if (RulesAssembly.TryGetFileNameFromType(ExistingType, out FileName))
				{
					return FileName.FullName;
				}
			}
			return null;
		}

        [Obsolete("GetModuleFilename is deprecated, use the ModuleDirectory property on any ModuleRules instead to get a path to your module.", true)]
        public static string GetModuleFilename(string TypeName)
        {
            throw new NotImplementedException("GetModuleFilename is deprecated, use the ModuleDirectory property on any ModuleRules instead to get a path to your module.");
        }
    }
}
