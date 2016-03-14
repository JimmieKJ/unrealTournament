// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class RazerChroma : ModuleRules
	{
        public RazerChroma(TargetInfo Target)
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

            PublicIncludePaths.Add("../../UnrealTournament/Plugins/RazerChroma/Source/ThirdParty/ChromaSDK/inc");
		}
	}
}