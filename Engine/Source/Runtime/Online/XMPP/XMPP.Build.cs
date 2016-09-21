// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XMPP : ModuleRules
{
	public XMPP(TargetInfo Target)
	{
		Definitions.Add("XMPP_PACKAGE=1");

        PrivateIncludePaths.AddRange(
			new string[] 
			{
				"Runtime/Online/XMPP/Private"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"Json"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
// jira UE-30298: temp undo
//            AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
//            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
			if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
			}
            AddEngineThirdPartyPrivateStaticDependencies(Target, "WebRTC");
        }
		else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "WebRTC");
        }
    }
}
