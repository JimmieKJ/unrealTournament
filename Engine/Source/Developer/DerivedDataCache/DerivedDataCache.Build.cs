// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DerivedDataCache : ModuleRules
{
	public DerivedDataCache(TargetInfo Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		// Internal (NotForLicensees) module
		if (Directory.Exists(Path.Combine("Developer", "NotForLicensees", "DDCUtils")) && !UnrealBuildTool.UnrealBuildTool.BuildingRocket())
		{
			DynamicallyLoadedModuleNames.Add("DDCUtils");
		}
	}
}
