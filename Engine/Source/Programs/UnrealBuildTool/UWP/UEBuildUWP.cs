// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	public class UWPPlatformContext : UEBuildPlatformContext
	{
		/// True if we should only build against the app-local CRT and set the API-set flags
		public readonly bool bWinApiFamilyApp = true;


		public UWPPlatformContext(FileReference InProjectFile) : base(UnrealTargetPlatform.UWP, InProjectFile)
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
			if (ModuleName == "Core")
			{
				Rules.PrivateDependencyModuleNames.Add("UWPSDK");
			}
			else if (ModuleName == "Engine")
			{
				Rules.PrivateDependencyModuleNames.Add("zlib");
				Rules.PrivateDependencyModuleNames.Add("UElibPNG");
				Rules.PublicDependencyModuleNames.Add("UEOgg");
				Rules.PublicDependencyModuleNames.Add("Vorbis");
			}
			else if (ModuleName == "D3D11RHI")
			{
				Rules.Definitions.Add("D3D11_WITH_DWMAPI=0");
				Rules.Definitions.Add("WITH_DX_PERF=0");
			}
			else if (ModuleName == "DX11")
			{
				// Clear out all the Windows include paths and libraries...
				// The UWPSDK module handles proper paths and libs for UWP.
				// However, the D3D11RHI module will include the DX11 module.
				Rules.PublicIncludePaths.Clear();
				Rules.PublicLibraryPaths.Clear();
				Rules.PublicAdditionalLibraries.Clear();
				Rules.Definitions.Remove("WITH_D3DX_LIBS=1");
				Rules.Definitions.Add("WITH_D3DX_LIBS=0");
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
			else if (ModuleName == "XAudio2")
			{
				Rules.Definitions.Add("XAUDIO_SUPPORTS_XMA2WAVEFORMATEX=0");
				Rules.Definitions.Add("XAUDIO_SUPPORTS_DEVICE_DETAILS=0");
				Rules.Definitions.Add("XAUDIO2_SUPPORTS_MUSIC=0");
				Rules.Definitions.Add("XAUDIO2_SUPPORTS_SENDLIST=0");
				Rules.PublicAdditionalLibraries.Add("XAudio2.lib");
			}
			else if (ModuleName == "DX11Audio")
			{
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
		}

		public override void ResetBuildConfiguration(UnrealTargetConfiguration Configuration)
		{
			UEBuildConfiguration.bCompileICU = true;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_DESKTOP=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_64BITS=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("EXCEPTIONS_DISABLED=0");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_WIN32_WINNT=0x0A00");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINVER=0x0A00");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_UWP=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("UWP=1");	// @todo UWP: This used to be "WINUAP=1".  Need to verify that 'UWP' is the correct define that we want here.

			if (bWinApiFamilyApp)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINAPI_FAMILY=WINAPI_FAMILY_APP");
			}

			// @todo UWP: UE4 is non-compliant when it comes to use of %s and %S
			// Previously %s meant "the current character set" and %S meant "the other one".
			// Now %s means multibyte and %S means wide. %Ts means "natural width".
			// Reverting this behavior until the UE4 source catches up.
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_CRT_STDIO_LEGACY_WIDE_SPECIFIERS=1");

			// No D3DX on UWP!
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("NO_D3DX_LIBS=1");


			if (InBuildTarget.Rules != null)
			{
				// Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
				// runtime library as the engine.
				if (InBuildTarget.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCRT");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCPRT");
				}
				else
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCRTD");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCPRTD");
				}
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBC");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCMT");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPMT");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCP");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCD");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCMTD");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPMTD");
				InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPD");
			}

			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("mincore.lib");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dloadhelper.lib");

			// Disable Simplygon support if compiling against the NULL RHI.
			if (InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Contains("USE_NULL_RHI=1"))
			{
				UEBuildConfiguration.bCompileSimplygon = false;
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
				// Default to Development; can be overriden by individual modules.
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
			return new UWPToolChain();
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <returns>True if the platform requires a deployment handler, false otherwise</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new UWPDeploy();
		}
	}

	public class UWPPlatform : UEBuildPlatform
	{
		/// Property caching.
		private static WindowsCompiler? CachedCompiler;

		/// Version of the compiler toolchain to use for Universal Windows Platform apps (UWP)
		public static WindowsCompiler Compiler
		{
			get
			{
				// Cache the result because Compiler is often used.
				if (CachedCompiler.HasValue)
				{
					return CachedCompiler.Value;
				}

				// First, default based on whether there is a command line override.
				// Allows build chain to partially progress even in the absence of installed tools
				if (UnrealBuildTool.CommandLineContains("-2015"))
				{
					CachedCompiler = WindowsCompiler.VisualStudio2015;
				}
				// Second, default based on what's installed
				else if (!String.IsNullOrEmpty(WindowsPlatform.GetVSComnToolsPath(WindowsCompiler.VisualStudio2015)))
				{
					CachedCompiler = WindowsCompiler.VisualStudio2015;
				}
				else
				{
					CachedCompiler = null;
				}

				return CachedCompiler.Value;
			}
		}

		// Enables the UWP platform and project file support in Unreal Build Tool
		// @todo UWP: Remove this variable when UWP support is fully implemented
		public static readonly bool bEnableUWPSupport = UnrealBuildTool.CommandLineContains("-uwp");

		/// True if we should only build against the app-local CRT and /APPCONTAINER linker flag
		public static readonly bool bBuildForStore = true;

		/// <summary>
		/// True if VS EnvDTE is available (false when building using Visual Studio Express)
		/// </summary>
		public static bool bHasVisualStudioDTE
		{
			get
			{
				return WindowsPlatform.bHasVisualStudioDTE;
			}
		}

		UWPPlatformSDK SDK;

		public UWPPlatform(UWPPlatformSDK InSDK) : base(UnrealTargetPlatform.UWP, CPPTargetPlatform.UWP)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public override bool RequiresDeployPrepAfterCompile()
		{
			return true;
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
			int VSVersion;

			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2015:
					VSVersion = 14;
					break;
				default:
					throw new NotSupportedException("Not supported compiler.");
			}

			string[] PossibleRegPaths = new string[] {
				@"Wow6432Node\Microsoft\VisualStudio",	// Non-express VS on 64-bit machine.
				@"Microsoft\VisualStudio",				// Non-express VS on 32-bit machine.
				@"Wow6432Node\Microsoft\WDExpress",		// Express VS on 64-bit machine.
				@"Microsoft\WDExpress"					// Express VS on 32-bit machine.
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
					return ".obj";
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
			return true;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
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
			return new UWPPlatformContext(ProjectFile);
		}
	}

	public class UWPPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			try
			{
				// @todo UWP: wire this up once the rest of cleanup has been resolved. right now, just leaving this to always return valid, like VC does.
				//WinUWPToolChain.FindBaseVSToolPath();
				return SDKStatus.Valid;
			}
			catch (BuildException)
			{
				return SDKStatus.Invalid;
			}
		}
	}

	class UWPPlatformFactory : UEBuildPlatformFactory
	{
		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms()
		{
			UWPPlatformSDK SDK = new UWPPlatformSDK();
			SDK.ManageAndValidateSDK();

			// Register this build platform for UWP
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.UWP.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new UWPPlatform(SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP, UnrealPlatformGroup.Microsoft);
		}

	}
}
