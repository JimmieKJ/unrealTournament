// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LeapMotionController : ModuleRules
	{
		public LeapMotionController(TargetInfo Target)
		{
			PublicDelayLoadDLLs.Add("Leap.dll");

			PublicDependencyModuleNames.AddRange( new string[] { "Core", "CoreUObject", "Engine", "InputCore" } ); 

            PublicDependencyModuleNames.AddRange(new string[] { "Leap" }); 
		}
	}
}