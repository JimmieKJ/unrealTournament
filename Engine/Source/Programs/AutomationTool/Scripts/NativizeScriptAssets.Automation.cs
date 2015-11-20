// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;

/// <summary>
/// Helper command used for converting script assets (Blueprints) into native (C++) code (aiming to help boost performance of the final executable).
/// <remarks>
/// Command line parameters used by this command:
/// </remarks>
public partial class Project : CommandUtils
{
    #region NativizeScriptAssets Command

    public static void NativizeScriptAssets(BuildCommand Command, ProjectParams Params)
    {
        bool bRunNativizeCommand = Command.ParseParam("NativizeAssets");
        if (!bRunNativizeCommand)
        {
            return;
        }
        Params.ValidateAndLog();

        Log("********** NATIVIZE-ASSETS COMMAND STARTED **********");

        string UE4EditorExe = HostPlatform.Current.GetUE4ExePath(Params.UE4Exe);
        if (!FileExists(UE4EditorExe))
        {
            throw new AutomationException("Missing " + UE4EditorExe + " executable. Needs to be built first.");
        }

        string GeneratedPluginName = "NativizedScript";
        string ProjectDir = Params.RawProjectPath.Directory.ToString();
        string GeneratedPluginDirectory = Path.GetFullPath(CommandUtils.CombinePaths(ProjectDir, "Intermediate", "Plugins", GeneratedPluginName));
        string NativizationManifestPath = Path.GetFullPath(CommandUtils.CombinePaths(GeneratedPluginDirectory, GeneratedPluginName + "-Manifest"));

        // delete the product file, so that we can properly detect if the 
        // commandlet did its job
        DeleteFile(NativizationManifestPath);

        string CommandletParams = "-output=\"" + GeneratedPluginDirectory + "\" " + "-pluginName=" + GeneratedPluginName;
        GenerateNativePluginFromBlueprintCommandlet(Params.RawProjectPath, NativizationManifestPath, Params.UE4Exe, CommandletParams);

        string PluginDescFilename  = GeneratedPluginName + ".uplugin";
        string GeneratedPluginPath = Path.GetFullPath(CommandUtils.CombinePaths(GeneratedPluginDirectory, PluginDescFilename));

        Params.NativizedScriptPlugin = new FileReference(GeneratedPluginPath);
        if (!Params.UseNativizedScriptPlugin())
        {
            throw new AutomationException("Missing the nativized plugin file (" + GeneratedPluginPath + "). It seems the commandlet didn't generate it as expected.");
        }

        Log("********** NATIVIZE-ASSETS COMMAND COMPLETED **********");
    }

    #endregion // NativizeScriptAssets Command
}
