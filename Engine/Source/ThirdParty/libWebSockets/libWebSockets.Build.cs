// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class libWebSockets : ModuleRules
{
	public libWebSockets(TargetInfo Target)
	{
		Type = ModuleType.External;
        string WebsocketPath = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "libWebSockets", "libwebsockets");
        string PlatformSubdir = (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32") ? "Win32" :
        	Target.Platform.ToString();
        
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 ||
			(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
        {
            PlatformSubdir = Path.Combine(PlatformSubdir, WindowsPlatform.GetVisualStudioCompilerVersionName());
            if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add("websockets_static_d.lib");
            }
            else
            {
                PublicAdditionalLibraries.Add("websockets_static.lib");
            }
		}
        else if ( Target.Platform == UnrealTargetPlatform.Mac)
        {
		    PublicAdditionalLibraries.Add(Path.Combine(WebsocketPath, "lib", PlatformSubdir, "libwebsockets.a"));
        }

        PublicLibraryPaths.Add(Path.Combine(WebsocketPath, "lib", PlatformSubdir));
        PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include"));
        PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include", PlatformSubdir));
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDependencyModuleNames.Add("OpenSSL");
		}

        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
			string platform = "/Linux/" + Target.Architecture;
			string IncludePath = WebsocketPath + "/include" + platform;
			string LibraryPath = WebsocketPath + "/lib" + platform;

            PublicIncludePaths.Add(WebsocketPath + "include/");
			PublicIncludePaths.Add(IncludePath);
			PublicLibraryPaths.Add(LibraryPath);

		    PublicAdditionalLibraries.Add(LibraryPath + "/libwebsockets.a");

			PublicDependencyModuleNames.Add("OpenSSL");
        }
     }
}


