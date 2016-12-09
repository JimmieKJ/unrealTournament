// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OnlineSubsystemGameCircle : ModuleRules
	{
		public OnlineSubsystemGameCircle(TargetInfo Target)
		{
			Definitions.Add("ONLINESUBSYSTEMGAMECIRCLE_PACKAGE=1");

			PrivateIncludePaths.AddRange(
				new string[] {
				"OnlineSubsystemGameCircle/Private",
				"ThirdParty/jni"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",
				"OnlineSubsystem",
				"Http",
				"GameCircleRuntimeSettings",
				"Launch"
				}
				);

			PublicLibraryPaths.Add(ModuleDirectory + "/../ThirdParty/jni/libs");
			PublicAdditionalLibraries.Add("AmazonGamesJni");

			// Additional Frameworks and Libraries for Android
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "OnlineSubsystemGameCircle_UPL.xml")));
			}
		}
	}
}
