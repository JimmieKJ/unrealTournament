// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TargetPlatform : ModuleRules
{
	public TargetPlatform(TargetInfo Target)
	{
		PrivateDependencyModuleNames.Add("Core");		
        PublicDependencyModuleNames.Add("DesktopPlatform");

        PrivateIncludePathModuleNames.Add("PhysXFormats");

		// no need for all these modules if the program doesn't want developer tools at all (like UnrealFileServer)
		if (!UEBuildConfiguration.bBuildRequiresCookedData && UEBuildConfiguration.bBuildDeveloperTools)
		{
            // these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
            DynamicallyLoadedModuleNames.Add("ShaderPreprocessor");
            DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
            DynamicallyLoadedModuleNames.Add("ImageWrapper");

			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
			{
				if (UEBuildConfiguration.bCompileLeanAndMeanUE == false)
				{
					DynamicallyLoadedModuleNames.Add("TextureFormatIntelISPCTexComp");
				}
			}

            if (Target.Platform == UnrealTargetPlatform.Win32 ||
                Target.Platform == UnrealTargetPlatform.Win64 ||
				(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
			{

                // these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
                DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
				DynamicallyLoadedModuleNames.Add("MetalShaderFormat");

                if (UEBuildConfiguration.bCompileLeanAndMeanUE == false)
				{
                    DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
                    DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
					DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
				}

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (UEBuildConfiguration.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM"); // For IOS cooking
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus"); 
				}

				if (Target.Type == TargetRules.TargetType.Editor || Target.Type == TargetRules.TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ASTCTargetPlatform");
                    DynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
                    DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
                    DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacNoEditorTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacServerTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacClientTargetPlatform");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				if (UEBuildConfiguration.bCompileLeanAndMeanUE == false)
				{
					DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
                    DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
					DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
				}

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (UEBuildConfiguration.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
				}

				if (Target.Type == TargetRules.TargetType.Editor || Target.Type == TargetRules.TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
					DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
				}
			}
            else if (Target.Platform == UnrealTargetPlatform.Linux)
            {
                if (UEBuildConfiguration.bCompileLeanAndMeanUE == false)
                {
                    DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
                    DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
					DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
                }

                DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

                if (UEBuildConfiguration.bCompileAgainstEngine)
                {
                    DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
                    DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
                }
            }
		}
        
        if (UEBuildConfiguration.bBuildDeveloperTools == true && (UEBuildConfiguration.bBuildRequiresCookedData || UEBuildConfiguration.bRuntimePhysicsCooking) && UEBuildConfiguration.bCompileAgainstEngine && UEBuildConfiguration.bCompilePhysX)
        {
            DynamicallyLoadedModuleNames.Add("PhysXFormats");
        }
	}
}
