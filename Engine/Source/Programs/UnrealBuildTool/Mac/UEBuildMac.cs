// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	class MacPlatformContext : UEBuildPlatformContext
	{
		public MacPlatformContext(FileReference InProjectFile) : base(UnrealTargetPlatform.Mac, InProjectFile)
		{
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
			bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;

			if (!UEBuildConfiguration.bBuildRequiresCookedData)
			{
				if (ModuleName == "TargetPlatform")
				{
					bBuildShaderFormats = true;
				}
			}

			// allow standalone tools to use target platform modules, without needing Engine
			if (ModuleName == "TargetPlatform")
			{
				if (UEBuildConfiguration.bForceBuildTargetPlatforms)
				{
					Rules.DynamicallyLoadedModuleNames.Add("MacTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("MacNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("MacClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("MacServerTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("AllDesktopTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					// Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
					Rules.DynamicallyLoadedModuleNames.Add("MetalShaderFormat");
				}
			}
		}

		public override void ResetBuildConfiguration(UnrealTargetConfiguration Configuration)
		{
			UEBuildConfiguration.bCompileSimplygon = false;
            UEBuildConfiguration.bCompileSimplygonSSF = false;
		}

		public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				// @todo: Temporarily disable precompiled header files when building remotely due to errors
				BuildConfiguration.bUsePCHFiles = false;
			}
			BuildConfiguration.bCheckExternalHeadersForModification = BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac;
			BuildConfiguration.bCheckSystemHeadersForModification = BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac;
			BuildConfiguration.ProcessorCountMultiplier = MacToolChain.GetAdjustedProcessorCountMultiplier();
			BuildConfiguration.bUseSharedPCHs = false;

			BuildConfiguration.bUsePDBFiles = bCreateDebugInfo && Configuration != CPPTargetConfiguration.Debug && Platform == CPPTargetPlatform.Mac && BuildConfiguration.bGeneratedSYMFile;

			// we always deploy - the build machines need to be able to copy the files back, which needs the full bundle
			BuildConfiguration.bDeployAfterCompile = true;
		}

		public override void ValidateUEBuildConfiguration()
		{
			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				// When generating project files we need intellisense generator to include info from all modules, including editor-only third party libs
				UEBuildConfiguration.bCompileLeanAndMeanUE = false;
			}
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_MAC=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_APPLE=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_TTS=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			if (!InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Contains("WITH_DATABASE_SUPPORT=0") && !InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Contains("WITH_DATABASE_SUPPORT=1"))
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_DATABASE_SUPPORT=0");
			}
			// Needs OS X 10.11 for Metal
			if (MacToolChain.MacOSSDKVersionFloat >= 10.11f && UEBuildConfiguration.bCompileAgainstEngine)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_METAL=1");
				InBuildTarget.ExtraModuleNames.Add("MetalRHI");
			}
			else
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_METAL=0");
			}

		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Configuration"> The UnrealTargetConfiguration being built</param>
		/// <returns>true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(UnrealTargetConfiguration Configuration)
		{
			return true;
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="Platform">The platform to create a toolchain for</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CPPTargetPlatform Platform)
		{
			return new MacToolChain(ProjectFile);
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <param name="ProjectFile">The project file of the target being deployed. Used to find any deployment specific settings.</param>
		/// <param name="DeploymentHandler">The output deployment handler</param>
		/// <returns>True if the platform requires a deployment handler, false otherwise</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new UEDeployMac();
		}
	}

	class MacPlatform : UEBuildPlatform
	{
		MacPlatformSDK SDK;

		public MacPlatform(MacPlatformSDK InSDK) : base(UnrealTargetPlatform.Mac, CPPTargetPlatform.Mac)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public override bool CanUseXGE()
		{
			return false;
		}

		public override bool CanUseDistcc()
		{
			return true;
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binrary type being built</param>
		/// <returns>string    The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".dylib";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
				case UEBuildBinaryType.Object:
					return ".o";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".gch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public override string GetDebugInfoExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return BuildConfiguration.bUsePDBFiles ? ".dSYM" : "";
				case UEBuildBinaryType.Executable:
					return BuildConfiguration.bUsePDBFiles ? ".dSYM" : "";
				case UEBuildBinaryType.StaticLibrary:
					return "";
				case UEBuildBinaryType.Object:
					return "";
				case UEBuildBinaryType.PrecompiledHeader:
					return "";
				default:
					return "";
			}
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
		}

		/// <summary>
		/// Whether the platform requires the extra UnityCPPWriter
		/// This is used to add an extra file for UBT to get the #include dependencies from
		/// </summary>
		/// <returns>bool true if it is required, false if not</returns>
		public override bool RequiresExtraUnityCPPWriter()
		{
			return true;
		}

		/// <summary>
		/// Return whether we wish to have this platform's binaries in our builds
		/// </summary>
		public override bool IsBuildRequired()
		{
			return false;
		}

		/// <summary>
		/// Return whether we wish to have this platform's binaries in our CIS tests
		/// </summary>
		public override bool IsCISRequired()
		{
			return false;
		}

		/// <summary>
		/// Creates a context for the given project on the current platform.
		/// </summary>
		/// <param name="ProjectFile">The project file for the current target</param>
		/// <returns>New platform context object</returns>
		public override UEBuildPlatformContext CreateContext(FileReference ProjectFile)
		{
			return new MacPlatformContext(ProjectFile);
		}
	}

	class MacPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			return SDKStatus.Valid;
		}
	}

	class MacPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Mac; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms()
		{
			MacPlatformSDK SDK = new MacPlatformSDK();
			SDK.ManageAndValidateSDK();

			// Register this build platform for Mac
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Mac.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new MacPlatform(SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Mac, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Mac, UnrealPlatformGroup.Apple);
		}
	}
}
