// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class RawInput : ModuleRules
	{
		public RawInput(TargetInfo Target)
		{
			PublicIncludePaths.Add("RawInput/Source/RawInput/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				    "Engine",
					"InputCore",
					"SlateCore",
					"Slate"
				}
			);

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"InputDevice",
				}
			);
		}
	}
}