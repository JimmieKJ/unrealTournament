// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;

namespace UnrealBuildTool
{
	class TVOSPlatformContext : IOSPlatformContext
	{
		/** Which version of the iOS to allow at run time */
		private const string RunTimeTVOSVersion = "9.0";

		/** which devices the game is allowed to run on */
		private const string RunTimeTVOSDevices = "3";

		// by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
		public static string TVOSArchitecture = "";

		public TVOSPlatformContext(FileReference InProjectFile)
			: base(UnrealTargetPlatform.IOS, InProjectFile)
		{
		}

		// The current architecture - affects everything about how UBT operates on IOS
		public override string GetActiveArchitecture()
		{
			return TVOSArchitecture;
		}

		public override string GetRunTimeVersion()
		{
			return RunTimeTVOSVersion;
		}

		public override string GetRunTimeDevices()
		{
			return RunTimeTVOSDevices;
		}

		// The name that Xcode uses for the platform
		private const string XcodeDevicePlatformName = "AppleTVOS";
		private const string XcodeSimulatorPlatformName = "AppleTVSimulator";

		public override string GetXcodePlatformName(bool bForDevice)
		{
			return bForDevice ? XcodeDevicePlatformName : XcodeSimulatorPlatformName;
		}

		public override string GetXcodeMinVersionParam()
		{
			return "tvos-version-min";
		}

		public override string GetCodesignPlatformName()
		{
			return "appletvos";
		}

		public override string GetArchitectureArgument(CPPTargetConfiguration Configuration, string UBTArchitecture)
		{
			// TV is only arm64
			return " -arch " + (UBTArchitecture == "-simulator" ? "i386" : "arm64");
		}

		public override void SetUpProjectEnvironment()
		{
			base.SetUpProjectEnvironment();

			// @todo tvos: Add ini settings and look them up when they matter - like when we get TVOS10.0 etc
		}

		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			base.SetUpEnvironment(InBuildTarget);
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_TVOS=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("__IPHONE_OS_VERSION_MIN_REQUIRED=__APPLETV_OS_VERSION_MIN_REQUIRED");

			// make sure we add Metal, in case base class got it wrong

			if (InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Contains("HAS_METAL=0"))
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Remove("HAS_METAL=0");
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_METAL=1");
				InBuildTarget.ExtraModuleNames.Add("MetalRHI");
			}
		}

		public override void ResetBuildConfiguration(UnrealTargetConfiguration InConfiguration)
		{
			base.ResetBuildConfiguration(InConfiguration);
			// @todo tvos: change what is compiled here
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="Platform">The platform to create a toolchain for</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CPPTargetPlatform Platform)
		{
			return new TVOSToolChain(ProjectFile, this);
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <param name="ProjectFile">The project file of the target being deployed. Used to find any deployment specific settings.</param>
		/// <param name="DeploymentHandler">The output deployment handler</param>
		/// <returns>True if the platform requires a deployment handler, false otherwise</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new UEDeployTVOS(ProjectFile);
		}

	}

	class TVOSPlatform : IOSPlatform
    {
		public TVOSPlatform(IOSPlatformSDK InSDK)
			: base(InSDK, UnrealTargetPlatform.TVOS, CPPTargetPlatform.TVOS)
		{
		}

		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
			base.ModifyModuleRulesForOtherPlatform(ModuleName, Rules, Target);

			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac))
			{
				// allow standalone tools to use targetplatform modules, without needing Engine
				if (UEBuildConfiguration.bForceBuildTargetPlatforms)
				{
					// @todo tvos: Make the module
					// InModule.AddPlatformSpecificDynamicallyLoadedModule("TVOSTargetPlatform");
				}
			}
		}
    
		public override UEBuildPlatformContext CreateContext(FileReference ProjectFile)
		{
			return new TVOSPlatformContext(ProjectFile);
		}
	}

	public class TVOSPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.TVOS; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms()
		{
			IOSPlatformSDK SDK = new IOSPlatformSDK();
			SDK.ManageAndValidateSDK();

			// Register this build platform for IOS
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.TVOS.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new TVOSPlatform(SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.TVOS, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.TVOS, UnrealPlatformGroup.Apple);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.TVOS, UnrealPlatformGroup.IOS);

			if (TVOSPlatformContext.TVOSArchitecture == "-simulator")
			{
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.TVOS, UnrealPlatformGroup.Simulator);
			}
			else
			{
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.TVOS, UnrealPlatformGroup.Device);
			}
		}
	}

}

