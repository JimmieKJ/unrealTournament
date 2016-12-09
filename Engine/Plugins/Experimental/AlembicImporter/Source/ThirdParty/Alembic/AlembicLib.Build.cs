// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Diagnostics;

public class AlembicLib : ModuleRules
{
    public AlembicLib(TargetInfo Target)
    {
        Type = ModuleType.External;
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
        {
            bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT);

            string LibDir = ModuleDirectory + "/Deploy/lib/";
            string Platform;
            bool bAllowDynamicLibs = true;
            switch (Target.Platform)
            {
                case UnrealTargetPlatform.Win64:
                    Platform = "x64";
                    LibDir += "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
                    break;
                case UnrealTargetPlatform.Win32:
                    Platform = "Win32";
                    LibDir += "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
                    break;
                case UnrealTargetPlatform.Mac:
                    Platform = "Mac";
                    bAllowDynamicLibs = false;
                    break;
                default:
                    return;
            }
            LibDir = LibDir + "/" + Platform;
            LibDir = LibDir + (BuildConfiguration.bDebugBuildsActuallyUseDebugCRT && bAllowDynamicLibs ? "/Dynamic" : "/Static") + (bDebug ? "Debug" : "Release");
            PublicLibraryPaths.Add(LibDir);
                        
            if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						"Half.lib",
						"Iex.lib",
						"IlmThread.lib",                        
						"Imath.lib",
						"hdf5.lib",
						"hdf5_hl.lib",						
						"AlembicAbc.lib",
						"AlembicAbcCollection.lib",
						"AlembicAbcCoreAbstract.lib",
						"AlembicAbcCoreFactory.lib",
						"AlembicAbcCoreHDF5.lib",
						"AlembicAbcCoreOgawa.lib",
						"AlembicAbcGeom.lib",
						"AlembicAbcMaterial.lib",
						"AlembicOgawa.lib",
						"AlembicUtil.lib",
                        "zlib_64.lib"
					}
				);

				if (BuildConfiguration.bDebugBuildsActuallyUseDebugCRT && bDebug)
				{                
					RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Plugins/Experimental/AlembicImporter/Binaries/ThirdParty/zlib/zlibd1.dll"));
				}
			}
            else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibDir + "/libilmbase.a",
						LibDir + "/libhdf5.a",
						LibDir + "/libhdf5_hl.a",
						LibDir + "/libAlembicAbc.a",
						LibDir + "/libAlembicAbcCollection.a",
						LibDir + "/libAlembicAbcCoreAbstract.a",
						LibDir + "/libAlembicAbcCoreFactory.a",
						LibDir + "/libAlembicAbcCoreHDF5.a",
						LibDir + "/libAlembicAbcCoreOgawa.a",
						LibDir + "/libAlembicAbcGeom.a",
						LibDir + "/libAlembicAbcMaterial.a",
						LibDir + "/libAlembicOgawa.a",
						LibDir + "/libAlembicUtil.a",
					}
				);
			}

            PublicIncludePaths.Add( ModuleDirectory + "/Deploy/include/" );
        }
    }
}

