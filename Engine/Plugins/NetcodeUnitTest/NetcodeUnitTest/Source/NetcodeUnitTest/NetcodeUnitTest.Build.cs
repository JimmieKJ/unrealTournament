// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NetcodeUnitTest : ModuleRules
	{
		public NetcodeUnitTest(TargetInfo Target)
		{
			PrivateIncludePaths.Add("NetcodeUnitTest/Private");

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"OnlineSubsystem",
					"OnlineSubsystemUtils",
					"InputCore",
					"SlateCore",
					"Slate"
				}
			);

			// @todo JohnB: Currently don't support standalone commandlet, with static builds (can't get past linker error in Win32)
			if (!Target.IsMonolithic)
			{
				PrivateDependencyModuleNames.AddRange
				(
					new string[]
					{
						"StandaloneRenderer"
					}
				);
			}
		}
	}
}

