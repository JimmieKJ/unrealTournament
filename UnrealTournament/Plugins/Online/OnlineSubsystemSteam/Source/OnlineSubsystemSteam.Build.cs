// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemSteam : ModuleRules
{
	public OnlineSubsystemSteam(TargetInfo Target)
	{
		string SteamVersion = "Steamv132";
		bool bSteamSDKFound = Directory.Exists(UEBuildConfiguration.UEThirdPartySourceDirectory + "Steamworks/" + SteamVersion) == true;

		Definitions.Add("STEAMSDK_FOUND=" + (bSteamSDKFound ? "1" : "0"));
		Definitions.Add("WITH_STEAMWORKS=" + (bSteamSDKFound ? "1" : "0"));

		Definitions.Add("ONLINESUBSYSTEMSTEAM_PACKAGE=1");

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"OnlineSubsystemUtils",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"Sockets", 
				"Voice",
				"OnlineSubsystem",
				"Json"
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
