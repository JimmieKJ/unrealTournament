// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class VorbisFile : ModuleRules
{
	public VorbisFile(TargetInfo Target)
	{
		Type = ModuleType.External;

		string VorbisPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";
		PublicIncludePaths.Add(VorbisPath + "include");
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string VorbisLibPath = VorbisPath + "Lib/win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(VorbisLibPath);
			PublicAdditionalLibraries.Add("libvorbisfile_64.lib");
			PublicDelayLoadDLLs.Add("libvorbisfile_64.dll");
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbisfile_64.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 )
		{
			string VorbisLibPath = VorbisPath + "Lib/win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(VorbisLibPath);
			PublicAdditionalLibraries.Add("libvorbisfile.lib");
			PublicDelayLoadDLLs.Add("libvorbisfile.dll");
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbisfile.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
            string VorbisLibPath = VorbisPath + "Lib/HTML5Win32/";
            PublicLibraryPaths.Add(VorbisLibPath);
            PublicAdditionalLibraries.Add("libvorbisfile.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            string VorbisLibPath = VorbisPath + "Lib/HTML5";
            PublicLibraryPaths.Add(VorbisLibPath);
            if (UEBuildConfiguration.bCompileForSize)
            {
                PublicAdditionalLibraries.Add(VorbisPath + "Lib/HTML5/libvorbisfile_Oz.bc");
            }
            else
            {
                PublicAdditionalLibraries.Add(VorbisPath + "Lib/HTML5/libvorbisfile.bc");
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// filtered in toolchain
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/ARMv7");
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/ARM64");
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/x86");
			PublicLibraryPaths.Add(VorbisPath + "Lib/Android/x64");

			PublicAdditionalLibraries.Add("vorbisfile");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            PublicAdditionalLibraries.Add(VorbisPath + "lib/Linux/" + Target.Architecture + "/libvorbisfile.a");
            PublicAdditionalLibraries.Add(VorbisPath + "lib/Linux/" + Target.Architecture + "/libvorbisenc.a");
		}
    }
}

