// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class ICU : ModuleRules
{
    enum EICULinkType
    {
        None,
        Static,
        Dynamic
    }

	public ICU(TargetInfo Target)
	{
		Type = ModuleType.External;

		string ICUVersion = "icu4c-53_1";
		string ICURootPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "ICU/" + ICUVersion + "/";

		// Includes
		PublicSystemIncludePaths.Add(ICURootPath + "include" + "/");

		string PlatformFolderName = Target.Platform.ToString();

        string TargetSpecificPath = ICURootPath + PlatformFolderName + "/";
        if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
            TargetSpecificPath = ICURootPath + "Win32/";
        }

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32) || 
            (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			string VSVersionFolderName = "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			TargetSpecificPath += VSVersionFolderName + "/";

			string[] LibraryNameStems =
			{
				"dt",   // Data
				"uc",   // Unicode Common
				"in",   // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;

            // Library Paths
            PublicLibraryPaths.Add(TargetSpecificPath + "lib" + "/");

			EICULinkType ICULinkType = Target.IsMonolithic ? EICULinkType.Static : EICULinkType.Dynamic;
            switch(ICULinkType)
            {
            case EICULinkType.Static:
			    foreach (string Stem in LibraryNameStems)
			    {
				    string LibraryName = "sicu" + Stem + LibraryNamePostfix + "." + "lib";
				    PublicAdditionalLibraries.Add(LibraryName);
			    }
                break;
            case EICULinkType.Dynamic:
				foreach (string Stem in LibraryNameStems)
				{
					string LibraryName = "icu" + Stem + LibraryNamePostfix + "." + "lib";
					PublicAdditionalLibraries.Add(LibraryName);
				}

                foreach (string Stem in LibraryNameStems)
                {
                    string LibraryName = "icu" + Stem + LibraryNamePostfix + "53" + "." + "dll";
                    PublicDelayLoadDLLs.Add(LibraryName);
                }

				if(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
				{
					string BinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/ICU/{0}/{1}/VS{2}/", ICUVersion, Target.Platform.ToString(), WindowsPlatform.GetVisualStudioCompilerVersionName());
					foreach(string Stem in LibraryNameStems)
					{
						string LibraryName = BinariesDir + String.Format("icu{0}{1}53.dll", Stem, LibraryNamePostfix);
						RuntimeDependencies.Add(new RuntimeDependency(LibraryName));
					}
				}

                break;
            }
		}
        else if	(Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Android)
        {
            string StaticLibraryExtension = "a";

            switch (Target.Platform)
            {
	            case UnrealTargetPlatform.Linux:
		            TargetSpecificPath += Target.Architecture + "/";
			        break;
				case UnrealTargetPlatform.Android:
					PublicLibraryPaths.Add(TargetSpecificPath + "ARMv7/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "ARM64/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "x86/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "x64/lib");
					break;
			}

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
            string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ?
                "d" : string.Empty;

            // Library Paths
			EICULinkType ICULinkType = (Target.Platform == UnrealTargetPlatform.Android || Target.IsMonolithic) ? EICULinkType.Static : EICULinkType.Dynamic;
            switch (ICULinkType)
            {
                case EICULinkType.Static:
                    foreach (string Stem in LibraryNameStems)
                    {
                        string LibraryName = "icu" + Stem + LibraryNamePostfix;
                        if (Target.Platform == UnrealTargetPlatform.Android)
                        {
							// we will filter out in the toolchain
							PublicAdditionalLibraries.Add(LibraryName); // Android requires only the filename.
                        }
                        else
                        {
                            PublicAdditionalLibraries.Add(TargetSpecificPath + "lib/" + "lib" + LibraryName + "." + StaticLibraryExtension); // Linux seems to need the path, not just the filename.
                        }
                    }
                    break;
                case EICULinkType.Dynamic:
                    if (Target.Platform == UnrealTargetPlatform.Linux)
                    {
                        string PathToBinary = String.Format("$(EngineDir)/Binaries/ThirdParty/ICU/{0}/{1}/{2}/", ICUVersion, Target.Platform.ToString(), 
                            Target.Architecture);
                            
                        foreach (string Stem in LibraryNameStems)
                        {
                            string LibraryName = "icu" + Stem + LibraryNamePostfix;
                            string LibraryPath = UEBuildConfiguration.UEThirdPartyBinariesDirectory + "ICU/icu4c-53_1/Linux/" + Target.Architecture + "/";

                            PublicLibraryPaths.Add(LibraryPath);
                            PublicAdditionalLibraries.Add(LibraryName);
                            
                            // add runtime dependencies (for staging)
                            RuntimeDependencies.Add(new RuntimeDependency(PathToBinary + "lib" + LibraryName + ".so"));
                            RuntimeDependencies.Add(new RuntimeDependency(PathToBinary + "lib" + LibraryName + ".so.53"));  // version-dependent
                        }
                    }
                    break;
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
            string StaticLibraryExtension = "a";
            string DynamicLibraryExtension = "dylib";

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
            string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ?
                "d" : string.Empty;

			EICULinkType ICULinkType = (Target.Platform == UnrealTargetPlatform.IOS || Target.IsMonolithic) ? EICULinkType.Static : EICULinkType.Dynamic;
            // Library Paths
            switch (ICULinkType)
            {
                case EICULinkType.Static:
                    foreach (string Stem in LibraryNameStems)
                    {
                        string LibraryName = "libicu" + Stem + LibraryNamePostfix + "." + StaticLibraryExtension;
                        PublicAdditionalLibraries.Add(TargetSpecificPath + "lib/" + LibraryName);
						if (Target.Platform == UnrealTargetPlatform.IOS)
						{
							PublicAdditionalShadowFiles.Add(TargetSpecificPath + "lib/" + LibraryName);
						}
                    }
                    break;
                case EICULinkType.Dynamic:
                    foreach (string Stem in LibraryNameStems)
                    {
                        if (Target.Platform == UnrealTargetPlatform.Mac)
                        {
                            string LibraryName = "libicu" + Stem + ".53.1" + LibraryNamePostfix + "." + DynamicLibraryExtension;
                            string LibraryPath = UEBuildConfiguration.UEThirdPartyBinariesDirectory + "ICU/icu4c-53_1/Mac/" + LibraryName;

                            PublicDelayLoadDLLs.Add(LibraryPath);
                            PublicAdditionalShadowFiles.Add(LibraryPath);
                        }
                        else if (Target.Platform == UnrealTargetPlatform.Linux)
                        {
                            string LibraryName = "icu" + Stem + LibraryNamePostfix;
                            string LibraryPath = UEBuildConfiguration.UEThirdPartyBinariesDirectory + "ICU/icu4c-53_1/Linux/" + Target.Architecture + "/";

                            PublicLibraryPaths.Add(LibraryPath);
                            PublicAdditionalLibraries.Add(LibraryName);
                        }
                    }
                    break;
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            // we don't bother with debug libraries on HTML5. Mainly because debugging isn't viable on html5 currently
            string StaticLibraryExtension = "bc";

            string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};

            foreach (string Stem in LibraryNameStems)
            {
                string LibraryName = "libicu" + Stem + "." + StaticLibraryExtension;
                PublicAdditionalLibraries.Add(TargetSpecificPath + LibraryName);
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			string LibraryNamePrefix = "sicu";
			string[] LibraryNameStems =
			{
				"dt",	// Data
				"uc",   // Unicode Common
				"in",	// Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug) ?
				"d" : string.Empty;
			string LibraryExtension = "lib";
			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = ICURootPath + "PS4/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
				PublicAdditionalLibraries.Add(LibraryName);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			string LibraryNamePrefix = "sicu";
			string[] LibraryNameStems =
			{
				"dt",	// Data
				"uc",   // Unicode Common
				"in",	// Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
            string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;
			string LibraryExtension = "lib";
			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = ICURootPath + "XboxOne/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
				PublicAdditionalLibraries.Add(LibraryName);
			}
		}

		// common defines
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
            (Target.Platform == UnrealTargetPlatform.Win32) ||
            (Target.Platform == UnrealTargetPlatform.Linux) ||
            (Target.Platform == UnrealTargetPlatform.Android) ||
            (Target.Platform == UnrealTargetPlatform.Mac) ||
			(Target.Platform == UnrealTargetPlatform.IOS) ||
			(Target.Platform == UnrealTargetPlatform.PS4) ||
            (Target.Platform == UnrealTargetPlatform.XboxOne) ||
            (Target.Platform == UnrealTargetPlatform.HTML5))
		{
			// Definitions
			Definitions.Add("U_USING_ICU_NAMESPACE=0"); // Disables a using declaration for namespace "icu".
			Definitions.Add("U_STATIC_IMPLEMENTATION"); // Necessary for linking to ICU statically.
			Definitions.Add("U_NO_DEFAULT_INCLUDE_UTF_HEADERS=1"); // Disables unnecessary inclusion of headers - inclusions are for ease of use.
			Definitions.Add("UNISTR_FROM_CHAR_EXPLICIT=explicit"); // Makes UnicodeString constructors for ICU character types explicit.
			Definitions.Add("UNISTR_FROM_STRING_EXPLICIT=explicit"); // Makes UnicodeString constructors for "char"/ICU string types explicit.
            Definitions.Add("UCONFIG_NO_TRANSLITERATION=1"); // Disables declarations and compilation of unused ICU transliteration functionality.
		}
		
		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			// Definitions			
            Definitions.Add("ICU_NO_USER_DATA_OVERRIDE=1");
            Definitions.Add("U_PLATFORM=U_PF_ORBIS");
		}

        if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Definitions			
            Definitions.Add("ICU_NO_USER_DATA_OVERRIDE=1");
            Definitions.Add("U_PLATFORM=U_PF_DURANGO");
		}
	}
}
