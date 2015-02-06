// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WebBrowserAsPlugin : ModuleRules
	{
		public WebBrowserAsPlugin(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"WebBrowser"
				}
			);
		}
	}
}
