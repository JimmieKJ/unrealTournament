// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class PhysXVehicleLib : ModuleRules
{
    // PhysXLibraryMode, GetPhysXLibraryMode and GetPhysXLibrarySuffix duplicated from PhysX.Build.cs

    enum PhysXLibraryMode
    {
        Debug,
        Profile,
        Checked,
        Shipping
    }

    static PhysXLibraryMode GetPhysXLibraryMode(UnrealTargetConfiguration Config)
    {
        switch (Config)
        {
            case UnrealTargetConfiguration.Debug:
                if (BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
                {
                    return PhysXLibraryMode.Debug;
                }
                else
                {
                    return PhysXLibraryMode.Checked;
                }
            case UnrealTargetConfiguration.Shipping:
            case UnrealTargetConfiguration.Test:
                return PhysXLibraryMode.Shipping;
            case UnrealTargetConfiguration.Development:
            case UnrealTargetConfiguration.DebugGame:
            case UnrealTargetConfiguration.Unknown:
            default:
                if (BuildConfiguration.bUseShippingPhysXLibraries)
                {
                    return PhysXLibraryMode.Shipping;
                }
                else if (BuildConfiguration.bUseCheckedPhysXLibraries)
                {
                    return PhysXLibraryMode.Checked;
                }
                else
                {
                    return PhysXLibraryMode.Profile;
                }
        }
    }

    static string GetPhysXLibrarySuffix(PhysXLibraryMode Mode)
    {
        switch (Mode)
        {
            case PhysXLibraryMode.Debug:
                return "DEBUG";
            case PhysXLibraryMode.Checked:
                return "CHECKED";
            case PhysXLibraryMode.Profile:
                return "PROFILE";
            case PhysXLibraryMode.Shipping:
            default:
                return "";
        }
    }

    public PhysXVehicleLib(TargetInfo Target)
    {
        Type = ModuleType.External;

        // Determine which kind of libraries to link against
        PhysXLibraryMode LibraryMode = GetPhysXLibraryMode(Target.Configuration);
        string LibrarySuffix = GetPhysXLibrarySuffix(LibraryMode);

        string PhysXLibDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/Lib/";

        // Libraries and DLLs for windows platform
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());

            PublicAdditionalLibraries.Add(String.Format("PhysX3Vehicle{0}_x64.lib", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.Win32 || (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
        {
            PublicLibraryPaths.Add(PhysXLibDir + "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());

            PublicAdditionalLibraries.Add(String.Format("PhysX3Vehicle{0}_x86.lib", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "Mac");

            PublicAdditionalLibraries.Add(String.Format(PhysXLibDir + "Mac/libPhysX3Vehicle{0}.a", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "Android/ARMv7");
            PublicLibraryPaths.Add(PhysXLibDir + "Android/x86");
            PublicLibraryPaths.Add(PhysXLibDir + "Android/ARM64");
            PublicLibraryPaths.Add(PhysXLibDir + "Android/x64");

            PublicAdditionalLibraries.Add(String.Format("PhysX3Vehicle{0}", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "Linux/" + Target.Architecture);

            PublicAdditionalLibraries.Add(String.Format("PhysX3Vehicle{0}", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "IOS");

            PublicAdditionalLibraries.Add("PhysX3Vehicle" + LibrarySuffix);
        }
        else if (Target.Platform == UnrealTargetPlatform.TVOS)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "TVOS");

            PublicAdditionalLibraries.Add("PhysX3Vehicle" + LibrarySuffix);
        }
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            PublicAdditionalLibraries.Add(PhysXLibDir + "HTML5/PhysX3Vehicle" + (UEBuildConfiguration.bCompileForSize ? "_Oz" : "") + ".bc");
        }
        else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "PS4");

            PublicAdditionalLibraries.Add(String.Format("PhysX3Vehicle{0}", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            PublicLibraryPaths.Add(Path.Combine(PhysXLibDir, "XboxOne\\VS" + WindowsPlatform.GetVisualStudioCompilerVersionName()));

            PublicAdditionalLibraries.Add(String.Format("PhysX3Vehicle{0}.lib", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.Switch)
        {
            PublicLibraryPaths.Add(PhysXLibDir + "Switch");

            PublicAdditionalLibraries.Add("PhysX3Vehicle" + LibrarySuffix);
        }
    }
}
