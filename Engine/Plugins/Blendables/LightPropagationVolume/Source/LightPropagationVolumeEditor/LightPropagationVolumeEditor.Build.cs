// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LightPropagationVolumeEditor : ModuleRules
{
    public LightPropagationVolumeEditor(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {	
				"Core",
				"CoreUObject",
                "UnrealEd",         // for FAssetEditorManager
				"AssetTools",       // class FAssetTypeActions_Base
				"LightPropagationVolumeRuntime",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"UnrealEd",
			}
        );
	}
}
