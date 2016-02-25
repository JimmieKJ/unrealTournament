// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystem : ModuleRules
{
	public OnlineSubsystem(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Json",
			}
		);

        PublicIncludePaths.Add("Runtime/Online/OnlineSubsystem/Public/Interfaces");

		PrivateIncludePaths.Add("Runtime/Online/OnlineSubsystem/Private");		

		Definitions.Add("ONLINESUBSYSTEM_PACKAGE=1");

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"ImageCore",
				"Sockets",
				"JsonUtilities"
			}
		);


	}

  
}
