// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateReflector : ModuleRules
{
	public SlateReflector(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"InputCore",
				"Slate",
				"SlateCore",
				"Json",
				"AssetRegistry",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Messaging",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/SlateReflector/Private",
				"Developer/SlateReflector/Private/Models",
				"Developer/SlateReflector/Private/Widgets",
			}
		);

		// Editor builds include SessionServices to populate the remote target drop-down for remote widget snapshots
		if (Target.Type == TargetRules.TargetType.Editor)
		{
			Definitions.Add("SLATE_REFLECTOR_HAS_SESSION_SERVICES=1");

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SessionServices",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SessionServices",
				}
			);
		}
		else
		{
			Definitions.Add("SLATE_REFLECTOR_HAS_SESSION_SERVICES=0");
		}

		// DesktopPlatform is only available for Editor and Program targets (running on a desktop platform)
		bool IsDesktopPlatformType = Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Win32
			|| Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Mac
			|| Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Linux;
		if (Target.Type == TargetRules.TargetType.Editor || (Target.Type == TargetRules.TargetType.Program && IsDesktopPlatformType))
		{
			Definitions.Add("SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM=1");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"DesktopPlatform",
				}
			);
		}
		else
		{
			Definitions.Add("SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM=0");
		}
	}
}
