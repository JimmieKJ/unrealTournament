// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutomationDriver : ModuleRules
{
    public AutomationDriver(TargetInfo Target)
    {
        PublicIncludePaths.AddRange(
            new string[] {
				"Developer/AutomationDriver/Public",
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                "Developer/AutomationDriver/Private",
                "Developer/AutomationDriver/Private/Locators",
                "Developer/AutomationDriver/Private/Specs",
                "Developer/AutomationDriver/Private/MetaData",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "InputCore",
                "Json",
                "Slate",
                "SlateCore",
            }
        );
    }
}
