// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VisualStudioSourceCodeAccess : ModuleRules
	{
        public VisualStudioSourceCodeAccess(TargetInfo Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SourceCodeAccess",
					"DesktopPlatform",
				}
			);

			if (UEBuildConfiguration.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("HotReload");
			}

			if (WindowsPlatform.bHasVisualStudioDTE)
			{
				Definitions.Add("VSACCESSOR_HAS_DTE=1");
			}
			else
			{
				Definitions.Add("VSACCESSOR_HAS_DTE=0");
			}

			bBuildLocallyWithSNDBS = true;
		}
	}
}