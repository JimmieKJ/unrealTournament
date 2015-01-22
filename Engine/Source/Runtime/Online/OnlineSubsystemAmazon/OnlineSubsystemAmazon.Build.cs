// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemAmazon : ModuleRules
{
	public OnlineSubsystemAmazon(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMAMAZON_PACKAGE=1");		

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"HTTP",
				"Json",
                "OnlineSubsystem", 
			}
		);
	}
}
