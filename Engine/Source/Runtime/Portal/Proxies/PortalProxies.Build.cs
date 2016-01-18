// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PortalProxies : ModuleRules
	{
		public PortalProxies(TargetInfo Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                    "Messaging",
                    "MessagingRpc",
                }
            );

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"PortalMessages",
				}
			);

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                    "Messaging",
                    "MessagingRpc",
                    "PortalServices",
                }
            );

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Portal/Proxies/Private",
					"Runtime/Portal/Proxies/Private/Account",
					"Runtime/Portal/Proxies/Private/Application",
					"Runtime/Portal/Proxies/Private/Package",
				}
			);
		}
	}
}
