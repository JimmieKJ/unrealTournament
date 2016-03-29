// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Twitch : ModuleRules
{
	public Twitch( TargetInfo Target )
	{
		Type = ModuleType.External;

		// Only Windows is supported for now
		if( Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 )
		{
			string TwitchBaseDir = Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "..", "..", "Plugins", "Runtime", "TwitchLiveStreaming", "Source", "ThirdParty", "NotForLicensees", "Twitch" );   // Check the NotForLicensees folder first
			string TwitchDLLDir = "$(PluginDir)/Binaries/ThirdParty/NotForLicensees/Twitch";
			if ( !Directory.Exists( TwitchBaseDir ) )
			{
				TwitchBaseDir = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "..", "..", "Plugins", "Runtime", "TwitchLiveStreaming", "Source", "ThirdParty", "Twitch");       // Use the normal location
				TwitchDLLDir = "$(PluginDir)/Binaries/ThirdParty/Twitch/Twitch";
			}

			if( Target.Platform == UnrealTargetPlatform.Win64 )
			{ 
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win64/twitchsdk_64_release.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win64/avutil-ttv-51.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win64/libmfxsw64.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win64/libmp3lame-ttv.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win64/swresample-ttv-0.dll" ) );
			}
			else if( Target.Platform == UnrealTargetPlatform.Win32 )
			{
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win32/twitchsdk_32_release.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win32/avutil-ttv-51.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win32/libmfxsw32.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win32/libmp3lame-ttv.dll" ) );
				RuntimeDependencies.Add( new RuntimeDependency( TwitchDLLDir + "/Win32/swresample-ttv-0.dll" ) );
			}

			// Add the various base include paths for the Twitch SDK
			PublicIncludePaths.Add( Path.Combine( TwitchBaseDir, "include" ) );
			PublicIncludePaths.Add( Path.Combine( TwitchBaseDir, "twitchcore", "include" ) );
			PublicIncludePaths.Add( Path.Combine( TwitchBaseDir, "twitchchat", "include" ) );
		}
	}
}
