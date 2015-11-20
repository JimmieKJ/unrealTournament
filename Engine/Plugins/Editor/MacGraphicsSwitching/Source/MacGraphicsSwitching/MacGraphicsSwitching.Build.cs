// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MacGraphicsSwitching : ModuleRules
	{
        public MacGraphicsSwitching(TargetInfo Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "EditorStyle",
					"Engine",
                    "InputCore",
					"LevelEditor",
					"Slate"
				}
			);

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",
					"SlateCore",
					"UnrealEd"
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] 
				{
					"MainFrame",
					"Settings"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] 
				{
					"MainFrame",
					"Settings"
				}
			);

			AddThirdPartyPrivateStaticDependencies(Target, "OpenGL");
		}
	}
}