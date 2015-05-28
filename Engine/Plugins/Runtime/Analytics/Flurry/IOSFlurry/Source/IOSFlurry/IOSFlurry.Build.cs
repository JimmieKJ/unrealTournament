// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IOSFlurry : ModuleRules
	{
        public IOSFlurry(TargetInfo Target)
		{
			BinariesSubFolder = "NotForLicensees";

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					// ... add private dependencies that you statically link with here ...
				}
				);

            PublicIncludePathModuleNames.Add("Analytics");

			PublicFrameworks.AddRange(
				new string[] {
					"CoreTelephony",
					"SystemConfiguration",
					"UIKit",
					"Foundation",
					"CoreGraphics",
					"MobileCoreServices",
					"StoreKit",
					"CFNetwork",
					"CoreData",
					"Security",
					"CoreLocation"
				});

            bool bHasFlurrySDK =
                (System.IO.Directory.Exists(System.IO.Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "Flurry")) &&
                  System.IO.Directory.Exists(System.IO.Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "Flurry", "IOS"))) ||
                (System.IO.Directory.Exists(System.IO.Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "NotForLicensees")) &&
                  System.IO.Directory.Exists(System.IO.Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "NotForLicensees", "Flurry")) &&
                  System.IO.Directory.Exists(System.IO.Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "NotForLicensees", "Flurry", "IOS")));
            if (bHasFlurrySDK)
            {
                PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees/Flurry/IOS/");
                PublicAdditionalLibraries.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees/Flurry/IOS/libFlurry_6.3.0.a");

                Definitions.Add("WITH_FLURRY=1");
            }
            else
            {
                Definitions.Add("WITH_FLURRY=0");
            }
        }
	}
}