// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Facebook : ModuleRules
{
    public Facebook(TargetInfo Target)
    {
        Type = ModuleType.External;

        Definitions.Add("WITH_FACEBOOK=1");
		Definitions.Add("UE4_FACEBOOK_VER=4");

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
			// Access to Facebook core
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBSDKCoreKit",
					"IOS/FacebookSDK/FBSDKCoreKit.embeddedframework.zip"
				)
			);

			// Add the FBAudienceNetwork framework
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBAudienceNetwork",
					"IOS/FacebookSDK/FBAudienceNetwork.embeddedframework.zip"
				)
			);

			// Access to Facebook login
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBSDKLoginKit",
					"IOS/FacebookSDK/FBSDKLoginKit.embeddedframework.zip"
				)
			);


			// Access to Facebook sharing
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBSDKShareKit",
					"IOS/FacebookSDK/FBSDKShareKit.embeddedframework.zip"
				)
			);
        }
    }
}
