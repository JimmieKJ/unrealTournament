// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CoreUObject : ModuleRules
{
	public CoreUObject(TargetInfo Target)
	{
		SharedPCHHeaderFile = "Runtime/CoreUObject/Public/CoreUObject.h";

		PrivateIncludePaths.Add("Runtime/CoreUObject/Private");

        PrivateIncludePathModuleNames.AddRange(
                new string[] 
			    {
				    "TargetPlatform",
				    "CookingStats",
			    }
            );

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.Add("Projects");

	}

}
