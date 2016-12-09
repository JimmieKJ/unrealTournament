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
		/** Architecture as stored in the ini. */
		public enum LinuxArchitecture
		{
			/** x86_64, most commonly used architecture.*/
			X86_64UnknownLinuxGnu,

			/** A.k.a. AArch32, ARM 32-bit with hardware floats */
			ArmUnknownLinuxGnueabihf,

			/** AArch64, ARM 64-bit */
			AArch64UnknownLinuxGnueabi
		}

		/** Currently active architecture */
		private string ActiveArchitecture = LinuxPlatform.DefaultArchitecture;

		public LinuxPlatformContext(FileReference InProjectFile) : base(UnrealTargetPlatform.Linux, InProjectFile)
		{
			// read settings from the config
			string EngineIniPath = ProjectFile != null ? ProjectFile.Directory.FullName : null;
			if (String.IsNullOrEmpty(EngineIniPath))
			{
				// If the project file hasn't been specified, try to get the path from -remoteini command line param
				EngineIniPath = UnrealBuildTool.GetRemoteIniPath();
			}
			DirectoryReference EngineIniDir = !String.IsNullOrEmpty(EngineIniPath) ? new DirectoryReference(EngineIniPath) : null;

			ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.Linux, "Engine", EngineIniDir);

			string LinuxArchitectureString;
			if (Ini.GetString("/Script/LinuxTargetPlatform.LinuxTargetSettings", "TargetArchitecture", out LinuxArchitectureString))
			{
				LinuxArchitecture Architecture;
				if (Enum.TryParse(LinuxArchitectureString, out Architecture))
				{
					switch (Architecture)
					{
						default:
							System.Console.WriteLine("Architecture enum value {0} does not map to a valid triplet.", Architecture);
							break;

						case LinuxArchitecture.X86_64UnknownLinuxGnu:
							ActiveArchitecture = "x86_64-unknown-linux-gnu";
							break;

						case LinuxArchitecture.ArmUnknownLinuxGnueabihf:
							ActiveArchitecture = "arm-unknown-linux-gnueabihf";
							break;

						case LinuxArchitecture.AArch64UnknownLinuxGnueabi:
							ActiveArchitecture = "aarch64-unknown-linux-gnueabi";
							break;
					}
				}
			}
		}

		/// <summary>
		/// The current architecture
		/// </summary>
		public override string GetActiveArchitecture()
		{
			return ActiveArchitecture;
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
			// depends on arch, APEX cannot be as of November'16 compiled for AArch32/64
			UEBuildConfiguration.bCompileAPEX = GetActiveArchitecture().StartsWith("x86_64");
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

			// Don't stop compilation at first error...
			BuildConfiguration.bStopXGECompilationAfterErrors = true;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_LINUX=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("LINUX=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_SUPPORTS_JEMALLOC=1");	// this define does not set jemalloc as default, just indicates its support
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_DATABASE_SUPPORT=0");		//@todo linux: valid?

			if (GetActiveArchitecture().StartsWith("arm"))	// AArch64 doesn't strictly need that - aligned access improves perf, but this will be likely offset by memcpys we're doing to guarantee it.
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

			// At the moment ICU has not been compiled for AArch64. Also, localization isn't needed on servers by default, and ICU is pretty heavy
			if (GetActiveArchitecture().StartsWith("aarch64") || InBuildTarget.TargetType == TargetRules.TargetType.Server)
			{
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
		//public const string DefaultArchitecture = "arm-unknown-linux-gnueabihf";
		//public const string DefaultArchitecture = "aarch64-unknown-linux-gnueabi";

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
			// XGE crashes with very high probability when v8_clang-3.9.0-centos cross-toolchain is used on Windows. Please make sure this is resolved before re-enabling it.
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
		static string ExpectedSDKVersion = "v8_clang-3.9.0-centos7";	// now unified for all the architectures

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
			return ExpectedSDKVersion;
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

			string MultiArchRoot = Environment.GetEnvironmentVariable("LINUX_MULTIARCH_ROOT");
			string BaseLinuxPath;

			if (MultiArchRoot != null)
			{
				// FIXME: UBT should loop across all the architectures and compile for all the selected ones.
				BaseLinuxPath = Path.Combine(MultiArchRoot, LinuxPlatform.DefaultArchitecture);
			}
			else
			{
				// support the existing, non-multiarch toolchains for continuity
				BaseLinuxPath = Environment.GetEnvironmentVariable("LINUX_ROOT");
			}

			// we don't have an LINUX_ROOT specified
			if (String.IsNullOrEmpty(BaseLinuxPath))
			{
				return SDKStatus.Invalid;
			}

			// paths to our toolchains
			BaseLinuxPath = BaseLinuxPath.Replace("\"", "");
			string ClangPath = Path.Combine(BaseLinuxPath, @"bin\clang++.exe");

			if (File.Exists(ClangPath))
			{
				return SDKStatus.Valid;
			}

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
