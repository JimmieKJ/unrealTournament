// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebSockets : ModuleRules
{
    public WebSockets(TargetInfo Target)
    {
        Definitions.Add("WEBSOCKETS_PACKAGE=1");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		bool bShouldUseModule = 
			Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.Linux;

		if (bShouldUseModule)
		{
			Definitions.Add("WITH_WEBSOCKETS=1");

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Online/WebSockets/Private",
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "libWebSockets", "zlib");
			PrivateDependencyModuleNames.Add("SSL");
		}
	}
}
