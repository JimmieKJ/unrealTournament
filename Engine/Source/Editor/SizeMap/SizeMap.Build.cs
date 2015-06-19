// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SizeMap : ModuleRules
	{
        public SizeMap(TargetInfo Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
				    "CoreUObject",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    "EditorStyle",
				    "UnrealEd",
					"TreeMap"
			    }
			);

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
				    "AssetRegistry",
			    }
            );

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
				    "AssetRegistry",
			    }
            );
		}
	}
}