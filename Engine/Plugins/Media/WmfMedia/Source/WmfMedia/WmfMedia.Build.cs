// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WmfMedia : ModuleRules
	{
		public WmfMedia(TargetInfo Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                    "Media",
				}
            );

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"RenderCore",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
                    "Media",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"WmfMedia/Private",
                    "WmfMedia/Private/Player",
					"WmfMedia/Private/Shared",
					"WmfMedia/Private/Wmf",
				}
			);

			if (Target.Type == TargetRules.TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"Settings",
					}
				);

				PrivateIncludePathModuleNames.AddRange(
					new string[] {
						"Settings",
					}
				);
			}

			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
            {
                PublicDelayLoadDLLs.Add("mf.dll");
                PublicDelayLoadDLLs.Add("mfplat.dll");
                PublicDelayLoadDLLs.Add("mfplay.dll");
                PublicDelayLoadDLLs.Add("mfuuid.dll");
                PublicDelayLoadDLLs.Add("shlwapi.dll");
            }
		}
	}
}
