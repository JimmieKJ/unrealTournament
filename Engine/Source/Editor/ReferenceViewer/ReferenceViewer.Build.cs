// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ReferenceViewer : ModuleRules
	{
        public ReferenceViewer(TargetInfo Target)
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
				    "RenderCore",
                    "GraphEditor",
				    "UnrealEd",
			    }
			);

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
				    "AssetRegistry",
					"EditorWidgets",
					"CollectionManager",
					"SizeMap",
			    }
            );

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
				    "AssetRegistry",
					"EditorWidgets",
					"SizeMap",
			    }
            );
		}
	}
}