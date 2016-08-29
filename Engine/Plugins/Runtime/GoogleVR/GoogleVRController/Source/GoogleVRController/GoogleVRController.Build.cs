/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class GoogleVRController : ModuleRules
	{
		public GoogleVRController(TargetInfo Target)
		{
			string GoogleVRSDKDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "GoogleVR/";
			PrivateIncludePaths.AddRange(
				new string[] {
					"GoogleVRController/Private",
					// ... add other private include paths required here ...
					GoogleVRSDKDir + "include",
					GoogleVRSDKDir + "include/vr/gvr/capi/include",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"InputDevice",
					"HeadMountedDisplay",
					"GoogleVRHMD"
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("Launch");
			}

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
				if(Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
				{	
					PublicIncludePaths.Add("Developer/Android/AndroidDeviceDetection/Public");
					PublicIncludePaths.Add("Developer/Android/AndroidDeviceDetection/Public/Interfaces");
					PrivateDependencyModuleNames.AddRange(new string[] { "GoogleVR" });
				}
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "GoogleVR" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GoogleVRController_APL.xml")));
			}
		}
	}
}
