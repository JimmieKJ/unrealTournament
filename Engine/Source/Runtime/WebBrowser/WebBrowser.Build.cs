// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebBrowser : ModuleRules
{
	public WebBrowser(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/WebBrowser/Public");
		PrivateIncludePaths.Add("Runtime/WebBrowser/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RHI",
				"InputCore",
				"SlateCore",
				"Slate",
				"CEF3Utils",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64
		|| Target.Platform == UnrealTargetPlatform.Win32
        ||  Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddThirdPartyPrivateStaticDependencies(Target,
				"CEF3"
				);
		}
	}
}
