// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FriendsAndChat : ModuleRules
{
	public FriendsAndChat(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Slate",
				"OnlineSubsystem"
			}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[]
			{
				"SlateCore",
				"Sockets",
				"OnlineSubsystem"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Developer/FriendsAndChat/Private",
				"Developer/FriendsAndChat/Private/UI/Widgets",
				"Developer/FriendsAndChat/Private/Models",
				"Developer/FriendsAndChat/Private/Core",

			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Developer/FriendsAndChat/Public/Models",
				"Developer/FriendsAndChat/Public/Interfaces"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
		new string[] {
				"HTTP",
				"Analytics",
				"AnalyticsET",
				"EditorStyle",
				"OnlineSubsystem"
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
					"OnlineSubsystemMcp",
				}
			);
		}
	}
}
