// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UdpMessaging : ModuleRules
	{
		public UdpMessaging(TargetInfo Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                    "Messaging",
					"Settings",
				}
            );

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			); 

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"Json",
					"Networking",
					"Serialization",
					"Sockets",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"Settings",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"UdpMessaging/Private",
					"UdpMessaging/Private/Shared",
					"UdpMessaging/Private/Transport",
					"UdpMessaging/Private/Transport/Tests",
					"UdpMessaging/Private/Tunnel",
				}
			);
		}
	}
}
