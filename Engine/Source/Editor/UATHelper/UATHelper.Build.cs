// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UATHelper : ModuleRules
	{
        public UATHelper(TargetInfo Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
				    "CoreUObject",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    "GameProjectGeneration",
                    "UnrealEd",
					"Analytics",
			    }
			);
		}
	}
}