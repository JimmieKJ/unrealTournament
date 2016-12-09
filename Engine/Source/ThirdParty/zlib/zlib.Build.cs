// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class zlib : ModuleRules
{
	public zlib(TargetInfo Target)
	{
		Type = ModuleType.External;

		string zlibPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "zlib/v1.2.8/";

		// TODO: recompile for consoles and mobile platforms
		string OldzlibPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "zlib/zlib-1.2.5/";

        if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string platform = "/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicIncludePaths.Add(zlibPath + "include" + platform);
			PublicLibraryPaths.Add(zlibPath + "lib" + platform);
			PublicAdditionalLibraries.Add("zlibstatic.lib");
        }

		else if (Target.Platform == UnrealTargetPlatform.Win32 ||
				(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")) // simulator
		{
			string platform = "/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicIncludePaths.Add(zlibPath + "include" + platform);
			PublicLibraryPaths.Add(zlibPath + "lib" + platform);
			PublicAdditionalLibraries.Add("zlibstatic.lib");
		}

		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string platform = "/Mac/";
			PublicIncludePaths.Add(zlibPath + "include" + platform);
			// OSX needs full path
			PublicAdditionalLibraries.Add(zlibPath + "lib" + platform + "libz.a");
		}

		else if (Target.Platform == UnrealTargetPlatform.IOS||
				 Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PublicIncludePaths.Add(OldzlibPath + "inc");
			PublicAdditionalLibraries.Add("z");
		}

		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicIncludePaths.Add(OldzlibPath + "inc");
			PublicAdditionalLibraries.Add("z");
		}

		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
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
			PublicIncludePaths.Add(OldzlibPath + "Inc");
			PublicAdditionalLibraries.Add(OldzlibPath + "Lib/HTML5/zlib" + OpimizationSuffix + ".bc");
		}

		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string platform = "/Linux/" + Target.Architecture;
			PublicIncludePaths.Add(zlibPath + "include" + platform);
			PublicAdditionalLibraries.Add(zlibPath + "/lib/" + platform + (Target.IsMonolithic ? "/libz" : "/libz_fPIC") + ".a");
		}

        else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
			PublicIncludePaths.Add(OldzlibPath + "Inc");
            PublicLibraryPaths.Add(OldzlibPath + "Lib/PS4");
            PublicAdditionalLibraries.Add("z");
        }
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            // Use reflection to allow type not to exist if console code is not present
            System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
            if (XboxOnePlatformType != null)
            {
                System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				PublicIncludePaths.Add(OldzlibPath + "Inc");
				PublicLibraryPaths.Add(OldzlibPath + "Lib/XboxOne/VS" + VersionName.ToString());
                PublicAdditionalLibraries.Add("zlib125_XboxOne.lib");
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.Switch)
		{
			PublicIncludePaths.Add(OldzlibPath + "inc");
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(OldzlibPath, "Lib/Switch/libz.a"));
		}
    }
}
