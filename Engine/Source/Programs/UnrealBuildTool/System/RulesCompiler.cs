// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Diagnostics;

namespace UnrealBuildTool
{
	/// <summary>
	/// Information about a target, passed along when creating a module descriptor
	/// </summary>
	[Serializable]
	public class TargetInfo
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

		/// <summary>
		/// Constructs a TargetInfo
		/// </summary>
		/// <param name="InitPlatform">Target platform</param>
		/// <param name="InitConfiguration">Target build configuration</param>
		public TargetInfo( UnrealTargetPlatform InitPlatform, UnrealTargetConfiguration InitConfiguration )
		{
			Platform = InitPlatform;
			Configuration = InitConfiguration;

			// get the platform's architecture
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			Architecture = BuildPlatform.GetActiveArchitecture();
		}

		/// <summary>
		/// Constructs a TargetInfo
		/// </summary>
		/// <param name="InitPlatform">Target platform</param>
		/// <param name="InitConfiguration">Target build configuration</param>
		/// <param name="InitType">Target type</param>
		/// <param name="bInitIsMonolithic">Whether the target is monolithic</param>
		public TargetInfo( UnrealTargetPlatform InitPlatform, UnrealTargetConfiguration InitConfiguration, TargetRules.TargetType InitType, bool bInitIsMonolithic )
			: this(InitPlatform, InitConfiguration)
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
	public abstract class ModuleRules
	{
		/// Type of module
		public enum ModuleType
		{
			/// C++
			CPlusPlus,

			/// CLR module (mixed C++ and C++/CLI)
			CPlusPlusCLR,

			/// External (third-party)
			External,
		}

		/// Code optimization settings
		public enum CodeOptimization
		{
			/// Code should never be optimized if possible.
			Never,

			/// Code should only be optimized in non-debug builds (not in Debug).
			InNonDebugBuilds,

			/// Code should only be optimized in shipping builds (not in Debug, DebugGame, Development)
			InShippingBuildsOnly,

			/// Code should always be optimized if possible.
			Always,

			/// Default: 'InNonDebugBuilds' for game modules, 'Always' otherwise.
			Default,
		}

		/// Type of module
		public ModuleType Type = ModuleType.CPlusPlus;

		/// Subfolder of Binaries/PLATFORM folder to put this module in when building DLLs
		/// This should only be used by modules that are found via searching like the
		/// TargetPlatform or ShaderFormat modules. 
		/// If FindModules is not used to track them down, the modules will not be found.
		public string BinariesSubFolder = "";

		/// When this module's code should be optimized.
		public CodeOptimization OptimizeCode = CodeOptimization.Default;

		/// Header file name for a shared PCH provided by this module.  Must be a valid relative path to a public C++ header file.
		/// This should only be set for header files that are included by a significant number of other C++ modules.
		public string SharedPCHHeaderFile = String.Empty;

		public enum PCHUsageMode
		{
			/// Default: Engine modules use shared PCHs, game modules do not
			Default,

			/// Never use shared PCHs.  Always generate a unique PCH for this module if appropriate
			NoSharedPCHs,

			/// Shared PCHs are OK!
			UseSharedPCHs
		}

		/// Precompiled header usage for this module
		public PCHUsageMode PCHUsage = PCHUsageMode.Default;

		/** Use run time type information */
		public bool bUseRTTI = false;

		/** Enable buffer security checks.  This should usually be enabled as it prevents severe security risks. */
		public bool bEnableBufferSecurityChecks = true;

		/** Enable exception handling */
		public bool bEnableExceptions = false;

		/** Enable warnings for shadowed variables */
		public bool bEnableShadowVariableWarnings = true;

		/** If true and unity builds are enabled, this module will build without unity. */
		public bool bFasterWithoutUnity = false;

		/** Overrides BuildConfiguration.MinFilesUsingPrecompiledHeader if non-zero. */
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/// List of modules with header files that our module's public headers needs access to, but we don't need to "import" or link against.
		public List<string> PublicIncludePathModuleNames = new List<string>();

		/// List of public dependency module names.  These are modules that are required by our public source files.
		public List<string> PublicDependencyModuleNames = new List<string>();

		/// List of modules with header files that our module's private code files needs access to, but we don't need to "import" or link against.
		public List<string> PrivateIncludePathModuleNames = new List<string>();

		/// List of private dependency module names.  These are modules that our private code depends on but nothing in our public
		/// include files depend on.
		public List<string> PrivateDependencyModuleNames = new List<string>();

		/// List of module dependencies that should be treated as circular references.  This modules must have already been added to
		/// either the public or private dependent module list.
		public List<string> CircularlyReferencedDependentModules = new List<string>();

		/// System include paths.  These are public stable header file directories that are not checked when resolving header dependencies.
		public List<string> PublicSystemIncludePaths = new List<string>();

		/// List of all paths to include files that are exposed to other modules
		public List<string> PublicIncludePaths = new List<string>();

		/// List of all paths to this module's internal include files, not exposed to other modules
		public List<string> PrivateIncludePaths = new List<string>();

		/// List of library paths - typically used for External (third party) modules
		public List<string> PublicLibraryPaths = new List<string>();

		/// List of addition libraries - typically used for External (third party) modules
		public List<string> PublicAdditionalLibraries = new List<string>();

		// List of frameworks
		public List<string> PublicFrameworks = new List<string>();

		// List of weak frameworks (for OS version transitions)
		public List<string> PublicWeakFrameworks = new List<string>();

		/// List of addition frameworks - typically used for External (third party) modules on Mac and iOS
		public List<UEBuildFramework> PublicAdditionalFrameworks = new List<UEBuildFramework>();

		/// List of addition resources that should be copied to the app bundle for Mac or iOS
		public List<UEBuildBundleResource> AdditionalBundleResources = new List<UEBuildBundleResource>();

		/// For builds that execute on a remote machine (e.g. iOS), this list contains additional files that
		/// need to be copied over in order for the app to link successfully.  Source/header files and PCHs are
		/// automatically copied.  Usually this is simply a list of precompiled third party library dependencies.
		public List<string> PublicAdditionalShadowFiles = new List<string>();

		/// List of delay load DLLs - typically used for External (third party) modules
		public List<string> PublicDelayLoadDLLs = new List<string>();

		/// Additional compiler definitions for this module
		public List<string> Definitions = new List<string>();

		/** CLR modules only: The assemblies referenced by the module's private implementation. */
		public List<string> PrivateAssemblyReferences = new List<string>();

		/// Addition modules this module may require at run-time 
		public List<string> DynamicallyLoadedModuleNames = new List<string>();

        /// Extra modules this module may require at run time, that are on behalf of another platform (i.e. shader formats and the like)
        public List<string> PlatformSpecificDynamicallyLoadedModuleNames = new List<string>();

		/// List of files which this module depends on at runtime. These files will be staged along with the target.
		public List<RuntimeDependency> RuntimeDependencies = new List<RuntimeDependency>();

		/// <summary>
		/// Property for the directory containing this module. Useful for adding paths to third party dependencies.
		/// </summary>
		public string ModuleDirectory
		{
			get
			{
				return Path.GetDirectoryName(RulesCompiler.GetModuleFilename(GetType().Name));
			}
		}

		/// <summary>
		/// Add the given ThirdParty modules as static private dependencies
		///	Statically linked to this module, meaning they utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicStaticDependencies function.
		/// </summary>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddThirdPartyPrivateStaticDependencies(TargetInfo Target, params string[] InModuleNames)
		{
			if (UnrealBuildTool.RunningRocket() == false || Target.Type == TargetRules.TargetType.Game)
			{
				PrivateDependencyModuleNames.AddRange(InModuleNames);
			}
		}

		/// <summary>
		/// Add the given ThirdParty modules as dynamic private dependencies
		///	Dynamically linked to this module, meaning they do not utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicDynamicDependencies function.
		/// </summary>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddThirdPartyPrivateDynamicDependencies(TargetInfo Target, params string[] InModuleNames)
		{
			if (UnrealBuildTool.RunningRocket() == false || Target.Type == TargetRules.TargetType.Game)
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
				AddThirdPartyPrivateStaticDependencies(Target, "PhysX");
				Definitions.Add("WITH_PHYSX=1");
				if (UEBuildConfiguration.bCompileAPEX == true)
				{
					AddThirdPartyPrivateStaticDependencies(Target, "APEX");
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

            if(UEBuildConfiguration.bRuntimePhysicsCooking == true)
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
				AddThirdPartyPrivateStaticDependencies(Target, "Box2D");
			}

			// Box2D included define (required because pointer types may be in public exported structures)
			Definitions.Add(string.Format("WITH_BOX2D={0}", bSupported ? 1 : 0));
		}

		/** Redistribution override flag for this module. */
		public bool? IsRedistributableOverride { get; set; }

		/**
		 * Reads additional dependencies array for project module from project file and fills PrivateDependencyModuleNames. 
		 *
		 * @param ProjectFile A path to the .uproject file.
		 * @param ModuleName Name of the module.
		 */
		public void ReadAdditionalDependencies(string ProjectFile, string ModuleName)
		{
			// Create a case-insensitive dictionary of the contents
			Dictionary<string, object> Descriptor = fastJSON.JSON.Instance.ToObject<Dictionary<string, object>>(File.ReadAllText(ProjectFile));
			Descriptor = new Dictionary<string,object>(Descriptor, StringComparer.InvariantCultureIgnoreCase);

			// Get the list of plugins
			object ModulesObject;
			if (Descriptor.TryGetValue("Modules", out ModulesObject))
			{
				foreach(var ModuleObject in (ModulesObject as object[]).Cast<Dictionary<string, object>>())
				{
					object NameObject;
					object AdditionalDependenciesObject;

					if(!ModuleObject.TryGetValue("Name", out NameObject)
						|| !(NameObject as string).Equals(ModuleName)
						|| !ModuleObject.TryGetValue("AdditionalDependencies", out AdditionalDependenciesObject))
					{
						continue;
					}

					foreach (var AdditionalDependency in (AdditionalDependenciesObject as object[]).Cast<string>())
					{
						if(!PrivateDependencyModuleNames.Contains(AdditionalDependency))
						{
							PrivateDependencyModuleNames.Add(AdditionalDependency);
						}
					}
					break;
				}
			}
		}
	}

	/// <summary>
	/// TargetRules is a data structure that contains the rules for defining a target (application/executable)
	/// </summary>
	public abstract class TargetRules
	{
		/// Type of target
		[Serializable]
		public enum TargetType
		{
			/// Cooked monolithic game executable (GameName.exe).  Also used for a game-agnostic engine executable (UE4Game.exe or RocketGame.exe)
			Game,

			/// Uncooked modular editor executable and DLLs (UE4Editor.exe, UE4Editor*.dll, GameName*.dll)
			Editor,

            /// Cooked monolithic game client executable (GameNameClient.exe, but no server code)
            Client,

			/// Cooked monolithic game server executable (GameNameServer.exe, but no client code)
			Server,

			/// Program (standalone program, e.g. ShaderCompileWorker.exe, can be modular or monolithic depending on the program)
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
		/// Allow a target to specify a preferred sub-platform.
		/// Can be used to target a build using sub platform specifics.
		/// </summary>
		public string PreferredSubPlatform = String.Empty;

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
		/// A list of additional plugins which need to be included in this target. This allows referencing non-optional plugin modules
		/// which cannot be disabled, and allows building against specific modules in program targets which do not fit the categories
		/// in ModuleHostType.
		/// </summary>
		public List<string> AdditionalPlugins = new List<string>();		

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
				if( String.IsNullOrEmpty( ConfigurationNameVar ) )
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
		/// Get the supported platforms for this target
		/// </summary>
		/// <param name="OutPlatforms">The list of platforms supported</param>
		/// <returns>true if successful, false if not</returns>
		public virtual bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
		{
			if(Type == TargetType.Program)
			{
				// By default, all programs are desktop only.
				return UnrealBuildTool.GetAllDesktopPlatforms(ref OutPlatforms, false);
			}
			else if(IsEditorType(Type))
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
						if( IsEditorType( Type ) && 
							( Config == UnrealTargetConfiguration.Shipping || Config == UnrealTargetConfiguration.Test ) )
						{
							// We don't currently support a "shipping" editor config
						}
						else if( !bIncludeTestAndShippingConfigs && 
							( Config == UnrealTargetConfiguration.Shipping || Config == UnrealTargetConfiguration.Test ) )
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
			return UnrealBuildTool.RunningRocket() || (Target.Type != TargetType.Program && !Target.IsMonolithic);
		}

		/// <summary>
		/// Allows the target to specify modules which can be precompiled with the -Precompile/-UsePrecompiled arguments to UBT. 
		/// All dependencies of the specified modules will be included.
		/// </summary>
		/// <param name="Target">The target information, such as platform and configuration</param>
		/// <param name="ModuleNames">List which receives module names to precompile</param>
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
            var Result = new List<UnrealTargetPlatform>{HostPlatform};
            // hack to set up the templates without adding anything to their .targets.cs files
            if (!String.IsNullOrEmpty(TargetName) && TargetName.StartsWith("TP_"))
            {
				if (HostPlatform == UnrealTargetPlatform.Win64)
				{
					Result.Add(UnrealTargetPlatform.IOS);
					Result.Add(UnrealTargetPlatform.Android);
				}
                else if (HostPlatform == UnrealTargetPlatform.Mac)
                {
					Result.Add(UnrealTargetPlatform.IOS);
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
            var Result = new List<UnrealTargetPlatform> {};            
            return Result;
        }
        /// <summary>
        /// Return a list of configs for target platforms for the monolithic
        /// </summary>
        /// <returns>a list of configs for a target platforms for the monolithic</returns>        
        public virtual List<UnrealTargetConfiguration> GUBP_GetConfigs_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
        {
            return new List<UnrealTargetConfiguration>{UnrealTargetConfiguration.Development};
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
            var Result = new GUBPProjectOptions();
            // hack to set up the templates without adding anything to their .targets.cs files
			// tweaked to include FP_ folders too - which are temporary
            if (!String.IsNullOrEmpty(TargetName) && ( TargetName.StartsWith("TP_") || TargetName.StartsWith("FP_")) )
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
            var MacOption = HostPlatform == UnrealTargetPlatform.Mac ? " -Mac" : "";
            var Result = new Dictionary<string, string>();
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
            var Result = new Dictionary<string, string>();
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
            var Result = new Dictionary<string, string>();
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


	public class RulesCompiler
	{
		/// <summary>
		/// Helper class to avoid adding extra conditions when getting file extensions and suffixes for
		/// rule files in FindAllRulesFilesRecursively.
		/// </summary>
		public class RulesTypePropertiesAttribute : Attribute
		{
			public string Suffix;
			public string Extension;
			public RulesTypePropertiesAttribute(string Suffix, string Extension)
			{
				this.Suffix = Suffix;
				this.Extension = Extension;
			}
		}

		public enum RulesFileType
		{
			/// *.Build.cs files
			[RulesTypeProperties(Suffix: "Build", Extension: ".cs")]
			Module,

			/// *.Target.cs files
			[RulesTypeProperties(Suffix: "Target", Extension: ".cs")]
			Target,

			/// *.Automation.cs files
			[RulesTypeProperties(Suffix: "Automation", Extension: ".cs")]
			Automation,

			/// *.Automation.csproj files
			[RulesTypeProperties(Suffix: "Automation", Extension: ".csproj")]
			AutomationModule
		}

		class RulesFileCache
		{
			/// List of rules file paths for each of the known types in RulesFileType
			public List<string>[] RulesFilePaths = new List<string>[ typeof( RulesFileType ).GetEnumValues().Length ];
		}


		private static void FindAllRulesFilesRecursively( DirectoryInfo DirInfo, RulesFileCache RulesFileCache )
		{
			if( DirInfo.Exists )
			{
				var RulesFileTypeEnum = typeof(RulesFileType);
				bool bFoundModuleRulesFile = false;
				var RulesFileTypes = typeof( RulesFileType ).GetEnumValues();
				foreach( RulesFileType CurRulesType in RulesFileTypes )
				{
					// Get the suffix and extension associated with this RulesFileType enum value.
					var MemberInfo = RulesFileTypeEnum.GetMember(CurRulesType.ToString());
					var Attributes = MemberInfo[0].GetCustomAttributes(typeof(RulesTypePropertiesAttribute), false);
					var EnumProperties = (RulesTypePropertiesAttribute)Attributes[0];
					
					var SearchRuleSuffix = "." + EnumProperties.Suffix + EnumProperties.Extension; // match files with the right suffix and extension.
					var FilesInDirectory = DirInfo.GetFiles("*" + EnumProperties.Extension);
					foreach (var RuleFile in FilesInDirectory)
					{
						// test if filename has the appropriate suffix.
						// this handles filenames such as Foo.build.cs, Foo.Build.cs, foo.bUiLd.cs to fix bug 266743 on platforms where case-sensitivity matters
						if (RuleFile.Name.EndsWith(SearchRuleSuffix, StringComparison.InvariantCultureIgnoreCase))
						{
							// Skip Uncooked targets, as those are no longer valid.  This is just for easier backwards compatibility with existing projects.
							// @todo: Eventually we can eliminate this conditional and just allow it to be an error when these are compiled
							if( CurRulesType != RulesFileType.Target || !RuleFile.Name.EndsWith( "Uncooked" + SearchRuleSuffix, StringComparison.InvariantCultureIgnoreCase ) )
							{
								if (RulesFileCache.RulesFilePaths[(int)CurRulesType] == null)
								{
									RulesFileCache.RulesFilePaths[(int)CurRulesType] = new List<string>();
								}

								// Convert file info to the full file path for this file and update our cache
								RulesFileCache.RulesFilePaths[(int)CurRulesType].Add(RuleFile.FullName);

								// NOTE: Multiple rules files in the same folder are supported.  We'll continue iterating along.
								if( CurRulesType == RulesFileType.Module )
								{
									bFoundModuleRulesFile = true;
								}
							}
							else
							{
								Log.TraceVerbose("Skipped deprecated Target rules file with Uncooked extension: " + RuleFile.Name );
							}
						}
					}
				}

				// Only recurse if we didn't find a module rules file.  In the interest of performance and organizational sensibility
				// we don't want to support folders with Build.cs files containing other folders with Build.cs files.  Performance-
				// wise, this is really important to avoid scanning every folder in the Source/ThirdParty directory, for example.
				if( !bFoundModuleRulesFile )
				{
					// Add all the files recursively
					foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories() )
					{
						if( SubDirInfo.Name.Equals( "Intermediate", StringComparison.InvariantCultureIgnoreCase ) )
						{
							Console.WriteLine( "WARNING: UnrealBuildTool found an Intermediate folder while looking for rules '{0}'.  It should only ever be searching under 'Source' folders -- an Intermediate folder is unexpected and will greatly decrease iteration times!", SubDirInfo.FullName );
						}
						FindAllRulesFilesRecursively(SubDirInfo, RulesFileCache);
					}
				}
			}
		}

		/// Map of root folders to a cached list of all UBT-related source files in that folder or any of its sub-folders.
		/// We cache these file names so we can avoid searching for them later on.
		static Dictionary<string, RulesFileCache> RootFolderToRulesFileCache = new Dictionary<string, RulesFileCache>();

		/// Name of the assembly file to cache rules data within
		static string AssemblyName = String.Empty;

		/// List of all game folders that we will be able to search for rules files within.  This must be primed at startup.
		public static List<string> AllGameFolders
		{
			get;
			private set;
		}

		/// External folders to also search for rules files
		static List<string> ForeignPlugins;

		/// <summary>
		/// Sets which game folders to look at when harvesting for rules source files.  This must be called before
		/// other functions in the RulesCompiler.  The idea here is that we can actually cache rules files for multiple
		/// games in a single assembly, if necessary.  In practice, multiple game folder's rules should only be cached together
		/// when generating project files.
		/// </summary>
		/// <param name="GameFolders">List of all game folders that rules files will ever be requested for</param>
		/// <param name="InExtraPluginFolders">List of additional folders </param>
		public static void SetAssemblyNameAndGameFolders( string AssemblyName, List<string> GameFolders, List<string> InForeignPlugins = null)
		{
			RulesCompiler.AssemblyName = AssemblyName + "ModuleRules";

			AllGameFolders = new List<string>();
			AllGameFolders.AddRange( GameFolders );

			ForeignPlugins = (InForeignPlugins == null)? null : new List<string>(InForeignPlugins);
		}



		public static List<string> FindAllRulesSourceFiles( RulesFileType RulesFileType, List<string> AdditionalSearchPaths )
		{
			List<string> Folders = new List<string>();

			// Add all engine source (including third party source)
			Folders.Add( Path.Combine( ProjectFileGenerator.EngineRelativePath, "Source" ) );

			// @todo plugin: Disallow modules from including plugin modules as dependency modules? (except when the module is part of that plugin)

			// Get all the root folders for plugins
			List<string> RootFolders = new List<string>();
			RootFolders.Add(ProjectFileGenerator.EngineRelativePath);
			RootFolders.AddRange(AllGameFolders);

			// Find all the plugin source directories
			foreach(string RootFolder in RootFolders)
			{
				string PluginsFolder = Path.Combine(RootFolder, "Plugins");
				foreach(string PluginFile in Plugins.EnumeratePlugins(PluginsFolder))
				{
					string PluginDirectory = Path.GetDirectoryName(PluginFile);
					Folders.Add(Path.Combine(PluginDirectory, "Source"));
				}
			}

			// Add all the extra plugin folders
			if( ForeignPlugins != null )
			{
				foreach(string ForeignPlugin in ForeignPlugins)
				{
					string PluginDirectory = Path.GetDirectoryName(Path.GetFullPath(ForeignPlugin));
					Folders.Add(Path.Combine(PluginDirectory, "Source"));
				}
			}

			// Add in the game folders to search
			if( AllGameFolders != null )
			{
				foreach( var GameFolder in AllGameFolders )
				{
					var GameSourceFolder = Path.GetFullPath(Path.Combine( GameFolder, "Source" ));
					Folders.Add( GameSourceFolder );
					var GameIntermediateSourceFolder = Path.GetFullPath(Path.Combine(GameFolder, "Intermediate", "Source"));
					Folders.Add(GameIntermediateSourceFolder);
				}
			}

			// Process the additional search path, if sent in
			if( AdditionalSearchPaths != null )
			{
				foreach( var AdditionalSearchPath in AdditionalSearchPaths )
				{
					if (!string.IsNullOrEmpty(AdditionalSearchPath))
					{
						if (Directory.Exists(AdditionalSearchPath))
						{
							Folders.Add(AdditionalSearchPath);
						}
						else
						{
							throw new BuildException( "Couldn't find AdditionalSearchPath for rules source files '{0}'", AdditionalSearchPath );
						}
					}
				}
			}

			var SourceFiles = new List<string>();

			// Iterate over all the folders to check
			foreach( string Folder in Folders )
			{
				// Check to see if we've already cached source files for this folder
				RulesFileCache FolderRulesFileCache;
				if (!RootFolderToRulesFileCache.TryGetValue(Folder, out FolderRulesFileCache))
				{
					FolderRulesFileCache = new RulesFileCache();
					FindAllRulesFilesRecursively(new DirectoryInfo(Folder), FolderRulesFileCache);
					RootFolderToRulesFileCache[Folder] = FolderRulesFileCache;

					if (BuildConfiguration.bPrintDebugInfo)
					{
						foreach (var CurType in Enum.GetValues(typeof(RulesFileType)))
						{
							var RulesFiles = FolderRulesFileCache.RulesFilePaths[(int)CurType];
							if (RulesFiles != null)
							{
								Log.TraceVerbose("Found {0} rules files for folder {1} of type {2}", RulesFiles.Count, Folder, CurType.ToString());
							}
						}
					}
				}

				var RulesFilePathsForType = FolderRulesFileCache.RulesFilePaths[(int)RulesFileType];
				if (RulesFilePathsForType != null)
				{
					foreach (string RulesFilePath in RulesFilePathsForType)
					{
						if (!SourceFiles.Contains(RulesFilePath))
						{
							SourceFiles.Add(RulesFilePath);
						}
					}
				}
			}

			return SourceFiles;
		}

		/// Assembly that contains object types for module rules definitions, loaded (or compiled) on demand */
		private static Assembly RulesAssembly = null;

		/// Maps module names to their actual xxx.Module.cs file on disk
		private static Dictionary<string, string> ModuleNameToModuleFileMap = new Dictionary<string, string>( StringComparer.InvariantCultureIgnoreCase );

		/// Maps target names to their actual xxx.Target.cs file on disk
		private static Dictionary<string, string> TargetNameToTargetFileMap = new Dictionary<string, string>( StringComparer.InvariantCultureIgnoreCase );

		private class LoadedAssemblyData
		{
			public LoadedAssemblyData(Assembly InAssembly, List<string> InGameFolders)
			{
				ExistingAssembly    = InAssembly;
				ExistingGameFolders = InGameFolders;
			}

			public Assembly     ExistingAssembly     { get; private set; }
			public List<string> ExistingGameFolders  { get; private set; }
		}

		/// Map of assembly names we've already compiled and loaded to their Assembly and list of game folders.  This is used to prevent
		/// trying to recompile the same assembly when ping-ponging between different types of targets
		private static Dictionary<string, LoadedAssemblyData> LoadedAssemblyMap = new Dictionary<string, LoadedAssemblyData>(StringComparer.InvariantCultureIgnoreCase);

		private static void ConditionallyCompileAndLoadRulesAssembly()
		{
			if( String.IsNullOrEmpty( AssemblyName ) )
			{
				throw new BuildException( "Module or target rules data was requested, but not rules assembly name was set yet!" );
			}

			// Did we already have a RulesAssembly and cached data about rules files and modules?  If so, then we'll
			// check to see if we need to flush everything and start over.  This can happen if UBT wants to built
			// different targets in a single invocation, or when generating project files before or after building
			// a target
			LoadedAssemblyData LoadedAssembly;
			if (LoadedAssemblyMap.TryGetValue(AssemblyName, out LoadedAssembly))
			{
				Assembly     ExistingAssembly    = LoadedAssembly.ExistingAssembly;
				List<string> ExistingGameFolders = LoadedAssembly.ExistingGameFolders;

				RulesAssembly = ExistingAssembly;

				// Make sure the game folder list wasn't changed since we last compiled this assembly
				if( ExistingGameFolders != AllGameFolders )	// Quick-check pointers first to avoid iterating
				{
					var AnyGameFoldersDifferent = false;
					if( ExistingGameFolders.Count != AllGameFolders.Count )
					{
						AnyGameFoldersDifferent = true;
					}
					else
					{
						foreach( var NewGameFolder in AllGameFolders )
						{
							if( !ExistingGameFolders.Contains( NewGameFolder ) )
							{
								AnyGameFoldersDifferent = true;
								break;
							}
						}
						foreach( var OldGameFolder in ExistingGameFolders )
						{
							if( !AllGameFolders.Contains( OldGameFolder ) )
							{
								AnyGameFoldersDifferent = true;
								break;
							}
						}
					}

					if( AnyGameFoldersDifferent )
					{
						throw new BuildException( "SetAssemblyNameAndGameFolders() was called with an assembly name that had already been compiled, but with DIFFERENT game folders.  This is not allowed." );
					}
				}

				return;
			}

			RulesAssembly = null;

			var AdditionalSearchPaths = new List<string>();

			if (UnrealBuildTool.HasUProjectFile())
			{
				// Add the game project's source folder
				var ProjectSourceDirectory = Path.Combine( UnrealBuildTool.GetUProjectPath(), "Source" );
				if( Directory.Exists( ProjectSourceDirectory ) )
				{
					AdditionalSearchPaths.Add( ProjectSourceDirectory );
				}
				// Add the games project's intermediate source folder
				var ProjectIntermediateSourceDirectory = Path.Combine(UnrealBuildTool.GetUProjectPath(), "Intermediate", "Source");
				if (Directory.Exists(ProjectIntermediateSourceDirectory))
				{
					AdditionalSearchPaths.Add(ProjectIntermediateSourceDirectory);
				}
			}
			var ModuleFileNames = FindAllRulesSourceFiles(RulesFileType.Module, AdditionalSearchPaths);

			var AssemblySourceFiles = new List<string>();
			AssemblySourceFiles.AddRange( ModuleFileNames );
			if( AssemblySourceFiles.Count == 0 )
			{
				throw new BuildException("No module rules source files were found in any of the module base directories!");
			}
			var TargetFileNames = FindAllRulesSourceFiles(RulesFileType.Target, AdditionalSearchPaths);
			AssemblySourceFiles.AddRange( TargetFileNames );

			// Create a path to the assembly that we'll either load or compile
			string BaseIntermediatePath = UnrealBuildTool.HasUProjectFile() ?
				Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.BaseIntermediateFolder) : BuildConfiguration.BaseIntermediatePath;

			string OutputAssemblyPath = Path.GetFullPath(Path.Combine(BaseIntermediatePath, "BuildRules", AssemblyName + ".dll"));

			RulesAssembly = DynamicCompilation.CompileAndLoadAssembly( OutputAssemblyPath, AssemblySourceFiles );

			// Setup the module map
			foreach( var CurModuleFileName in ModuleFileNames )
			{
				var CleanFileName = Utils.CleanDirectorySeparators( CurModuleFileName );
				var ModuleName = Path.GetFileNameWithoutExtension( Path.GetFileNameWithoutExtension( CleanFileName ) );	// Strip both extensions
				if( !ModuleNameToModuleFileMap.ContainsKey( ModuleName ) )
				{
					ModuleNameToModuleFileMap.Add( ModuleName, CurModuleFileName );
				}
			}

			// Setup the target map
			foreach( var CurTargetFileName in TargetFileNames )
			{
				var CleanFileName = Utils.CleanDirectorySeparators( CurTargetFileName );
				var TargetName = Path.GetFileNameWithoutExtension( Path.GetFileNameWithoutExtension( CleanFileName ) );	// Strip both extensions
				if( !TargetNameToTargetFileMap.ContainsKey( TargetName ) )
				{
					TargetNameToTargetFileMap.Add( TargetName, CurTargetFileName );
				}
			}

			// Remember that we loaded this assembly
			var RulesAssemblyName = Path.GetFileNameWithoutExtension( RulesAssembly.Location );
			LoadedAssemblyMap[RulesAssemblyName] = new LoadedAssemblyData(RulesAssembly, AllGameFolders);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="InModuleName"></param>
		/// <returns></returns>
		public static string GetModuleFilename(string InModuleName)
		{
			// Make sure the module file is known to us
			if (!ModuleNameToModuleFileMap.ContainsKey(InModuleName))
			{
				return "";
			}

			// Return the module file name to the caller
			return ModuleNameToModuleFileMap[InModuleName];
		}

		public static string GetTargetFilename(string InTargetName)
		{
			// Make sure the target file is known to us
			if (!TargetNameToTargetFileMap.ContainsKey(InTargetName))
			{
				return "";
			}

			// Return the target file name to the caller
			return TargetNameToTargetFileMap[InTargetName];
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="InModuleName"></param>
		/// <returns></returns>
		public static bool IsRocketProjectModule(string InModuleName)
		{
			if (UnrealBuildTool.HasUProjectFile() == false)
			{
				return false;
			}

			string Filename = GetModuleFilename(InModuleName);
			if (string.IsNullOrEmpty(Filename))
			{
				return false;
			}

			return (Utils.IsFileUnderDirectory( Filename, UnrealBuildTool.GetUProjectPath() ));
		}

		/// <summary>
		/// Creates an instance of a module rules descriptor object for the specified module name
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="Target">Information about the target associated with this module</param>
		/// <param name="Rules">Output </param>
		/// <returns>Compiled module rule info</returns>
		public static bool TryCreateModuleRules( string ModuleName, TargetInfo Target, out ModuleRules Rules )
		{
			if(GetModuleFilename( ModuleName ) == null)
			{
				Rules = null;
				return false;
			}
			else
			{
				Rules = CreateModuleRules( ModuleName, Target );
				return true;
			}
		}

		/// <summary>
		/// Creates an instance of a module rules descriptor object for the specified module name
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="Target">Information about the target associated with this module</param>
		/// <returns>Compiled module rule info</returns>
		public static ModuleRules CreateModuleRules( string ModuleName, TargetInfo Target )
		{
			string ModuleFileName;
			return CreateModuleRules(ModuleName, Target, out ModuleFileName);
		}

		/// <summary>
		/// Creates an instance of a module rules descriptor object for the specified module name
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="Target">Information about the target associated with this module</param>
		/// <param name="ModuleFileName">The original source file name for the Module.cs file for this module</param>
		/// <returns>Compiled module rule info</returns>
		public static ModuleRules CreateModuleRules( string ModuleName, TargetInfo Target, out string ModuleFileName )
		{
			ConditionallyCompileAndLoadRulesAssembly();
			var AssemblyFileName = Path.GetFileNameWithoutExtension( RulesAssembly.Location );

			// Currently, we expect the user's rules object type name to be the same as the module name
			var ModuleTypeName = ModuleName;

			// Make sure the module file is known to us
			if( !ModuleNameToModuleFileMap.ContainsKey( ModuleName ) )
			{
				throw new MissingModuleException( ModuleName );
			}

			// Return the module file name to the caller
			ModuleFileName = ModuleNameToModuleFileMap[ ModuleName ];

			UnrealTargetPlatform LocalPlatform = Target.Platform;
			UnrealTargetConfiguration LocalConfiguration = Target.Configuration;
			TargetInfo LocalTarget = new TargetInfo(LocalPlatform, LocalConfiguration, Target.Type.Value, Target.bIsMonolithic.Value);

			// The build module must define a type named 'Rules' that derives from our 'ModuleRules' type.  
			var RulesObjectType = RulesAssembly.GetType( ModuleName );

			if (RulesObjectType == null)
			{
				// Temporary hack to avoid System namespace collisions
				// @todo projectfiles: Make rules assemblies require namespaces.
				RulesObjectType = RulesAssembly.GetType("UnrealBuildTool.Rules." + ModuleName);
			}

			if( RulesObjectType == null )
			{
				throw new BuildException( "Expecting to find a type to be declared in a module rules named '{0}' in {1}.  This type must derive from the 'ModuleRules' type defined by Unreal Build Tool.", ModuleTypeName, RulesAssembly.FullName );
			}

			// Create an instance of the module's rules object
			ModuleRules RulesObject;
			try
			{
				RulesObject = (ModuleRules)Activator.CreateInstance(RulesObjectType, LocalTarget);
			}
			catch( Exception Ex )
			{
				throw new BuildException( Ex, "Unable to instantiate instance of '{0}' object type from compiled assembly '{1}'.  Unreal Build Tool creates an instance of your module's 'Rules' object in order to find out about your module's requirements.  The CLR exception details may provide more information:  {2}", ModuleTypeName, AssemblyFileName, Ex.ToString() );
			}

			// Have to do absolute here as this could be a project that is under the root
			var FullUProjectPath = string.IsNullOrWhiteSpace(UnrealBuildTool.GetUProjectPath())
				? ""
				: Path.GetFullPath(UnrealBuildTool.GetUProjectPath());
			var bProjectModule = string.IsNullOrWhiteSpace(FullUProjectPath)
				? false
				: Utils.IsFileUnderDirectory(ModuleFileName, FullUProjectPath);

			if (bProjectModule)
			{
				RulesObject.ReadAdditionalDependencies(UnrealBuildTool.GetUProjectFile(), ModuleName);
			}

			// Validate rules object
			{
				if( RulesObject.Type == ModuleRules.ModuleType.CPlusPlus )
				{
					if (RulesObject.PrivateAssemblyReferences.Count > 0)
					{
						throw new BuildException("Module rules for '{0}' may not specify PrivateAssemblyReferences unless it is a CPlusPlusCLR module type.", AssemblyFileName);
					}

					// Choose code optimization options based on module type (game/engine) if
					// default optimization method is selected.
					bool bIsEngineModule = Utils.IsFileUnderDirectory( ModuleFileName, ProjectFileGenerator.EngineRelativePath );
					if (RulesObject.OptimizeCode == ModuleRules.CodeOptimization.Default)
					{
						// Engine/Source and Engine/Plugins are considered 'Engine' code...
						if (bIsEngineModule)
						{
							// Engine module - always optimize (except Debug).
							RulesObject.OptimizeCode = ModuleRules.CodeOptimization.Always;
						}
						else
						{
							// Game module - do not optimize in Debug and DebugGame builds.
							RulesObject.OptimizeCode = ModuleRules.CodeOptimization.InNonDebugBuilds;
						}
					}

					// Disable shared PCHs for game modules by default
					if (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.Default)
					{
						// Note that bIsEngineModule includes Engine/Plugins, so Engine/Plugins will use shared PCHs.
						var IsProgramTarget = Target.Type != null && Target.Type == TargetRules.TargetType.Program;
						if (bIsEngineModule || IsProgramTarget)
						{
							// Engine module or plugin module -- allow shared PCHs
							RulesObject.PCHUsage = ModuleRules.PCHUsageMode.UseSharedPCHs;
						}
						else
						{
							// Game module.  Do not enable shared PCHs by default, because games usually have a large precompiled header of their own and compile times would suffer.
							RulesObject.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
						}
					}
				}
			}

			return RulesObject;
		}

		/// <summary>
		/// Determines whether the given module name is a game module (as opposed to an engine module)
		/// </summary>
		public static bool IsGameModule(string InModuleName)
		{
			string ModuleFileName = GetModuleFilename(InModuleName);
			return ModuleFileName.Length > 0 && !Utils.IsFileUnderDirectory(ModuleFileName, BuildConfiguration.RelativeEnginePath);
		}

		/// <summary>
		/// Add the standard default include paths to the given modulerules object
		/// </summary>
		/// <param name="InModuleName">The name of the module</param>
		/// <param name="InModuleFilename">The filename to the module rules file (Build.cs)</param>
		/// <param name="InModuleFileRelativeToEngineDirectory">The module file relative to the engine directory</param>
		/// <param name="IsGameModule">true if it is a game module, false if not</param>
		/// <param name="RulesObject">The module rules object itself</param>
		public static void AddDefaultIncludePathsToModuleRules(string InModuleName, string InModuleFilename, string InModuleFileRelativeToEngineDirectory, bool IsGameModule, ModuleRules RulesObject)
		{
			// Grab the absolute path of the Engine/Source folder for use later
			string AbsEngineSourceDirectory = Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source");
			AbsEngineSourceDirectory = Path.GetFullPath(AbsEngineSourceDirectory);
			AbsEngineSourceDirectory = AbsEngineSourceDirectory.Replace("\\", "/");

			// Find the module path relative to the Engine/Source folder
			string ModuleDirectoryRelativeToEngineSourceDirectory = Utils.MakePathRelativeTo(InModuleFilename, Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source"));
			ModuleDirectoryRelativeToEngineSourceDirectory = ModuleDirectoryRelativeToEngineSourceDirectory.Replace("\\", "/");
			// Remove the build.cs file from the directory if present
			if (ModuleDirectoryRelativeToEngineSourceDirectory.EndsWith("Build.cs", StringComparison.InvariantCultureIgnoreCase))
			{
				Int32 LastSlashIdx = ModuleDirectoryRelativeToEngineSourceDirectory.LastIndexOf("/");
				if (LastSlashIdx != -1)
				{
					ModuleDirectoryRelativeToEngineSourceDirectory = ModuleDirectoryRelativeToEngineSourceDirectory.Substring(0, LastSlashIdx + 1);
				}
			}
			else
			{
				throw new BuildException("Invalid module filename '{0}'.", InModuleFilename);
			}

			// Determine the 'Game/Source' folder
			//@todo.Rocket: This currently requires following our standard format for folder layout.
			//				Also, it assumes the module itself is not named Source...
			string GameSourceIncludePath = (IsGameModule == true) ? ModuleDirectoryRelativeToEngineSourceDirectory : "";
			if (string.IsNullOrEmpty(GameSourceIncludePath) == false)
			{
				Int32 SourceIdx = GameSourceIncludePath.IndexOf("/Source/");
				if (SourceIdx != -1)
				{
					GameSourceIncludePath = GameSourceIncludePath.Substring(0, SourceIdx + 8);
					RulesObject.PublicIncludePaths.Add(GameSourceIncludePath);
				}
			}

			// Setup the directories for Classes, Public, and Intermediate			
			string ClassesDirectory = Path.Combine(ModuleDirectoryRelativeToEngineSourceDirectory, "Classes/");	// @todo uht: Deprecate eventually.  Or force it to be manually specified...
			string PublicDirectory = Path.Combine(ModuleDirectoryRelativeToEngineSourceDirectory, "Public/");

			if(!Utils.IsFileUnderDirectory(InModuleFilename, AbsEngineSourceDirectory))
			{
				// This will be either the format 
				//		../<Game>/Source/<Module>
				// or
				//		c:/PATH/<Game>/Source/<Module>
                string SourceDirName = "Source";
                Int32 SourceSlashIdx = ModuleDirectoryRelativeToEngineSourceDirectory.IndexOf(SourceDirName);
				string SourceDirectoryPath = null;
				if(SourceSlashIdx != -1)
				{
					try
					{
                        SourceDirectoryPath = Path.GetFullPath(ModuleDirectoryRelativeToEngineSourceDirectory.Substring(0, SourceSlashIdx + SourceDirName.Length));
					}
					catch(Exception Exc)
					{
						throw new BuildException(Exc, "Failed to resolve module source directory for private include paths for module {0}.", InModuleName);
					}
				}
				else
				{
					//@todo. throw a build exception here?
				}

				// Resolve private include paths against the module source root so they are simpler and don't have to be engine root relative.
				if(SourceDirectoryPath != null)
				{
					List<string> ResolvedPrivatePaths = new List<string>();
					foreach(var PrivatePath in RulesObject.PrivateIncludePaths)
					{
						try
						{
							if(!Path.IsPathRooted(PrivatePath))
							{
								ResolvedPrivatePaths.Add(Path.Combine(SourceDirectoryPath, PrivatePath));
							}
							else
							{
								ResolvedPrivatePaths.Add(PrivatePath);
							}
						}
						catch(Exception Exc)
						{
							throw new BuildException(Exc, "Failed to resolve private include path {0}.", PrivatePath);
						}
					}
					RulesObject.PrivateIncludePaths = ResolvedPrivatePaths;
				}
			}

			string IncludePath_Classes = "";
			string IncludePath_Public = "";

			bool bModulePathIsRooted = Path.IsPathRooted(ModuleDirectoryRelativeToEngineSourceDirectory);

			string ClassesFolderName = (bModulePathIsRooted == false) ? Path.Combine(AbsEngineSourceDirectory, ClassesDirectory) : ClassesDirectory;
			if (Directory.Exists(ClassesFolderName) == true)
			{
				IncludePath_Classes = ClassesDirectory;
			}

			string PublicFolderName = (bModulePathIsRooted == false) ? Path.Combine(AbsEngineSourceDirectory, PublicDirectory) : PublicDirectory;
			if (Directory.Exists(PublicFolderName) == true)
			{
				IncludePath_Public = PublicDirectory;
			}

			// Add them if they are required...
			if (IncludePath_Classes.Length > 0)
			{
				RulesObject.PublicIncludePaths.Add(IncludePath_Classes);
			}
			if (IncludePath_Public.Length > 0)
			{
				RulesObject.PublicIncludePaths.Add(IncludePath_Public);

				// Add subdirectories of Public if present
				DirectoryInfo PublicInfo = new DirectoryInfo(IncludePath_Public);
				DirectoryInfo[] PublicSubDirs = PublicInfo.GetDirectories("*", SearchOption.AllDirectories);
				if (PublicSubDirs.Length > 0)
				{
					foreach (DirectoryInfo SubDir in PublicSubDirs)
					{
						string PartialDir = SubDir.FullName.Replace(PublicInfo.FullName, "");
						string NewDir = IncludePath_Public + PartialDir;
						NewDir = Utils.CleanDirectorySeparators(NewDir, '/');
						RulesObject.PublicIncludePaths.Add(NewDir);
					}
				}
			}
		}

		protected static bool GetTargetTypeAndRulesInstance(string InTargetName, TargetInfo InTarget, out System.Type OutRulesObjectType, out TargetRules OutRulesObject)
		{
			// The build module must define a type named '<TargetName>Target' that derives from our 'TargetRules' type.  
			OutRulesObjectType = RulesAssembly.GetType(InTargetName);
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
				var AssemblyFileName = Path.GetFileNameWithoutExtension(RulesAssembly.Location);
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
		/// <param name="TargetFileName">The original source file name of the Target.cs file for this target</param>
		/// <returns>The build target rules for the specified target</returns>
		public static TargetRules CreateTargetRules(string TargetName, TargetInfo Target, bool bInEditorRecompile, out string TargetFileName)
		{
			ConditionallyCompileAndLoadRulesAssembly();

			// Make sure the target file is known to us
			bool bFoundTargetName = TargetNameToTargetFileMap.ContainsKey(TargetName);
			if (bFoundTargetName == false)
			{
				if (UnrealBuildTool.RunningRocket())
				{
					//@todo Rocket: Remove this when full game support is implemented
					// If we are Rocket, they will currently only have an editor target.
					// See if that exists
					bFoundTargetName = TargetNameToTargetFileMap.ContainsKey(TargetName + "Editor");
					if (bFoundTargetName)
					{
						TargetName += "Editor";
					}
				}
			}

			if (bFoundTargetName == false)
			{
//				throw new BuildException("Couldn't find target rules file for target '{0}' in rules assembly '{1}'.", TargetName, RulesAssembly.FullName);
				string ExceptionMessage = "Couldn't find target rules file for target '";
				ExceptionMessage += TargetName;
				ExceptionMessage += "' in rules assembly '";
				ExceptionMessage += RulesAssembly.FullName;
				ExceptionMessage += "'.\n";

				ExceptionMessage += "Location: " + RulesAssembly.Location + "\n";

				ExceptionMessage += "Target rules found:\n";
				foreach (KeyValuePair<string, string> entry in TargetNameToTargetFileMap)
				{
					ExceptionMessage += "\t" + entry.Key + " - " + entry.Value + "\n";
				}

				throw new BuildException(ExceptionMessage);
			}

			// Return the target file name to the caller
			TargetFileName = TargetNameToTargetFileMap[ TargetName ];
	
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
						string TargetSourceFolder = TargetFileName;
						Int32 SourceFolderIndex = -1;
						if (Utils.IsRunningOnMono)
						{
							TargetSourceFolder = TargetSourceFolder.Replace("\\", "/");
							SourceFolderIndex = TargetSourceFolder.LastIndexOf("/Source/", StringComparison.InvariantCultureIgnoreCase);
						}
						else
						{
							TargetSourceFolder = TargetSourceFolder.Replace("/", "\\");
							SourceFolderIndex = TargetSourceFolder.LastIndexOf("\\Source\\", StringComparison.InvariantCultureIgnoreCase);
						}
						if (SourceFolderIndex != -1)
						{
							TargetSourceFolder = TargetSourceFolder.Substring(0, SourceFolderIndex + 8);
							foreach (KeyValuePair<string, string> CheckEntry in TargetNameToTargetFileMap)
							{
								if (CheckEntry.Value.StartsWith(TargetSourceFolder, StringComparison.InvariantCultureIgnoreCase))
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
		/// Creates a target object for the specified target name.
		/// </summary>
		/// <param name="GameFolder">Root folder for the target's game, if this is a game target</param>
		/// <param name="TargetName">Name of the target</param>
		/// <param name="Target">Information about the target associated with this target</param>
		/// <returns>The build target object for the specified build rules source file</returns>
		public static UEBuildTarget CreateTarget(TargetDescriptor Desc)
		{
			var CreateTargetStartTime = DateTime.UtcNow;

			string TargetFileName;
			TargetRules RulesObject = CreateTargetRules(Desc.TargetName, new TargetInfo(Desc.Platform, Desc.Configuration), Desc.bIsEditorRecompile, out TargetFileName);
			if (Desc.bIsEditorRecompile)
			{
				// Now that we found the actual Editor target, make sure we're no longer using the old TargetName (which is the Game target)
				var TargetSuffixIndex = RulesObject.TargetName.LastIndexOf("Target");
				Desc.TargetName = (TargetSuffixIndex > 0) ? RulesObject.TargetName.Substring(0, TargetSuffixIndex) : RulesObject.TargetName;
			}
			if ((ProjectFileGenerator.bGenerateProjectFiles == false) && (RulesObject.SupportsPlatform(Desc.Platform) == false))
			{
				if (UEBuildConfiguration.bCleanProject)
				{
					return null;
				}
				throw new BuildException("{0} does not support the {1} platform.", Desc.TargetName, Desc.Platform.ToString());
			}

			// Generate a build target from this rules module
			UEBuildTarget BuildTarget = null;
			switch (RulesObject.Type)
			{
				case TargetRules.TargetType.Game:
					BuildTarget = new UEBuildGame(Desc, RulesObject, TargetFileName);
					break;
				case TargetRules.TargetType.Editor:
					BuildTarget = new UEBuildEditor(Desc, RulesObject, TargetFileName);
					break;
                case TargetRules.TargetType.Client:
                    BuildTarget = new UEBuildClient(Desc, RulesObject, TargetFileName);
                    break;
				case TargetRules.TargetType.Server:
					BuildTarget = new UEBuildServer(Desc, RulesObject, TargetFileName);
					break;
				case TargetRules.TargetType.Program:
					BuildTarget = new UEBuildTarget(Desc, RulesObject, null, TargetFileName);
					break;
			}

			if( BuildConfiguration.bPrintPerformanceInfo )
			{ 
				var CreateTargetTime = (DateTime.UtcNow - CreateTargetStartTime).TotalSeconds;
				Log.TraceInformation( "CreateTarget for " + Desc.TargetName + " took " + CreateTargetTime + "s" );
			}

			if (BuildTarget == null)
			{
				throw new BuildException("Failed to create build target for '{0}'.", Desc.TargetName);
			}

			return BuildTarget;
		}

	}
}
