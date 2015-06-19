// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class Vorbis : ModuleRules
{
	public Vorbis(TargetInfo Target)
	{
		Type = ModuleType.External;

		string VorbisPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";

		PublicIncludePaths.Add(VorbisPath + "include");
		Definitions.Add("WITH_OGGVORBIS=1");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string VorbisLibPath = VorbisPath + "Lib/win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(VorbisLibPath);

			PublicAdditionalLibraries.Add("libvorbis_64.lib");
			PublicDelayLoadDLLs.Add("libvorbis_64.dll");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbis_64.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			string VorbisLibPath = VorbisPath + "Lib/win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(VorbisLibPath);

			PublicAdditionalLibraries.Add("libvorbis.lib");
			PublicDelayLoadDLLs.Add("libvorbis.dll");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbis.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
            string VorbisLibPath = VorbisPath + "Lib/HTML5Win32/";
            PublicLibraryPaths.Add(VorbisLibPath);
            PublicAdditionalLibraries.Add("libvorbis.lib");
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.AddRange(
				new string[] {
					VorbisPath + "macosx/libvorbis.dylib",
				}
				);
		}
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
			if (UEBuildConfiguration.bCompileForSize)
			{
				PublicAdditionalLibraries.Add(VorbisPath + "Lib/HTML5/libvorbis_Oz.bc");
			}
			else
			{
				PublicAdditionalLibraries.Add(VorbisPath + "Lib/HTML5/libvorbis.bc");
			}
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// toolchain will filter
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/ARMv7");
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/ARM64");
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/x86");
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/x64");

			PublicAdditionalLibraries.Add("vorbis");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            PublicAdditionalLibraries.Add(VorbisPath + "lib/Linux/" + Target.Architecture + "/libvorbis.a");
		}
	}
}

