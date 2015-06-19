// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool
{
	public class UEBuildConfiguration
	{
		/** Whether to include PhysX support */
		[XmlConfig]
		public static bool bCompilePhysX;

		/** Whether to include PhysX APEX support */
		[XmlConfig]
		public static bool bCompileAPEX;

        /** Whether to allow runtime cooking of physics */
        [XmlConfig]
        public static bool bRuntimePhysicsCooking;

		/** Whether to include Box2D support */
		[XmlConfig]
		public static bool bCompileBox2D;

        /** Whether to include ICU unicode/i18n support in core */
		[XmlConfig]
        public static bool bCompileICU;

		/** Whether to build a stripped down version of the game specifically for dedicated server. */
		[Obsolete("bBuildDedicatedServer has been deprecated and will be removed in future release. Update your code to use TargetInfo.Type instead or your code will not compile.")]
		public static bool bBuildDedicatedServer;

		/** Whether to compile the editor or not. Only desktop platforms (Windows or Mac) will use this, other platforms force this to false */
		[XmlConfig]
		public static bool bBuildEditor;

		/** Whether to compile code related to building assets. Consoles generally cannot build assets. Desktop platforms generally can. */
		[XmlConfig]
		public static bool bBuildRequiresCookedData;

		/** Whether to compile WITH_EDITORONLY_DATA disabled. Only Windows will use this, other platforms force this to false */
		[XmlConfig]
		public static bool bBuildWithEditorOnlyData;

		/** Whether to compile the developer tools. */
		[XmlConfig]
		public static bool bBuildDeveloperTools;

		/** Whether to force compiling the target platform modules, even if they wouldn't normally be built */
		[XmlConfig]
		public static bool bForceBuildTargetPlatforms;

		/** Whether to force compiling shader format modules, even if they wouldn't normally be built. */
		[XmlConfig]
		public static bool bForceBuildShaderFormats;

		/** Whether we should compile in support for Simplygon or not. */
		[XmlConfig]
		public static bool bCompileSimplygon;

        /** Whether we should compile in support for Steam OnlineSubsystem or not. [RCL] FIXME 2014-Apr-17: bCompileSteamOSS means "bHasSteamworksInstalled" for some code, these meanings need to be untangled */
		[XmlConfig]
		public static bool bCompileSteamOSS;

		/** Whether we should compile in support for Mcp OnlineSubsystem or not. */
		[XmlConfig]
		public static bool bCompileMcpOSS;

		/** Whether to compile lean and mean version of UE. */
		[XmlConfig]
		public static bool bCompileLeanAndMeanUE;

		/** Whether to generate a list of external files that are required to build a target */
		[XmlConfig]
		public static bool bGenerateExternalFileList;

		/** Whether to merge to the existing list of external files */
		[XmlConfig]
		public static bool bMergeExternalFileList;

		/** Whether to generate a manifest file that contains the files to add to Perforce */
		[XmlConfig]
		public static bool bGenerateManifest;

		/** Whether to add to the existing manifest (if it exists), or start afresh */
		[XmlConfig]
		public static bool bMergeManifests;

		/** Whether to 'clean' the given project */
		[XmlConfig]
		public static bool bCleanProject;

		/** Whether we are just running the PrepTargetForDeployment step */
		[XmlConfig]
		public static bool bPrepForDeployment;

		/** Enabled for all builds that include the engine project.  Disabled only when building standalone apps that only link with Core. */
		[XmlConfig]
		public static bool bCompileAgainstEngine;

		/** Enabled for all builds that include the CoreUObject project.  Disabled only when building standalone apps that only link with Core. */
		[XmlConfig]
		public static bool bCompileAgainstCoreUObject;

		/** If true, include ADO database support in core */
		[XmlConfig]
		public static bool bIncludeADO;

		/** Directory for the third party files/libs */
        [Obsolete("Use UEThirdPartySourceDirectory instead of UEThirdPartyDirectory.", true)]
		[XmlConfig]
		public static string UEThirdPartyDirectory;

        /** Directory for the third party source */
        [XmlConfig]
        public static string UEThirdPartySourceDirectory;

        /** Directory for the third party binaries */
        [XmlConfig]
        public static string UEThirdPartyBinariesDirectory;

		/** If true, force header regeneration. Intended for the build machine */
		[XmlConfig]
		public static bool bForceHeaderGeneration;

		/** If true, do not build UHT, assume it is already built */
		[XmlConfig]
		public static bool bDoNotBuildUHT;

		/** If true, fail if any of the generated header files is out of date. */
		[XmlConfig]
		public static bool bFailIfGeneratedCodeChanges;

		/** Whether to compile Recast navmesh generation */
		[XmlConfig]
		public static bool bCompileRecast;

		/** Whether to compile SpeedTree support. */
		[XmlConfig]
		public static bool bCompileSpeedTree;

		/** Enable exceptions for all modules */
		[XmlConfig]
		public static bool bForceEnableExceptions;

		/** Compile server-only code. */
		[XmlConfig]
		public static bool bWithServerCode;

		/** Whether to include stats support even without the engine */
		[XmlConfig]
		public static bool bCompileWithStatsWithoutEngine;

		/** Whether to include plugin support */
		[XmlConfig]
		public static bool bCompileWithPluginSupport;

		/** Whether to turn on logging for test/shipping builds */
		[XmlConfig]
		public static bool bUseLoggingInShipping;

		/** True if we need PhysX vehicle support */
		[XmlConfig]
		public static bool bCompilePhysXVehicle;

		/** True if we need FreeType support */
		[XmlConfig]
		public static bool bCompileFreeType;

		/** True if we want to favor optimizing size over speed */
		[XmlConfig]
		public static bool bCompileForSize;

		/** True if hot-reload from IDE is allowed */
		[XmlConfig]
		public static bool bAllowHotReloadFromIDE;

		/** True if performing hot-reload from IDE */
		public static bool bHotReloadFromIDE;

		/** When true, the targets won't execute their link actions if there was nothing to compile */
		public static bool bSkipLinkingWhenNothingToCompile;

		/** Whether to compile CEF3 support. */
		[XmlConfig]
		public static bool bCompileCEF3;

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
			bCompileSteamOSS = true;
			bCompileMcpOSS = true;
			bCompilePhysXVehicle = true;
			bCompileFreeType = true;
			bCompileForSize = false;
			bHotReloadFromIDE = false;
			bAllowHotReloadFromIDE = true;
			bSkipLinkingWhenNothingToCompile = false;
			bCompileCEF3 = true;
		}

		/// <summary>
		/// Function to call to after reset default data.
		/// </summary>
		public static void PostReset()
		{
			// Configuration overrides.
			string SteamVersion = "Steamv132";
			bCompileSteamOSS = bCompileSteamOSS
			   && Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "Steamworks/" + SteamVersion) == true;

			bCompileMcpOSS = bCompileMcpOSS
			   && Directory.Exists("Runtime/Online/NotForLicensees/OnlineSubsystemMcp") == true;


			bCompileSimplygon = bCompileSimplygon
				&& Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees") == true
				&& Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees/Simplygon") == true
				&& Directory.Exists("Developer/SimplygonMeshReduction") == true
				&& !(ProjectFileGenerator.bGenerateProjectFiles && ProjectFileGenerator.bGeneratingRocketProjectFiles);

			if (UnrealBuildTool.CommandLineContains(@"UnrealCodeAnalyzer") || UnrealBuildTool.CommandLineContains(@"UnrealHeaderTool"))
			{
				BuildConfiguration.bRunUnrealCodeAnalyzer = false;
			}

			if (BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				BuildConfiguration.bUsePCHFiles = false;
				BuildConfiguration.bUseUnityBuild = false;
				BuildConfiguration.bUseSharedPCHs = false;
				BuildConfiguration.bAllowXGE = false;
			}
		}

		/**
		 * Validates the configuration.
		 * 
		 * @warning: the order of validation is important
		 */
		public static void ValidateConfiguration()
		{
			// Lean and mean means no Editor and other frills.
			if (bCompileLeanAndMeanUE)
			{
				bBuildEditor = false;
				bBuildDeveloperTools = false;
				bCompileSimplygon = false;
				bCompileSpeedTree = false;
			}
		}
	}
}
