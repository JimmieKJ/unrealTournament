// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MetalRHI : ModuleRules
{	
	public MetalRHI(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				"ShaderCore",
				"UtilityShaders"
			}
			);
			
		PublicWeakFrameworks.Add("Metal");

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("QuartzCore");

			string UseMetalStats = System.Environment.GetEnvironmentVariable("ENABLE_METAL_STATISTICS");
			var StatsModule = System.IO.Path.Combine("Runtime", "NotForLicensees", "Mac", "MetalStatistics", "MetalStatistics.Build.cs");
			bool bMetalStats = (UseMetalStats == "1") && System.IO.File.Exists(StatsModule);

			if ( bMetalStats )
			{
				Definitions.Add("METAL_STATISTICS=1");

				PrivateIncludePathModuleNames.AddRange(
					new string[] {
						"MetalStatistics",
					}
				);

				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"MetalStatistics",
					}
				);
			}
		}
	}
}
