// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GlsLang : ModuleRules
{
	public GlsLang(TargetInfo Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "glslang/glslang/src/glslang_lib");

		string LibPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "glslang/glslang/lib/";
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			LibPath = LibPath + (Target.Platform == UnrealTargetPlatform.Win32 ? "Win32/" : "Win64/");
			LibPath = LibPath + "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			
			PublicLibraryPaths.Add(LibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add("glslangd_64.lib");
				}
				else if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicAdditionalLibraries.Add("glslangd.lib");
				}
			}
			else
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add("glslang_64.lib");
				}
				else if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicAdditionalLibraries.Add("glslang.lib");
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(LibPath + "Mac/libglslangd.a");
				PublicAdditionalLibraries.Add(LibPath + "Mac/libOSDependentd.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(LibPath + "Mac/libglslang.a");
				PublicAdditionalLibraries.Add(LibPath + "Mac/libOSDependent.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(LibPath + "Linux/" + Target.Architecture + "/libglslang.a");
		}
		else
		{
			string Err = string.Format("Attempt to build against GlsLang on unsupported platform {0}", Target.Platform);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}
	}
}

