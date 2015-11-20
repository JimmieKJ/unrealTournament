// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CollectionManager : ModuleRules
{
	public CollectionManager(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"DirectoryWatcher",
				"SourceControl"
			}
			);
	}
}
