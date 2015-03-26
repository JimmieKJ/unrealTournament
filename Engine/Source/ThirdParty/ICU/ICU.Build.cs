// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
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

		string ICURootPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "ICU/icu4c-53_1/";

		// Includes
		PublicSystemIncludePaths.Add(ICURootPath + "include" + "/");

		string PlatformFolderName = Target.Platform.ToString();

        EICULinkType ICULinkType;
        switch (Target.Type)
        {
            case TargetRules.TargetType.Client:
                ICULinkType = EICULinkType.Static;
                break;
            case TargetRules.TargetType.Game:
            case TargetRules.TargetType.Server:
            case TargetRules.TargetType.Editor:
            case TargetRules.TargetType.Program:
                ICULinkType = EICULinkType.Dynamic;
                break;
            default:
                ICULinkType = EICULinkType.None;
                break;
        }

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
                    foreach (string Stem in LibraryNameStems)
                    {
                        if (Target.Platform == UnrealTargetPlatform.Linux)
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

		ICULinkType = (Target.Platform == UnrealTargetPlatform.IOS || Target.IsMonolithic || Target.Type == TargetRules.TargetType.Game) ? EICULinkType.Static : EICULinkType.Dynamic;
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

		// common defines
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
            (Target.Platform == UnrealTargetPlatform.Win32) ||
            (Target.Platform == UnrealTargetPlatform.Linux) ||
            (Target.Platform == UnrealTargetPlatform.Android) ||
            (Target.Platform == UnrealTargetPlatform.Mac) ||
			(Target.Platform == UnrealTargetPlatform.IOS) ||
			(Target.Platform == UnrealTargetPlatform.PS4) ||
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
	}
}
