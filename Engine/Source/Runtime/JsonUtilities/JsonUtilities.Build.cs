// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JsonUtilities : ModuleRules
{
	public JsonUtilities( TargetInfo Target )
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Json",
			}
		);
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 			
				"CoreUObject",
			}
		);
	}
}
