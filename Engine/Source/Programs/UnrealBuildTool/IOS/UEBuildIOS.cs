// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;

namespace UnrealBuildTool
{
    class IOSPlatform : UEBuildPlatform
    {
        // by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
        [XmlConfig]
        public static string IOSArchitecture = "";

        // The current architecture - affects everything about how UBT operates on IOS
        public override string GetActiveArchitecture()
        {
            return IOSArchitecture;
        }

        protected override SDKStatus HasRequiredManualSDKInternal()
        {
			if (!Utils.IsRunningOnMono)
			{
				// check to see if iTunes is installed
				string dllPath = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "iTunesMobileDeviceDLL", null) as string;
				if (String.IsNullOrEmpty(dllPath) || !File.Exists(dllPath))
				{
					return SDKStatus.Invalid;
				}
			}
			return SDKStatus.Valid;
        }

        /**
         *	Register the platform with the UEBuildPlatform class
         */
        protected override void RegisterBuildPlatformInternal()
        {
            // Register this build platform for IOS
            Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.IOS.ToString());
            UEBuildPlatform.RegisterBuildPlatform(UnrealTargetPlatform.IOS, this);
            UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Unix);
            UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Apple);

            if (GetActiveArchitecture() == "-simulator")
            {
                UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Simulator);
            }
            else
            {
                UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Device);
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
                case UnrealTargetPlatform.IOS:
                    return CPPTargetPlatform.IOS;
            }
            throw new BuildException("IOSPlatform::GetCPPTargetPlatform: Invalid request for {0}", InUnrealTargetPlatform.ToString());
        }

        /**
         *	Get the extension to use for the given binary type
         *	
         *	@param	InBinaryType		The binary type being built
         *	
         *	@return	string				The binary extenstion (ie 'exe' or 'dll')
         */
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
            return BuildConfiguration.bGeneratedSYMFile ? ".dsym" : "";
        }

        public override bool CanUseXGE()
        {
            return false;
        }

		public override bool CanUseDistcc()
		{
			return true;
		}

        /**
         *	Setup the target environment for building
         *	
         *	@param	InBuildTarget		The target being built
         */
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

            InBuildTarget.GlobalLinkEnvironment.Config.AdditionalFrameworks.Add( new UEBuildFramework( "GameKit" ) );
            InBuildTarget.GlobalLinkEnvironment.Config.AdditionalFrameworks.Add( new UEBuildFramework( "StoreKit" ) );
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

        public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
        {
            return true; // for iOS can only run cooked. this is mostly for testing console code paths.
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
            return true;
        }

        public override void ResetBuildConfiguration(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
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

        public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
        {
            // This platform currently always compiles monolithic
            return true;
        }

        public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
        {
            BuildConfiguration.bUsePCHFiles = false;
            BuildConfiguration.bUseSharedPCHs = false;
            BuildConfiguration.bCheckExternalHeadersForModification = false;
            BuildConfiguration.bCheckSystemHeadersForModification = false;
            BuildConfiguration.ProcessorCountMultiplier = IOSToolChain.GetAdjustedProcessorCountMultiplier();
            BuildConfiguration.bDeployAfterCompile = true;
        }

        /**
         *	Whether the platform requires the extra UnityCPPWriter
         *	This is used to add an extra file for UBT to get the #include dependencies from
         *	
         *	@return	bool	true if it is required, false if not
         */
        public override bool RequiresExtraUnityCPPWriter()
        {
            return true;
        }

        /**
         *     Modify the newly created module passed in for this platform.
         *     This is not required - but allows for hiding details of a
         *     particular platform.
         *     
         *     @param InModule             The newly loaded module
         *     @param GameName             The game being build
         *     @param Target               The target being build
         */
        public override void ModifyNewlyLoadedModule(UEBuildModule InModule, TargetInfo Target)
        {
            if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac))
            {
                bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;
                if (!UEBuildConfiguration.bBuildRequiresCookedData)
                {
                    if (InModule.ToString() == "Engine")
                    {
                        if (UEBuildConfiguration.bBuildDeveloperTools)
                        {
                            InModule.AddPlatformSpecificDynamicallyLoadedModule("IOSTargetPlatform");
                        }
                    }
                    else if (InModule.ToString() == "TargetPlatform")
                    {
                        bBuildShaderFormats = true;
						InModule.AddDynamicallyLoadedModule("TextureFormatPVR");
						InModule.AddDynamicallyLoadedModule("TextureFormatASTC");
                        if (UEBuildConfiguration.bBuildDeveloperTools)
                        {
                            InModule.AddPlatformSpecificDynamicallyLoadedModule("AudioFormatADPCM");
                        }
                    }
                }

                // allow standalone tools to use targetplatform modules, without needing Engine
                if (UEBuildConfiguration.bForceBuildTargetPlatforms)
                {
                    InModule.AddPlatformSpecificDynamicallyLoadedModule("IOSTargetPlatform");
                }

                if (bBuildShaderFormats)
                {
                    InModule.AddPlatformSpecificDynamicallyLoadedModule("MetalShaderFormat");
                }
            }
        }
    }
}

