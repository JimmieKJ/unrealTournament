// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WebBrowserWidget : ModuleRules
	{
		public WebBrowserWidget(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
					"WebBrowser",
                    "Slate",
                    "SlateCore",
					"UMG"
				}
			);
		}
	}
}
