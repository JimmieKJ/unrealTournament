// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Facebook : ModuleRules
{
    public Facebook(TargetInfo Target)
    {
        Type = ModuleType.External;

        Definitions.Add("WITH_FACEBOOK=1");

		string FacebookPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Facebook/";
        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            string FacebookVersion = "3.16";
            if( FacebookVersion == "3.16" )
            {
                Definitions.Add("FACEBOOK_VER_3_16=1");
            }

            FacebookPath += ("Facebook-IOS-" + FacebookVersion + "/");

            PublicIncludePaths.Add(FacebookPath + "Include");

			string LibDir = FacebookPath + "Lib/Release" + Target.Architecture;

            PublicLibraryPaths.Add(LibDir);
            PublicAdditionalLibraries.Add("Facebook-IOS-"+FacebookVersion);

            PublicAdditionalShadowFiles.Add(LibDir + "/libFacebook-IOS-" + FacebookVersion + ".a");
            
            AddThirdPartyPrivateStaticDependencies( Target, "Bolts" );


            // Needed for the facebook sdk to link.
            PublicFrameworks.Add("CoreGraphics");
        }
    }
}
