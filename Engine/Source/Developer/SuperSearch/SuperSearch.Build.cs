// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SuperSearch : ModuleRules
{
    public SuperSearch(TargetInfo Target)
    {
        if (UEBuildConfiguration.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"Core",
					"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
					"Engine",
	                "InputCore",
					"Json",
					"UnrealEd",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"HTTP",
					"IntroTutorials"
				}
            );
        }
        else
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"Core",
					"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
                    "InputCore",
					"Slate",
					"SlateCore",
	                "HTTP",
                    "Json",
			    }
            );
        }
    }
}
