// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class TwitchLiveStreaming : ModuleRules
    {
        public TwitchLiveStreaming(TargetInfo Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
					"RenderCore",
					"RHI"
				});

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

 			bool bHaveTwitchSDK =
				!UnrealBuildTool.BuildingRocket() &&	// @todo: Twitch support disabled in released builds until it is cleared by legal
 				( System.IO.Directory.Exists( System.IO.Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "Twitch" ) ) &&
 				  System.IO.Directory.Exists( System.IO.Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "Twitch", "Twitch-6.17" ) ) ) ||
 				( System.IO.Directory.Exists( System.IO.Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "NotForLicensees" ) ) &&
 				  System.IO.Directory.Exists( System.IO.Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "NotForLicensees", "Twitch" ) ) &&
				  System.IO.Directory.Exists( System.IO.Path.Combine( UEBuildConfiguration.UEThirdPartySourceDirectory, "NotForLicensees", "Twitch", "Twitch-6.17" ) ) );
 			if( bHaveTwitchSDK )
			{
				AddThirdPartyPrivateStaticDependencies( Target, "Twitch" );
				Definitions.Add( "WITH_TWITCH=1" );
			}
			else
			{
				Definitions.Add( "WITH_TWITCH=0" );
			}
        }
    }
}
