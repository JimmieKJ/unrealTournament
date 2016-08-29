// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Linq;

namespace UnrealBuildTool
{
	class AndroidPlatformContext : UEBuildPlatformContext
	{
		public AndroidPlatformContext(FileReference InProjectFile) : base(UnrealTargetPlatform.Android, InProjectFile)
		{
		}

		// The current architecture - affects everything about how UBT operates on Android
		public override string GetActiveArchitecture()
		{
			// internal architectures are handled inside the toolchain to be able to build all at once, so we no longer need an architecture here
			return base.GetActiveArchitecture();
		}

		private bool IsVulkanSDKAvailable()
		{
			bool bHaveVulkan = false;

			// First look for VulkanSDK (two possible env variables)
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
			if (String.IsNullOrEmpty(VulkanSDKPath))
			{
				VulkanSDKPath = Environment.GetEnvironmentVariable("VK_SDK_PATH");
			}

			// Note: header is the same for all architectures so just use arch-arm
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			string NDKVulkanIncludePath = NDKPath + "/platforms/android-24/arch-arm/usr/include/vulkan";

			// Use NDK Vulkan header if discovered, or VulkanSDK if available
			if (File.Exists(NDKVulkanIncludePath + "/vulkan.h"))
			{
				bHaveVulkan = true;
			}
			else
			if (!String.IsNullOrEmpty(VulkanSDKPath))
			{
				bHaveVulkan = true;
			}
			else
			if (File.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "Vulkan/Windows/Include/vulkan/vulkan.h"))
			{
				bHaveVulkan = true;
			}

			return bHaveVulkan;
		}

		private bool IsVulkanSupportEnabled()
		{
			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", DirectoryReference.FromFile(ProjectFile));
			bool bSupportsVulkan = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportsVulkan", out bSupportsVulkan);

			return bSupportsVulkan;
		}

		public override void AddExtraModules(TargetInfo Target, List<string> PlatformExtraModules)
		{
			bool bVulkanExists = IsVulkanSDKAvailable();
			if (bVulkanExists)
			{
				bool bSupportsVulkan = IsVulkanSupportEnabled();
				
				if (bSupportsVulkan)
				{
					PlatformExtraModules.Add("VulkanRHI");
				}
				else
				{
					Log.TraceInformationOnce("Vulkan SDK is installed, but the project disabled Vulkan (bSupportsVulkan setting in Engine). Disabling Vulkan RHI for Android");
				}
			}
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

		public override void ResetBuildConfiguration(UnrealTargetConfiguration Configuration)
		{
			ValidateUEBuildConfiguration();
			//BuildConfiguration.bDeployAfterCompile = true;
		}

		public override void ValidateUEBuildConfiguration()
		{
			UEBuildConfiguration.bCompileLeanAndMeanUE = true;
			UEBuildConfiguration.bCompilePhysX = true;
			UEBuildConfiguration.bCompileAPEX = false;
			UEBuildConfiguration.bRuntimePhysicsCooking = false;

			UEBuildConfiguration.bBuildEditor = false;
			UEBuildConfiguration.bBuildDeveloperTools = false;
			UEBuildConfiguration.bCompileSimplygon = false;
			UEBuildConfiguration.bCompileSimplygonSSF = false;

			UEBuildConfiguration.bCompileRecast = true;

			// Don't stop compilation at first error...
			BuildConfiguration.bStopXGECompilationAfterErrors = true;

			BuildConfiguration.bUseSharedPCHs = false;
		}

		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			// we want gcc toolchain 4.9, but fall back to 4.8 or 4.6 for now if it doesn't exist
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			NDKPath = NDKPath.Replace("\"", "");

			AndroidToolChain ToolChain = new AndroidToolChain(InBuildTarget.ProjectFile);

			string GccVersion = "4.6";
			int NDKVersionInt = ToolChain.GetNdkApiLevelInt();
			if (Directory.Exists(Path.Combine(NDKPath, @"sources/cxx-stl/gnu-libstdc++/4.9")))
			{
				GccVersion = "4.9";
			} else
			if (Directory.Exists(Path.Combine(NDKPath, @"sources/cxx-stl/gnu-libstdc++/4.8")))
			{
				GccVersion = "4.8";
			}

			Log.TraceInformation("NDK version: {0}, GccVersion: {1}", NDKVersionInt.ToString(), GccVersion);

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_DESKTOP=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_CAN_SUPPORT_EDITORONLY_DATA=0");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_OGGVORBIS=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("UNICODE");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_UNICODE");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_ANDROID=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("ANDROID=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_DATABASE_SUPPORT=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_EDITOR=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("USE_NULL_RHI=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("REQUIRES_ALIGNED_INT_ACCESS");

			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/include");

			// the toolchain will actually filter these out
			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/armeabi-v7a/include");
			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/arm64-v8a/include");
			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86/include");
			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86_64/include");

			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/armeabi-v7a");
			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/arm64-v8a");
			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86");
			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86_64");

			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/android/native_app_glue");
			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/android/cpufeatures");

			//@TODO: Tegra Gfx Debugger - standardize locations - for now, change the hardcoded paths and force this to return true to test
			if (UseTegraGraphicsDebugger(InBuildTarget))
			{
				//InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "NVIDIA/TegraGfxDebugger");
				//InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add("F:/NVPACK/android-kk-egl-t124-a32/stub");
				//InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("Nvidia_gfx_debugger_stub");
			}

			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("gnustl_shared");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("gcc");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("z");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("c");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("m");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("log");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dl");
			if (!UseTegraGraphicsDebugger(InBuildTarget))
			{
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("GLESv2");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("EGL");
			}
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("OpenSLES");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("android");

			UEBuildConfiguration.bCompileSimplygon = false;
			UEBuildConfiguration.bCompileSimplygonSSF = false;
			BuildConfiguration.bDeployAfterCompile = true;

			bool bBuildWithVulkan = IsVulkanSDKAvailable() && IsVulkanSupportEnabled();
			if (bBuildWithVulkan)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_ANDROID_VULKAN=1");
                Log.TraceInformationOnce("building with VULKAN define");
			}
			else
			{
				Log.TraceInformationOnce("building WITHOUT VULKAN define");
			}
		}

		private bool UseTegraGraphicsDebugger(UEBuildTarget InBuildTarget)
		{
			// Disable for now
			return false;
		}

		public override bool ShouldCreateDebugInfo(UnrealTargetConfiguration Configuration)
		{
			switch (Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
				case UnrealTargetConfiguration.Debug:
				default:
					return true;
			};
		}

		public override UEToolChain CreateToolChain(CPPTargetPlatform Platform)
		{
			return new AndroidToolChain(ProjectFile);
		}

		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new UEDeployAndroid(ProjectFile);
		}
	}

	class AndroidPlatform : UEBuildPlatform
	{
		AndroidPlatformSDK SDK;

		public AndroidPlatform(AndroidPlatformSDK InSDK) : base(UnrealTargetPlatform.Android, CPPTargetPlatform.Android)
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

		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
				case UEBuildBinaryType.Executable:
					return ".so";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
				case UEBuildBinaryType.Object:
					return ".o";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".gch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		public override bool ShouldUsePCHFiles(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration)
		{
			return true;
		}

		public override string GetDebugInfoExtension(UEBuildBinaryType InBinaryType)
		{
			return "";
		}

		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectPath)
		{
			string[] BoolKeys = new string[] {
				"bBuildForArmV7", "bBuildForArm64", "bBuildForX86", "bBuildForX8664", 
				"bBuildForES2", "bBuildForESDeferred", "bSupportsVulkan", "bBuildForES3"
            };

			// look up Android specific settings
			if (!DoProjectSettingsMatchDefault(Platform, ProjectPath, "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings",
				BoolKeys, null, null))
			{
				return false;
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectPath);
		}

		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			// This platform currently always compiles monolithic
			return true;
		}

		public override bool ShouldUsePDBFiles(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration, bool bCreateDebugInfo)
		{
			return true;
		}

		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool RequiresDeployPrepAfterCompile()
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
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ASTCTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("TextureFormatAndroid");    // ATITC, ETC1 and ETC2
						if (UEBuildConfiguration.bBuildDeveloperTools)
						{
							//Rules.DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");	//@todo android: android audio
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (UEBuildConfiguration.bForceBuildTargetPlatforms)
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ASTCTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
					}

					if (bBuildShaderFormats)
					{
						//Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatAndroid");		//@todo android: ShaderFormatAndroid
					}
				}
			}
		}

		public override List<FileReference> FinalizeBinaryPaths(FileReference BinaryName, FileReference ProjectFile)
		{
			AndroidToolChain ToolChain = new AndroidToolChain(ProjectFile);

			var Architectures = ToolChain.GetAllArchitectures();
			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();

			// make multiple output binaries
			List<FileReference> AllBinaries = new List<FileReference>();
			foreach (string Architecture in Architectures)
			{
				foreach (string GPUArchitecture in GPUArchitectures)
				{
					AllBinaries.Add(new FileReference(AndroidToolChain.InlineArchName(BinaryName.FullName, Architecture, GPUArchitecture)));
				}
			}

			return AllBinaries;
		}

		/// <summary>
		/// Creates a context for the given project on the current platform.
		/// </summary>
		/// <param name="ProjectFile">The project file for the current target</param>
		/// <returns>New platform context object</returns>
		public override UEBuildPlatformContext CreateContext(FileReference ProjectFile)
		{
			return new AndroidPlatformContext(ProjectFile);
		}
	}

	class AndroidPlatformSDK : UEBuildPlatformSDK
	{
		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		public override string GetSDKTargetPlatformName()
		{
			return "Android";
		}

		protected override string GetRequiredSDKString()
		{
			return "-21";
		}

		protected override String GetRequiredScriptVersionString()
		{
			return "3.1";
		}

		// prefer auto sdk on android as correct 'manual' sdk detection isn't great at the moment.
		protected override bool PreferAutoSDK()
		{
			return true;
		}

		/// <summary>
		/// checks if the sdk is installed or has been synced
		/// </summary>
		/// <returns></returns>
		private bool HasAnySDK()
		{
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			{
				var configCacheIni = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.Unknown, "Engine", (DirectoryReference)null);
				var AndroidEnv = new Dictionary<string, string>();

				Dictionary<string, string> EnvVarNames = new Dictionary<string, string> { 
                                                         {"ANDROID_HOME", "SDKPath"}, 
                                                         {"NDKROOT", "NDKPath"}, 
                                                         {"ANT_HOME", "ANTPath"},
                                                         {"JAVA_HOME", "JavaPath"}
                                                         };

				string path;
				foreach (var kvp in EnvVarNames)
				{
					if (configCacheIni.GetPath("/Script/AndroidPlatformEditor.AndroidSDKSettings", kvp.Value, out path) && !string.IsNullOrEmpty(path))
					{
						AndroidEnv.Add(kvp.Key, path);
					}
					else
					{
						var envValue = Environment.GetEnvironmentVariable(kvp.Key);
						if (!String.IsNullOrEmpty(envValue))
						{
							AndroidEnv.Add(kvp.Key, envValue);
						}
					}
				}

				// If we are on Mono and we are still missing a key then go and find it from the .bash_profile
				if (Utils.IsRunningOnMono && !EnvVarNames.All(s => AndroidEnv.ContainsKey(s.Key)))
				{
					string BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bash_profile");
					if (File.Exists(BashProfilePath))
					{
						string[] BashProfileContents = File.ReadAllLines(BashProfilePath);

						// Walk backwards so we keep the last export setting instead of the first
						for (int LineIndex = BashProfileContents.Length - 1; LineIndex >= 0; --LineIndex)
						{
							foreach (var kvp in EnvVarNames)
							{
								if (AndroidEnv.ContainsKey(kvp.Key))
								{
									continue;
								}

								if (BashProfileContents[LineIndex].StartsWith("export " + kvp.Key + "="))
								{
									string PathVar = BashProfileContents[LineIndex].Split('=')[1].Replace("\"", "");
									AndroidEnv.Add(kvp.Key, PathVar);
								}
							}
						}
					}
				}

				// Set for the process
				foreach (var kvp in AndroidEnv)
				{
					Environment.SetEnvironmentVariable(kvp.Key, kvp.Value);
				}

				// See if we have an NDK path now...
				AndroidEnv.TryGetValue("NDKROOT", out NDKPath);
			}

			// we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				return false;
			}

			NDKPath = NDKPath.Replace("\"", "");

			// need a supported llvm
			if (!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.6")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.5")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.3")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.1")))
			{
				return false;
			}
			return true;
		}

		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			// if any autosdk setup has been done then the local process environment is suspect
			if (HasSetupAutoSDK())
			{
				return SDKStatus.Invalid;
			}

			if (HasAnySDK())
			{
				return SDKStatus.Valid;
			}

			return SDKStatus.Invalid;
		}
	}

	public class AndroidPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Android; }
		}

		protected override void RegisterBuildPlatforms()
		{
			AndroidPlatformSDK SDK = new AndroidPlatformSDK();
			SDK.ManageAndValidateSDK();

			if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid) || Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
			{
				bool bRegisterBuildPlatform = true;

				FileReference AndroidTargetPlatformFile = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Developer", "Android", "AndroidTargetPlatform", "AndroidTargetPlatform.Build.cs");
				if (AndroidTargetPlatformFile.Exists() == false)
				{
					bRegisterBuildPlatform = false;
				}

				if (bRegisterBuildPlatform == true)
				{
					// Register this build platform
					Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Android.ToString());
					UEBuildPlatform.RegisterBuildPlatform(new AndroidPlatform(SDK));
					UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Android, UnrealPlatformGroup.Android);
				}
			}
		}
	}
}
