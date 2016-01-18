// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AddContentDialog : ModuleRules
{
	public AddContentDialog(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"ContentBrowser",
				"SuperSearch"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
				"InputCore",
				"Json",
				"EditorStyle",
				"DirectoryWatcher",
				"DesktopPlatform",
				"PakFile",
				"ImageWrapper",
				"UnrealEd",
				"CoreUObject",				
				"WidgetCarousel",				
			
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"SuperSearch",
			}
		);
		
		PublicIncludePathModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/AddContentDialog/Private",
				"Editor/AddContentDialog/Private/ViewModels",
				"Editor/AddContentDialog/Private/ContentSourceProviders/AssetPack",
				"Editor/AddContentDialog/Private/ContentSourceProviders/FeaturePack",
			}
		);
	}
}
