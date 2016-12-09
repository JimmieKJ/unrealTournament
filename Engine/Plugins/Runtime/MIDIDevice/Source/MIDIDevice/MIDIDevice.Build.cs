// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MIDIDevice : ModuleRules
	{
        public MIDIDevice(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
				}
			);
			AddEngineThirdPartyPrivateStaticDependencies(Target, "portmidi");
		}
	}
}