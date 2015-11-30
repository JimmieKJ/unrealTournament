// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Linq;

namespace UnrealBuildTool
{
	/// <summary>
	/// Available compiler toolchains on Windows platform
	/// </summary>
	public enum WindowsCompiler
	{
		/// Visual Studio 2012 (Visual C++ 11.0). No longer supported for building on Windows, but required for other platform toolchains.
		VisualStudio2012,

		/// Visual Studio 2013 (Visual C++ 12.0)
		VisualStudio2013,

		/// Visual Studio 2015 (Visual C++ 14.0)
		VisualStudio2015,
	}

	public class WindowsPlatformContext : UEBuildPlatformContext
	{
		/// <summary>
		/// True if we're targeting Windows XP as a minimum spec.  In Visual Studio 2012 and higher, this may change how
		/// we compile and link the application (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
		/// This is a flag to determine we should support XP if possible from XML
		/// </summary>
		public bool SupportWindowsXP;

		public WindowsPlatformContext(UnrealTargetPlatform InPlatform, FileReference InProjectFile) : base(InPlatform, InProjectFile)
		{
			if(Platform == UnrealTargetPlatform.Win32)
			{
				// ...check if it was supported from a config.
				ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.Win64, "Engine", DirectoryReference.FromFile(ProjectFile));
				string MinimumOS;
				if (Ini.GetString("/Script/WindowsTargetPlatform.WindowsTargetSettings", "MinimumOSVersion", out MinimumOS))
				{
					if (string.IsNullOrEmpty(MinimumOS) == false)
					{
						SupportWindowsXP = MinimumOS == "MSOS_XP";
					}
				}
			}
		}

		/// <summary>
		/// The current architecture
		/// </summary>
		public override string GetActiveArchitecture()
		{
			return SupportWindowsXP ? "_XP" : base.GetActiveArchitecture();
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
					Rules.DynamicallyLoadedModuleNames.Add("WindowsTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("WindowsNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("WindowsServerTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("WindowsClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("AllDesktopTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");

					//#todo-rco: Remove when public
					string VulkanSDKPath = Environment.GetEnvironmentVariable("VK_SDK_PATH");
					{
						if (!String.IsNullOrEmpty(VulkanSDKPath))
						{
							Rules.DynamicallyLoadedModuleNames.Add("VulkanShaderFormat");
						}
					}
				}
			}

			if (ModuleName == "D3D11RHI")
			{
				// To enable platform specific D3D11 RHI Types
				Rules.PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private/Windows");
			}

			if (ModuleName == "D3D12RHI")
			{
				// To enable platform specific D3D12 RHI Types
				Rules.PrivateIncludePaths.Add("Runtime/Windows/D3D12RHI/Private/Windows");
			}

			if(SupportWindowsXP)
			{
				Rules.DynamicallyLoadedModuleNames.Remove("D3D12RHI");
				Rules.DynamicallyLoadedModuleNames.Remove("VulkanRHI");

				// If we're targeting Windows XP, then always delay-load D3D11 as it won't exist on that architecture
				if(ModuleName == "DX11")
				{
					Rules.PublicDelayLoadDLLs.Add("d3d11.dll");
					Rules.PublicDelayLoadDLLs.Add("dxgi.dll");
				}
			}
		}

		public override void ResetBuildConfiguration(UnrealTargetConfiguration Configuration)
		{
			UEBuildConfiguration.bCompileICU = true;
		}

		public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
			if (WindowsPlatform.bCompileWithClang)
			{
				// @todo clang: Shared PCHs don't work on clang yet because the PCH will have definitions assigned to different values
				// than the consuming translation unit.  Unlike the warning in MSVC, this is a compile in Clang error which cannot be suppressed
				BuildConfiguration.bUseSharedPCHs = false;

				// @todo clang: PCH files aren't supported by "clang-cl" yet (no /Yc support, and "-x c++-header" cannot be specified)
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					BuildConfiguration.bUsePCHFiles = false;
				}
			}
		}

		/// <summary>
		/// Validate the UEBuildConfiguration for this platform
		/// This is called BEFORE calling UEBuildConfiguration to allow setting
		/// various fields used in that function such as CompileLeanAndMean...
		/// </summary>
		public override void ValidateUEBuildConfiguration()
		{
			UEBuildConfiguration.bCompileICU = true;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WIN32=1");

			// Should we enable Windows XP support
			if(UEBuildConfiguration.PreferredSubPlatform.Equals("WindowsXP", StringComparison.InvariantCultureIgnoreCase))
			{
				SupportWindowsXP = true;
			}

			if (WindowsPlatform.bUseWindowsSDK10 && WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
			{
				if (SupportWindowsXP)
				{
					throw new NotSupportedException("Windows XP support is not possible when targeting the Windows 10 SDK");
				}
				// Windows 8 or higher required
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_WIN32_WINNT=0x0602");
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINVER=0x0602");
			}
			else
			{
				if (SupportWindowsXP)
				{
					// Windows XP SP3 or higher required
					InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_WIN32_WINNT=0x0502");
					InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINVER=0x0502");
				}
				else
				{
					// Windows Vista or higher required
					InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_WIN32_WINNT=0x0600");
					InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINVER=0x0600");
				}
			}
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_WINDOWS=1");

			String MorpheusShaderPath = Path.Combine(BuildConfiguration.RelativeEnginePath, "Shaders/PS4/PostProcessHMDMorpheus.usf");
			//@todo: VS2015 currently does not have support for Morpheus
			if (File.Exists(MorpheusShaderPath) && WindowsPlatform.Compiler != WindowsCompiler.VisualStudio2015)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_MORPHEUS=1");

				//on PS4 the SDK now handles distortion correction.  On PC we will still have to handle it manually,				
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("MORPHEUS_ENGINE_DISTORTION=1");
			}

			if (InBuildTarget.Rules != null)
			{
				// Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
				// runtime library as the engine.
				bool bUseDebugCRT = InBuildTarget.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT;
				if (!InBuildTarget.Rules.bUseStaticCRT || bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCMT");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPMT");
				}
				if (!InBuildTarget.Rules.bUseStaticCRT || !bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCMTD");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPMTD");
				}
				if (InBuildTarget.Rules.bUseStaticCRT || bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCRT");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCPRT");
				}
				if (InBuildTarget.Rules.bUseStaticCRT || !bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCRTD");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCPRTD");
				}
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBC");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCP");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCD");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPD");

				//@todo ATL: Currently, only VSAccessor requires ATL (which is only used in editor builds)
				// When compiling games, we do not want to include ATL - and we can't when compiling Rocket games
				// due to VS 2012 Express not including ATL.
				// If more modules end up requiring ATL, this should be refactored into a BuildTarget flag (bNeedsATL)
				// that is set by the modules the target includes to allow for easier tracking.
				// Alternatively, if VSAccessor is modified to not require ATL than we should always exclude the libraries.
				if (InBuildTarget.ShouldCompileMonolithic() && (InBuildTarget.Rules != null) &&
					(TargetRules.IsGameType(InBuildTarget.TargetType)) && (TargetRules.IsEditorType(InBuildTarget.TargetType) == false))
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atl");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atls");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atlsd");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atlsn");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atlsnd");
				}

				// Add the library used for the delayed loading of DLLs.
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("delayimp.lib");

				//@todo - remove once FB implementation uses Http module
				if (UEBuildConfiguration.bCompileAgainstEngine)
				{
					// link against wininet (used by FBX and Facebook)
					InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("wininet.lib");
				}

				// Compile and link with Win32 API libraries.
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("rpcrt4.lib");
				//InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("wsock32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("ws2_32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dbghelp.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("comctl32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("Winmm.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("kernel32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("user32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("gdi32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("winspool.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("comdlg32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("advapi32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("shell32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("ole32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("oleaut32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("uuid.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("odbc32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("odbccp32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("netapi32.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("iphlpapi.lib");
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("setupapi.lib"); //  Required for access monitor device enumeration

				// Windows Vista/7 Desktop Windows Manager API for Slate Windows Compliance
				if (SupportWindowsXP == false)		// Windows XP does not support DWM
				{
					InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dwmapi.lib");
				}

				// IME
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("imm32.lib");

				// Setup assembly path for .NET modules.  This will only be used when CLR is enabled. (CPlusPlusCLR module types)
				InBuildTarget.GlobalCompileEnvironment.Config.SystemDotNetAssemblyPaths.Add(
					Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86) +
					"/Reference Assemblies/Microsoft/Framework/.NETFramework/v4.0");

				// Setup default assemblies for .NET modules.  These will only be used when CLR is enabled. (CPlusPlusCLR module types)
				InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.dll");
				InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Data.dll");
				InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Drawing.dll");
				InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Xml.dll");
				InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Management.dll");
				InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Windows.Forms.dll");
				InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("WindowsBase.dll");
			}

			// Disable Simplygon support if compiling against the NULL RHI.
			if (InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Contains("USE_NULL_RHI=1"))
			{
				UEBuildConfiguration.bCompileSimplygon = false;
			}

			// For 64-bit builds, we'll forcibly ignore a linker warning with DirectInput.  This is
			// Microsoft's recommended solution as they don't have a fixed .lib for us.
			if (InBuildTarget.Platform == UnrealTargetPlatform.Win64)
			{
				InBuildTarget.GlobalLinkEnvironment.Config.AdditionalArguments += " /ignore:4078";
			}
		}

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpConfigurationEnvironment(TargetInfo Target, CPPEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			// Determine the C++ compile/link configuration based on the Unreal configuration.
			CPPTargetConfiguration CompileConfiguration;
			//@todo SAS: Add a true Debug mode!
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
				// Default to Development; can be overridden by individual modules.
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

			// NOTE: Even when debug info is turned off, we currently force the linker to generate debug info
			//       anyway on Visual C++ platforms.  This will cause a PDB file to be generated with symbols
			//       for most of the classes and function/method names, so that crashes still yield somewhat
			//       useful call stacks, even though compiler-generate debug info may be disabled.  This gives
			//       us much of the build-time savings of fully-disabled debug info, without giving up call
			//       data completely.
			GlobalLinkEnvironment.Config.bCreateDebugInfo = true;
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Configuration"> The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(UnrealTargetConfiguration Configuration)
		{
			switch (Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
					return !BuildConfiguration.bOmitPCDebugInfoInDevelopment;
				case UnrealTargetConfiguration.DebugGame:
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
			return new VCToolChain(Platform, SupportWindowsXP);
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <returns>New deployment handler, or null if not required</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new BaseWindowsDeploy();
		}
	}

	public class WindowsPlatform : UEBuildPlatform
	{
		/// Property caching.
		private static WindowsCompiler? CachedCompiler;

		/// Version of the compiler toolchain to use on Windows platform
		public static WindowsCompiler Compiler
		{
			get
			{
				// Cache the result because Compiler is often used.
				if (CachedCompiler.HasValue)
				{
					return CachedCompiler.Value;
				}

				// First, default based on whether there is a command line override...
				if (UnrealBuildTool.CommandLineContains("-2012"))
				{
					// We don't support compiling with VS 2012 on Windows platform, but you can still generate project files that are 2012-compatible.  That's handled elsewhere
				}

				if (UnrealBuildTool.CommandLineContains("-2013"))
				{
					CachedCompiler = WindowsCompiler.VisualStudio2013;
				}
				else if (UnrealBuildTool.CommandLineContains("-2015"))
				{
					CachedCompiler = WindowsCompiler.VisualStudio2015;
				}

				// Second, default based on what's installed, test for 2013 first
				else if (!String.IsNullOrEmpty(WindowsPlatform.GetVSComnToolsPath(WindowsCompiler.VisualStudio2013)))
				{
					CachedCompiler = WindowsCompiler.VisualStudio2013;
				}
 				else if (!String.IsNullOrEmpty(WindowsPlatform.GetVSComnToolsPath(WindowsCompiler.VisualStudio2015)))
 				{
 					CachedCompiler = WindowsCompiler.VisualStudio2015;
 				}
				else
				{
					// Finally assume 2013 is installed to defer errors somewhere else like VCToolChain
					CachedCompiler = WindowsCompiler.VisualStudio2013;
				}

				return CachedCompiler.Value;
			}
		}

		/// <summary>
		/// When true, throws some CL and link flags (/Bt+ and /link) to output detailed timing info.
		/// </summary>
		[XmlConfig]
		public static bool bLogDetailedCompilerTimingInfo = false;

		/// True if we should use Clang/LLVM instead of MSVC to compile code on Windows platform
		public static readonly bool bCompileWithClang = false;

		/// When using Clang, enabling enables the MSVC-like "clang-cl" wrapper, otherwise we pass arguments to Clang directly
		public static readonly bool bUseVCCompilerArgs = !bCompileWithClang || false;

		/// True if we should use the Clang linker (LLD) when bCompileWithClang is enabled, otherwise we use the MSVC linker
		public static readonly bool bAllowClangLinker = bCompileWithClang && false;

		/// Whether to compile against the Windows 10 SDK, instead of the Windows 8.1 SDK.  This requires the Visual Studio 2015
		/// compiler or later, and the Windows 10 SDK must be installed.  The application will require at least Windows 8.x to run.
		// @todo UWP: Expose this to be enabled more easily for building Windows 10 desktop apps
		public static readonly bool bUseWindowsSDK10 = false;

		/// True if we allow using addresses larger than 2GB on 32 bit builds
		public static bool bBuildLargeAddressAwareBinary = true;

		/// <summary>
		/// True if VS EnvDTE is available (false when building using Visual Studio Express)
		/// </summary>
		public static bool bHasVisualStudioDTE
		{
			get
			{
				// @todo clang: DTE #import doesn't work with Clang compiler
				if (bCompileWithClang)
				{
					return false;
				}

				try
				{
					// Interrogate the Win32 registry
					string DTEKey = null;
					switch (Compiler)
					{
						case WindowsCompiler.VisualStudio2015:
							DTEKey = "VisualStudio.DTE.14.0";
							break;
						case WindowsCompiler.VisualStudio2013:
							DTEKey = "VisualStudio.DTE.12.0";
							break;
						case WindowsCompiler.VisualStudio2012:
							DTEKey = "VisualStudio.DTE.11.0";
							break;
					}
					return RegistryKey.OpenBaseKey(RegistryHive.ClassesRoot, RegistryView.Registry32).OpenSubKey(DTEKey) != null;
				}
				catch (Exception)
				{
					return false;
				}
			}
		}

		WindowsPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InPlatform">Creates a windows platform with the given enum value</param>
		public WindowsPlatform(UnrealTargetPlatform InPlatform, CPPTargetPlatform InDefaultCppPlatform, WindowsPlatformSDK InSDK) : base(InPlatform, InDefaultCppPlatform)
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
		/// Returns VisualStudio common tools path for current compiler.
		/// </summary>
		/// <returns>Common tools path.</returns>
		public static string GetVSComnToolsPath()
		{
			return GetVSComnToolsPath(Compiler);
		}

		/// <summary>
		/// Returns VisualStudio common tools path for given compiler.
		/// </summary>
		/// <param name="Compiler">Compiler for which to return tools path.</param>
		/// <returns>Common tools path.</returns>
		public static string GetVSComnToolsPath(WindowsCompiler Compiler)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win64 && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win32)
			{
				return null;
			}

			int VSVersion;

			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2012:
					VSVersion = 11;
					break;
				case WindowsCompiler.VisualStudio2013:
					VSVersion = 12;
					break;
				case WindowsCompiler.VisualStudio2015:
					VSVersion = 14;
					break;
				default:
					throw new NotSupportedException("Not supported compiler.");
			}

			string[] PossibleRegPaths = new string[] {
                @"Wow6432Node\Microsoft\VisualStudio",	// Non-express VS2013 on 64-bit machine.
                @"Microsoft\VisualStudio",				// Non-express VS2013 on 32-bit machine.
                @"Wow6432Node\Microsoft\WDExpress",		// Express VS2013 on 64-bit machine.
                @"Microsoft\WDExpress"					// Express VS2013 on 32-bit machine.
            };

			string VSPath = null;

			foreach (var PossibleRegPath in PossibleRegPaths)
			{
				VSPath = (string)Registry.GetValue(string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\{0}\{1}.0", PossibleRegPath, VSVersion), "InstallDir", null);

				if (VSPath != null)
				{
					break;
				}
			}

			if (VSPath == null)
			{
				return null;
			}

			return new DirectoryInfo(Path.Combine(VSPath, "..", "Tools")).FullName;
		}

		/// <summary>
		/// If this platform can be compiled with SN-DBS
		/// </summary>
		public override bool CanUseSNDBS()
		{
			// Check that SN-DBS is available
			string SCERootPath = Environment.GetEnvironmentVariable("SCE_ROOT_DIR");
			if (!String.IsNullOrEmpty(SCERootPath))
			{
				string SNDBSPath = Path.Combine(SCERootPath, "common", "sn-dbs", "bin", "dbsbuild.exe");
				bool bIsSNDBSAvailable = File.Exists(SNDBSPath);
				return bIsSNDBSAvailable;
			}
			else
			{
				return false;
			}
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
					return ".dll";
				case UEBuildBinaryType.Executable:
					return ".exe";
				case UEBuildBinaryType.StaticLibrary:
					return ".lib";
				case UEBuildBinaryType.Object:
					if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
					{
						return ".obj";
					}
					else
					{
						return @".includes";
					}
				case UEBuildBinaryType.PrecompiledHeader:
					return ".pch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}


		/// <summary>
		/// When using a Visual Studio compiler, returns the version name as a string
		/// </summary>
		/// <returns>The Visual Studio compiler version name (e.g. "2012")</returns>
		public static string GetVisualStudioCompilerVersionName()
		{
			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2012:
					return "2012";

				case WindowsCompiler.VisualStudio2013:
					return "2013";

				case WindowsCompiler.VisualStudio2015:
					return "2015";

				default:
					throw new BuildException("Unexpected WindowsCompiler version for GetVisualStudioCompilerVersionName().  Either not using a Visual Studio compiler or switch block needs to be updated");
			}
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
				case UEBuildBinaryType.Executable:
					return ".pdb";
			}
			return "";
		}

		/// <summary>
		/// Whether incremental linking should be used
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="InConfiguration"> The CPPTargetConfiguration being built</param>
		/// <returns>bool true if incremental linking should be used, false if not</returns>
		public override bool ShouldUseIncrementalLinking(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration)
		{
			return (Configuration == CPPTargetConfiguration.Debug);
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
			// Only supported on PC.
			if (bCreateDebugInfo && ShouldUseIncrementalLinking(Platform, Configuration))
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectPath)
		{
			if (Platform == UnrealTargetPlatform.Win32)
			{
				string[] StringKeys = new string[] {
					"MinimumOSVersion"
				};

				// look up OS specific settings
				if (!DoProjectSettingsMatchDefault(Platform, ProjectPath, "/Script/WindowsTargetPlatform.WindowsTargetSettings",
					null, null, StringKeys))
				{
					return false;
				}
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectPath);
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
		/// Return whether this platform has uniquely named binaries across multiple games
		/// </summary>
		public override bool HasUniqueBinaries()
		{
			// Windows applications have many shared binaries between games
			return false;
		}

		/// <summary>
		/// Creates a context for the given project on the current platform.
		/// </summary>
		/// <param name="ProjectFile">The project file for the current target</param>
		/// <returns>New platform context object</returns>
		public override UEBuildPlatformContext CreateContext(FileReference ProjectFile)
		{
			return new WindowsPlatformContext(Platform, ProjectFile);
		}
	}

	public class WindowsPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			return SDKStatus.Valid;
		}
	}

	class WindowsPlatformFactory : UEBuildPlatformFactory
	{
		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms()
		{
			WindowsPlatformSDK SDK = new WindowsPlatformSDK();
			SDK.ManageAndValidateSDK();

			// Register this build platform for both Win64 and Win32
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win64.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new WindowsPlatform(UnrealTargetPlatform.Win64, CPPTargetPlatform.Win64, SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Windows);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Microsoft);

			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win32.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new WindowsPlatform(UnrealTargetPlatform.Win32, CPPTargetPlatform.Win32, SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win32, UnrealPlatformGroup.Windows);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win32, UnrealPlatformGroup.Microsoft);
		}
	}
}
