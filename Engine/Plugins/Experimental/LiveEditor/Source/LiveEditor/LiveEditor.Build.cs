// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveEditor : ModuleRules
	{
        public LiveEditor(TargetInfo Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"LiveEditor/Private",
					// ... add other private include paths required here ...
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
                    "Slate",
                    "EditorStyle",
					"Engine",
					"UnrealEd",
                    "Sockets",
                    "Networking",
                    "BlueprintGraph",
                    "InputCore",
                    "Kismet",
                    "KismetCompiler",
                    "GraphEditor",
                    "WorkspaceMenuStructure",//@TODO: This makes it depend on a module in editor, even if that module does not depend on other editor stuff!
                    "LiveEditorListenServer"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SlateCore",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					// ... add any modules that your module loads dynamically here ...
				}
			);
			
			AddThirdPartyPrivateStaticDependencies(Target, "portmidi");
		}
	}
}