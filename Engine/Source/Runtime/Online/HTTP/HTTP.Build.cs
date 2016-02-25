// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTP : ModuleRules
{
    public HTTP(TargetInfo Target)
    {
        Definitions.Add("HTTP_PACKAGE=1");

        PrivateIncludePaths.AddRange(
            new string[] {
				"Runtime/Online/HTTP/Private",
			}
            );

        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
			}
			);

        if (Target.Platform == UnrealTargetPlatform.Win32 ||
            Target.Platform == UnrealTargetPlatform.Win64)
        {
            AddThirdPartyPrivateStaticDependencies(Target, "WinInet");
            AddThirdPartyPrivateStaticDependencies(Target, "libcurl");
        }

        if (Target.Platform == UnrealTargetPlatform.Linux ||
            Target.Platform == UnrealTargetPlatform.Android)
        {
            AddThirdPartyPrivateStaticDependencies(Target, "libcurl");
        }
        if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            if (Target.Architecture == "-win32")
            {
                PrivateDependencyModuleNames.Add("HTML5Win32");
            }
            else
            {
                PrivateDependencyModuleNames.Add("HTML5JS");
            }
        }
    }
}
