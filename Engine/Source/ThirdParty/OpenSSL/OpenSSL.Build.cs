// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OpenSSL : ModuleRules
{
	public OpenSSL(TargetInfo Target)
	{
		Type = ModuleType.External;

		//string OpenSSLPath = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "OpenSSL", "1.0.1g");
		string OpenSSL102Path = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "OpenSSL", "1.0.2g");
		string PlatformSubdir = Target.Platform.ToString();

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPath = Path.Combine(OpenSSL102Path, "lib", PlatformSubdir);
			PublicIncludePaths.Add(Path.Combine(OpenSSL102Path, "include", PlatformSubdir));
			PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.a"));
			PublicAdditionalLibraries.Add("z");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			string LibPath = Path.Combine(OpenSSL102Path, "lib", PlatformSubdir, "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());
			string LibPostfixAndExt = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d.lib" : ".lib";
			PublicIncludePaths.Add(Path.Combine(OpenSSL102Path, "include", PlatformSubdir, "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName()));
			PublicLibraryPaths.Add(LibPath);


			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libeay" + LibPostfixAndExt));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "ssleay" + LibPostfixAndExt));
		}
	}
}
