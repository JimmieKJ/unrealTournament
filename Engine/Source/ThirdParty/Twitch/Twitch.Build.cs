// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Twitch : ModuleRules
{
	public Twitch( TargetInfo Target )
	{
		Type = ModuleType.External;

		// Only Windows, Mac and iOS platforms are supported for now
		if( Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || 
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.IOS )
		{
			string TwitchBaseDir = Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "NotForLicensees", "Twitch", "Twitch-6.17" );	// Check the NotForLicensees folder first
			if( !Directory.Exists( TwitchBaseDir ) )
			{
				TwitchBaseDir = Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "Twitch", "Twitch-6.17" );		// Use the normal location
			}

			// Add the various base include paths for the Twitch SDK
			PublicIncludePaths.Add( Path.Combine( TwitchBaseDir, "include" ) );
			PublicIncludePaths.Add( Path.Combine( TwitchBaseDir, "twitchcore", "include" ) );
			PublicIncludePaths.Add( Path.Combine( TwitchBaseDir, "twitchchat", "include" ) );

			string TwitchLibPath;
			if( Target.Platform == UnrealTargetPlatform.IOS )
			{
				TwitchLibPath = Path.Combine( TwitchBaseDir, "ios", (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release" );
				PublicLibraryPaths.Add( TwitchLibPath );
				PublicAdditionalLibraries.Add( "libtwitchsdk-ios" );
			}
			else
			{
				// Other platforms link with Twitch dynamically, and don't need an import library to be statically linked
			}
		}
	}
}
