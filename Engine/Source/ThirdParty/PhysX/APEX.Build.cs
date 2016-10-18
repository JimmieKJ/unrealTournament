// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.Collections.Generic;

public class APEX : ModuleRules
{
	enum APEXLibraryMode
	{
		Debug,
		Profile,
		Checked,
		Shipping
	}

	static APEXLibraryMode GetAPEXLibraryMode(UnrealTargetConfiguration Config)
	{
		switch (Config)
		{
			case UnrealTargetConfiguration.Debug:
				if( BuildConfiguration.bDebugBuildsActuallyUseDebugCRT )
				{ 
					return APEXLibraryMode.Debug;
				}
				else if(BuildConfiguration.bUseCheckedPhysXLibraries)
                {
   	                return APEXLibraryMode.Checked;
       	        }
				else
				{
                	return APEXLibraryMode.Profile;
				}
			case UnrealTargetConfiguration.Shipping:
			case UnrealTargetConfiguration.Test:
			case UnrealTargetConfiguration.Development:
			case UnrealTargetConfiguration.DebugGame:
			case UnrealTargetConfiguration.Unknown:
			default:
                if(BuildConfiguration.bUseShippingPhysXLibraries)
                {
                    return APEXLibraryMode.Shipping;
                }
                else if(BuildConfiguration.bUseCheckedPhysXLibraries)
                {
                    return APEXLibraryMode.Checked;
                }
                else
                {
                    return APEXLibraryMode.Profile;
                }
		}
	}

	static string GetAPEXLibrarySuffix(APEXLibraryMode Mode)
	{

		switch (Mode)
		{
			case APEXLibraryMode.Debug:
				return "DEBUG";
			case APEXLibraryMode.Checked:
				return "CHECKED";
			case APEXLibraryMode.Profile:
				return "PROFILE";
			default:
			case APEXLibraryMode.Shipping:
                return "";
		}
	}

	public APEX(TargetInfo Target)
	{
		Type = ModuleType.External;

		// Determine which kind of libraries to link against
		APEXLibraryMode LibraryMode = GetAPEXLibraryMode(Target.Configuration);

		// Quick Mac hack
		if (Target.Platform == UnrealTargetPlatform.Mac && (LibraryMode == APEXLibraryMode.Checked || LibraryMode == APEXLibraryMode.Shipping))
		{
			LibraryMode = APEXLibraryMode.Profile;
		}

		string LibrarySuffix = GetAPEXLibrarySuffix(LibraryMode);

		Definitions.Add("WITH_APEX=1");

		string APEXDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/APEX-1.3/";

		string APEXLibDir = APEXDir + "lib";

		PublicSystemIncludePaths.AddRange(
			new string[] {
				APEXDir + "public",
				APEXDir + "framework/public",
				APEXDir + "framework/public/PhysX3",
				APEXDir + "module/destructible/public",
				APEXDir + "module/clothing/public",
				APEXDir + "module/legacy/public",
				APEXDir + "NxParameterized/public",
			}
			);

		// List of default library names (unused unless LibraryFormatString is non-null)
		List<string> ApexLibraries = new List<string>();
		ApexLibraries.AddRange(
			new string[]
			{
				"ApexCommon{0}",
				"ApexFramework{0}",
				"ApexShared{0}",
				"APEX_Destructible{0}",
				"APEX_Clothing{0}",
			});
		string LibraryFormatString = null;

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			APEXLibDir += "/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(APEXLibDir);

			PublicAdditionalLibraries.Add(String.Format("APEXFramework{0}_x64.lib", LibrarySuffix));
			PublicDelayLoadDLLs.Add(String.Format("APEXFramework{0}_x64.dll", LibrarySuffix));

			string[] RuntimeDependenciesX64 =
			{
				"APEX_Clothing{0}_x64.dll",
				"APEX_Destructible{0}_x64.dll",
				"APEX_Legacy{0}_x64.dll",
				"ApexFramework{0}_x64.dll",
			};

			string ApexBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/APEX-1.3/Win64/VS{0}/", WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach(string RuntimeDependency in RuntimeDependenciesX64)
			{
				string FileName = ApexBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
				RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
				RuntimeDependencies.Add(FileName + ".pdb", StagedFileType.DebugNonUFS);
			}
            if(LibrarySuffix != "")
            {
                Definitions.Add("UE_APEX_SUFFIX=" + LibrarySuffix);
            }
			
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			APEXLibDir += "/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(APEXLibDir);

			PublicAdditionalLibraries.Add(String.Format("APEXFramework{0}_x86.lib", LibrarySuffix));
			PublicDelayLoadDLLs.Add(String.Format("APEXFramework{0}_x86.dll", LibrarySuffix));

			string[] RuntimeDependenciesX86 =
			{
				"APEX_Clothing{0}_x86.dll",
				"APEX_Destructible{0}_x86.dll",
				"APEX_Legacy{0}_x86.dll",
				"ApexFramework{0}_x86.dll",
			};

			string ApexBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/APEX-1.3/Win32/VS{0}/", WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach(string RuntimeDependency in RuntimeDependenciesX86)
			{
				string FileName = ApexBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
				RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
				RuntimeDependencies.Add(FileName + ".pdb", StagedFileType.DebugNonUFS);
			}
            if (LibrarySuffix != "")
            {
                Definitions.Add("UE_APEX_SUFFIX=" + LibrarySuffix);
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			APEXLibDir += "/osx64";
			Definitions.Add("APEX_STATICALLY_LINKED=1");

			ApexLibraries.Add("APEX_Legacy{0}");

			LibraryFormatString = APEXLibDir + "/lib{0}" + ".a";
		}
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            APEXLibDir += "/Linux/" + Target.Architecture;
            Definitions.Add("APEX_STATICALLY_LINKED=1");

            ApexLibraries.Add("APEX_Legacy{0}");

            LibraryFormatString = APEXLibDir + "/lib{0}" + ".a";
        }
        else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			Definitions.Add("APEX_STATICALLY_LINKED=1");
			Definitions.Add("WITH_APEX_LEGACY=0");

			APEXLibDir += "/PS4";
			PublicLibraryPaths.Add(APEXLibDir);

			LibraryFormatString = "{0}";
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				Definitions.Add("APEX_STATICALLY_LINKED=1");
				Definitions.Add("WITH_APEX_LEGACY=0");

				// This MUST be defined for XboxOne!
				Definitions.Add("PX_HAS_SECURE_STRCPY=1");

				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				APEXLibDir += "/XboxOne/VS" + VersionName.ToString();
				PublicLibraryPaths.Add(APEXLibDir);

				LibraryFormatString = "{0}.lib";
			}
		}

		// Add the libraries needed (used for all platforms except Windows)
		if (LibraryFormatString != null)
		{
			foreach (string Lib in ApexLibraries)
			{
				string ConfiguredLib = String.Format(Lib, LibrarySuffix);
				string FinalLib = String.Format(LibraryFormatString, ConfiguredLib);
				PublicAdditionalLibraries.Add(FinalLib);
			}
		}
	}
}
