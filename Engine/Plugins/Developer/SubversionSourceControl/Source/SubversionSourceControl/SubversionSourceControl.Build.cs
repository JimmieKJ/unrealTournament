// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SubversionSourceControl : ModuleRules
{
	public SubversionSourceControl(TargetInfo Target)
	{
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
				"XmlParser",
		    }
		);
	}
}
