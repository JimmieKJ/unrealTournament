// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace UnrealBuildTool
{
	public enum SDKStatus
	{
		Valid,			// Desired SDK is installed and set up.
		Invalid,		// Could not find the desired SDK, SDK setup failed, etc.		
	};

	public abstract class UEBuildPlatformContext
	{
		private bool bInitializedProject = false;

		/// <summary>
		/// The specific platform that this context is for
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// The project file for the target being compiled
		/// </summary>
		public readonly FileReference ProjectFile;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InPlatform">The platform that this context is for</param>
		/// <param name="InProjectFile">The project to read settings from, if any</param>
		public UEBuildPlatformContext(UnrealTargetPlatform InPlatform, FileReference InProjectFile)
		{
			Platform = InPlatform;
			ProjectFile = InProjectFile;
		}

		/// <summary>
		/// Get a list of extra modules the platform requires.
		/// This is to allow undisclosed platforms to add modules they need without exposing information about the platform.
		/// </summary>
		/// <param name="Target">The target being build</param>
		/// <param name="ExtraModuleNames">List of extra modules the platform needs to add to the target</param>
		public virtual void AddExtraModules(TargetInfo Target, List<string> ExtraModuleNames)
		{
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public virtual void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
		}

		/// <summary>
		/// Gives the platform a chance to 'override' the configuration settings that are overridden on calls to RunUBT.
		/// </summary>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public virtual void ResetBuildConfiguration(UnrealTargetConfiguration InConfiguration)
		{
		}

		/// <summary>
		/// Validate the UEBuildConfiguration for this platform
		/// This is called BEFORE calling UEBuildConfiguration to allow setting
		/// various fields used in that function such as CompileLeanAndMean...
		/// </summary>
		public virtual void ValidateUEBuildConfiguration()
		{
		}

		/// <summary>
		/// Validate configuration for this platform
		/// NOTE: This function can/will modify BuildConfiguration!
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="InConfiguration"> The CPPTargetConfiguration being built</param>
		/// <param name="bInCreateDebugInfo">true if debug info is getting create, false if not</param>
		public virtual void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public abstract void SetUpEnvironment(UEBuildTarget InBuildTarget);

		/// <summary>
		/// Allow the platform to set an optional architecture
		/// </summary>
		public virtual string GetActiveArchitecture()
		{
			// by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
			return "";
		}

		/// <summary>
		/// Get name for architecture-specific directories (can be shorter than architecture name itself)
		/// </summary>
		public virtual string GetActiveArchitectureFolderName()
		{
			// by default, use the architecture name
			return GetActiveArchitecture();
		}

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public virtual void SetUpConfigurationEnvironment(TargetInfo Target, CPPEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			// Determine the C++ compile/link configuration based on the Unreal configuration.
			CPPTargetConfiguration CompileConfiguration;
			UnrealTargetConfiguration CheckConfig = Target.Configuration;

			switch (CheckConfig)
			{
				default:
				case UnrealTargetConfiguration.Debug:
					CompileConfiguration = CPPTargetConfiguration.Debug;
					if (BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
					{
						GlobalCompileEnvironment.Config.Definitions.Add("_DEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
					}
					else
					{
						GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
					}
					GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_DEBUG=1");
					break;
				case UnrealTargetConfiguration.DebugGame:
				// Individual game modules can be switched to be compiled in debug as necessary. By default, everything is compiled in development.
				case UnrealTargetConfiguration.Development:
					CompileConfiguration = CPPTargetConfiguration.Development;
					GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
					GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_DEVELOPMENT=1");
					break;
				case UnrealTargetConfiguration.Shipping:
					CompileConfiguration = CPPTargetConfiguration.Shipping;
					GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
					GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_SHIPPING=1");
					break;
				case UnrealTargetConfiguration.Test:
					CompileConfiguration = CPPTargetConfiguration.Shipping;
					GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
					GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_TEST=1");
					break;
			}

			// Set up the global C++ compilation and link environment.
			GlobalCompileEnvironment.Config.Target.Configuration = CompileConfiguration;
			GlobalLinkEnvironment.Config.Target.Configuration = CompileConfiguration;

			// Create debug info based on the heuristics specified by the user.
			GlobalCompileEnvironment.Config.bCreateDebugInfo =
				!BuildConfiguration.bDisableDebugInfo && ShouldCreateDebugInfo(CheckConfig);
			GlobalLinkEnvironment.Config.bCreateDebugInfo = GlobalCompileEnvironment.Config.bCreateDebugInfo;
		}

		/// <summary>
		/// Setup the project environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public virtual void SetUpProjectEnvironment()
		{
			if (!bInitializedProject)
			{
				ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(Platform, "Engine", DirectoryReference.FromFile(ProjectFile));
				bool bValue = UEBuildConfiguration.bCompileAPEX;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileApex", out bValue))
				{
					UEBuildConfiguration.bCompileAPEX = bValue;
				}

				bValue = UEBuildConfiguration.bCompileBox2D;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileBox2D", out bValue))
				{
					UEBuildConfiguration.bCompileBox2D = bValue;
				}

				bValue = UEBuildConfiguration.bCompileICU;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileICU", out bValue))
				{
					UEBuildConfiguration.bCompileICU = bValue;
				}

				bValue = UEBuildConfiguration.bCompileSimplygon;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileSimplygon", out bValue))
				{
					UEBuildConfiguration.bCompileSimplygon = bValue;
				}

                bValue = UEBuildConfiguration.bCompileSimplygonSSF;
                if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileSimplygonSSF", out bValue))
                {
                    UEBuildConfiguration.bCompileSimplygonSSF = bValue;
                }

				bValue = UEBuildConfiguration.bCompileLeanAndMeanUE;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileLeanAndMeanUE", out bValue))
				{
					UEBuildConfiguration.bCompileLeanAndMeanUE = bValue;
				}

				bValue = UEBuildConfiguration.bIncludeADO;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bIncludeADO", out bValue))
				{
					UEBuildConfiguration.bIncludeADO = bValue;
				}

				bValue = UEBuildConfiguration.bCompileRecast;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileRecast", out bValue))
				{
					UEBuildConfiguration.bCompileRecast = bValue;
				}

				bValue = UEBuildConfiguration.bCompileSpeedTree;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileSpeedTree", out bValue))
				{
					UEBuildConfiguration.bCompileSpeedTree = bValue;
				}

				bValue = UEBuildConfiguration.bCompileWithPluginSupport;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileWithPluginSupport", out bValue))
				{
					UEBuildConfiguration.bCompileWithPluginSupport = bValue;
				}

				bValue = UEBuildConfiguration.bCompilePhysXVehicle;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompilePhysXVehicle", out bValue))
				{
					UEBuildConfiguration.bCompilePhysXVehicle = bValue;
				}

				bValue = UEBuildConfiguration.bCompileFreeType;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileFreeType", out bValue))
				{
					UEBuildConfiguration.bCompileFreeType = bValue;
				}

				bValue = UEBuildConfiguration.bCompileForSize;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileForSize", out bValue))
				{
					UEBuildConfiguration.bCompileForSize = bValue;
				}

				bValue = UEBuildConfiguration.bCompileCEF3;
				if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileCEF3", out bValue))
				{
					UEBuildConfiguration.bCompileCEF3 = bValue;
				}

				bInitializedProject = true;
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public abstract bool ShouldCreateDebugInfo(UnrealTargetConfiguration Configuration);

		/// <summary>
		/// Creates a toolchain instance for the default C++ platform.
		/// </summary>
		/// <returns>New toolchain instance.</returns>
		public UEToolChain CreateToolChainForDefaultCppPlatform()
		{
			return CreateToolChain(UEBuildPlatform.GetBuildPlatform(Platform).DefaultCppPlatform);
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform. There should be a single toolchain instance per-target, as their may be
		/// state data and configuration cached between calls.
		/// </summary>
		/// <param name="Platform">The platform to create a toolchain for</param>
		/// <returns>New toolchain instance.</returns>
		public abstract UEToolChain CreateToolChain(CPPTargetPlatform Platform);

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <param name="DeploymentHandler">The output deployment handler</param>
		/// <returns>Deployment handler for this platform, or null if not required</returns>
		public abstract UEBuildDeploy CreateDeploymentHandler();
	}

	public abstract class UEBuildPlatform
	{
		private static Dictionary<UnrealTargetPlatform, UEBuildPlatform> BuildPlatformDictionary = new Dictionary<UnrealTargetPlatform, UEBuildPlatform>();

		// a mapping of a group to the platforms in the group (ie, Microsoft contains Win32 and Win64)
		static Dictionary<UnrealPlatformGroup, List<UnrealTargetPlatform>> PlatformGroupDictionary = new Dictionary<UnrealPlatformGroup, List<UnrealTargetPlatform>>();

		/// <summary>
		/// The corresponding target platform enum
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// The default C++ target platform to use
		/// </summary>
		public readonly CPPTargetPlatform DefaultCppPlatform;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InPlatform">The enum value for this platform</param>
		public UEBuildPlatform(UnrealTargetPlatform InPlatform, CPPTargetPlatform InDefaultCPPPlatform)
		{
			Platform = InPlatform;
			DefaultCppPlatform = InDefaultCPPPlatform;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform. Could be either a manual install or an AutoSDK.
		/// </summary>
		public abstract SDKStatus HasRequiredSDKsInstalled();

		/// <summary>
		/// Gets all the registered platforms
		/// </summary>
		/// <returns>Sequence of registered platforms</returns>
		public static IEnumerable<UnrealTargetPlatform> GetRegisteredPlatforms()
		{
			return BuildPlatformDictionary.Keys;
		}

		/// <summary>
		/// Attempt to convert a string to an UnrealTargetPlatform enum entry
		/// </summary>
		/// <returns>UnrealTargetPlatform.Unknown on failure (the platform didn't match the enum)</returns>
		public static UnrealTargetPlatform ConvertStringToPlatform(string InPlatformName)
		{
			// special case x64, to not break anything
			// @todo: Is it possible to remove this hack?
			if (InPlatformName.Equals("X64", StringComparison.InvariantCultureIgnoreCase))
			{
				return UnrealTargetPlatform.Win64;
			}

			// we can't parse the string into an enum because Enum.Parse is case sensitive, so we loop over the enum
			// looking for matches
			foreach (string PlatformName in Enum.GetNames(typeof(UnrealTargetPlatform)))
			{
				if (InPlatformName.Equals(PlatformName, StringComparison.InvariantCultureIgnoreCase))
				{
					// convert the known good enum string back to the enum value
					return (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), PlatformName);
				}
			}
			return UnrealTargetPlatform.Unknown;
		}

		/// <summary>
		/// Determines whether a given platform is available
		/// </summary>
		/// <param name="Platform">The platform to check for</param>
		/// <returns>True if it's available, false otherwise</returns>
		public static bool IsPlatformAvailable(UnrealTargetPlatform Platform)
		{
			return BuildPlatformDictionary.ContainsKey(Platform);
		}

		/// <summary>
		/// Register the given platforms UEBuildPlatform instance
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform to register with</param>
		/// <param name="InBuildPlatform"> The UEBuildPlatform instance to use for the InPlatform</param>
		public static void RegisterBuildPlatform(UEBuildPlatform InBuildPlatform)
		{
			if (BuildPlatformDictionary.ContainsKey(InBuildPlatform.Platform) == true)
			{
				Log.TraceWarning("RegisterBuildPlatform Warning: Registering build platform {0} for {1} when it is already set to {2}",
					InBuildPlatform.ToString(), InBuildPlatform.Platform.ToString(), BuildPlatformDictionary[InBuildPlatform.Platform].ToString());
				BuildPlatformDictionary[InBuildPlatform.Platform] = InBuildPlatform;
			}
			else
			{
				BuildPlatformDictionary.Add(InBuildPlatform.Platform, InBuildPlatform);
			}
		}

		/// <summary>
		/// Assign a platform as a member of the given group
		/// </summary>
		public static void RegisterPlatformWithGroup(UnrealTargetPlatform InPlatform, UnrealPlatformGroup InGroup)
		{
			// find or add the list of groups for this platform
			PlatformGroupDictionary.GetOrAddNew(InGroup).Add(InPlatform);
		}

		/// <summary>
		/// Retrieve the list of platforms in this group (if any)
		/// </summary>
		public static List<UnrealTargetPlatform> GetPlatformsInGroup(UnrealPlatformGroup InGroup)
		{
			List<UnrealTargetPlatform> PlatformList;
			PlatformGroupDictionary.TryGetValue(InGroup, out PlatformList);
			return PlatformList;
		}

		/// <summary>
		/// Enumerates all the platform groups for a given platform
		/// </summary>
		/// <param name="InPlatform">The platform to look for</param>
		/// <returns>List of platform groups that this platform is a member of</returns>
		public static IEnumerable<UnrealPlatformGroup> GetPlatformGroups(UnrealTargetPlatform Platform)
		{
			return PlatformGroupDictionary.Where(x => x.Value.Contains(Platform)).Select(x => x.Key);
		}

		/// <summary>
		/// Retrieve the IUEBuildPlatform instance for the given TargetPlatform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="bInAllowFailure"> If true, do not throw an exception and return null</param>
		/// <returns>UEBuildPlatform  The instance of the build platform</returns>
		public static UEBuildPlatform GetBuildPlatform(UnrealTargetPlatform InPlatform, bool bInAllowFailure = false)
		{
			if (BuildPlatformDictionary.ContainsKey(InPlatform) == true)
			{
				return BuildPlatformDictionary[InPlatform];
			}
			if (bInAllowFailure == true)
			{
				return null;
			}
			throw new BuildException("GetBuildPlatform: No BuildPlatform found for {0}", InPlatform.ToString());
		}

		/// <summary>
		/// Retrieve the IUEBuildPlatform instance for the given CPPTargetPlatform
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="bInAllowFailure"> If true, do not throw an exception and return null</param>
		/// <returns>UEBuildPlatform  The instance of the build platform</returns>
		public static UEBuildPlatform GetBuildPlatformForCPPTargetPlatform(CPPTargetPlatform InPlatform, bool bInAllowFailure = false)
		{
			UnrealTargetPlatform UTPlatform = UEBuildTarget.CPPTargetPlatformToUnrealTargetPlatform(InPlatform);
			if (BuildPlatformDictionary.ContainsKey(UTPlatform) == true)
			{
				return BuildPlatformDictionary[UTPlatform];
			}
			if (bInAllowFailure == true)
			{
				return null;
			}
			throw new BuildException("UEBuildPlatform::GetBuildPlatformForCPPTargetPlatform: No BuildPlatform found for {0}", InPlatform.ToString());
		}

		/// <summary>
		/// Allow all registered build platforms to modify the newly created module
		/// passed in for the given platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="">Name   The name of the module</param>
		/// <param name="Module">  The module rules</param>
		/// <param name="Target">  The target being build</param>
		/// <param name="Only">  If this is not unknown, then only run that platform</param>
		public static void PlatformModifyHostModuleRules(string ModuleName, ModuleRules Rules, TargetInfo Target, UnrealTargetPlatform Only = UnrealTargetPlatform.Unknown)
		{
			foreach (KeyValuePair<UnrealTargetPlatform, UEBuildPlatform> PlatformEntry in BuildPlatformDictionary)
			{
				if (Only == UnrealTargetPlatform.Unknown || PlatformEntry.Key == Only)
				{
					PlatformEntry.Value.ModifyModuleRulesForOtherPlatform(ModuleName, Rules, Target);
				}
			}
		}

		/// <summary>
		/// Returns the delimiter used to separate paths in the PATH environment variable for the platform we are executing on.
		/// </summary>
		public static String GetPathVarDelimiter()
		{
			switch (BuildHostPlatform.Current.Platform)
			{
				case UnrealTargetPlatform.Linux:
				case UnrealTargetPlatform.Mac:
					return ":";
				case UnrealTargetPlatform.Win32:
				case UnrealTargetPlatform.Win64:
					return ";";
				default:
					Log.TraceWarning("PATH var delimiter unknown for platform " + BuildHostPlatform.Current.Platform.ToString() + " using ';'");
					return ";";
			}
		}



		/// <summary>
		/// If this platform can be compiled with XGE
		/// </summary>
		public virtual bool CanUseXGE()
		{
			return true;
		}

		/// <summary>
		/// If this platform can be compiled with DMUCS/Distcc
		/// </summary>
		public virtual bool CanUseDistcc()
		{
			return false;
		}

		/// <summary>
		/// If this platform can be compiled with SN-DBS
		/// </summary>
		public virtual bool CanUseSNDBS()
		{
			return false;
		}

		/// <summary>
		/// Return whether the given platform requires a monolithic build
		/// </summary>
		/// <param name="InPlatform">The platform of interest</param>
		/// <param name="InConfiguration">The configuration of interest</param>
		/// <returns></returns>
		public static bool PlatformRequiresMonolithicBuilds(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// Some platforms require monolithic builds...
			UEBuildPlatform BuildPlatform = GetBuildPlatform(InPlatform, true);
			if (BuildPlatform != null)
			{
				return BuildPlatform.ShouldCompileMonolithicBinary(InPlatform);
			}

			// We assume it does not
			return false;
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (i.e. 'exe' or 'dll')</returns>
		public virtual string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			throw new BuildException("GetBinaryExtensiton for {0} not handled in {1}", InBinaryType.ToString(), this.ToString());
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public virtual string GetDebugInfoExtension(UEBuildBinaryType InBinaryType)
		{
			throw new BuildException("GetDebugInfoExtension for {0} not handled in {1}", InBinaryType.ToString(), this.ToString());
		}

		/// <summary>
		/// Whether incremental linking should be used
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="InConfiguration"> The CPPTargetConfiguration being built</param>
		/// <returns>bool true if incremental linking should be used, false if not</returns>
		public virtual bool ShouldUseIncrementalLinking(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration)
		{
			return false;
		}

		/// <summary>
		/// Whether PDB files should be used
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="InConfiguration"> The CPPTargetConfiguration being built</param>
		/// <param name="bInCreateDebugInfo">true if debug info is getting create, false if not</param>
		/// <returns>bool true if PDB files should be used, false if not</returns>
		public virtual bool ShouldUsePDBFiles(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration, bool bCreateDebugInfo)
		{
			return false;
		}

		/// <summary>
		/// Whether PCH files should be used
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="InConfiguration"> The CPPTargetConfiguration being built</param>
		/// <returns>bool    true if PCH files should be used, false if not</returns>
		public virtual bool ShouldUsePCHFiles(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration)
		{
			return BuildConfiguration.bUsePCHFiles;
		}

		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public virtual bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Whether this build should support ONLY cooked data or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public virtual bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Whether the platform requires the extra UnityCPPWriter
		/// This is used to add an extra file for UBT to get the #include dependencies from
		/// </summary>
		/// <returns>bool true if it is required, false if not</returns>
		public virtual bool RequiresExtraUnityCPPWriter()
		{
			return false;
		}

		/// <summary>
		/// Whether this platform should build a monolithic binary
		/// </summary>
		public virtual bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			return false;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public virtual void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
		}

		/// <summary>
		/// Allow the platform to override the NMake output name
		/// </summary>
		public virtual FileReference ModifyNMakeOutput(FileReference ExeName)
		{
			// by default, use original
			return ExeName;
		}

		/// <summary>
		/// Allows the platform to override whether the architecture name should be appended to the name of binaries.
		/// </summary>
		/// <returns>True if the architecture name should be appended to the binary</returns>
		public virtual bool RequiresArchitectureSuffix()
		{
			return true;
		}

		/// <summary>
		/// For platforms that need to output multiple files per binary (ie Android "fat" binaries)
		/// this will emit multiple paths. By default, it simply makes an array from the input
		/// </summary>
		public virtual List<FileReference> FinalizeBinaryPaths(FileReference BinaryName, FileReference ProjectFile)
		{
			List<FileReference> TempList = new List<FileReference>() { BinaryName };
			return TempList;
		}

		/// <summary>
		/// Return whether this platform has uniquely named binaries across multiple games
		/// </summary>
		public virtual bool HasUniqueBinaries()
		{
			return true;
		}

		/// <summary>
		/// Return whether we wish to have this platform's binaries in our builds
		/// </summary>
		public virtual bool IsBuildRequired()
		{
			return true;
		}

		/// <summary>
		/// Return whether we wish to have this platform's binaries in our CIS tests
		/// </summary>
		public virtual bool IsCISRequired()
		{
			return true;
		}

		/// <summary>
		/// Whether the build platform requires deployment prep
		/// </summary>
		/// <returns></returns>
		public virtual bool RequiresDeployPrepAfterCompile()
		{
			return false;
		}

		/// <summary>
		/// Return all valid configurations for this platform
		/// Typically, this is always Debug, Development, and Shipping - but Test is a likely future addition for some platforms
		/// </summary>
		public virtual List<UnrealTargetConfiguration> GetConfigurations(UnrealTargetPlatform InUnrealTargetPlatform, bool bIncludeDebug)
		{
			List<UnrealTargetConfiguration> Configurations = new List<UnrealTargetConfiguration>()
			{
				UnrealTargetConfiguration.Development, 
			};

			if (bIncludeDebug)
			{
				Configurations.Insert(0, UnrealTargetConfiguration.Debug);
			}

			return Configurations;
		}

		protected static bool DoProjectSettingsMatchDefault(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName, string Section, string[] BoolKeys, string[] IntKeys, string[] StringKeys)
		{
			ConfigCacheIni ProjIni = ConfigCacheIni.CreateConfigCacheIni(Platform, "Engine", ProjectDirectoryName);
			ConfigCacheIni DefaultIni = ConfigCacheIni.CreateConfigCacheIni(Platform, "Engine", (DirectoryReference)null);

			// look at all bool values
			if (BoolKeys != null) foreach (string Key in BoolKeys)
				{
					bool Default = false, Project = false;
					DefaultIni.GetBool(Section, Key, out Default);
					ProjIni.GetBool(Section, Key, out Project);
					if (Default != Project)
					{
						Console.WriteLine(Key + " is not set to default. (" + Default + " vs. " + Project + ")");
						return false;
					}
				}

			// look at all int values
			if (IntKeys != null) foreach (string Key in IntKeys)
				{
					int Default = 0, Project = 0;
					DefaultIni.GetInt32(Section, Key, out Default);
					ProjIni.GetInt32(Section, Key, out Project);
					if (Default != Project)
					{
						Console.WriteLine(Key + " is not set to default. (" + Default + " vs. " + Project + ")");
						return false;
					}
				}

			// look for all string values
			if (StringKeys != null) foreach (string Key in StringKeys)
				{
					string Default = "", Project = "";
					DefaultIni.GetString(Section, Key, out Default);
					ProjIni.GetString(Section, Key, out Project);
					if (Default != Project)
					{
						Console.WriteLine(Key + " is not set to default. (" + Default + " vs. " + Project + ")");
						return false;
					}
				}

			// if we get here, we match all important settings
			return true;
		}

		/// <summary>
		/// Check for the default configuration
		/// return true if the project uses the default build config
		/// </summary>
		public virtual bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			string[] BoolKeys = new string[] {
				"bCompileApex", "bCompileBox2D", "bCompileICU", "bCompileSimplygon", "bCompileSimplygonSSF",
				"bCompileLeanAndMeanUE", "bIncludeADO", "bCompileRecast", "bCompileSpeedTree", 
				"bCompileWithPluginSupport", "bCompilePhysXVehicle", "bCompileFreeType", 
				"bCompileForSize", "bCompileCEF3"
			};

			return DoProjectSettingsMatchDefault(Platform, ProjectDirectoryName, "/Script/BuildSettings.BuildSettings",
				BoolKeys, null, null);
		}

		/// <summary>
		/// Creates a context for the given project on the current platform.
		/// </summary>
		/// <param name="ProjectFile">The project file for the current target</param>
		/// <returns>New platform context object</returns>
		public abstract UEBuildPlatformContext CreateContext(FileReference ProjectFile);
	}

	public abstract class UEBuildPlatformSDK
	{
		// AutoSDKs handling portion

		#region protected AutoSDKs Utility

		/// <summary>
		/// Name of the file that holds currently install SDK version string
		/// </summary>
		protected static string CurrentlyInstalledSDKStringManifest = "CurrentlyInstalled.txt";

		/// <summary>
		/// name of the file that holds the last succesfully run SDK setup script version
		/// </summary>
		protected static string LastRunScriptVersionManifest = "CurrentlyInstalled.Version.txt";

		/// <summary>
		/// Name of the file that holds environment variables of current SDK
		/// </summary>
		protected static string SDKEnvironmentVarsFile = "OutputEnvVars.txt";

		protected static readonly string SDKRootEnvVar = "UE_SDKS_ROOT";

		protected static string AutoSetupEnvVar = "AutoSDKSetup";

		public static bool bShouldLogInfo = false;

		/// <summary>
		/// Whether platform supports switching SDKs during runtime
		/// </summary>
		/// <returns>true if supports</returns>
		protected virtual bool PlatformSupportsAutoSDKs()
		{
			return false;
		}

		static private bool bCheckedAutoSDKRootEnvVar = false;
		static private bool bAutoSDKSystemEnabled = false;
		static private bool HasAutoSDKSystemEnabled()
		{
			if (!bCheckedAutoSDKRootEnvVar)
			{
				string SDKRoot = Environment.GetEnvironmentVariable(SDKRootEnvVar);
				if (SDKRoot != null)
				{
					bAutoSDKSystemEnabled = true;
				}
				bCheckedAutoSDKRootEnvVar = true;
			}
			return bAutoSDKSystemEnabled;
		}

		// Whether AutoSDK setup is safe. AutoSDKs will damage manual installs on some platforms.
		protected bool IsAutoSDKSafe()
		{
			return !IsAutoSDKDestructive() || !HasAnyManualInstall();
		}

		/// <summary>
		/// Returns SDK string as required by the platform
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected virtual string GetRequiredSDKString()
		{
			return "";
		}

		/// <summary>
		/// Gets the version number of the SDK setup script itself.  The version in the base should ALWAYS be the master revision from the last refactor.
		/// If you need to force a rebuild for a given platform, override this for the given platform.
		/// </summary>
		/// <returns>Setup script version</returns>
		protected virtual String GetRequiredScriptVersionString()
		{
			return "3.0";
		}

		/// <summary>
		/// Returns path to platform SDKs
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected string GetPathToPlatformAutoSDKs()
		{
			string SDKPath = "";
			string SDKRoot = Environment.GetEnvironmentVariable(SDKRootEnvVar);
			if (SDKRoot != null)
			{
				if (SDKRoot != "")
				{
					SDKPath = Path.Combine(SDKRoot, "Host" + BuildHostPlatform.Current.Platform, GetSDKTargetPlatformName());
				}
			}
			return SDKPath;
		}

		/// <summary>
		/// Because most ManualSDK determination depends on reading env vars, if this process is spawned by a process that ALREADY set up
		/// AutoSDKs then all the SDK env vars will exist, and we will spuriously detect a Manual SDK. (children inherit the environment of the parent process).
		/// Therefore we write out an env var to set in the command file (OutputEnvVars.txt) such that child processes can determine if their manual SDK detection
		/// is bogus.  Make it platform specific so that platforms can be in different states.
		/// </summary>
		protected string GetPlatformAutoSDKSetupEnvVar()
		{
			return GetSDKTargetPlatformName() + AutoSetupEnvVar;
		}

		/// <summary>
		/// Gets currently installed version
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="OutInstalledSDKVersionString">version string as currently installed</param>
		/// <returns>true if was able to read it</returns>
		protected bool GetCurrentlyInstalledSDKString(string PlatformSDKRoot, out string OutInstalledSDKVersionString)
		{
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(VersionFilename))
				{
					using (StreamReader Reader = new StreamReader(VersionFilename))
					{
						string Version = Reader.ReadLine();
						string Type = Reader.ReadLine();

						// don't allow ManualSDK installs to count as an AutoSDK install version.
						if (Type != null && Type == "AutoSDK")
						{
							if (Version != null)
							{
								OutInstalledSDKVersionString = Version;
								return true;
							}
						}
					}
				}
			}

			OutInstalledSDKVersionString = "";
			return false;
		}

		/// <summary>
		/// Gets the version of the last successfully run setup script.
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="OutLastRunScriptVersion">version string</param>
		/// <returns>true if was able to read it</returns>
		protected bool GetLastRunScriptVersionString(string PlatformSDKRoot, out string OutLastRunScriptVersion)
		{
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					using (StreamReader Reader = new StreamReader(VersionFilename))
					{
						string Version = Reader.ReadLine();
						if (Version != null)
						{
							OutLastRunScriptVersion = Version;
							return true;
						}
					}
				}
			}

			OutLastRunScriptVersion = "";
			return false;
		}

		/// <summary>
		/// Sets currently installed version
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="InstalledSDKVersionString">SDK version string to set</param>
		/// <returns>true if was able to set it</returns>
		protected bool SetCurrentlyInstalledAutoSDKString(String InstalledSDKVersionString)
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				using (StreamWriter Writer = File.CreateText(VersionFilename))
				{
					Writer.WriteLine(InstalledSDKVersionString);
					Writer.WriteLine("AutoSDK");
					return true;
				}
			}

			return false;
		}

		protected void SetupManualSDK()
		{
			if (PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				String InstalledSDKVersionString = GetRequiredSDKString();
				String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
                if (!Directory.Exists(PlatformSDKRoot))
                {
                    Directory.CreateDirectory(PlatformSDKRoot);
                }

				{
					string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
					if (File.Exists(VersionFilename))
					{
						File.Delete(VersionFilename);
					}

					string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
					if (File.Exists(EnvVarFile))
					{
						File.Delete(EnvVarFile);
					}

					using (StreamWriter Writer = File.CreateText(VersionFilename))
					{
						Writer.WriteLine(InstalledSDKVersionString);
						Writer.WriteLine("ManualSDK");
					}
				}
			}
		}

		protected bool SetLastRunAutoSDKScriptVersion(string LastRunScriptVersion)
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				using (StreamWriter Writer = File.CreateText(VersionFilename))
				{
					Writer.WriteLine(LastRunScriptVersion);
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Returns Hook names as needed by the platform
		/// (e.g. can be overridden with custom executables or scripts)
		/// </summary>
		/// <param name="Hook">Hook type</param>
		protected virtual string GetHookExecutableName(SDKHookType Hook)
		{
			if (Hook == SDKHookType.Uninstall)
			{
				return "unsetup.bat";
			}

			return "setup.bat";
		}

		/// <summary>
		/// Runs install/uninstall hooks for SDK
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="SDKVersionString">version string to run for (can be empty!)</param>
		/// <param name="Hook">which one of hooks to run</param>
		/// <param name="bHookCanBeNonExistent">whether a non-existing hook means failure</param>
		/// <returns>true if succeeded</returns>
		protected virtual bool RunAutoSDKHooks(string PlatformSDKRoot, string SDKVersionString, SDKHookType Hook, bool bHookCanBeNonExistent = true)
		{
			if (!IsAutoSDKSafe())
			{
				Console.ForegroundColor = ConsoleColor.Red;
				LogAutoSDK(GetSDKTargetPlatformName() + " attempted to run SDK hook which could have damaged manual SDK install!");
				Console.ResetColor();

				return false;
			}
			if (SDKVersionString != "")
			{
				string SDKDirectory = Path.Combine(PlatformSDKRoot, SDKVersionString);
				string HookExe = Path.Combine(SDKDirectory, GetHookExecutableName(Hook));

				if (File.Exists(HookExe))
				{
					LogAutoSDK("Running {0} hook {1}", Hook, HookExe);

					// run it
					Process HookProcess = new Process();
					HookProcess.StartInfo.WorkingDirectory = SDKDirectory;
					HookProcess.StartInfo.FileName = HookExe;
					HookProcess.StartInfo.Arguments = "";
					HookProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;

					// seems to break the build machines?
					//HookProcess.StartInfo.UseShellExecute = false;
					//HookProcess.StartInfo.RedirectStandardOutput = true;
					//HookProcess.StartInfo.RedirectStandardError = true;					

					using (ScopedTimer HookTimer = new ScopedTimer("Time to run hook: ", bShouldLogInfo ? LogEventType.Log : LogEventType.Verbose))
					{
						//installers may require administrator access to succeed. so run as an admmin.
						HookProcess.StartInfo.Verb = "runas";
						HookProcess.Start();
						HookProcess.WaitForExit();
					}

					//LogAutoSDK(HookProcess.StandardOutput.ReadToEnd());
					//LogAutoSDK(HookProcess.StandardError.ReadToEnd());
					if (HookProcess.ExitCode != 0)
					{
						LogAutoSDK("Hook exited uncleanly (returned {0}), considering it failed.", HookProcess.ExitCode);
						return false;
					}

					return true;
				}
				else
				{
					LogAutoSDK("File {0} does not exist", HookExe);
				}
			}
			else
			{
				LogAutoSDK("Version string is blank for {0}. Can't determine {1} hook.", PlatformSDKRoot, Hook.ToString());
			}

			return bHookCanBeNonExistent;
		}

		/// <summary>
		/// Loads environment variables from SDK
		/// If any commands are added or removed the handling needs to be duplicated in
		/// TargetPlatformManagerModule.cpp
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK</param>
		/// <returns>true if succeeded</returns>
		protected bool SetupEnvironmentFromAutoSDK(string PlatformSDKRoot)
		{
			string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
			if (File.Exists(EnvVarFile))
			{
				using (StreamReader Reader = new StreamReader(EnvVarFile))
				{
					List<string> PathAdds = new List<string>();
					List<string> PathRemoves = new List<string>();

					List<string> EnvVarNames = new List<string>();
					List<string> EnvVarValues = new List<string>();

					bool bNeedsToWriteAutoSetupEnvVar = true;
					String PlatformSetupEnvVar = GetPlatformAutoSDKSetupEnvVar();
					for (; ; )
					{
						string VariableString = Reader.ReadLine();
						if (VariableString == null)
						{
							break;
						}

						string[] Parts = VariableString.Split('=');
						if (Parts.Length != 2)
						{
							LogAutoSDK("Incorrect environment variable declaration:");
							LogAutoSDK(VariableString);
							return false;
						}

						if (String.Compare(Parts[0], "strippath", true) == 0)
						{
							PathRemoves.Add(Parts[1]);
						}
						else if (String.Compare(Parts[0], "addpath", true) == 0)
						{
							PathAdds.Add(Parts[1]);
						}
						else
						{
							if (String.Compare(Parts[0], PlatformSetupEnvVar) == 0)
							{
								bNeedsToWriteAutoSetupEnvVar = false;
							}
							// convenience for setup.bat writers.  Trim any accidental whitespace from var names/values.
							EnvVarNames.Add(Parts[0].Trim());
							EnvVarValues.Add(Parts[1].Trim());
						}
					}

					// don't actually set anything until we successfully validate and read all values in.
					// we don't want to set a few vars, return a failure, and then have a platform try to
					// build against a manually installed SDK with half-set env vars.
					for (int i = 0; i < EnvVarNames.Count; ++i)
					{
						string EnvVarName = EnvVarNames[i];
						string EnvVarValue = EnvVarValues[i];
						if (BuildConfiguration.bPrintDebugInfo)
						{
							LogAutoSDK("Setting variable '{0}' to '{1}'", EnvVarName, EnvVarValue);
						}
						Environment.SetEnvironmentVariable(EnvVarName, EnvVarValue);
					}


                    // actually perform the PATH stripping / adding.
                    String OrigPathVar = Environment.GetEnvironmentVariable("PATH");
                    String PathDelimiter = UEBuildPlatform.GetPathVarDelimiter();
                    String[] PathVars = { };
                    if (!String.IsNullOrEmpty(OrigPathVar))
                    {
                        PathVars = OrigPathVar.Split(PathDelimiter.ToCharArray());
                    }
                    else
                    {
                        LogAutoSDK("Path environment variable is null during AutoSDK");
                    }

					List<String> ModifiedPathVars = new List<string>();
					ModifiedPathVars.AddRange(PathVars);

					// perform removes first, in case they overlap with any adds.
					foreach (String PathRemove in PathRemoves)
					{
						foreach (String PathVar in PathVars)
						{
							if (PathVar.IndexOf(PathRemove, StringComparison.OrdinalIgnoreCase) >= 0)
							{
								LogAutoSDK("Removing Path: '{0}'", PathVar);
								ModifiedPathVars.Remove(PathVar);
							}
						}
					}

					// remove all the of ADDs so that if this function is executed multiple times, the paths will be guaranteed to be in the same order after each run.
					// If we did not do this, a 'remove' that matched some, but not all, of our 'adds' would cause the order to change.
					foreach (String PathAdd in PathAdds)
					{
						foreach (String PathVar in PathVars)
						{
							if (String.Compare(PathAdd, PathVar, true) == 0)
							{
								LogAutoSDK("Removing Path: '{0}'", PathVar);
								ModifiedPathVars.Remove(PathVar);
							}
						}
					}

					// perform adds, but don't add duplicates
					foreach (String PathAdd in PathAdds)
					{
						if (!ModifiedPathVars.Contains(PathAdd))
						{
							LogAutoSDK("Adding Path: '{0}'", PathAdd);
							ModifiedPathVars.Add(PathAdd);
						}
					}

					String ModifiedPath = String.Join(PathDelimiter, ModifiedPathVars);
					Environment.SetEnvironmentVariable("PATH", ModifiedPath);

					Reader.Close();

					// write out env var command so any process using this commandfile will mark itself as having had autosdks set up.
					// avoids child processes spuriously detecting manualsdks.
					if (bNeedsToWriteAutoSetupEnvVar)
					{
						using (StreamWriter Writer = File.AppendText(EnvVarFile))
						{
							Writer.WriteLine("{0}=1", PlatformSetupEnvVar);
						}
						// set the var in the local environment in case this process spawns any others.
						Environment.SetEnvironmentVariable(PlatformSetupEnvVar, "1");
					}

					// make sure we know that we've modified the local environment, invalidating manual installs for this run.
					bLocalProcessSetupAutoSDK = true;

					return true;
				}
			}
			else
			{
				LogAutoSDK("Cannot set up environment for {1} because command file {1} does not exist.", PlatformSDKRoot, EnvVarFile);
			}

			return false;
		}

		protected void InvalidateCurrentlyInstalledAutoSDK()
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string SDKFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(SDKFilename))
				{
					File.Delete(SDKFilename);
				}

				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
				if (File.Exists(EnvVarFile))
				{
					File.Delete(EnvVarFile);
				}
			}
		}

		/// <summary>
		/// Currently installed AutoSDK is written out to a text file in a known location.
		/// This function just compares the file's contents with the current requirements.
		/// </summary>
		protected SDKStatus HasRequiredAutoSDKInstalled()
		{
			if (PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				string AutoSDKRoot = GetPathToPlatformAutoSDKs();
				if (AutoSDKRoot != "")
				{
					// check script version so script fixes can be propagated without touching every build machine's CurrentlyInstalled file manually.
					bool bScriptVersionMatches = false;
					string CurrentScriptVersionString;
					if (GetLastRunScriptVersionString(AutoSDKRoot, out CurrentScriptVersionString) && CurrentScriptVersionString == GetRequiredScriptVersionString())
					{
						bScriptVersionMatches = true;
					}

					// check to make sure OutputEnvVars doesn't need regenerating
					string EnvVarFile = Path.Combine(AutoSDKRoot, SDKEnvironmentVarsFile);
					bool bEnvVarFileExists = File.Exists(EnvVarFile);

					string CurrentSDKString;
					if (bEnvVarFileExists && GetCurrentlyInstalledSDKString(AutoSDKRoot, out CurrentSDKString) && CurrentSDKString == GetRequiredSDKString() && bScriptVersionMatches)
					{
						return SDKStatus.Valid;
					}
					return SDKStatus.Invalid;
				}
			}
			return SDKStatus.Invalid;
		}

		// This tracks if we have already checked the sdk installation.
		private Int32 SDKCheckStatus = -1;

		// true if we've ever overridden the process's environment with AutoSDK data.  After that, manual installs cannot be considered valid ever again.
		private bool bLocalProcessSetupAutoSDK = false;

		protected bool HasSetupAutoSDK()
		{
			return bLocalProcessSetupAutoSDK || HasParentProcessSetupAutoSDK();
		}

		protected bool HasParentProcessSetupAutoSDK()
		{
			bool bParentProcessSetupAutoSDK = false;
			String AutoSDKSetupVarName = GetPlatformAutoSDKSetupEnvVar();
			String AutoSDKSetupVar = Environment.GetEnvironmentVariable(AutoSDKSetupVarName);
			if (!String.IsNullOrEmpty(AutoSDKSetupVar))
			{
				bParentProcessSetupAutoSDK = true;
			}
			return bParentProcessSetupAutoSDK;
		}

		protected SDKStatus HasRequiredManualSDK()
		{
			if (HasSetupAutoSDK())
			{
				return SDKStatus.Invalid;
			}

			// manual installs are always invalid if we have modified the process's environment for AutoSDKs
			return HasRequiredManualSDKInternal();
		}

		// for platforms with destructive AutoSDK.  Report if any manual sdk is installed that may be damaged by an autosdk.
		protected virtual bool HasAnyManualInstall()
		{
			return false;
		}

		// tells us if the user has a valid manual install.
		protected abstract SDKStatus HasRequiredManualSDKInternal();

		// some platforms will fail if there is a manual install that is the WRONG manual install.
		protected virtual bool AllowInvalidManualInstall()
		{
			return true;
		}

		// platforms can choose if they prefer a correct the the AutoSDK install over the manual install.
		protected virtual bool PreferAutoSDK()
		{
			return true;
		}

		// some platforms don't support parallel SDK installs.  AutoSDK on these platforms will
		// actively damage an existing manual install by overwriting files in it.  AutoSDK must NOT
		// run any setup if a manual install exists in this case.
		protected virtual bool IsAutoSDKDestructive()
		{
			return false;
		}

		/// <summary>
		/// Runs batch files if necessary to set up required AutoSDK.
		/// AutoSDKs are SDKs that have not been setup through a formal installer, but rather come from
		/// a source control directory, or other local copy.
		/// </summary>
		private void SetupAutoSDK()
		{
			if (IsAutoSDKSafe() && PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				// run installation for autosdk if necessary.
				if (HasRequiredAutoSDKInstalled() == SDKStatus.Invalid)
				{
					//reset check status so any checking sdk status after the attempted setup will do a real check again.
					SDKCheckStatus = -1;

					string AutoSDKRoot = GetPathToPlatformAutoSDKs();
					string CurrentSDKString;
					GetCurrentlyInstalledSDKString(AutoSDKRoot, out CurrentSDKString);

					// switch over (note that version string can be empty)
					if (!RunAutoSDKHooks(AutoSDKRoot, CurrentSDKString, SDKHookType.Uninstall))
					{
						LogAutoSDK("Failed to uninstall currently installed SDK {0}", CurrentSDKString);
						InvalidateCurrentlyInstalledAutoSDK();
						return;
					}
					// delete Manifest file to avoid multiple uninstalls
					InvalidateCurrentlyInstalledAutoSDK();

					if (!RunAutoSDKHooks(AutoSDKRoot, GetRequiredSDKString(), SDKHookType.Install, false))
					{
						LogAutoSDK("Failed to install required SDK {0}.  Attemping to uninstall", GetRequiredSDKString());
						RunAutoSDKHooks(AutoSDKRoot, GetRequiredSDKString(), SDKHookType.Uninstall, false);
						return;
					}

					string EnvVarFile = Path.Combine(AutoSDKRoot, SDKEnvironmentVarsFile);
					if (!File.Exists(EnvVarFile))
					{
						LogAutoSDK("Installation of required SDK {0}.  Did not generate Environment file {1}", GetRequiredSDKString(), EnvVarFile);
						RunAutoSDKHooks(AutoSDKRoot, GetRequiredSDKString(), SDKHookType.Uninstall, false);
						return;
					}

					SetCurrentlyInstalledAutoSDKString(GetRequiredSDKString());
					SetLastRunAutoSDKScriptVersion(GetRequiredScriptVersionString());
				}

				// fixup process environment to match autosdk
				SetupEnvironmentFromAutoSDK();
			}
		}

		#endregion

		#region public AutoSDKs Utility

		/// <summary>
		/// Enum describing types of hooks a platform SDK can have
		/// </summary>
		public enum SDKHookType
		{
			Install,
			Uninstall
		};

		/// <summary>
		/// Returns platform-specific name used in SDK repository
		/// </summary>
		/// <returns>path to SDK Repository</returns>
		public virtual string GetSDKTargetPlatformName()
		{
			return "";
		}

		/* Whether or not we should try to automatically switch SDKs when asked to validate the platform's SDK state. */
		public static bool bAllowAutoSDKSwitching = true;

		public SDKStatus SetupEnvironmentFromAutoSDK()
		{
			string PlatformSDKRoot = GetPathToPlatformAutoSDKs();

			// load environment variables from current SDK
			if (!SetupEnvironmentFromAutoSDK(PlatformSDKRoot))
			{
				LogAutoSDK("Failed to load environment from required SDK {0}", GetRequiredSDKString());
				InvalidateCurrentlyInstalledAutoSDK();
				return SDKStatus.Invalid;
			}
			return SDKStatus.Valid;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform.
		/// Could be either a manual install or an AutoSDK.
		/// </summary>
		public SDKStatus HasRequiredSDKsInstalled()
		{
			// avoid redundant potentially expensive SDK checks.
			if (SDKCheckStatus == -1)
			{
				bool bHasManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				bool bHasAutoSDK = HasRequiredAutoSDKInstalled() == SDKStatus.Valid;

				// Per-Platform implementations can choose how to handle non-Auto SDK detection / handling.
				SDKCheckStatus = (bHasManualSDK || bHasAutoSDK) ? 1 : 0;
			}
			return SDKCheckStatus == 1 ? SDKStatus.Valid : SDKStatus.Invalid;
		}

		// Arbitrates between manual SDKs and setting up AutoSDK based on program options and platform preferences.
		public void ManageAndValidateSDK()
		{
			bShouldLogInfo = BuildConfiguration.bPrintDebugInfo || Environment.GetEnvironmentVariable("IsBuildMachine") == "1";

			// do not modify installed manifests if parent process has already set everything up.
			// this avoids problems with determining IsAutoSDKSafe and doing an incorrect invalidate.
			if (bAllowAutoSDKSwitching && !HasParentProcessSetupAutoSDK())
			{
				bool bSetSomeSDK = false;
				bool bHasRequiredManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				if (IsAutoSDKSafe() && (PreferAutoSDK() || !bHasRequiredManualSDK))
				{
					SetupAutoSDK();
					bSetSomeSDK = true;
				}

				//Setup manual SDK if autoSDK setup was skipped or failed for whatever reason.
				if (bHasRequiredManualSDK && (HasRequiredAutoSDKInstalled() != SDKStatus.Valid))
				{
					SetupManualSDK();
					bSetSomeSDK = true;
				}

				if (!bSetSomeSDK)
				{
					InvalidateCurrentlyInstalledAutoSDK();
				}
			}


			if (bShouldLogInfo)
			{
				PrintSDKInfo();
			}
		}

		public void PrintSDKInfo()
		{
			if (HasRequiredSDKsInstalled() == SDKStatus.Valid)
			{
				bool bHasRequiredManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				if (HasSetupAutoSDK())
				{
					string PlatformSDKRoot = GetPathToPlatformAutoSDKs();
					LogAutoSDK(GetSDKTargetPlatformName() + " using SDK from: " + Path.Combine(PlatformSDKRoot, GetRequiredSDKString()));
				}
				else if (bHasRequiredManualSDK)
				{
					LogAutoSDK(this.ToString() + " using manually installed SDK " + GetRequiredSDKString());
				}
				else
				{
					LogAutoSDK(this.ToString() + " setup error.  Inform platform team.");
				}
			}
			else
			{
				LogAutoSDK(this.ToString() + " has no valid SDK");
			}
		}

		protected static void LogAutoSDK(string Format, params object[] Args)
		{
			if (bShouldLogInfo)
			{
				Log.WriteLine(LogEventType.Log, Format, Args);
			}
		}

		protected static void LogAutoSDK(String Message)
		{
			if (bShouldLogInfo)
			{
				Log.WriteLine(LogEventType.Log, Message);
			}
		}

		#endregion
	}

	public abstract class UEBuildPlatformFactory
	{
		/// <summary>
		/// Attempt to register a build platform, checking whether it is a valid platform in installed builds
		/// </summary>
		public void TryRegisterBuildPlatforms(bool bValidatingPlatforms)
		{
			// We need all platforms to be registered when we run -validateplatform command to check SDK status of each
			if (bValidatingPlatforms || InstalledPlatformInfo.Current.IsValidPlatform(TargetPlatform))
			{
				RegisterBuildPlatforms();
			}
		}

		/// <summary>
		/// Gets the target platform for an individual factory
		/// </summary>
		protected abstract UnrealTargetPlatform TargetPlatform
		{
			get;
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected abstract void RegisterBuildPlatforms();
	}
}
