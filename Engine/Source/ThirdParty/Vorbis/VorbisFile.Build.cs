// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
        else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32") // simulator
        {
            string VorbisLibPath = VorbisPath + "Lib/HTML5Win32/";
            PublicLibraryPaths.Add(VorbisLibPath);
            PublicAdditionalLibraries.Add("libvorbisfile.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            string VorbisLibPath = VorbisPath + "Lib/HTML5/";
            PublicLibraryPaths.Add(VorbisLibPath);

            string OpimizationSuffix = "";
            if (UEBuildConfiguration.bCompileForSize)
            {
                OpimizationSuffix = "_Oz";
            }
            else
            {
                if (Target.Configuration == UnrealTargetConfiguration.Development)
                {
                    OpimizationSuffix = "_O2";
                }
                else if (Target.Configuration == UnrealTargetConfiguration.Shipping)
                {
                    OpimizationSuffix = "_O3";
                }
            }
            PublicAdditionalLibraries.Add(VorbisLibPath + "libvorbisfile" + OpimizationSuffix + ".bc");
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
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				PublicLibraryPaths.Add(VorbisPath + "lib/XboxOne/VS" + VersionName.ToString());
				PublicAdditionalLibraries.Add("libvorbisfile_static.lib");
			}
		}
	}
}

