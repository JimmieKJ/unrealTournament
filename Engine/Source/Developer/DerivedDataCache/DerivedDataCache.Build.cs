// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DerivedDataCache : ModuleRules
{
	public DerivedDataCache(TargetInfo Target)
	{
		PrivateDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.Add("CookingStats");

		// Internal (NotForLicensees) module
		var DDCUtilsModule = Path.Combine("Developer", "NotForLicensees", "DDCUtils", "DDCUtils.Build.cs");
		if (File.Exists(DDCUtilsModule))
		{
			DynamicallyLoadedModuleNames.Add("DDCUtils");
		}
	}
}
