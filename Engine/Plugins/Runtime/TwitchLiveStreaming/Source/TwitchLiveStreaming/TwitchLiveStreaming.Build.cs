// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class TwitchLiveStreaming : ModuleRules
    {
        public TwitchLiveStreaming(TargetInfo Target)
        {
			BinariesSubFolder = "NotForLicensees";

            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
					"RenderCore",
					"RHI"
				});

			if (Target.Type == TargetRules.TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[]
				{
					"Settings"
				});

				PublicIncludePathModuleNames.AddRange(
					new string[]
				{
					"Settings"
				});
			}

			string TwitchNotForLicenseesLibDir = System.IO.Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "..", "..", "Plugins", "Runtime", "TwitchLiveStreaming", "Source", "ThirdParty", "NotForLicensees", "Twitch", "lib" );   // Check the NotForLicensees folder first
			string TwitchLibDir = System.IO.Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "..", "..", "Plugins", "Runtime", "TwitchLiveStreaming", "Source", "ThirdParty", "Twitch", "lib" );
			bool bHaveTwitchSDK = false;

			try
			{
				bHaveTwitchSDK = System.IO.Directory.Exists( TwitchNotForLicenseesLibDir ) || System.IO.Directory.Exists( TwitchLibDir );
			}
			catch( System.Exception )
			{
			}

 			if( bHaveTwitchSDK )
			{
				AddEngineThirdPartyPrivateStaticDependencies( Target, "Twitch" );
				Definitions.Add( "WITH_TWITCH=1" );
			}
			else
			{
				Definitions.Add( "WITH_TWITCH=0" );
			}
        }
    }
}
