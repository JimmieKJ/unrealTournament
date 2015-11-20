// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;

namespace UnrealBuildTool
{
	public class IOSPlatformContext : UEBuildPlatformContext
	{
		private bool bInitializedProject = false;

		/// <summary>
		/// Which version of the iOS to allow at run time
		/// </summary>
		public string RunTimeIOSVersion = "7.0";

		/// <summary>
		/// which devices the game is allowed to run on
		/// </summary>
		public string RunTimeIOSDevices = "1,2";

		/// <summary>
		/// The architecture(s) to compile
		/// </summary>
		public string NonShippingArchitectures = "armv7";
		public string ShippingArchitectures = "armv7,arm64";

		/// <summary>
		/// additional linker flags for shipping
		/// </summary>
		public string AdditionalShippingLinkerFlags = "";

		/// <summary>
		/// additional linker flags for non-shipping
		/// </summary>
		public string AdditionalLinkerFlags = "";

		/// <summary>
		/// mobile provision to use for code signing
		/// </summary>
		public string MobileProvision = "";

		/// <summary>
		/// signing certificate to use for code signing
		/// </summary>
		public string SigningCertificate = "";

		public IOSPlatformContext(FileReference InProjectFile) : base(UnrealTargetPlatform.IOS, InProjectFile)
		{
		}

		// The current architecture - affects everything about how UBT operates on IOS
		public override string GetActiveArchitecture()
		{
			return IOSPlatform.IOSArchitecture;
		}

		public string GetRunTimeVersion()
		{
			return RunTimeIOSVersion;
		}

		public string GetRunTimeDevices()
		{
			return RunTimeIOSDevices;
		}

		public string GetAdditionalLinkerFlags(CPPTargetConfiguration InConfiguration)
		{
			if (InConfiguration != CPPTargetConfiguration.Shipping)
			{
				return AdditionalLinkerFlags;
			}
			else
			{
				return AdditionalShippingLinkerFlags;
			}

		}

		public override void SetUpProjectEnvironment()
		{
			if (!bInitializedProject)
			{
				base.SetUpProjectEnvironment();

				// update the configuration based on the project file
				// look in ini settings for what platforms to compile for
				ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(Platform, "Engine", DirectoryReference.FromFile(ProjectFile));
				string MinVersion = "IOS_6";
				if (Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MinimumiOSVersion", out MinVersion))
				{
					switch (MinVersion)
					{
						case "IOS_61":
							RunTimeIOSVersion = "6.1";
							break;
						case "IOS_7":
							RunTimeIOSVersion = "7.0";
							break;
						case "IOS_8":
							RunTimeIOSVersion = "8.0";
							break;
					}
				}

				bool biPhoneAllowed = true;
				bool biPadAllowed = true;
				Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPhone", out biPhoneAllowed);
				Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPad", out biPadAllowed);
				if (biPhoneAllowed && biPadAllowed)
				{
					RunTimeIOSDevices = "1,2";
				}
				else if (biPadAllowed)
				{
					RunTimeIOSDevices = "2";
				}
				else if (biPhoneAllowed)
				{
					RunTimeIOSDevices = "1";
				}

				List<string> ProjectArches = new List<string>();
				bool bBuild = true;
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArmV7", out bBuild) && bBuild)
				{
					ProjectArches.Add("armv7");
				}
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArm64", out bBuild) && bBuild)
				{
					ProjectArches.Add("arm64");
				}
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArmV7S", out bBuild) && bBuild)
				{
					ProjectArches.Add("armv7s");
				}

				// force armv7 if something went wrong
				if (ProjectArches.Count == 0)
				{
					ProjectArches.Add("armv7");
				}
				NonShippingArchitectures = ProjectArches[0];
				for (int Index = 1; Index < ProjectArches.Count; ++Index)
				{
					NonShippingArchitectures += "," + ProjectArches[Index];
				}

				ProjectArches.Clear();
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArmV7", out bBuild) && bBuild)
				{
					ProjectArches.Add("armv7");
				}
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArm64", out bBuild) && bBuild)
				{
					ProjectArches.Add("arm64");
				}
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArmV7S", out bBuild) && bBuild)
				{
					ProjectArches.Add("armv7s");
				}

				// force armv7 if something went wrong
				if (ProjectArches.Count == 0)
				{
					ProjectArches.Add("armv7");
					ProjectArches.Add("arm64");
				}
				ShippingArchitectures = ProjectArches[0];
				for (int Index = 1; Index < ProjectArches.Count; ++Index)
				{
					ShippingArchitectures += "," + ProjectArches[Index];
				}

				// determine if we need to generate the dsym
				Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGeneratedSYMFile", out BuildConfiguration.bGeneratedSYMFile);

				Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalLinkerFlags", out AdditionalLinkerFlags);
				Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalShippingLinkerFlags", out AdditionalShippingLinkerFlags);

				Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MobileProvision", out MobileProvision);
				Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "SigningCertificate", out SigningCertificate);

				bInitializedProject = true;
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Configuration"> The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(UnrealTargetConfiguration Configuration)
		{
			return true;
		}

		public override void ResetBuildConfiguration(UnrealTargetConfiguration Configuration)
		{
			UEBuildConfiguration.bBuildEditor = false;
			UEBuildConfiguration.bBuildDeveloperTools = false;
			UEBuildConfiguration.bCompileAPEX = false;
			UEBuildConfiguration.bRuntimePhysicsCooking = false;
			UEBuildConfiguration.bCompileSimplygon = false;
			UEBuildConfiguration.bBuildDeveloperTools = false;
			UEBuildConfiguration.bCompileICU = true;

			// we currently don't have any simulator libs for PhysX
			if (GetActiveArchitecture() == "-simulator")
			{
				UEBuildConfiguration.bCompilePhysX = false;
			}
		}

		public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
			// check the base first
			base.ValidateBuildConfiguration(Configuration, Platform, bCreateDebugInfo);

			BuildConfiguration.bUsePCHFiles = false;
			BuildConfiguration.bUseSharedPCHs = false;
			BuildConfiguration.bCheckExternalHeadersForModification = false;
			BuildConfiguration.bCheckSystemHeadersForModification = false;
			BuildConfiguration.ProcessorCountMultiplier = IOSToolChain.GetAdjustedProcessorCountMultiplier();
			BuildConfiguration.bDeployAfterCompile = true;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_IOS=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_APPLE=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_TTS=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_DATABASE_SUPPORT=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_EDITOR=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("USE_NULL_RHI=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("REQUIRES_ALIGNED_INT_ACCESS");

			if (GetActiveArchitecture() == "-simulator")
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_SIMULATOR=1");
			}

			// needs IOS8 for Metal
			if (IOSToolChain.IOSSDKVersionFloat >= 8.0 && UEBuildConfiguration.bCompileAgainstEngine)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_METAL=1");
				InBuildTarget.ExtraModuleNames.Add("MetalRHI");
			}
			else
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_METAL=0");
			}

			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalFrameworks.Add(new UEBuildFramework("GameKit"));
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalFrameworks.Add(new UEBuildFramework("StoreKit"));
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
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="Platform">The platform to create a toolchain for</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CPPTargetPlatform Platform)
		{
			return new IOSToolChain(ProjectFile, this);
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <param name="ProjectFile">The project file of the target being deployed. Used to find any deployment specific settings.</param>
		/// <param name="DeploymentHandler">The output deployment handler</param>
		/// <returns>True if the platform requires a deployment handler, false otherwise</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new UEDeployIOS();
		}
	}

	public class IOSPlatform : UEBuildPlatform
	{
		// by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
		[XmlConfig]
		public static string IOSArchitecture = "";

		IOSPlatformSDK SDK;

		public IOSPlatform(IOSPlatformSDK InSDK) : base(UnrealTargetPlatform.IOS, CPPTargetPlatform.IOS)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
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

		public override string GetDebugInfoExtension(UEBuildBinaryType InBinaryType)
		{
			return BuildConfiguration.bGeneratedSYMFile ? ".dSYM" : "";
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
		/// Check for the default configuration
		/// return true if the project uses the default build config
		/// </summary>
		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			string[] BoolKeys = new string[] {
				"bDevForArmV7", "bDevForArm64", "bDevForArmV7S", "bShipForArmV7", 
				"bShipForArm64", "bShipForArmV7S", "bGeneratedSYMFile",
			};
			string[] StringKeys = new string[] {
				"MinimumiOSVersion", 
				"AdditionalLinkerFlags",
				"AdditionalShippingLinkerFlags"
			};

			// look up iOS specific settings
			if (!DoProjectSettingsMatchDefault(Platform, ProjectDirectoryName, "/Script/IOSRuntimeSettings.IOSRuntimeSettings",
					BoolKeys, null, StringKeys))
			{
				return false;
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectDirectoryName);
		}

		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true; // for iOS can only run cooked. this is mostly for testing console code paths.
		}

		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			// This platform currently always compiles monolithic
			return true;
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
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac))
			{
				bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;
				if (!UEBuildConfiguration.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (UEBuildConfiguration.bBuildDeveloperTools)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
						if (UEBuildConfiguration.bBuildDeveloperTools)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (UEBuildConfiguration.bForceBuildTargetPlatforms)
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					}

					if (bBuildShaderFormats)
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("MetalShaderFormat");
					}
				}
			}
		}

		/// <summary>
		/// Creates a context for the given project on the current platform.
		/// </summary>
		/// <param name="ProjectFile">The project file for the current target</param>
		/// <returns>New platform context object</returns>
		public override UEBuildPlatformContext CreateContext(FileReference ProjectFile)
		{
			return new IOSPlatformContext(ProjectFile);
		}
	}

	public class IOSPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			if (!Utils.IsRunningOnMono)
			{
				// check to see if iTunes is installed
				string dllPath = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "iTunesMobileDeviceDLL", null) as string;
				if (String.IsNullOrEmpty(dllPath) || !File.Exists(dllPath))
				{
					dllPath = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "MobileDeviceDLL", null) as string;
					if (String.IsNullOrEmpty(dllPath) || !File.Exists(dllPath))
					{
						return SDKStatus.Invalid;
					}
				}
			}
			return SDKStatus.Valid;
		}
	}

	public class IOSPlatformFactory : UEBuildPlatformFactory
	{
		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms()
		{
			IOSPlatformSDK SDK = new IOSPlatformSDK();
			SDK.ManageAndValidateSDK();

			// Register this build platform for IOS
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.IOS.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new IOSPlatform(SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Apple);

			if (IOSPlatform.IOSArchitecture == "-simulator")
			{
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Simulator);
			}
			else
			{
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Device);
			}
		}
	}
}

