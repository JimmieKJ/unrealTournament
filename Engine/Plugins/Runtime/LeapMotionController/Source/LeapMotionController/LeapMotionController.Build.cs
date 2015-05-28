// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LeapMotionController : ModuleRules
	{
		public LeapMotionController(TargetInfo Target)
		{
            if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
            {
                Definitions.Add("LEAP_USE_DEBUG_LIB=1");
                PublicDelayLoadDLLs.Add("Leapd.dll");
            }
            else
            {
                PublicDelayLoadDLLs.Add("Leap.dll");
            }

			PublicDependencyModuleNames.AddRange( new string[] { "Core", "CoreUObject", "Engine", "InputCore" } ); 

            PublicDependencyModuleNames.AddRange(new string[] { "Leap" }); 
		}
	}
}