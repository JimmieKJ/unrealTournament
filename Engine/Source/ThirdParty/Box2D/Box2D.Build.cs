// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Box2D : ModuleRules
{
	public Box2D(TargetInfo Target)
	{
		Type = ModuleType.External;

		// Determine the root directory of Box2D
		string ModuleCSFilename = UnrealBuildTool.RulesCompiler.GetFileNameFromType(GetType());
		string ModuleBaseDirectory = Path.GetDirectoryName(ModuleCSFilename);
		string Box2DBaseDir = Path.Combine(ModuleBaseDirectory, "Box2D_v2.3.1");

		// Add the libraries for the current platform
		//@TODO: This need to be kept in sync with RulesCompiler.cs for now
		bool bSupported = false;
		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
			bSupported = true;

			string WindowsVersion = "vs" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			string ArchitectureVersion = (Target.Platform == UnrealTargetPlatform.Win64) ? "x64" : "x32";
			string ConfigVersion = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

			string Box2DLibDir = Path.Combine(Box2DBaseDir, "build", WindowsVersion, "bin", ArchitectureVersion, ConfigVersion);
			PublicLibraryPaths.Add(Box2DLibDir);

			string LibName = "Box2D.lib";
			PublicAdditionalLibraries.Add(LibName);
		}

		// Box2D included define (this should be handled by using SetupModuleBox2DSupport instead)
		//Definitions.Add(string.Format("WITH_BOX2D={0}", bSupported ? 1 : 0));

		if (bSupported)
		{
			// Include path
			PublicSystemIncludePaths.Add(Box2DBaseDir);
		}
	}
}
