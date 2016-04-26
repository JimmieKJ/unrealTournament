// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SignificanceManager : ModuleRules
{
	public SignificanceManager(TargetInfo Target)
	{
		PublicIncludePaths.Add("SignificanceManager/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
		        "Engine",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "SignificanceManager/Private"
            })
		;
	}
}
