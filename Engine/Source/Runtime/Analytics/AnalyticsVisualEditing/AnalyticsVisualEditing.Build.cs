// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnalyticsVisualEditing : ModuleRules
	{
        public AnalyticsVisualEditing(TargetInfo Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					"Analytics",
    				"Core",
	    			"CoreUObject",
                    "Engine"
				}
			);
		}
	}
}
