// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidLocalNotification : ModuleRules
{
	public AndroidLocalNotification(TargetInfo Target)
	{
		BinariesSubFolder = "Android";

        PublicIncludePaths.AddRange(new string[]
        {
            "Runtime/Android/AndroidLocalNotification/Public",
            "Runtime/Engine/Public",
            "Runtime/Launch/Android/Public",
            "/Runtime/Launch/Private"
        });


        PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
            "Launch"
		});
	}
}
