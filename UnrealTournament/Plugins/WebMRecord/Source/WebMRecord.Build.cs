// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class WebMRecord : ModuleRules
	{
        public WebMRecord(TargetInfo Target)
        {
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "UnrealTournament",
					"InputCore",
					"Slate",
					"SlateCore",
					"ShaderCore",
					"RenderCore",
					"RHI"
				}
				);

            PublicIncludePaths.Add("../../UnrealTournament/Plugins/WebMRecord/Source/ThirdParty");
            
            var LIBPath = Path.Combine("..", "..", "UnrealTournament", "Plugins", "WebMRecord", "Source", "ThirdParty", "vpx", "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());

            if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                LIBPath = Path.Combine("..", "..", "UnrealTournament", "Plugins", "WebMRecord", "Source", "ThirdParty", "vpx32", "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());
            }

            var VPXLibPath = Path.Combine(LIBPath, "vpxmd.lib");
            //var VPXLibPath = Path.Combine(LIBPath, "vpxmdd.lib");
            
			// Lib file
            PublicLibraryPaths.Add(LIBPath);
            PublicAdditionalLibraries.Add(VPXLibPath);
            PublicAdditionalLibraries.Add("avrt.lib");
            
            // LibGD
            var LIBGDPath = Path.Combine("..", "..", "UnrealTournament", "Plugins", "WebMRecord", "Source", "ThirdParty", "libgd", "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());
            if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                LIBGDPath = Path.Combine("..", "..", "UnrealTournament", "Plugins", "WebMRecord", "Source", "ThirdParty", "libgd32", "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());
            }
            PublicLibraryPaths.Add(LIBGDPath);
            var GDLibPath = Path.Combine(LIBGDPath, "libgd.lib");
            PublicAdditionalLibraries.Add(GDLibPath);
            Definitions.Add("BGDWIN32");

            AddEngineThirdPartyPrivateStaticDependencies(Target, "Vorbis", "UEOgg");

            string VorbisPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";

            PublicIncludePaths.Add(VorbisPath + "include");
            Definitions.Add("WITH_OGGVORBIS=1");
		}
	}
}