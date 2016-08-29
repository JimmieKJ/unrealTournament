// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationController : ModuleRules
	{
		public AutomationController(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			); 
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AutomationMessages",
					"CoreUObject",
					"UnrealEdMessages",
                    "MessageLog"
				}
			);

            if (UEBuildConfiguration.bBuildEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
                        "UnrealEd",
                        "Engine", // Needed for UWorld/GWorld to find current level
				    }
                );
            }

            PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Messaging",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AutomationController/Private"
				}
			);
		}
	}
}
