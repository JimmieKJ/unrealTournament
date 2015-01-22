// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NetworkFile : ModuleRules
{
	public NetworkFile(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.Add("DerivedDataCache");

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Sockets" });
        PublicIncludePaths.Add("Runtime/CoreUObject/Public/Interfaces");

		if (!UEBuildConfiguration.bBuildRequiresCookedData)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{ 
					"DerivedDataCache",
					"PackageDependencyInfo",
				}
				);
		}
	}
}