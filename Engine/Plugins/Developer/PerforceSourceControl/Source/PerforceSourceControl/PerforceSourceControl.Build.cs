// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PerforceSourceControl : ModuleRules
{
	public PerforceSourceControl(TargetInfo Target)
	{
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
			}
		);

		AddThirdPartyPrivateStaticDependencies(Target, "Perforce");
		Definitions.Add("USE_P4_API=1");

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}
	}
}
