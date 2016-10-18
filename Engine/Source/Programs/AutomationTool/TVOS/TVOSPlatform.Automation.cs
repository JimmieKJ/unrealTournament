// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;

public class TVOSPlatform : IOSPlatform
{
	public TVOSPlatform()
		:base(UnrealTargetPlatform.TVOS)
	{
		TargetIniPlatformType = UnrealTargetPlatform.IOS;
	}

	public override UnrealBuildTool.UEDeployIOS GetDeployHandler(FileReference InProject)
	{
		Console.WriteLine("Getting TVOS Deploy()");
		return new UnrealBuildTool.UEDeployTVOS(InProject);
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return "TVOS";
	}

    public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
    {
        //		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
        {
            // copy the icons/launch screens from the engine
            {
                string SourcePath = CombinePaths(SC.LocalRoot, "Engine", "Binaries", "TVOS");
                SC.StageFiles(StagedFileType.NonUFS, SourcePath, "Assets.car", false, null, "", true, false);
            }

            // copy any additional framework assets that will be needed at runtime
            {
                string SourcePath = CombinePaths((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "\\Engine"), "Intermediate", "TVOS", "FrameworkAssets");
                if (Directory.Exists(SourcePath))
                {
                    SC.StageFiles(StagedFileType.NonUFS, SourcePath, "*.*", true, null, "", true, false);
                }
            }

            // copy the icons/launch screens from the game (may stomp the engine copies)
            {
                string SourcePath = CombinePaths(SC.ProjectRoot, "Binaries", "TVOS");
                SC.StageFiles(StagedFileType.NonUFS, SourcePath, "Assets.car", false, null, "", true, false);
            }

            // copy the plist (only if code signing, as it's protected by the code sign blob in the executable and can't be modified independently)
            if (GetCodeSignDesirability(Params))
            {
                string SourcePath = CombinePaths((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "/Engine"), "Intermediate", "TVOS");
                string TargetPListFile = Path.Combine(SourcePath, (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + "-Info.plist");
                //				if (!File.Exists(TargetPListFile))
                {
                    // ensure the plist, entitlements, and provision files are properly copied
                    Console.WriteLine("CookPlat {0}, this {1}", GetCookPlatform(false, false), ToString());
                    if (!SC.IsCodeBasedProject)
                    {
                        UnrealBuildTool.UnrealBuildTool.SetRemoteIniPath(SC.ProjectRoot);
                    }
					GetDeployHandler(new FileReference(SC.ProjectRoot)).GeneratePList((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "/Engine"), !SC.IsCodeBasedProject, (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game"), SC.ShortProjectName, SC.LocalRoot + "/Engine", (SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "/Engine") + "/Binaries/TVOS/Payload/" + (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + ".app");
                }

                SC.StageFiles(StagedFileType.NonUFS, SourcePath, Path.GetFileName(TargetPListFile), false, null, "", false, false, "Info.plist");
            }
        }

        // copy the movies from the project
        {
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Build/TVOS/Resources/Movies"), "*", false, null, "", true, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Movies"), "*", true, null, "", true, false);
        }
    }
}
