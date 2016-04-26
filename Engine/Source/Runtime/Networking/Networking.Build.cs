// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
