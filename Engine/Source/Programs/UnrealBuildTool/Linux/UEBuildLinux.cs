// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	class LinuxPlatformContext : UEBuildPlatformContext
	{
		public LinuxPlatformContext(FileReference InProjectFile) : base(UnrealTargetPlatform.Linux, InProjectFile)
		{
		}

		/// <summary>
		/// The current architecture
		/// </summary>
		public override string GetActiveArchitecture()
		{
			return LinuxPlatform.DefaultArchitecture;
		}

		/// <summary>
		/// Get name for architecture-specific directories (can be shorter than architecture name itself)
		/// </summary>
		public override string GetActiveArchitectureFolderName()
		{
			// shorten the string (heuristically)
			string Arch = GetActiveArchitecture();
			uint Sum = 0;
			int Len = Arch.Length;
			for (int Index = 0; Index < Len; ++Index)
			{
				Sum += (uint)(Arch[Index]);
				Sum <<= 1;	// allowed to overflow
			}
			return Sum.ToString("X");
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
					Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("AllDesktopTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					// Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
				}
			}
		}

		/// <summary>
		/// Gives the platform a chance to 'override' the configuration settings
		/// that are overridden on calls to RunUBT.
		/// </summary>
		/// <param name="Configuration"> The UnrealTargetConfiguration being built</param>
		public override void ResetBuildConfiguration(UnrealTargetConfiguration Configuration)
		{
			ValidateUEBuildConfiguration();
		}

		/// <summary>
		/// Validate configuration for this platform
		/// NOTE: This function can/will modify BuildConfiguration!
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="InConfiguration"> The CPPTargetConfiguration being built</param>
		/// <param name="bInCreateDebugInfo">true if debug info is getting create, false if not</param>
		public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
			UEBuildConfiguration.bCompileSimplygon = false;
            UEBuildConfiguration.bCompileSimplygonSSF = false;
		}

		/// <summary>
		/// Validate the UEBuildConfiguration for this platform
		/// This is called BEFORE calling UEBuildConfiguration to allow setting
		/// various fields used in that function such as CompileLeanAndMean...
		/// </summary>
		public override void ValidateUEBuildConfiguration()
		{
			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				// When generating project files we need intellisense generator to include info from all modules,
				// including editor-only third party libs
				UEBuildConfiguration.bCompileLeanAndMeanUE = false;
			}

			BuildConfiguration.bUseUnityBuild = true;

			// Don't stop compilation at first error...
			BuildConfiguration.bStopXGECompilationAfterErrors = true;

			BuildConfiguration.bUseSharedPCHs = false;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_LINUX=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("LINUX=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_DATABASE_SUPPORT=0");		//@todo linux: valid?

			if (GetActiveArchitecture().StartsWith("arm"))
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("REQUIRES_ALIGNED_INT_ACCESS");
			}

			// link with Linux libraries.
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("pthread");

			// Disable Simplygon support if compiling against the NULL RHI.
			if (InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Contains("USE_NULL_RHI=1"))
			{
				UEBuildConfiguration.bCompileSimplygon = false;
                UEBuildConfiguration.bCompileSimplygonSSF = false;
			}

			if (InBuildTarget.TargetType == TargetRules.TargetType.Server)
			{
				// Localization shouldn't be needed on servers by default, and ICU is pretty heavy
				UEBuildConfiguration.bCompileICU = false;
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
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

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="Platform">The platform to create a toolchain for</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CPPTargetPlatform Platform)
		{
			return new LinuxToolChain(this);
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <param name="ProjectFile">The project file of the target being deployed. Used to find any deployment specific settings.</param>
		/// <param name="DeploymentHandler">The output deployment handler</param>
		/// <returns>True if the platform requires a deployment handler, false otherwise</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return null;
		}
	}

	class LinuxPlatform : UEBuildPlatform
	{
		/// <summary>
		/// Linux architecture (compiler target triplet)
		/// </summary>
		// FIXME: for now switching between architectures is hard-coded
		public const string DefaultArchitecture = "x86_64-unknown-linux-gnu";
		//static private string DefaultArchitecture = "arm-unknown-linux-gnueabihf";

		LinuxPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		public LinuxPlatform(LinuxPlatformSDK InSDK) : base(UnrealTargetPlatform.Linux, CPPTargetPlatform.Linux)
		{
			SDK = InSDK;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform. Could be either a manual install or an AutoSDK.
		/// </summary>
		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		/// <summary>
		/// Allows the platform to override whether the architecture name should be appended to the name of binaries.
		/// </summary>
		/// <returns>True if the architecture name should be appended to the binary</returns>
		public override bool RequiresArchitectureSuffix()
		{
			// Linux ignores architecture-specific names, although it might be worth it to prepend architecture
			return false;
		}

		public override bool CanUseXGE()
		{
			// [RCL] 2015-08-04 FIXME: we have seen XGE builds fail on Windows
			return BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux;
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (i.e. 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
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
			return "";
		}

		/// <summary>
		/// Whether PDB files should be used
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="InConfiguration"> The CPPTargetConfiguration being built</param>
		/// <param name="bInCreateDebugInfo">true if debug info is getting create, false if not</param>
		/// <returns>bool true if PDB files should be used, false if not</returns>
		public override bool ShouldUsePDBFiles(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration, bool bCreateDebugInfo)
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
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				if (!UEBuildConfiguration.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (UEBuildConfiguration.bBuildDeveloperTools)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (UEBuildConfiguration.bForceBuildTargetPlatforms && ModuleName == "TargetPlatform")
				{
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
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
			return new LinuxPlatformContext(ProjectFile);
		}
	}

	class LinuxPlatformSDK : UEBuildPlatformSDK
	{
		/// <summary>
		/// This is the SDK version we support
		/// </summary>
		static private Dictionary<string, string> ExpectedSDKVersions = new Dictionary<string, string>()
		{
			{ "x86_64-unknown-linux-gnu", "v7_clang-3.7.0_ld-2.24_glibc-2.12.2" },
			{ "arm-unknown-linux-gnueabihf", "arm-unknown-linux-gnueabihf_v5_clang-3.5.0-ld-2.23.1-glibc-2.13" },
		};

		/// <summary>
		/// Platform name (embeds architecture for now)
		/// </summary>
		static private string TargetPlatformName = "Linux_x64";

		/// <summary>
		/// Whether platform supports switching SDKs during runtime
		/// </summary>
		/// <returns>true if supports</returns>
		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		/// <summary>
		/// Returns platform-specific name used in SDK repository
		/// </summary>
		/// <returns>path to SDK Repository</returns>
		public override string GetSDKTargetPlatformName()
		{
			return TargetPlatformName;
		}

		/// <summary>
		/// Returns SDK string as required by the platform
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected override string GetRequiredSDKString()
		{
			string SDKString;
			if (!ExpectedSDKVersions.TryGetValue(LinuxPlatform.DefaultArchitecture, out SDKString))
			{
				throw new BuildException("LinuxPlatform::GetRequiredSDKString: no toolchain set up for architecture '{0}'", LinuxPlatform.DefaultArchitecture);
			}

			return SDKString;
		}

		protected override String GetRequiredScriptVersionString()
		{
			return "3.0";
		}

		protected override bool PreferAutoSDK()
		{
			// having LINUX_ROOT set (for legacy reasons or for convenience of cross-compiling certain third party libs) should not make UBT skip AutoSDKs
			return true;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform
		/// </summary>
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				return SDKStatus.Valid;
			}

			string BaseLinuxPath = Environment.GetEnvironmentVariable("LINUX_ROOT");

			// we don't have an LINUX_ROOT specified
			if (String.IsNullOrEmpty(BaseLinuxPath))
				return SDKStatus.Invalid;

			// paths to our toolchains
			BaseLinuxPath = BaseLinuxPath.Replace("\"", "");
			string ClangPath = Path.Combine(BaseLinuxPath, @"bin\Clang++.exe");

			if (File.Exists(ClangPath))
				return SDKStatus.Valid;

			return SDKStatus.Invalid;
		}
	}

	class LinuxPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Linux; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms()
		{
			LinuxPlatformSDK SDK = new LinuxPlatformSDK();
			SDK.ManageAndValidateSDK();

			if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid))
			{
				bool bRegisterBuildPlatform = true;

				string EngineSourcePath = Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine", "Source");
				string LinuxTargetPlatformFile = Path.Combine(EngineSourcePath, "Developer", "Linux", "LinuxTargetPlatform", "LinuxTargetPlatform.Build.cs");

				if (File.Exists(LinuxTargetPlatformFile) == false)
				{
					bRegisterBuildPlatform = false;
				}

				if (bRegisterBuildPlatform == true)
				{
					// Register this build platform for Linux
					if (BuildConfiguration.bPrintDebugInfo)
					{
						Console.WriteLine("        Registering for {0}", UnrealTargetPlatform.Linux.ToString());
					}
					UEBuildPlatform.RegisterBuildPlatform(new LinuxPlatform(SDK));
					UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Linux, UnrealPlatformGroup.Unix);
				}
			}
		}
	}
}
