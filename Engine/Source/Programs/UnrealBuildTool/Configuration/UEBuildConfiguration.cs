// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool
{
	public class UEBuildConfiguration
	{
		static UEBuildConfiguration()
		{
			if (!UnrealBuildTool.bIsSafeToReferenceConfigurationValues)
			{
				throw new BuildException("UEBuildConfiguration was referenced before the XmlConfig files could be loaded.");
			}
		}

		/// <summary>
		/// Whether to include PhysX support
		/// </summary>
		[XmlConfig]
		public static bool bCompilePhysX;

		/// <summary>
		/// Whether to include PhysX APEX support
		/// </summary>
		[XmlConfig]
		public static bool bCompileAPEX;

		/// <summary>
		/// Whether to allow runtime cooking of physics
		/// </summary>
		[XmlConfig]
		public static bool bRuntimePhysicsCooking;

		/// <summary>
		/// Whether to include Box2D support
		/// </summary>
		[XmlConfig]
		public static bool bCompileBox2D;

		/// <summary>
		/// Whether to include ICU unicode/i18n support in core
		/// </summary>
		[XmlConfig]
		public static bool bCompileICU;

		/// <summary>
		/// Whether to build a stripped down version of the game specifically for dedicated server.
		/// </summary>
		[Obsolete("bBuildDedicatedServer has been deprecated and will be removed in future release. Update your code to use TargetInfo.Type instead or your code will not compile.")]
		public static bool bBuildDedicatedServer;

		/// <summary>
		/// Whether to compile the editor or not. Only desktop platforms (Windows or Mac) will use this, other platforms force this to false
		/// </summary>
		[XmlConfig]
		public static bool bBuildEditor;

		/// <summary>
		/// Whether to compile code related to building assets. Consoles generally cannot build assets. Desktop platforms generally can.
		/// </summary>
		[XmlConfig]
		public static bool bBuildRequiresCookedData;

		/// <summary>
		/// Whether to compile WITH_EDITORONLY_DATA disabled. Only Windows will use this, other platforms force this to false
		/// </summary>
		[XmlConfig]
		public static bool bBuildWithEditorOnlyData;

		/// <summary>
		/// Whether to compile the developer tools.
		/// </summary>
		[XmlConfig]
		public static bool bBuildDeveloperTools;

		/// <summary>
		/// Whether to force compiling the target platform modules, even if they wouldn't normally be built
		/// </summary>
		[XmlConfig]
		public static bool bForceBuildTargetPlatforms;

		/// <summary>
		/// Whether to force compiling shader format modules, even if they wouldn't normally be built.
		/// </summary>
		[XmlConfig]
		public static bool bForceBuildShaderFormats;

		/// <summary>
		/// Whether we should compile in support for Simplygon or not.
		/// </summary>
		[XmlConfig]
		public static bool bCompileSimplygon;

        /// <summary>
        /// Whether we should compile in support for Simplygon's SSF library or not.
        /// </summary>
        [XmlConfig]
        public static bool bCompileSimplygonSSF;

        /// <summary>
        /// Whether we should compile in support for Steam OnlineSubsystem or not.
        /// </summary>
        [Obsolete("To use OnlineSubsystemSteam, include the OnlineSubsystemSteam uplugin in your uproject", true)]
        [XmlConfig]
        public static bool bCompileSteamOSS;

        /// <summary>
		/// Whether to compile lean and mean version of UE.
		/// </summary>
		[XmlConfig]
		public static bool bCompileLeanAndMeanUE;

		/// <summary>
		/// Whether to generate a list of external files that are required to build a target
		/// </summary>
		[XmlConfig]
		public static bool bGenerateExternalFileList;

		/// <summary>
		/// Whether to merge to the existing list of external files
		/// </summary>
		[XmlConfig]
		public static bool bMergeExternalFileList;

		/** Whether to generate a list of folders used by the build */
		public static bool bListBuildFolders;

		/// <summary>
		/// Whether to generate a manifest file that contains the files to add to Perforce
		/// </summary>
		[XmlConfig]
		public static bool bGenerateManifest;

		/// <summary>
		/// Whether to add to the existing manifest (if it exists), or start afresh
		/// </summary>
		[XmlConfig]
		public static bool bMergeManifests;

		/// <summary>
		/// Whether to 'clean' the given project
		/// </summary>
		[XmlConfig]
		public static bool bCleanProject;

		/// <summary>
		/// Whether we are just running the PrepTargetForDeployment step
		/// </summary>
		[XmlConfig]
		public static bool bPrepForDeployment;

		/// <summary>
		/// Enabled for all builds that include the engine project.  Disabled only when building standalone apps that only link with Core.
		/// </summary>
		[XmlConfig]
		public static bool bCompileAgainstEngine;

		/// <summary>
		/// Enabled for all builds that include the CoreUObject project.  Disabled only when building standalone apps that only link with Core.
		/// </summary>
		[XmlConfig]
		public static bool bCompileAgainstCoreUObject;

		/// <summary>
		/// If true, include ADO database support in core
		/// </summary>
		[XmlConfig]
		public static bool bIncludeADO;

		/// <summary>
		/// Directory for the third party files/libs
		/// </summary>
		[Obsolete("Use UEThirdPartySourceDirectory instead of UEThirdPartyDirectory.", true)]
		[XmlConfig]
		public static string UEThirdPartyDirectory;

		/// <summary>
		/// Directory for the third party source
		/// </summary>
		[XmlConfig]
		public static string UEThirdPartySourceDirectory;

		/// <summary>
		/// Directory for the third party binaries
		/// </summary>
		[XmlConfig]
		public static string UEThirdPartyBinariesDirectory;

		/// <summary>
		/// If true, force header regeneration. Intended for the build machine
		/// </summary>
		[XmlConfig]
		public static bool bForceHeaderGeneration;

		/// <summary>
		/// If true, do not build UHT, assume it is already built
		/// </summary>
		[XmlConfig]
		public static bool bDoNotBuildUHT;

		/// <summary>
		/// If true, fail if any of the generated header files is out of date.
		/// </summary>
		[XmlConfig]
		public static bool bFailIfGeneratedCodeChanges;

		/// <summary>
		/// Whether to compile Recast navmesh generation
		/// </summary>
		[XmlConfig]
		public static bool bCompileRecast;

		/// <summary>
		/// Whether to compile SpeedTree support.
		/// </summary>
		[XmlConfig]
		public static bool bCompileSpeedTree;

		/// <summary>
		/// Enable exceptions for all modules
		/// </summary>
		[XmlConfig]
		public static bool bForceEnableExceptions;

		/// <summary>
		/// Compile server-only code.
		/// </summary>
		[XmlConfig]
		public static bool bWithServerCode;

		/// <summary>
		/// Whether to include stats support even without the engine
		/// </summary>
		[XmlConfig]
		public static bool bCompileWithStatsWithoutEngine;

		/// <summary>
		/// Whether to include plugin support
		/// </summary>
		[XmlConfig]
		public static bool bCompileWithPluginSupport;

		/// <summary>
		/// Whether to turn on logging for test/shipping builds
		/// </summary>
		[XmlConfig]
		public static bool bUseLoggingInShipping;

        /// <summary>
        /// Whether to check that the process was launched through an external launcher
        /// </summary>
        [XmlConfig]
        public static bool bUseLauncherChecks;

		/// <summary>
		/// Whether to turn on checks (asserts) for test/shipping builds
		/// </summary>
		[XmlConfig]
		public static bool bUseChecksInShipping;

		/// <summary>
		/// True if we need PhysX vehicle support
		/// </summary>
		[XmlConfig]
		public static bool bCompilePhysXVehicle;

		/// <summary>
		/// True if we need FreeType support
		/// </summary>
		[XmlConfig]
		public static bool bCompileFreeType;

		/// <summary>
		/// True if we want to favor optimizing size over speed
		/// </summary>
		[XmlConfig]
		public static bool bCompileForSize;

		/// <summary>
		/// True if hot-reload from IDE is allowed
		/// </summary>
		[XmlConfig]
		public static bool bAllowHotReloadFromIDE;

		/// <summary>
		/// True if performing hot-reload from IDE
		/// </summary>
		public static bool bHotReloadFromIDE;

		/// <summary>
		/// When true, the targets won't execute their link actions if there was nothing to compile
		/// </summary>
		public static bool bSkipLinkingWhenNothingToCompile;

		/// <summary>
		/// Whether to compile CEF3 support.
		/// </summary>
		[XmlConfig]
		public static bool bCompileCEF3;

		/// <summary>
		/// Allow a target to specify a preferred sub-platform. Can be used to target a build using sub platform specifics.
		/// </summary>
		public static string PreferredSubPlatform = "";

		/// <summary>
		/// Lists Architectures that you want to build
		/// </summary>
		public static string[] Architectures = new string[0];

		/// <summary>
		/// Lists GPU Architectures that you want to build (mostly used for mobile etc.)
		/// </summary>
		public static string[] GPUArchitectures = new string[0];

		/// <summary>
		/// Whether to include a dependency on ShaderCompileWorker when generating project files for the editor.
		/// </summary>
		[XmlConfig]
		public static bool bEditorDependsOnShaderCompileWorker;

        /// <summary>
        /// Whether to compile development automation tests.
        /// </summary>
        [XmlConfig]
        public static bool bForceCompileDevelopmentAutomationTests;

        /// <summary>
        /// Whether to compile performance automation tests.
        /// </summary>
        [XmlConfig]
        public static bool bForceCompilePerformanceAutomationTests;

		/// <summary>
		/// Sets the configuration back to defaults.
		/// </summary>
		public static void LoadDefaults()
		{
			//@todo. Allow disabling PhysX/APEX via these values...
			// Currently, WITH_PHYSX is forced to true in Engine.h (as it isn't defined anywhere by the builder)
			bCompilePhysX = true;
			bCompileAPEX = true;
			bRuntimePhysicsCooking = true;
			bCompileBox2D = true;
			bCompileICU = true;
			bBuildEditor = true;
			bBuildRequiresCookedData = false;
			bBuildWithEditorOnlyData = true;
			bBuildDeveloperTools = true;
			bForceBuildTargetPlatforms = false;
			bForceBuildShaderFormats = false;
			bCompileSimplygon = true;
            bCompileSimplygonSSF = true;
			bCompileLeanAndMeanUE = false;
			bCompileAgainstEngine = true;
			bCompileAgainstCoreUObject = true;
			UEThirdPartySourceDirectory = "ThirdParty/";
			UEThirdPartyBinariesDirectory = "../Binaries/ThirdParty/";
			bCompileRecast = true;
			bForceEnableExceptions = false;
			bWithServerCode = true;
			bCompileSpeedTree = true;
			bCompileWithStatsWithoutEngine = false;
			bCompileWithPluginSupport = false;
			bUseLoggingInShipping = false;
			bUseChecksInShipping = false;
            bUseLauncherChecks = false;
			bCompilePhysXVehicle = true;
			bCompileFreeType = true;
			bCompileForSize = false;
			bHotReloadFromIDE = false;
			bAllowHotReloadFromIDE = true;
			bSkipLinkingWhenNothingToCompile = false;
			bCompileCEF3 = true;
			PreferredSubPlatform = "";
			Architectures = new string[0];
			GPUArchitectures = new string[0];
			bEditorDependsOnShaderCompileWorker = true;
            bForceCompileDevelopmentAutomationTests = false;
            bForceCompilePerformanceAutomationTests = false;
		}

		/// <summary>
		/// Function to call to after reset default data.
		/// </summary>
		public static void PostReset()
		{
			// Configuration overrides.
			bCompileSimplygon = bCompileSimplygon
				&& Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees") == true
				&& Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees/Simplygon") == true
				&& Directory.Exists("Developer/SimplygonMeshReduction") == true;

            bCompileSimplygonSSF = bCompileSimplygonSSF
            && Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees") == true
            && Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees/SSF");
		}

		/// <summary>
		/// Validates the configuration.
		/// Warning: the order of validation is important
		/// </summary>
		public static void ValidateConfiguration()
		{
			// Lean and mean means no Editor and other frills.
			if (bCompileLeanAndMeanUE)
			{
				bBuildEditor = false;
				bBuildDeveloperTools = false;
				bCompileSimplygon = false;
                bCompileSimplygonSSF = false;
				bCompileSpeedTree = false;
			}
		}
	}
}
