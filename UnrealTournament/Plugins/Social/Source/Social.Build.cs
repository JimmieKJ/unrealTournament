// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Social : ModuleRules
{
	public Social(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Slate",
				"OnlineSubsystem",
				"FriendsAndChat",
				"Json",
				"ImageDownload"
			}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[]
			{
				"SlateCore",
				"Sockets",
				"OnlineSubsystem",
				"FriendsAndChat",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Private",
				"Private/UI/Widgets",
				"Private/Models",
				"Private/Core",
				"Private/Layers/DataAccess",
		 		"Private/Layers/DataAccess/Analytics",
				"Private/Layers/Domain",
				"Private/Layers/Presentation",
				"Private/Layers/UI",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Public/Models",
				"Public/Interfaces",
				"Public/Layers/DataAccess",
				"Public/Layers/Domain",
				"Public/Layers/Presentation",
// 				"Public/Layers/UI"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
		new string[] {
				"HTTP",
				"Analytics",
				"AnalyticsET",
				"EditorStyle",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"HTTP",
				"Analytics",
				"AnalyticsET",
				"EditorStyle",
			}
		);

		if (UEBuildConfiguration.bCompileMcpOSS == true)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"OnlineSubsystemMcp",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"OnlineSubsystemMcp",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
//					"OnlineSubsystemMcp",
				}
			);

		}
	}
}
