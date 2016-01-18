// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
				// This module requires atlbase.h to be included before Windows headers, so we can make use of shared PCHs.  This
				// module will always have its own private PCH generated, if necessary.
				PCHUsage = PCHUsageMode.NoSharedPCHs;
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