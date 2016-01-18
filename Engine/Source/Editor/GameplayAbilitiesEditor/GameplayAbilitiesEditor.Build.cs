// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayAbilitiesEditor : ModuleRules
	{
        public GameplayAbilitiesEditor(TargetInfo Target)
		{
            PrivateIncludePaths.AddRange(
                new string[] {
                    "Editor/GraphEditor/Private",
				    "Editor/Kismet/Private",
					"Editor/GameplayAbilitiesEditor/Private",
                    "Developer/AssetTools/Private",
				}
			);

            PublicDependencyModuleNames.Add("GameplayTasks");

            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"ClassViewer",
                    "GameplayTags",
					"GameplayAbilities",
                    "GameplayTasksEditor",
                    "InputCore",
                    "PropertyEditor",
					"Slate",
					"SlateCore",
                    "EditorStyle",
					"BlueprintGraph",
                    "Kismet",
					"KismetCompiler",
					"GraphEditor",
					"MainFrame",
					"UnrealEd",
                    "WorkspaceMenuStructure",
                    "ContentBrowser",
                    "EditorWidgets",
                    "SourceControl",
				}
			);
		}
	}
}
