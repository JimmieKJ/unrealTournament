// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SubversionSourceControl : ModuleRules
{
	public SubversionSourceControl(TargetInfo Target)
	{
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
				"XmlParser",
		    }
		);

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			// Add contents of binaries directory as runtime dependencies
			foreach (string FilePath in Directory.EnumerateFiles(UEBuildConfiguration.UEThirdPartyBinariesDirectory + "svn/" + Target.Platform.ToString() + "/", "*", SearchOption.AllDirectories))
			{
				RuntimeDependencies.Add(new RuntimeDependency(FilePath));
			}
		}
	}
}
