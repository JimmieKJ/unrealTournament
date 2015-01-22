// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Messaging : ModuleRules
	{
		public Messaging(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			); 

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Messaging/Private",
					"Runtime/Messaging/Private/Bus",
					"Runtime/Messaging/Private/Bridge",
					"Runtime/Messaging/Private/Serialization",
				}
			);
		}
	}
}
