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

        if (Target.Platform == UnrealTargetPlatform.HTML5)
        { 
            Definitions.Add("ENABLE_HTTP_FOR_NFS=1");
            if (Target.Architecture == "-win32")
            {
                PrivateDependencyModuleNames.Add("HTML5Win32");
            }
            else
            {
                PrivateDependencyModuleNames.Add("HTML5JS");
            }
        }
	}
}