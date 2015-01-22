// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SDL2 : ModuleRules
{
	public SDL2(TargetInfo Target)
	{
		Type = ModuleType.External;

		string SDL2Path = UEBuildConfiguration.UEThirdPartySourceDirectory + "SDL2/SDL-gui-backend/";
		string SDL2LibPath = SDL2Path + "lib/";

		// assume SDL to be built with extensions
		Definitions.Add("SDL_WITH_EPIC_EXTENSIONS=1");

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            PublicIncludePaths.Add(SDL2Path + "include");
            if (Target.IsMonolithic)
            {
                PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2_fPIC.a");
            }
		}
        else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
            PublicIncludePaths.Add(SDL2Path + "include");
            SDL2LibPath += "Win32/";
            PublicAdditionalLibraries.Add(SDL2LibPath + "SDL2.lib");
            PublicDelayLoadDLLs.Add("SDL2.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture != "-win32")
        {
            PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "SDL2/HTML5/SDL2-master/include/");
            SDL2LibPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "SDL2/HTML5/SDL2-master/libs/";
            PublicAdditionalLibraries.Add(SDL2LibPath + "/libSDL2.a");
        }
     
	}
}
