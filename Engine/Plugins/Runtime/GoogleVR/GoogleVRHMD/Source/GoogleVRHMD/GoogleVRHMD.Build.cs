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
	public class GoogleVRHMD : ModuleRules
	{
		public GoogleVRHMD(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"GoogleVRHMD/Private",
					"../../../../../Source/Runtime/Renderer/Private",
					"../../../../../Source/Runtime/Core/Private"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay",
					"InputCore"
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("Launch");
			}

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
			else
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "OpenGLDrv" });
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
				PrivateIncludePaths.AddRange(
					new string[] {
					"../../../../../Source/Runtime/OpenGLDrv/Private",
					// ... add other private include paths required here ...
					}
				);
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "GoogleVR" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GoogleVRHMD_APL.xml")));
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "GoogleVR" });
			}
		}
	}
}
