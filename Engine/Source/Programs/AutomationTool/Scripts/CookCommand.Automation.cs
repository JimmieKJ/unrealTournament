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
/// Helper command used for cooking.
/// </summary>
/// <remarks>
/// Command line parameters used by this command:
/// -clean
/// </remarks>
public partial class Project : CommandUtils
{
	#region Cook Command

	public static void Cook(ProjectParams Params)
	{
		if ((!Params.Cook && !(Params.CookOnTheFly && !Params.SkipServer)) || Params.SkipCook)
		{
			return;
		}
		Params.ValidateAndLog();

		Log("********** COOK COMMAND STARTED **********");

		string UE4EditorExe = HostPlatform.Current.GetUE4ExePath(Params.UE4Exe);
		if (!FileExists(UE4EditorExe))
		{
			throw new AutomationException("Missing " + UE4EditorExe + " executable. Needs to be built first.");
		}

		if (Params.CookOnTheFly && !Params.SkipServer)
		{
            if (Params.HasDLCName)
            {
                throw new AutomationException("Cook on the fly doesn't support cooking dlc");
            }
			if (Params.ClientTargetPlatforms.Count > 0)
			{
				var LogFolderOutsideOfSandbox = GetLogFolderOutsideOfSandbox();
				if (!GlobalCommandLine.Installed)
				{
					// In the installed runs, this is the same folder as CmdEnv.LogFolder so delete only in not-installed
					DeleteDirectory(LogFolderOutsideOfSandbox);
					CreateDirectory(LogFolderOutsideOfSandbox);
				}
				var ServerLogFile = CombinePaths(LogFolderOutsideOfSandbox, "Server.log");
				Platform ClientPlatformInst = Params.ClientTargetPlatformInstances[0];
				string TargetCook = ClientPlatformInst.GetCookPlatform(false, Params.HasDedicatedServerAndClient, Params.CookFlavor);
				ServerProcess = RunCookOnTheFlyServer(Params.RawProjectPath, Params.NoClient ? "" : ServerLogFile, TargetCook, Params.RunCommandline);

				if (ServerProcess != null)
				{
					Log("Waiting a few seconds for the server to start...");
					Thread.Sleep(5000);
				}
			}
			else
			{
				throw new AutomationException("Failed to run, client target platform not specified");
			}
		}
		else
		{
			var PlatformsToCook = new HashSet<string>();

			if (!Params.NoClient)
			{
				foreach (var ClientPlatform in Params.ClientTargetPlatforms)
				{
					// Use the data platform, sometimes we will copy another platform's data
					var DataPlatform = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
					PlatformsToCook.Add(Params.GetTargetPlatformInstance(DataPlatform).GetCookPlatform(false, Params.HasDedicatedServerAndClient, Params.CookFlavor));
				}
			}
			if (Params.DedicatedServer)
			{
				foreach (var ServerPlatform in Params.ServerTargetPlatforms)
				{
					// Use the data platform, sometimes we will copy another platform's data
					var DataPlatform = Params.GetCookedDataPlatformForServerTarget(ServerPlatform);
					PlatformsToCook.Add(Params.GetTargetPlatformInstance(DataPlatform).GetCookPlatform(true, false, Params.CookFlavor));
				}
			}

			if (Params.Clean.HasValue && Params.Clean.Value && !Params.IterativeCooking)
			{
				Log("Cleaning cooked data.");
				CleanupCookedData(PlatformsToCook.ToList(), Params);
			}

			// cook the set of maps, or the run map, or nothing
			string[] Maps = null;
			if (Params.HasMapsToCook)
			{
				Maps = Params.MapsToCook.ToArray();
                foreach (var M in Maps)
                {
                    Log("HasMapsToCook " + M.ToString());
                }
                foreach (var M in Params.MapsToCook)
                {
                    Log("Params.HasMapsToCook " + M.ToString());
                }
			}

			string[] Dirs = null;
			if (Params.HasDirectoriesToCook)
			{
				Dirs = Params.DirectoriesToCook.ToArray();
			}

            string InternationalizationPreset = null;
            if (Params.HasInternationalizationPreset)
            {
                InternationalizationPreset = Params.InternationalizationPreset;
            }

			string[] Cultures = null;
			if (Params.HasCulturesToCook)
			{
				Cultures = Params.CulturesToCook.ToArray();
			}

            try
            {
                var CommandletParams = "-buildmachine -fileopenlog";
                if (Params.UnversionedCookedContent)
                {
                    CommandletParams += " -unversioned";
                }
                if (Params.UseDebugParamForEditorExe)
                {
                    CommandletParams += " -debug";
                }
                if (Params.Manifests)
                {
                    CommandletParams += " -manifests";
                }
                if (Params.IterativeCooking)
                {
                    CommandletParams += " -iterate";
                }
                if (Params.CookMapsOnly)
                {
                    CommandletParams += " -mapsonly";
                }
                if (Params.NewCook)
                {
                    CommandletParams += " -newcook";
                }
                if (Params.OldCook)
                {
                    CommandletParams += " -oldcook";
                }
                if (Params.CookAll)
                {
                    CommandletParams += " -cookall";
                }
                if (Params.CookMapsOnly)
                {
                    CommandletParams += " -mapsonly";
                }
                if (Params.HasCreateReleaseVersion)
                {
                    CommandletParams += " -createreleaseversion=" + Params.CreateReleaseVersion;
                }
                if (Params.HasDLCName)
                {
                    CommandletParams += " -dlcname=" + Params.DLCName;
                    if ( !Params.DLCIncludeEngineContent )
                    {
                        CommandletParams += " -errorOnEngineContentUse";
                    }
                }
                // don't include the based on release version unless we are cooking dlc or creating a new release version
                // in this case the based on release version is used in packaging
                if (Params.HasBasedOnReleaseVersion && (Params.HasDLCName || Params.HasCreateReleaseVersion))
                {
                    CommandletParams += " -basedonreleaseversion=" + Params.BasedOnReleaseVersion;
                }
                // if we are not going to pak but we specified compressed then compress in the cooker ;)
                // otherwise compress the pak files
                if (!Params.Pak && !Params.SkipPak && Params.Compressed)
                {
                    CommandletParams += " -compressed";
                }
                if (Params.HasAdditionalCookerOptions)
                {
                    string FormatedAdditionalCookerParams = Params.AdditionalCookerOptions.TrimStart(new char[] { '\"', ' ' }).TrimEnd(new char[] { '\"', ' ' });
                    CommandletParams += " ";
                    CommandletParams += FormatedAdditionalCookerParams;
                }

                if (!Params.NoClient)
                {
                    var MapsList = Maps == null ? new List<string>() :  Maps.ToList(); 
                    foreach (var ClientPlatform in Params.ClientTargetPlatforms)
                    {
                        var DataPlatform = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
                        CommandletParams += (Params.GetTargetPlatformInstance(DataPlatform).GetCookExtraCommandLine(Params));
                        MapsList.AddRange((Params.GetTargetPlatformInstance(ClientPlatform).GetCookExtraMaps()));
                    }
                    Maps = MapsList.ToArray();
                }

                CookCommandlet(Params.RawProjectPath, Params.UE4Exe, Maps, Dirs, InternationalizationPreset, Cultures, CombineCommandletParams(PlatformsToCook.ToArray()), CommandletParams);
            }
			catch (Exception Ex)
			{
				// Delete cooked data (if any) as it may be incomplete / corrupted.
				Log("Cook failed. Deleting cooked data.");
				CleanupCookedData(PlatformsToCook.ToList(), Params);
				AutomationTool.ErrorReporter.Error("Cook failed.", (int)AutomationTool.ErrorCodes.Error_UnknownCookFailure);
				throw Ex;
			}
		}

		Log("********** COOK COMMAND COMPLETED **********");
	}

	private static void CleanupCookedData(List<string> PlatformsToCook, ProjectParams Params)
	{
		var ProjectPath = Path.GetFullPath(Params.RawProjectPath);
		var CookedSandboxesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Cooked");
		var CleanDirs = new string[PlatformsToCook.Count];
		for (int DirIndex = 0; DirIndex < CleanDirs.Length; ++DirIndex)
		{
			CleanDirs[DirIndex] = CombinePaths(CookedSandboxesPath, PlatformsToCook[DirIndex]);
		}

		const bool bQuiet = true;
		DeleteDirectory(bQuiet, CleanDirs);
	}

	#endregion
}
