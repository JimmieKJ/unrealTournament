// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FreeType2 : ModuleRules
{
	public FreeType2(TargetInfo Target)
	{
		Type = ModuleType.External;

        Definitions.Add("WITH_FREETYPE=1");
      
		string FreeType2Path;
		string FreeType2LibPath;

		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 ||
		  (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			FreeType2Path = UEBuildConfiguration.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.6/";
		}
		else
		{
			FreeType2Path = UEBuildConfiguration.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/";
		}

		FreeType2LibPath = FreeType2Path + "Lib/";

		PublicSystemIncludePaths.Add(FreeType2Path + "include");

        if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 ||
			(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
	
            FreeType2LibPath += (Target.Platform == UnrealTargetPlatform.Win64) ? "Win64/" : "Win32/";
            FreeType2LibPath += "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();

			PublicSystemIncludePaths.Add(FreeType2Path + "include");

            PublicLibraryPaths.Add(FreeType2LibPath);
			PublicAdditionalLibraries.Add("freetype26MT.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
            PublicAdditionalLibraries.Add(FreeType2LibPath + "Mac/libfreetype2412.a");
		}
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
			if (Target.Architecture == "-simulator")
            {
                PublicLibraryPaths.Add(FreeType2LibPath + "ios/Simulator");
				PublicAdditionalShadowFiles.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/Lib/ios/Simulator/libfreetype2412.a");
            }
            else
            {
                PublicLibraryPaths.Add(FreeType2LibPath + "ios/Device");
				PublicAdditionalShadowFiles.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/Lib/ios/Device/libfreetype2412.a");
            }

			PublicAdditionalLibraries.Add("freetype2412");
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// filtered out in the toolchain
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/ARMv7");
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/ARM64");
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/x86");
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/x64");

			PublicAdditionalLibraries.Add("freetype2412");
		}
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            if (Target.Type == TargetRules.TargetType.Server)
            {
                string Err = string.Format("{0} dedicated server is made to depend on {1}. We want to avoid this, please correct module dependencies.", Target.Platform.ToString(), this.ToString());
                System.Console.WriteLine(Err);
                throw new BuildException(Err);
            }

            if (Target.IsMonolithic)
            {
                PublicAdditionalLibraries.Add(FreeType2LibPath + "Linux/" + Target.Architecture + "/libfreetype2412.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(FreeType2LibPath + "Linux/" + Target.Architecture + "/libfreetype2412_fPIC.a");
            }
        }
       else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            PublicLibraryPaths.Add(FreeType2Path + "Lib/HTML5");
            PublicAdditionalLibraries.Add(FreeType2Path + "Lib/HTML5/libfreetype2412.bc");
        } 
	}
}
