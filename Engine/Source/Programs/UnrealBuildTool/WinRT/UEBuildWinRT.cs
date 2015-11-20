// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	class WinRTPlatformContext : UEBuildPlatformContext
	{
		public WinRTPlatformContext(UnrealTargetPlatform InPlatform, FileReference InProjectFile) : base(InPlatform, InProjectFile)
		{
		}

		/// <summary>
		/// Get a list of extra modules the platform requires.
		/// This is to allow undisclosed platforms to add modules they need without exposing information about the platfomr.
		/// </summary>
		/// <param name="Target">The target being build</param>
		/// <param name="PlatformExtraModules">List of extra modules the platform needs to add to the target</param>
		public override void AddExtraModules(TargetInfo Target, List<string> ExtraModuleNames)
		{
			ExtraModuleNames.Add("XInput");
			if (Target.Platform == UnrealTargetPlatform.WinRT)
			{
				ExtraModuleNames.Add("XAudio2");
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
			if (ModuleName == "Core")
			{
				Rules.PublicIncludePaths.Add("Runtime/Core/Public/WinRT");
				Rules.PublicDependencyModuleNames.Add("zlib");
			}
			else if (ModuleName == "Engine")
			{
				Rules.PrivateDependencyModuleNames.Add("zlib");
				Rules.PrivateDependencyModuleNames.Add("UElibPNG");
				Rules.PublicDependencyModuleNames.Add("UEOgg");
				Rules.PublicDependencyModuleNames.Add("Vorbis");
			}
			else if (ModuleName == "Launch")
			{
			}
			else if (ModuleName == "D3D11RHI")
			{
				Rules.Definitions.Add("D3D11_CUSTOM_VIEWPORT_CONSTRUCTOR=1");
				// To enable platform specific D3D11 RHI Types
				Rules.PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private/WinRT");
				// Hack to enable AllowWindowsPlatformTypes.h/HideWindowsPlatformTypes.h
				Rules.PublicIncludePaths.Add("Runtime/Core/Public/Windows");
			}
			else if (ModuleName == "Sockets")
			{
				// Hack to enable AllowWindowsPlatformTypes.h/HideWindowsPlatformTypes.h
				Rules.PublicIncludePaths.Add("Runtime/Core/Public/Windows");
			}
			else if (ModuleName == "PhysX")
			{
				string PhysXDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/PhysX-3.3/";

				Rules.PublicIncludePaths.Add("include/foundation/WinRT");
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(PhysXDir + "Lib/WinRT");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(PhysXDir + "Lib/WinRT/ARM");
				}

				if (Target.Configuration == UnrealTargetConfiguration.Debug)
				{
					Rules.PublicAdditionalLibraries.Add("PhysX3DEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3ExtensionsDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3CookingDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3CommonDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3VehicleDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("PxTaskDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("PhysXVisualDebuggerSDKDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("PhysXProfileSDKDEBUG.lib");
				}
				else if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					Rules.PublicAdditionalLibraries.Add("PhysX3PROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3ExtensionsPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3CookingPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3CommonPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3VehiclePROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("PxTaskPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("PhysXVisualDebuggerSDKPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("PhysXProfileSDKPROFILE.lib");
				}
				else // Test or Shipping
				{
					Rules.PublicAdditionalLibraries.Add("PhysX3.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3Extensions.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3Cooking.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3Common.lib");
					Rules.PublicAdditionalLibraries.Add("PhysX3Vehicle.lib");
					Rules.PublicAdditionalLibraries.Add("PxTask.lib");
					Rules.PublicAdditionalLibraries.Add("PhysXVisualDebuggerSDK.lib");
					Rules.PublicAdditionalLibraries.Add("PhysXProfileSDK.lib");
				}
			}
			else if (ModuleName == "APEX")
			{
				Rules.Definitions.Remove("APEX_STATICALLY_LINKED=0");
				Rules.Definitions.Add("APEX_STATICALLY_LINKED=1");

				string APEXDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/APEX-1.3/";
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(APEXDir + "lib/WinRT");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(APEXDir + "lib/WinRT/ARM");
				}

				if (Target.Configuration == UnrealTargetConfiguration.Debug)
				{
					Rules.PublicAdditionalLibraries.Add("ApexCommonDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("ApexFrameworkDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("ApexSharedDEBUG.lib");
					Rules.PublicAdditionalLibraries.Add("APEX_DestructibleDEBUG.lib");

				}
				else if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					Rules.PublicAdditionalLibraries.Add("ApexCommonPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("ApexFrameworkPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("ApexSharedPROFILE.lib");
					Rules.PublicAdditionalLibraries.Add("APEX_DestructiblePROFILE.lib");
				}
				else // Test or Shipping
				{
					Rules.PublicAdditionalLibraries.Add("ApexCommon.lib");
					Rules.PublicAdditionalLibraries.Add("ApexFramework.lib");
					Rules.PublicAdditionalLibraries.Add("ApexShared.lib");
					Rules.PublicAdditionalLibraries.Add("APEX_Destructible.lib");
				}
			}
			else if (ModuleName == "FreeType2")
			{
				string FreeType2Path = UEBuildConfiguration.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/";
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(FreeType2Path + "Lib/WinRT/Win64");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(FreeType2Path + "Lib/WinRT/ARM");
				}
				Rules.PublicAdditionalLibraries.Add("freetype2412MT.lib");
			}
			else if (ModuleName == "UElibPNG")
			{
				string libPNGPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2";
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(libPNGPath + "/lib/WinRT/Win64");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(libPNGPath + "/lib/WinRT/ARM");
				}
				Rules.PublicAdditionalLibraries.Add("libpng125.lib");
			}
			else if (ModuleName == "DX11")
			{
				// Clear out all the Windows include paths and libraries...
				// The WinRTSDK module handles proper paths and libs for WinRT.
				// However, the D3D11RHI module will include the DX11 module.
				Rules.PublicIncludePaths.Clear();
				Rules.PublicLibraryPaths.Clear();
				Rules.PublicAdditionalLibraries.Clear();
				Rules.Definitions.Remove("WITH_D3DX_LIBS=1");
				Rules.Definitions.Add("D3D11_WITH_DWMAPI=0");
				Rules.Definitions.Add("WITH_D3DX_LIBS=0");
				Rules.Definitions.Add("WITH_DX_PERF=0");
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
			else if (ModuleName == "XInput")
			{
				Rules.PublicAdditionalLibraries.Add("XInput.lib");
			}
			else if (ModuleName == "XAudio2")
			{
				Rules.Definitions.Add("XAUDIO_SUPPORTS_XMA2WAVEFORMATEX=0");
				Rules.Definitions.Add("XAUDIO_SUPPORTS_DEVICE_DETAILS=0");
				Rules.Definitions.Add("XAUDIO2_SUPPORTS_MUSIC=0");
				Rules.Definitions.Add("XAUDIO2_SUPPORTS_SENDLIST=0");
				Rules.PublicAdditionalLibraries.Add("XAudio2.lib");
				// Hack to enable AllowWindowsPlatformTypes.h/HideWindowsPlatformTypes.h
				Rules.PublicIncludePaths.Add("Runtime/Core/Public/Windows");
			}
			else if (ModuleName == "UEOgg")
			{
				string OggPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Ogg/libogg-1.2.2/";
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(OggPath + "WinRT/VS2012/WinRT/x64/Release");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(OggPath + "WinRT/VS2012/WinRT/ARM/Release");
				}
				Rules.PublicAdditionalLibraries.Add("libogg_static.lib");
			}
			else if (ModuleName == "Vorbis")
			{
				string VorbisPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(VorbisPath + "WinRT/VS2012/WinRT/x64/Release");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(VorbisPath + "WinRT/VS2012/WinRT/ARM/Release");
				}
				Rules.PublicAdditionalLibraries.Add("libvorbis_static.lib");
			}
			else if (ModuleName == "VorbisFile")
			{
				string VorbisPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(VorbisPath + "WinRT/VS2012/WinRT/x64/Release");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(VorbisPath + "WinRT/VS2012/WinRT/ARM/Release");
				}
				Rules.PublicAdditionalLibraries.Add("libvorbisfile_static.lib");
			}
			else if (ModuleName == "DX11Audio")
			{
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
			else if (ModuleName == "zlib")
			{
				if (Target.Platform == UnrealTargetPlatform.WinRT)
				{
					Rules.PublicLibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "zlib/zlib-1.2.5/Lib/WinRT/Win64");
				}
				else
				{
					Rules.PublicLibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "zlib/zlib-1.2.5/Lib/WinRT/ARM");
				}
				Rules.PublicAdditionalLibraries.Add("zlib125.lib");
			}
		}

		/// <summary>
		/// Gives the platform a chance to 'override' the configuration settings
		/// that are overridden on calls to RunUBT.
		/// </summary>
		/// <param name="Configuration">The UnrealTargetConfiguration being built</param>
		/// <returns>true if debug info should be generated, false if not</returns>
		public override void ResetBuildConfiguration(UnrealTargetConfiguration InConfiguration)
		{
			ValidateUEBuildConfiguration();
		}

		/// <summary>
		/// Validate the UEBuildConfiguration for this platform
		/// This is called BEFORE calling UEBuildConfiguration to allow setting
		/// various fields used in that function such as CompileLeanAndMean...
		/// </summary>
		public override void ValidateUEBuildConfiguration()
		{
			UEBuildConfiguration.bCompileLeanAndMeanUE = true;
			UEBuildConfiguration.bCompilePhysX = false;
			UEBuildConfiguration.bCompileAPEX = false;
			UEBuildConfiguration.bRuntimePhysicsCooking = false;

			BuildConfiguration.bUseUnityBuild = false;

			// Don't stop compilation at first error...
			BuildConfiguration.bStopXGECompilationAfterErrors = false;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_DESKTOP=0");
			if (InBuildTarget.Platform == UnrealTargetPlatform.WinRT)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_64BITS=1");
			}
			else
			{
				//WinRT_ARM
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_64BITS=0");
			}
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("EXCEPTIONS_DISABLED=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_OGGVORBIS=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_CAN_SUPPORT_EDITORONLY_DATA=0");

			//@todo.WINRT: REMOVE THIS
			// Temporary disabling of automation... to get things working
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_AUTOMATION_WORKER=0");

			//@todo.WinRT: Add support for version enforcement
			//InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("MIN_EXPECTED_XDK_VER=1070");
			//InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("MAX_EXPECTED_XDK_VER=1070");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("UNICODE");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_UNICODE");
			//InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WIN32_LEAN_AND_MEAN");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINAPI_FAMILY=WINAPI_FAMILY_APP");

			if (InBuildTarget.Platform == UnrealTargetPlatform.WinRT)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINRT=1");
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_WINRT=1");
			}
			else
			{
				//WinRT_ARM
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINRT=1");
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_WINRT_ARM=1");
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_WINRT=1");
				// 				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WinRT_ARM=1");
			}

			// No D3DX on WinRT!
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("NO_D3DX_LIBS=1");

			// Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
			// runtime library as the engine.
			if (InBuildTarget.Configuration == UnrealTargetConfiguration.Debug)
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

			// Compile and link with Win32 API libraries.
			// 			d2d1.lib
			// 			d3d11.lib
			// 			dxgi.lib
			// 			ole32.lib
			// 			windowscodecs.lib
			// 			dwrite.lib
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("runtimeobject.lib");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("mincore.lib");
			//InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("mincore_legacy.lib");
			//InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("mincore_obsolete.lib");
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("user32.lib");

			// Windows Vista/7 Desktop Windows Manager API for Slate Windows Compliance
			//InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dwmapi.lib");

			UEBuildConfiguration.bCompileSimplygon = false;

			// For 64-bit builds, we'll forcibly ignore a linker warning with DirectInput.  This is
			// Microsoft's recommended solution as they don't have a fixed .lib for us.
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalArguments += " /ignore:4078";

			// WinRT assemblies
			//InBuildTarget.GlobalCompileEnvironment.Config.SystemDotNetAssemblyPaths.Add("$(DurangoXDK)\\console\\Metadata");
			//InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("Windows.Foundation.winmd");
			//InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("Windows.winmd");
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
			return new WinRTToolChain(Platform);
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <returns>True if the platform requires a deployment handler, false otherwise</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new WinRTDeploy();
		}
	}

	class WinRTPlatform : UEBuildPlatform
	{
		/// <summary>
		/// Should the app be compiled as WinRT
		/// </summary>
		[XmlConfig]
		public static bool bCompileWinRT = false;

		WinRTPlatformSDK SDK;

		public WinRTPlatform(UnrealTargetPlatform InPlatform, CPPTargetPlatform InDefaultCPPPlatform, WinRTPlatformSDK InSDK) : base(InPlatform, InDefaultCPPPlatform)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public static bool IsVisualStudioInstalled()
		{
			string BaseVSToolPath = WindowsPlatform.GetVSComnToolsPath();
			if (string.IsNullOrEmpty(BaseVSToolPath) == false)
			{
				return true;
			}
			return false;
		}

		public static bool IsWindows8()
		{
			// Are we a Windows8 machine?
			if ((Environment.OSVersion.Version.Major == 6) && (Environment.OSVersion.Version.Minor == 2))
			{
				return true;
			}
			return false;
		}

		public override bool CanUseXGE()
		{
			return false;
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
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public override string GetDebugInfoExtension(UEBuildBinaryType InBinaryType)
		{
			return ".pdb";
		}

		/// <summary>
		/// Whether this platform should build a monolithic binary
		/// </summary>
		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			return true;
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
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		/// <summary>
		/// Whether the platform requires cooked data
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the platform requires cooked data, false if not</returns>
		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
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
				//              bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;
				// 				if (!UEBuildConfiguration.bBuildRequiresCookedData)
				// 				{
				// 					if (ModuleName == "Engine")
				// 					{
				// 						if (UEBuildConfiguration.bBuildDeveloperTools)
				// 						{
				// 							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("WinRTTargetPlatform");
				// 						}
				// 					}
				// 					else if (ModuleName == "TargetPlatform")
				// 					{
				// 		                bBuildShaderFormats = true;
				// 					}
				// 				}

				// 				// allow standalone tools to use targetplatform modules, without needing Engine
				// 				if (UEBuildConfiguration.bForceBuildTargetPlatforms)
				// 				{
				// 					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("WinRTTargetPlatform");
				// 				}

				//              if (bBuildShaderFormats)
				//              {
				//                  Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("ShaderFormatWinRT");
				//              }
			}
		}

		//
		// WinRT specific functions
		//
		/// <summary>
		/// Should the app be compiled as WinRT
		/// </summary>
		public static bool ShouldCompileWinRT()
		{
			return WinRTPlatform.bCompileWinRT;
		}

		/// <summary>
		/// Creates a context for the given project on the current platform.
		/// </summary>
		/// <param name="ProjectFile">The project file for the current target</param>
		/// <returns>New platform context object</returns>
		public override UEBuildPlatformContext CreateContext(FileReference ProjectFile)
		{
			return new WinRTPlatformContext(Platform, ProjectFile);
		}
	}

	public class WinRTPlatformSDK : UEBuildPlatformSDK
	{
		/// <summary>
		/// Whether the required external SDKs are installed for this platform
		/// </summary>
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			return !Utils.IsRunningOnMono && WinRTPlatform.IsVisualStudioInstalled() ? SDKStatus.Valid : SDKStatus.Invalid;
		}
	}

	public class WinRTPlatformFactory : UEBuildPlatformFactory
	{
		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms()
		{
			//@todo.Rocket: Add platform support
			if (UnrealBuildTool.RunningRocket() || Utils.IsRunningOnMono)
			{
				return;
			}

			WinRTPlatformSDK SDK = new WinRTPlatformSDK();
			SDK.ManageAndValidateSDK();

			if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (WinRTPlatform.IsVisualStudioInstalled() == true))
			{
				bool bRegisterBuildPlatform = true;

				// We also need to check for the generated projects... to handle the case where someone generates projects w/out WinRT.
				// Hardcoding this for now - but ideally it would be dynamically discovered.
				string EngineSourcePath = Path.Combine(ProjectFileGenerator.EngineRelativePath, "Source");
				string WinRTRHIFile = Path.Combine(EngineSourcePath, "Runtime", "Windows", "D3D11RHI", "D3D11RHI.build.cs");
				if (File.Exists(WinRTRHIFile) == false)
				{
					bRegisterBuildPlatform = false;
				}

				if (bRegisterBuildPlatform == true)
				{
					// Register this build platform for WinRT
					Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.WinRT.ToString());
					UEBuildPlatform.RegisterBuildPlatform(new WinRTPlatform(UnrealTargetPlatform.WinRT, CPPTargetPlatform.WinRT, SDK));
					UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.WinRT, UnrealPlatformGroup.Microsoft);

					// For now only register WinRT_ARM is truly a Windows 8 machine.
					// This will prevent people who do all platform builds from running into the compiler issue.
					if (WinRTPlatform.IsWindows8() == true)
					{
						Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.WinRT_ARM.ToString());
						UEBuildPlatform.RegisterBuildPlatform(new WinRTPlatform(UnrealTargetPlatform.WinRT_ARM, CPPTargetPlatform.WinRT_ARM, SDK));
						UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.WinRT_ARM, UnrealPlatformGroup.Microsoft);
					}
				}
			}
		}
	}
}
