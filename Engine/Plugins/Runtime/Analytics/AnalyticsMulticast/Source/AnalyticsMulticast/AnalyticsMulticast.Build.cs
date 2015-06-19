// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnalyticsMulticast : ModuleRules
	{
		public AnalyticsMulticast(TargetInfo Target)
		{
            PublicDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
					// ... add other public dependencies that you statically link with here ...
				}
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"Analytics",
					// ... add private dependencies that you statically link with here ...
				}
                );

            PublicIncludePathModuleNames.Add("Analytics");
        }
	}
}