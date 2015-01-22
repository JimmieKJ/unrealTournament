// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CodeView : ModuleRules
	{
		public CodeView(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
                    "InputCore",
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"EditorStyle",
					"PropertyEditor",
					"DesktopPlatform",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DetailCustomizations",
					"SlateCore",
				}
			);
		}
	}
}