// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImageWrapper : ModuleRules
{
	public ImageWrapper(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Developer/ImageWrapper/Private");

		Definitions.Add("WITH_UNREALPNG=1");
        Definitions.Add("WITH_UNREALJPEG=1");

		PrivateDependencyModuleNames.Add("Core");

		AddThirdPartyPrivateStaticDependencies(Target, 
			"zlib",
			"UElibPNG",
			"UElibJPG"
			);

        // Add openEXR lib for windows builds.
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
        {
            Definitions.Add("WITH_UNREALEXR=1");
            AddThirdPartyPrivateStaticDependencies(Target, "UEOpenEXR");
        }

        bEnableShadowVariableWarnings = false;
    }
}
