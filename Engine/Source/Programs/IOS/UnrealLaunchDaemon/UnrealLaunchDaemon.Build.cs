// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealLaunchDaemon : ModuleRules
{
	public UnrealLaunchDaemon( TargetInfo Target )
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/Launch/Public"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"NetworkFile",
				"Projects",
				"StreamingFile",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"StandaloneRenderer",
				"LaunchDaemonMessages",
				"Projects",
				"Messaging",
				"UdpMessaging"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
			}
		);

		PrivateIncludePaths.Add("Runtime/Launch/Private");
	}
}