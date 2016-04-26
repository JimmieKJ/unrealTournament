// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class QoSReporter : ModuleRules
	{
		public QoSReporter(TargetInfo Target)
		{
			PublicDependencyModuleNames.Add("Core");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"HTTP",
					"Json",
				}
			);

			if (Target.Type != TargetRules.TargetType.Program)
			{
				PrivateDependencyModuleNames.Add("Engine"); // for GAverageFPS - unused by programs
			}

			// servers expose QoS metrics via perfcounters
			if (Target.Type != TargetRules.TargetType.Client && Target.Type != TargetRules.TargetType.Program)
			{
				PrivateDependencyModuleNames.Add("PerfCounters");
			}

			Definitions.Add("WITH_QOSREPORTER=1");
		}
	}
}
