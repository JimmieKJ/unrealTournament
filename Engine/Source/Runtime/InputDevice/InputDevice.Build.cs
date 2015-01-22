// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InputDevice : ModuleRules
{
    public InputDevice(TargetInfo Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/InputDevice/Public"
			}
			);

		PrivateDependencyModuleNames.AddRange( new string[] { "Core", "CoreUObject", "Engine" } );
	}
}
