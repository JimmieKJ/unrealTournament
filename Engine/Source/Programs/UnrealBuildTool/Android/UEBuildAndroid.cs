// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;

namespace UnrealBuildTool
{
	class AndroidPlatform : UEBuildPlatform
	{
		/// <summary>
		/// Android settings.
		/// </summary>
		[XmlConfig]
		public static string AndroidNdkApiTarget = "android-19";
		[XmlConfig]
		public static string AndroidSdkApiTarget = "latest";

		// The current architecture - affects everything about how UBT operates on Android
		public override string GetActiveArchitecture()
		{
			// internal architectures are handled inside the toolchain to be able to build all at once, so we no longer need an architecture here
			return base.GetActiveArchitecture();
		}

		public override bool CanUseXGE()
		{
			return false;
		}

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
            return "-19";
        }

        protected override String GetRequiredScriptVersionString()
        {
            return "3.0";
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
			bool bNeedsNDKPath = string.IsNullOrEmpty(NDKPath);
			bool bNeedsAndroidHome = string.IsNullOrEmpty(Environment.GetEnvironmentVariable("ANDROID_HOME"));
			bool bNeedsAntHome = string.IsNullOrEmpty(Environment.GetEnvironmentVariable("ANT_HOME"));

			if (Utils.IsRunningOnMono && (bNeedsNDKPath || bNeedsAndroidHome || bNeedsAntHome))
			{
				// Try reading env variables we need from .bash_profile
				string BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bash_profile");
				if(File.Exists(BashProfilePath))
				{
					string[] BashProfileContents = File.ReadAllLines(BashProfilePath);
					foreach (string Line in BashProfileContents)
					{
						if (bNeedsAndroidHome && Line.StartsWith("export ANDROID_HOME="))
						{
							string PathVar = Line.Split('=')[1].Replace("\"", "");
							Environment.SetEnvironmentVariable("ANDROID_HOME", PathVar);
						}
						else if (bNeedsNDKPath && Line.StartsWith("export NDKROOT="))
						{
							string PathVar = Line.Split('=')[1].Replace("\"", "");
							Environment.SetEnvironmentVariable("NDKROOT", PathVar);
							NDKPath = PathVar;
						}
						else if (bNeedsAntHome && Line.StartsWith("export ANT_HOME="))
						{
							string PathVar = Line.Split('=')[1].Replace("\"", "");
							Environment.SetEnvironmentVariable("ANT_HOME", PathVar);
						}
					}
				}
			}

            // we don't have an NDKROOT specified
            if (String.IsNullOrEmpty(NDKPath))
            {
                return false;
            }

            NDKPath = NDKPath.Replace("\"", "");

            // need a supported llvm
            if (!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.5")) && 
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

		protected override void RegisterBuildPlatformInternal()
		{
			if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (HasRequiredSDKsInstalled() == SDKStatus.Valid))
			{
				bool bRegisterBuildPlatform = true;

				string EngineSourcePath = Path.Combine(ProjectFileGenerator.EngineRelativePath, "Source");
				string AndroidTargetPlatformFile = Path.Combine(EngineSourcePath, "Developer", "Android", "AndroidTargetPlatform", "AndroidTargetPlatform.Build.cs");

				if (File.Exists(AndroidTargetPlatformFile) == false)
				{
					bRegisterBuildPlatform = false;
				}

				if (bRegisterBuildPlatform == true)
				{
					// Register this build platform
					Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Android.ToString());
					UEBuildPlatform.RegisterBuildPlatform(UnrealTargetPlatform.Android, this);
					UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Android, UnrealPlatformGroup.Android);
				}
			}
		}

		public override CPPTargetPlatform GetCPPTargetPlatform(UnrealTargetPlatform InUnrealTargetPlatform)
		{
			switch (InUnrealTargetPlatform)
			{
				case UnrealTargetPlatform.Android:
					return CPPTargetPlatform.Android;
			}
			throw new BuildException("AndroidPlatform::GetCPPTargetPlatform: Invalid request for {0}", InUnrealTargetPlatform.ToString());
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

		public override void ResetBuildConfiguration(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			ValidateUEBuildConfiguration();
			//BuildConfiguration.bDeployAfterCompile = true;
        
            UEBuildConfiguration.bCompileICU = true;
		}

		public override void ValidateUEBuildConfiguration()
		{
			BuildConfiguration.bUseUnityBuild = true;

			UEBuildConfiguration.bCompileLeanAndMeanUE = true;
			UEBuildConfiguration.bCompilePhysX = true;
			UEBuildConfiguration.bCompileAPEX = false;
            UEBuildConfiguration.bRuntimePhysicsCooking = false;

			UEBuildConfiguration.bBuildEditor = false;
			UEBuildConfiguration.bBuildDeveloperTools = false;
			UEBuildConfiguration.bCompileSimplygon = false;

			UEBuildConfiguration.bCompileRecast = true;

			// Don't stop compilation at first error...
			BuildConfiguration.bStopXGECompilationAfterErrors = true;

			BuildConfiguration.bUseSharedPCHs = false;
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

		public override void GetExtraModules(TargetInfo Target, UEBuildTarget BuildTarget, ref List<string> PlatformExtraModules)
		{
		}

		public override void ModifyNewlyLoadedModule(UEBuildModule InModule, TargetInfo Target)
		{
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;
				if (!UEBuildConfiguration.bBuildRequiresCookedData)
				{
					if (InModule.ToString() == "Engine")
					{
						if (UEBuildConfiguration.bBuildDeveloperTools)
						{
							InModule.AddPlatformSpecificDynamicallyLoadedModule("AndroidTargetPlatform");
							InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_PVRTCTargetPlatform");
							InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ATCTargetPlatform");
							InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_DXTTargetPlatform");
							InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ETC1TargetPlatform");
							InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ETC2TargetPlatform");
							InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ES31TargetPlatform");
							// @todo gl4android				
							// InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_GL4TargetPlatform");
                        }
					}
					else if (InModule.ToString() == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						InModule.AddDynamicallyLoadedModule("TextureFormatPVR");
						InModule.AddDynamicallyLoadedModule("TextureFormatDXT");
						InModule.AddDynamicallyLoadedModule("TextureFormatASTC");
						InModule.AddPlatformSpecificDynamicallyLoadedModule("TextureFormatAndroid");    // ATITC, ETC1 and ETC2
						if (UEBuildConfiguration.bBuildDeveloperTools)
						{
							//InModule.AddDynamicallyLoadedModule("AudioFormatADPCM");	//@todo android: android audio
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (UEBuildConfiguration.bForceBuildTargetPlatforms)
				{
					InModule.AddPlatformSpecificDynamicallyLoadedModule("AndroidTargetPlatform");
					InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_PVRTCTargetPlatform");
					InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ATCTargetPlatform");
					InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_DXTTargetPlatform");
					InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ETC1TargetPlatform");
					InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ETC2TargetPlatform");
					InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_ES31TargetPlatform");
					// @todo gl4android				
					// InModule.AddPlatformSpecificDynamicallyLoadedModule("Android_GL4TargetPlatform");
                }

				if (bBuildShaderFormats)
				{
					//InModule.AddDynamicallyLoadedModule("ShaderFormatAndroid");		//@todo android: ShaderFormatAndroid
				}
			}
		}

		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			// we want gcc toolchain 4.8, but fall back to 4.6 for now if it doesn't exist
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			NDKPath = NDKPath.Replace("\"", "");

			string GccVersion = "4.8";
			if (!Directory.Exists(Path.Combine(NDKPath, @"sources/cxx-stl/gnu-libstdc++/4.8")))
			{
				GccVersion = "4.6";
			}


			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_DESKTOP=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_64BITS=0");
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
			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86/include");

			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/armeabi-v7a");
			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86");

			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/android/native_app_glue");
			InBuildTarget.GlobalCompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add("$(NDKROOT)/sources/android/cpufeatures");

            // Add path to statically compiled version of cxa_demangle
			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Android/cxa_demangle/armeabi-v7a");
			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Android/cxa_demangle/x86");

			//@TODO: Tegra Gfx Debugger
//			InBuildTarget.GlobalLinkEnvironment.Config.LibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "NVIDIA/TegraGfxDebugger");
//			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("Tegra_gfx_debugger");

            InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("gnustl_shared");
            InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("gcc");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("z");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("c");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("m");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("log");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dl");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("GLESv2");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("EGL");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("OpenSLES");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("android");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("cxa_demangle");

			UEBuildConfiguration.bCompileSimplygon = false;
			BuildConfiguration.bDeployAfterCompile = true;
		}

		public override bool ShouldCreateDebugInfo(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration)
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

		public override string[] FinalizeBinaryPaths(string BinaryName)
		{
			string[] Architectures = AndroidToolChain.GetAllArchitectures();
			string[] GPUArchitectures = AndroidToolChain.GetAllGPUArchitectures();

			// make multiple output binaries
			List<string> AllBinaries = new List<string>();
			foreach (string Architecture in Architectures)
			{
				foreach (string GPUArchitecture in GPUArchitectures)
				{
					AllBinaries.Add(AndroidToolChain.InlineArchName(BinaryName, Architecture, GPUArchitecture));
				}
			}

			return AllBinaries.ToArray();
		}
	}
}
