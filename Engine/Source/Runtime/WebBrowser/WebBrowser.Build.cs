// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebBrowser : ModuleRules
{
	public WebBrowser(TargetInfo Target)
	{
		PublicIncludePaths.Add("Developer/WebBrowser/Public");
		PrivateIncludePaths.Add("Developer/WebBrowser/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"InputCore",
				"SlateCore",
				"Slate",
				"CEF3Utils",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64
        ||  Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddThirdPartyPrivateStaticDependencies(Target,
				"CEF3"
				);
		}
	}
}
