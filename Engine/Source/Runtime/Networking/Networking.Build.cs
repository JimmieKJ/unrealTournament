// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Networking : ModuleRules
	{
		public Networking(TargetInfo Target)
		{
			PrivateIncludePaths.Add("Runtime/Networking/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Sockets"
				}
				);
		}
	}
}
