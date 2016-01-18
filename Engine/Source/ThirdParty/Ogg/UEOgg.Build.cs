// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEOgg : ModuleRules
{
	public UEOgg(TargetInfo Target)
	{
		Type = ModuleType.External;

		string OggPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Ogg/libogg-1.2.2/";

		PublicSystemIncludePaths.Add(OggPath + "include");

		string OggLibPath = OggPath + "lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			OggLibPath += "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add( OggLibPath );

			PublicAdditionalLibraries.Add("libogg_64.lib");

			PublicDelayLoadDLLs.Add("libogg_64.dll");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Ogg/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libogg_64.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 )
		{
			OggLibPath += "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add( OggLibPath );

			PublicAdditionalLibraries.Add("libogg.lib");

			PublicDelayLoadDLLs.Add("libogg.dll");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Ogg/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libogg.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
            OggLibPath += "HTML5Win32";
            PublicLibraryPaths.Add(OggLibPath);
			PublicAdditionalLibraries.Add("libogg.lib");
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(OggPath + "macosx/libogg.dylib");
		}
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
			if (UEBuildConfiguration.bCompileForSize)
			{
				PublicAdditionalLibraries.Add(OggLibPath + "HTML5/libogg_Oz.bc");
			}
			else
			{
				PublicAdditionalLibraries.Add(OggLibPath + "HTML5/libogg.bc");
			}
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Filtered in the toolchain.
			PublicLibraryPaths.Add(OggLibPath + "Android/ARMv7");
			PublicLibraryPaths.Add(OggLibPath + "Android/ARM64");
			PublicLibraryPaths.Add(OggLibPath + "Android/x86");
			PublicLibraryPaths.Add(OggLibPath + "Android/x64");

			PublicAdditionalLibraries.Add("ogg");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            if (Target.IsMonolithic)
            {
                PublicAdditionalLibraries.Add(OggLibPath + "Linux/" + Target.Architecture + "/libogg.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(OggLibPath + "Linux/" + Target.Architecture + "/libogg_fPIC.a");
            }
		}
	}
}

