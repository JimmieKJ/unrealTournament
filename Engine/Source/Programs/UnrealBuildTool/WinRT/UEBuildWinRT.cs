// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
    class WinRTPlatform : UEBuildPlatform
    {
        /// <summary>
        /// Should the app be compiled as WinRT
        /// </summary>
        [XmlConfig]
        public static bool bCompileWinRT = false;

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

        /**
         *	Whether the required external SDKs are installed for this platform
         */
        protected override SDKStatus HasRequiredManualSDKInternal()
        {
            return !Utils.IsRunningOnMono && IsVisualStudioInstalled() ? SDKStatus.Valid : SDKStatus.Invalid;
        }

        /**
         *	Register the platform with the UEBuildPlatform class
         */
        protected override void RegisterBuildPlatformInternal()
        {
            //@todo.Rocket: Add platform support
            if (UnrealBuildTool.RunningRocket() || Utils.IsRunningOnMono)
            {
                return;
            }

            if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (IsVisualStudioInstalled() == true))
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
                    UEBuildPlatform.RegisterBuildPlatform(UnrealTargetPlatform.WinRT, this);
                    UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.WinRT, UnrealPlatformGroup.Microsoft);

                    // For now only register WinRT_ARM is truly a Windows 8 machine.
                    // This will prevent people who do all platform builds from running into the compiler issue.
                    if (WinRTPlatform.IsWindows8() == true)
                    {
                        Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.WinRT_ARM.ToString());
                        UEBuildPlatform.RegisterBuildPlatform(UnrealTargetPlatform.WinRT_ARM, this);
                        UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.WinRT_ARM, UnrealPlatformGroup.Microsoft);
                    }
                }
            }
        }

        /**
         *	Retrieve the CPPTargetPlatform for the given UnrealTargetPlatform
         *
         *	@param	InUnrealTargetPlatform		The UnrealTargetPlatform being build
         *	
         *	@return	CPPTargetPlatform			The CPPTargetPlatform to compile for
         */
        public override CPPTargetPlatform GetCPPTargetPlatform(UnrealTargetPlatform InUnrealTargetPlatform)
        {
            switch (InUnrealTargetPlatform)
            {
                case UnrealTargetPlatform.WinRT:
                    return CPPTargetPlatform.WinRT;
                case UnrealTargetPlatform.WinRT_ARM:
                    return CPPTargetPlatform.WinRT_ARM;
            }
            throw new BuildException("WinRTPlatform::GetCPPTargetPlatform: Invalid request for {0}", InUnrealTargetPlatform.ToString());
        }

        /**
         *	Get the extension to use for the given binary type
         *	
         *	@param	InBinaryType		The binary type being built
         *	
         *	@return	string				The binary extension (i.e. 'exe' or 'dll')
         */
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

        /**
         *	Get the extension to use for debug info for the given binary type
         *	
         *	@param	InBinaryType		The binary type being built
         *	
         *	@return	string				The debug info extension (i.e. 'pdb')
         */
        public override string GetDebugInfoExtension(UEBuildBinaryType InBinaryType)
        {
            return ".pdb";
        }

        /**
         *	Gives the platform a chance to 'override' the configuration settings 
         *	that are overridden on calls to RunUBT.
         *	
         *	@param	InPlatform			The UnrealTargetPlatform being built
         *	@param	InConfiguration		The UnrealTargetConfiguration being built
         *	
         *	@return	bool				true if debug info should be generated, false if not
         */
        public override void ResetBuildConfiguration(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
        {
            ValidateUEBuildConfiguration();
        }

        /**
         *	Validate the UEBuildConfiguration for this platform
         *	This is called BEFORE calling UEBuildConfiguration to allow setting 
         *	various fields used in that function such as CompileLeanAndMean...
         */
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

        /**
         * Whether this platform should build a monolithic binary
         */
        public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
        {
            return true;
        }

        /**
         *	Whether PDB files should be used
         *	
         *	@param	InPlatform			The CPPTargetPlatform being built
         *	@param	InConfiguration		The CPPTargetConfiguration being built
         *	@param	bInCreateDebugInfo	true if debug info is getting create, false if not
         *	
         *	@return	bool	true if PDB files should be used, false if not
         */
        public override bool ShouldUsePDBFiles(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration, bool bCreateDebugInfo)
        {
            return true;
        }

        /**
         *	Whether the editor should be built for this platform or not
         *	
         *	@param	InPlatform		The UnrealTargetPlatform being built
         *	@param	InConfiguration	The UnrealTargetConfiguration being built
         *	@return	bool			true if the editor should be built, false if not
         */
        public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
        {
            return true;
        }

        /**
         *	Whether the platform requires cooked data
         *	
         *	@param	InPlatform		The UnrealTargetPlatform being built
         *	@param	InConfiguration	The UnrealTargetConfiguration being built
         *	@return	bool			true if the platform requires cooked data, false if not
         */
        public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
        {
            return true;
        }

        /**
         *	Get a list of extra modules the platform requires.
         *	This is to allow undisclosed platforms to add modules they need without exposing information about the platfomr.
         *	
         *	@param	Target						The target being build
         *	@param	BuildTarget					The UEBuildTarget getting build
         *	@param	PlatformExtraModules		OUTPUT the list of extra modules the platform needs to add to the target
         */
        public override void GetExtraModules(TargetInfo Target, UEBuildTarget BuildTarget, ref List<string> PlatformExtraModules)
        {
            if (Target.Platform == UnrealTargetPlatform.WinRT)
            {
                PlatformExtraModules.Add("XAudio2");
            }
        }

        /**
         *	Modify the newly created module passed in for this platform.
         *	This is not required - but allows for hiding details of a
         *	particular platform.
         *	
         *	@param	InModule		The newly loaded module
         *	@param	Target			The target being build
         */
        public override void ModifyNewlyLoadedModule(UEBuildModule InModule, TargetInfo Target)
        {
            if ((Target.Platform == UnrealTargetPlatform.WinRT) || 
                (Target.Platform == UnrealTargetPlatform.WinRT_ARM))
            {
                if (InModule.ToString() == "Core")
                {
                    InModule.AddPublicIncludePath("Runtime/Core/Public/WinRT");
                    InModule.AddPublicDependencyModule("zlib");
                }
                else if (InModule.ToString() == "Engine")
                {
                    InModule.AddPrivateDependencyModule("zlib");
                    InModule.AddPrivateDependencyModule("UElibPNG");
                    InModule.AddPublicDependencyModule("UEOgg");
                    InModule.AddPublicDependencyModule("Vorbis");
                }
                else if (InModule.ToString() == "Launch")
                {
                }
                else if (InModule.ToString() == "D3D11RHI")
                {
                    InModule.AddPublicDefinition("D3D11_CUSTOM_VIEWPORT_CONSTRUCTOR=1");
                    // To enable platform specific D3D11 RHI Types
                    InModule.AddPrivateIncludePath("Runtime/Windows/D3D11RHI/Private/WinRT");
                    // Hack to enable AllowWindowsPlatformTypes.h/HideWindowsPlatformTypes.h
                    InModule.AddPublicIncludePath("Runtime/Core/Public/Windows");
                }
                else if (InModule.ToString() == "Sockets")
                {
                    // Hack to enable AllowWindowsPlatformTypes.h/HideWindowsPlatformTypes.h
                    InModule.AddPublicIncludePath("Runtime/Core/Public/Windows");
                }
                else if (InModule.ToString() == "PhysX")
                {
                    string PhysXDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/PhysX-3.3/";

                    InModule.AddPublicIncludePath("include/foundation/WinRT");
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(PhysXDir + "Lib/WinRT");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(PhysXDir + "Lib/WinRT/ARM");
                    }

                    if (Target.Configuration == UnrealTargetConfiguration.Debug)
                    {
                        InModule.AddPublicAdditionalLibrary("PhysX3DEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3ExtensionsDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3CookingDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3CommonDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3VehicleDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("PxTaskDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("PhysXVisualDebuggerSDKDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("PhysXProfileSDKDEBUG.lib");
                    }
                    else if (Target.Configuration == UnrealTargetConfiguration.Development)
                    {
                        InModule.AddPublicAdditionalLibrary("PhysX3PROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3ExtensionsPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3CookingPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3CommonPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3VehiclePROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("PxTaskPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("PhysXVisualDebuggerSDKPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("PhysXProfileSDKPROFILE.lib");
                    }
                    else // Test or Shipping
                    {
                        InModule.AddPublicAdditionalLibrary("PhysX3.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3Extensions.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3Cooking.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3Common.lib");
                        InModule.AddPublicAdditionalLibrary("PhysX3Vehicle.lib");
                        InModule.AddPublicAdditionalLibrary("PxTask.lib");
                        InModule.AddPublicAdditionalLibrary("PhysXVisualDebuggerSDK.lib");
                        InModule.AddPublicAdditionalLibrary("PhysXProfileSDK.lib");
                    }
                }
                else if (InModule.ToString() == "APEX")
                {
                    InModule.RemovePublicDefinition("APEX_STATICALLY_LINKED=0");
                    InModule.AddPublicDefinition("APEX_STATICALLY_LINKED=1");
                    
                    string APEXDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/APEX-1.3/";
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(APEXDir + "lib/WinRT");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(APEXDir + "lib/WinRT/ARM");
                    }

                    if (Target.Configuration == UnrealTargetConfiguration.Debug)
                    {
                        InModule.AddPublicAdditionalLibrary("ApexCommonDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("ApexFrameworkDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("ApexSharedDEBUG.lib");
                        InModule.AddPublicAdditionalLibrary("APEX_DestructibleDEBUG.lib");

                    }
                    else if (Target.Configuration == UnrealTargetConfiguration.Development)
                    {
                        InModule.AddPublicAdditionalLibrary("ApexCommonPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("ApexFrameworkPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("ApexSharedPROFILE.lib");
                        InModule.AddPublicAdditionalLibrary("APEX_DestructiblePROFILE.lib");
                    }
                    else // Test or Shipping
                    {
                        InModule.AddPublicAdditionalLibrary("ApexCommon.lib");
                        InModule.AddPublicAdditionalLibrary("ApexFramework.lib");
                        InModule.AddPublicAdditionalLibrary("ApexShared.lib");
                        InModule.AddPublicAdditionalLibrary("APEX_Destructible.lib");
                    }
                }
                else if (InModule.ToString() == "FreeType2")
                {
                    string FreeType2Path = UEBuildConfiguration.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/";
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(FreeType2Path + "Lib/WinRT/Win64");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(FreeType2Path + "Lib/WinRT/ARM");
                    }
                    InModule.AddPublicAdditionalLibrary("freetype2412MT.lib");
                }
                else if (InModule.ToString() == "UElibPNG")
                {
                    string libPNGPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2";
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(libPNGPath + "/lib/WinRT/Win64");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(libPNGPath + "/lib/WinRT/ARM");
                    }
                    InModule.AddPublicAdditionalLibrary("libpng125.lib");
                }
                else if (InModule.ToString() == "DX11")
                {
                    // Clear out all the Windows include paths and libraries...
                    // The WinRTSDK module handles proper paths and libs for WinRT.
                    // However, the D3D11RHI module will include the DX11 module.
                    InModule.ClearPublicIncludePaths();
                    InModule.ClearPublicLibraryPaths();
                    InModule.ClearPublicAdditionalLibraries();
                    InModule.RemovePublicDefinition("WITH_D3DX_LIBS=1");
                    InModule.AddPublicDefinition("D3D11_WITH_DWMAPI=0");
                    InModule.AddPublicDefinition("WITH_D3DX_LIBS=0");
                    InModule.AddPublicDefinition("WITH_DX_PERF=0");
                    InModule.RemovePublicAdditionalLibrary("X3DAudio.lib");
                    InModule.RemovePublicAdditionalLibrary("XAPOFX.lib");
                }
                else if (InModule.ToString() == "XInput")
                {
                    InModule.AddPublicAdditionalLibrary("XInput.lib");
                }
                else if (InModule.ToString() == "XAudio2")
                {
                    InModule.AddPublicDefinition("XAUDIO_SUPPORTS_XMA2WAVEFORMATEX=0");
                    InModule.AddPublicDefinition("XAUDIO_SUPPORTS_DEVICE_DETAILS=0");
                    InModule.AddPublicDefinition("XAUDIO2_SUPPORTS_MUSIC=0");
                    InModule.AddPublicDefinition("XAUDIO2_SUPPORTS_SENDLIST=0");
                    InModule.AddPublicAdditionalLibrary("XAudio2.lib");
                    // Hack to enable AllowWindowsPlatformTypes.h/HideWindowsPlatformTypes.h
                    InModule.AddPublicIncludePath("Runtime/Core/Public/Windows");
                }
                else if (InModule.ToString() == "UEOgg")
                {
                    string OggPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Ogg/libogg-1.2.2/";
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(OggPath + "WinRT/VS2012/WinRT/x64/Release");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(OggPath + "WinRT/VS2012/WinRT/ARM/Release");
                    }
                    InModule.AddPublicAdditionalLibrary("libogg_static.lib");
                }
                else if (InModule.ToString() == "Vorbis")
                {
                    string VorbisPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(VorbisPath + "WinRT/VS2012/WinRT/x64/Release");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(VorbisPath + "WinRT/VS2012/WinRT/ARM/Release");
                    }
                    InModule.AddPublicAdditionalLibrary("libvorbis_static.lib");
                }
                else if (InModule.ToString() == "VorbisFile")
                {
                    string VorbisPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(VorbisPath + "WinRT/VS2012/WinRT/x64/Release");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(VorbisPath + "WinRT/VS2012/WinRT/ARM/Release");
                    }
                    InModule.AddPublicAdditionalLibrary("libvorbisfile_static.lib");
                }
                else if (InModule.ToString() == "DX11Audio")
                {
                    InModule.RemovePublicAdditionalLibrary("X3DAudio.lib");
                    InModule.RemovePublicAdditionalLibrary("XAPOFX.lib");
                }
                else if (InModule.ToString() == "zlib")
                {
                    if (Target.Platform == UnrealTargetPlatform.WinRT)
                    {
                        InModule.AddPublicLibraryPath(UEBuildConfiguration.UEThirdPartySourceDirectory + "zlib/zlib-1.2.5/Lib/WinRT/Win64");
                    }
                    else
                    {
                        InModule.AddPublicLibraryPath(UEBuildConfiguration.UEThirdPartySourceDirectory + "zlib/zlib-1.2.5/Lib/WinRT/ARM");
                    }
                    InModule.AddPublicAdditionalLibrary("zlib125.lib");
                }
            }
            else if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
            {
//              bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;
// 				if (!UEBuildConfiguration.bBuildRequiresCookedData)
// 				{
// 					if (InModule.ToString() == "Engine")
// 					{
// 						if (UEBuildConfiguration.bBuildDeveloperTools)
// 						{
                // 							InModule.AddPlatformSpecificDynamicallyLoadedModule("WinRTTargetPlatform");
// 						}
// 					}
// 					else if (InModule.ToString() == "TargetPlatform")
// 					{
// 		                bBuildShaderFormats = true;
// 					}
// 				}

// 				// allow standalone tools to use targetplatform modules, without needing Engine
// 				if (UEBuildConfiguration.bForceBuildTargetPlatforms)
// 				{
                // 					InModule.AddPlatformSpecificDynamicallyLoadedModule("WinRTTargetPlatform");
// 				}

//              if (bBuildShaderFormats)
//              {
                //                  InModule.AddPlatformSpecificDynamicallyLoadedModule("ShaderFormatWinRT");
//              }
            }
        }

        /**
         *	Setup the target environment for building
         *	
         *	@param	InBuildTarget		The target being built
         */
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

        /**
         *	Whether this platform should create debug information or not
         *	
         *	@param	InPlatform			The UnrealTargetPlatform being built
         *	@param	InConfiguration		The UnrealTargetConfiguration being built
         *	
         *	@return	bool				true if debug info should be generated, false if not
         */
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

        /**
         *	Setup the binaries for this specific platform.
         *	
         *	@param	InBuildTarget		The target being built
         */
        public override void SetupBinaries(UEBuildTarget InBuildTarget)
        {
            InBuildTarget.ExtraModuleNames.Add("XInput");
        }

        //
        // WinRT specific functions
        //
        /**
         *	Should the app be compiled as WinRT
         */
        public static bool ShouldCompileWinRT()
        {
            return WinRTPlatform.bCompileWinRT;
        }
    }
}
