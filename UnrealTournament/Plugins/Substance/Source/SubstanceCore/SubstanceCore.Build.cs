// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

namespace UnrealBuildTool.Rules
{
    public class SubstanceCore : ModuleRules
    {
        public SubstanceCore(TargetInfo Target)
        {
            // internal defines
            Definitions.Add("WITH_SUBSTANCE=1");
            Definitions.Add("SUBSTANCE_PLATFORM_BLEND=1");

            if (Target.Platform == UnrealTargetPlatform.Win32 ||
                Target.Platform == UnrealTargetPlatform.Win64 ||
                Target.Platform == UnrealTargetPlatform.XboxOne)
            {
                Definitions.Add("AIR_USE_WIN32_SYNC=1");
            }
            else if (Target.Platform == UnrealTargetPlatform.Mac ||
                     Target.Platform == UnrealTargetPlatform.PS4 ||
                     Target.Platform == UnrealTargetPlatform.Linux)
            {
                Definitions.Add("AIR_USE_PTHREAD_SYNC=1");
            }

            PrivateIncludePaths.Add("SubstanceCore/Private");

            PrivateDependencyModuleNames.AddRange(new string[] {
                "Projects",
                "Slate",
                "SlateCore",
        });

            PublicDependencyModuleNames.AddRange(new string[] {
                    "AssetRegistry",
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI",
                    "ImageWrapper",
                    "SessionServices",
                    "RHI"
                });

            if (UEBuildConfiguration.bBuildEditor == true)
            {
                PublicDependencyModuleNames.AddRange(new string[] {
                    "UnrealEd",
                    "AssetTools",
                    "ContentBrowser",
                    "Settings",
                    "TargetPlatform"
                });
            }

            if (Target.Type == TargetRules.TargetType.Editor)
            {
                //we load the engines dynamically at runtime for the Editor
                Definitions.Add("SUBSTANCE_LIB_DYNAMIC=1");
            }
            else
            {
                string engineSuffix = GetEngineSuffix();

                //determine the root directory
                string SubstanceLibPath = ModuleDirectory + "/../../Libs/";

                //include static lib
                if (Target.Platform == UnrealTargetPlatform.Linux)
                {
                    SubstanceLibPath += "Linux/";
                    PublicAdditionalLibraries.Add(SubstanceLibPath + "libSubstance" + engineSuffix + ".a");

                    if (engineSuffix == "GPU")
                    {
                        PublicAdditionalLibraries.AddRange(new string[] { "X11", "glut", "GLU", "GL" });
                    }
                    RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/Substance/Binaries/Linux/libSubstanceCPU.so"));
                    RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/Substance/Binaries/Linux/libSubstanceGPU.so"));
                }
                else if (Target.Platform == UnrealTargetPlatform.Mac)
                {
                    SubstanceLibPath += "Mac/";
                    PublicAdditionalLibraries.Add(SubstanceLibPath + "libSubstance" + engineSuffix + ".a");
                    RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/Substance/Binaries/Mac/libSubstanceCPU.dylib"));
                    RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/Substance/Binaries/Mac/libSubstanceGPU.dylib"));
                }
				else if (Target.Platform == UnrealTargetPlatform.PS4)
				{
					SubstanceLibPath += "PS4/";
					PublicAdditionalLibraries.Add(SubstanceLibPath + "SubstanceCPU.a");
                }
                else if (Target.Platform == UnrealTargetPlatform.Win32)
                {
                    SubstanceLibPath += "Win32" + GetWindowsLibSuffix() + "/";
                    PublicAdditionalLibraries.Add(SubstanceLibPath + "Substance" + engineSuffix + ".lib");
                }
                else if (Target.Platform == UnrealTargetPlatform.Win64)
                {
                    SubstanceLibPath += "Win64" + GetWindowsLibSuffix() + "/";
                    PublicAdditionalLibraries.Add(SubstanceLibPath + "Substance" + engineSuffix + ".lib");
                    RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/Substance/Binaries/Win64/SubstanceCPU.dll"));
                    RuntimeDependencies.Add(new RuntimeDependency("../../UnrealTournament/Plugins/Substance/Binaries/Win64/SubstanceGPU.dll"));
                }
				else if (Target.Platform == UnrealTargetPlatform.XboxOne)
				{
					SubstanceLibPath += "XboxOne/";
					PublicAdditionalLibraries.Add(SubstanceLibPath + "SubstanceCPU.lib");
				}
                else
                {
                    throw new BuildException("Platform not supported by Substance.");
                }
            }
        }

		internal string GetWindowsLibSuffix()
		{
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
			{
				return "_2013";
			}
			else
			{
				return "";
			}
		}

        internal string GetEngineSuffix()
        {
            //grab first project listed in order to resolve ini path
            string iniPath = "";
            List<UProjectInfo> projects = UProjectInfo.FilterGameProjects(false, null);
            foreach (UProjectInfo upi in projects)
            {
                iniPath = upi.Folder.FullName;
                break;
            }

            //no INI, default to CPU
            if (iniPath.Length == 0)
                return "CPU";

            //We can't use the ConfigCacheIni file here because it bypasses the Substance section.
			try
			{
				iniPath = Path.Combine(iniPath,Path.Combine("Config","DefaultEngine.ini"));

				//parse INI file line by line until we find our result
				StreamReader file = new StreamReader(iniPath);
				string line;
				string suffix = "CPU";
				string startsWith = "SubstanceEngine=";
				while ((line = file.ReadLine()) != null)
				{
					if (line.StartsWith(startsWith))
					{
						string value = line.Substring(startsWith.Length);
						
						if (value == "SET_CPU")
							suffix = "CPU";
						else if (value == "SET_GPU")
							suffix = "GPU";

						break;
					}
				}
				
				return suffix;
			}
			catch(Exception)
			{
				return "CPU";
			}
        }
    }
}
