// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSLocalNotification : ModuleRules
{
	public IOSLocalNotification(TargetInfo Target)
	{
		BinariesSubFolder = "IOS";

		PublicIncludePaths.AddRange(new string[]
		{
			"Runtime/IOS/IOSLocalNotification/Public",
			"Runtime/Engine/Public",
		});
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});
	}
}
