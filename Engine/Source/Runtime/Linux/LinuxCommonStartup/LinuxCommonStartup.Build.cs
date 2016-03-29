// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LinuxCommonStartup : ModuleRules
{
	public LinuxCommonStartup(TargetInfo Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Projects");
		
		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SourceControl",
				}
			);
		}
	}
}
