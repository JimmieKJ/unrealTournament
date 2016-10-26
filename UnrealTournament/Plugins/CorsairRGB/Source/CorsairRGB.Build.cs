// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class CorsairRGB : ModuleRules
	{
        public CorsairRGB(TargetInfo Target)
        {
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "UnrealTournament",
					"InputCore",
					"SlateCore",
				}
                );

            PublicIncludePaths.Add("../../UnrealTournament/Plugins/CorsairRGB/Source/ThirdParty/CorsairRGB/include");

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicLibraryPaths.Add("../../UnrealTournament/Plugins/CorsairRGB/Source/ThirdParty/CorsairRGB/lib/x64");
                PublicAdditionalLibraries.Add("../../UnrealTournament/Plugins/CorsairRGB/Source/ThirdParty/CorsairRGB/lib/x64/CUESDK.x64_2013.lib");
                PublicDelayLoadDLLs.Add("../../UnrealTournament/Plugins/CorsairRGB/Binaries/Win64/CUESDK.x64_2013.dll");
                RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/CorsairRGB/Binaries/Win64/CUESDK.x64_2013.dll"));
            }
            else if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                PublicLibraryPaths.Add("../../UnrealTournament/Plugins/CorsairRGB/Source/ThirdParty/CorsairRGB/lib/i386");
                PublicAdditionalLibraries.Add("../../UnrealTournament/Plugins/CorsairRGB/Source/ThirdParty/CorsairRGB/lib/i386/CUESDK_2013.lib");
                PublicDelayLoadDLLs.Add("../../UnrealTournament/Plugins/CorsairRGB/Binaries/Win32/CUESDK_2013.dll");
                RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/CorsairRGB/Binaries/Win32/CUESDK_2013.dll"));
            }
        }
	}
}