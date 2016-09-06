// Copyright 2014-2016 Allegorithmic, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SubstanceEditor : ModuleRules
{
	public SubstanceEditor(TargetInfo Target)
	{
		PrivateIncludePaths.Add("SubstanceEditor/Private");

		PublicDependencyModuleNames.AddRange(new string[] {
				"AssetTools",
				"BlueprintGraph",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
				"EditorStyle",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"KismetCompiler",
				"LevelEditor",
				"MainFrame",
				"PropertyEditor",
				"RenderCore",
				"RHI",
				"ShaderCore",
				"SubstanceCore",
				"TextureEditor",
				"UnrealEd"
			});
			
		PrivateDependencyModuleNames.AddRange(new string[] {
                "AppFramework",
				"Slate",
                "SlateCore",
                "Projects",
        });
	}
}
