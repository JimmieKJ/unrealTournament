// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OpenSSL : ModuleRules
{
	public OpenSSL(TargetInfo Target)
	{
		Type = ModuleType.External;

		string OpenSSL101Path = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "OpenSSL", "1.0.1g");
		string OpenSSL102hPath = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "OpenSSL", "1_0_2h");
		string OpenSSL102Path = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "OpenSSL", "1.0.2g");

        string PlatformSubdir = (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32") ? "Win32" :
        	Target.Platform.ToString();
		string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicIncludePaths.Add(Path.Combine(OpenSSL102Path, "include", PlatformSubdir));

			string LibPath = Path.Combine(OpenSSL102Path, "lib", PlatformSubdir, ConfigFolder);
			//PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.a"));
			PublicAdditionalLibraries.Add("z");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			string IncludePath = UEBuildConfiguration.UEThirdPartySourceDirectory + "OpenSSL/1.0.2g" + "/" + "include/PS4";
			string LibraryPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "OpenSSL/1.0.2g" + "/" + "lib/PS4/release";
			PublicIncludePaths.Add(IncludePath);
			PublicAdditionalLibraries.Add(LibraryPath + "/" + "libssl.a");
			PublicAdditionalLibraries.Add(LibraryPath + "/" + "libcrypto.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 ||
				(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			string LibPath = Path.Combine(OpenSSL102Path, "lib", PlatformSubdir, "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());
			string LibPostfixAndExt = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d.lib" : ".lib";
			PublicIncludePaths.Add(Path.Combine(OpenSSL102Path, "include", PlatformSubdir, "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName()));
			PublicLibraryPaths.Add(LibPath);


			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libeay" + LibPostfixAndExt));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "ssleay" + LibPostfixAndExt));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string platform = "/Linux/" + Target.Architecture;
			string IncludePath = OpenSSL102hPath + "/include" + platform;
			string LibraryPath = OpenSSL102hPath + "/lib" + platform;

			PublicIncludePaths.Add(IncludePath);
			PublicLibraryPaths.Add(LibraryPath);
		    PublicAdditionalLibraries.Add(LibraryPath + "/libssl.a");
		    PublicAdditionalLibraries.Add(LibraryPath + "/libcrypto.a");

			PublicDependencyModuleNames.Add("zlib");
//			PublicAdditionalLibraries.Add("z");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string LibPath = Path.Combine(OpenSSL101Path, "lib", PlatformSubdir);
			PublicLibraryPaths.Add(LibPath);
		}
	}
}
