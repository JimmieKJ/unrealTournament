// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NUTUnrealEngine4 : ModuleRules
	{
		public NUTUnrealEngine4(TargetInfo Target)
		{
			PrivateIncludePaths.Add("NUTUnrealEngine4/Private");

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"OnlineSubsystem",
					"OnlineSubsystemUtils",
					"NetcodeUnitTest"
				}
			);
		}
	}
}
