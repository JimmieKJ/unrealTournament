// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Internationalization : ModuleRules
	{
		public Internationalization(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			); 

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Json",
				}
			); 

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Internationalization/Private",
				}
			);
		}
	}
}
