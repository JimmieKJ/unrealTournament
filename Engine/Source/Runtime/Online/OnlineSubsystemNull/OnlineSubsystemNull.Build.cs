// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemNull : ModuleRules
{
	public OnlineSubsystemNull(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMNULL_PACKAGE=1");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"Sockets", 
				"OnlineSubsystem", 
				"OnlineSubsystemUtils",
				"Json"
			}
			);
	}
}
