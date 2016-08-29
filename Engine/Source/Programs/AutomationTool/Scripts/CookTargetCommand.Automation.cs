// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;

[Help("Cooks a target")]
[Help("Project", "Specify the project with the targets to build.")]
[Help("Target", "Specify a list of target descriptor files for the targets to build, separated by '+' characters (eg. -Target=Game+Client+Server). Default is Game.")]
[Help("TargetPlatforms", "Specify a list of target platforms to build, separated by '+' characters (eg. -TargetPlatforms=Win32+Win64+IOS). Default is Win64.")]
class CookTarget : BuildCommand
{
	private ParamList<string> ParseParamList(string InArgument, string InDefault = null)
	{
		var ArgumentList = ParseParamValue(InArgument);
		if (ArgumentList != null)
		{
			return new ParamList<string>(ArgumentList.Split('+'));
		}
		else if (!String.IsNullOrEmpty(InDefault))
		{
			return new ParamList<string>(InDefault);
		}
		return null;
	}

	public override void ExecuteBuild()
	{
		// get the project
		var UProjectFileName = ParseParamValue("Project");
		if (UProjectFileName == null)
		{
			throw new AutomationException("Project was not specified via the -project argument.");
		}

		// get the list of platforms
		var PlatformList = ParseParamList("TargetPlatforms", "WindowsNoEditor");
		

		/*foreach (string Platform in PlatformList)
		{
			TargetPlatforms.Add((UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), Platform, true));
		}*/

		// get the list of maps

		// parse any extra parameters
		bool bCookOnTheFly = ParseParam("CookOnTheFly");
		string ExecutableName = ParseParamValue("ExeCmd", "UE4Editor.exe");

		// add the target platforms to the cook agenda
		string Targets = "";
        bool bFirstTarget = true;
		foreach ( var PlatformTarget in PlatformList)
        {
            if ( bFirstTarget)
            {
                Targets += PlatformTarget;
                bFirstTarget = false;
            }
            else
            {
                Targets += "+" + PlatformTarget;
            }
        }

		// set up the platform list

		// start the server
		if (bCookOnTheFly)
		{
			RunCookOnTheFlyServer(UProjectFileName, ExecutableName, Targets);
		}
		else
		{
			RunCookCommandlet(UProjectFileName, ExecutableName, Targets);
		}
	}

	private void RunCookCommandlet(string InUProjectFile, string InExecutable, string InTargets, string InArgs = "")
	{
		string Params = String.Format("-targetplatform={0} {1}", InTargets, InArgs);
        FileReference UProjectFile = new FileReference(InUProjectFile);
		RunCommandlet(UProjectFile, InExecutable, "cook", Params);
	}

	private void RunCookOnTheFlyServer(string InUProjectFile, string InExecutable, string InTargets, string InArgs = "")
	{
		var ServerApp = HostPlatform.Current.GetUE4ExePath(InExecutable);
		string Params = String.Format("\"{0}\" -run=cook -cookonthefly -targetplatform={1} {2}", InUProjectFile, InTargets, InArgs);
		Project.Run(ServerApp, Params, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);
	}

/*#region Cook Command

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

				String COTFCommandLine = Params.RunCommandline;
				if (Params.IterativeCooking)
				{
					COTFCommandLine += " -iterate";
				}
				if (Params.UseDebugParamForEditorExe)
				{
					COTFCommandLine += " -debug";
				}

				var ServerLogFile = CombinePaths(LogFolderOutsideOfSandbox, "Server.log");
				Platform ClientPlatformInst = Params.ClientTargetPlatformInstances[0];
				string TargetCook = ClientPlatformInst.GetCookPlatform(false, Params.HasDedicatedServerAndClient, Params.CookFlavor);
				ServerProcess = RunCookOnTheFlyServer(Params.RawProjectPath, Params.NoClient ? "" : ServerLogFile, TargetCook, COTFCommandLine);

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
					PlatformsToCook.Add(Params.GetTargetPlatformInstance(DataPlatform).GetCookPlatform(false, Params.Client, Params.CookFlavor));
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
                var CommandletParams = IsBuildMachine ? "-buildmachine -fileopenlog" : "-fileopenlog";
                if (Params.UnversionedCookedContent)
                {
                    CommandletParams += " -unversioned";
                }
				if (Params.FastCook)
				{
					CommandletParams += " -FastCook";
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
                if ( Params.SkipCookingEditorContent)
                {
                    CommandletParams += " -skipeditorcontent";
                }
                if ( Params.NumCookersToSpawn != 0)
                {
                    CommandletParams += " -numcookerstospawn=" + Params.NumCookersToSpawn;

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
				if (Params.IgnoreCookErrors)
				{
					LogWarning("Ignoring cook failure.");
				}
				else
				{
					// Delete cooked data (if any) as it may be incomplete / corrupted.
					Log("Cook failed. Deleting cooked data.");
					CleanupCookedData(PlatformsToCook.ToList(), Params);
					throw new AutomationException(ExitCode.Error_UnknownCookFailure, Ex, "Cook failed.");
				}
			}

            if (Params.HasDiffCookedContentPath)
            {
                try
                {
                    DiffCookedContent(Params);
                }
                catch ( Exception Ex )
                {
                    // Delete cooked data (if any) as it may be incomplete / corrupted.
                    Log("Cook failed. Deleting cooked data.");
                    CleanupCookedData(PlatformsToCook.ToList(), Params);
                    throw new AutomationException(ExitCode.Error_UnknownCookFailure, Ex, "Cook failed.");
                }
            }
            
		}


		Log("********** COOK COMMAND COMPLETED **********");
	}

    private static void DiffCookedContent( ProjectParams Params)
    {
        List<UnrealTargetPlatform> PlatformsToCook = Params.ClientTargetPlatforms;
        string ProjectPath = Path.GetFullPath(Params.RawProjectPath);

        var CookedSandboxesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Cooked");

        for (int CookPlatformIndex = 0; CookPlatformIndex < PlatformsToCook.Count; ++CookPlatformIndex)
        {
            // temporary directory to save the pak file to (pak file is usually not local and on network drive)
            var TemporaryPakPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Temp", "LocalPKG");
            // extracted files from pak file
            var TemporaryFilesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Temp", "LocalFiles");

            

            try
            {
                Directory.Delete(TemporaryPakPath, true);
                Directory.Delete(TemporaryFilesPath, true);
            }
            catch(Exception )
            {
                Log("Failed deleting temporary directories "+TemporaryPakPath+" "+TemporaryFilesPath+" continuing.");
            }

            Directory.CreateDirectory(TemporaryPakPath);
            Directory.CreateDirectory(TemporaryFilesPath);

            Platform CurrentPlatform = Params.GetTargetPlatformInstance(PlatformsToCook[CookPlatformIndex]);

            string SourceCookedContentPath = Params.DiffCookedContentPath;

            List<string> PakFiles = new List<string>();

            if (Path.HasExtension(SourceCookedContentPath) && (!SourceCookedContentPath.EndsWith(".pak")))
            {
                // must be a per platform pkg file try this
                CurrentPlatform.ExtractPackage(Params, Params.DiffCookedContentPath, TemporaryPakPath);

                // find the pak file
                PakFiles = Directory.EnumerateFiles(TemporaryPakPath, "*.pak").ToList();
            }

            string CookPlatformString = CurrentPlatform.GetCookPlatform(false, Params.HasDedicatedServerAndClient, Params.CookFlavor);

            if (!Path.HasExtension(SourceCookedContentPath))
            {
                // try find the pak or pkg file
                string SourceCookedContentPlatformPath = CombinePaths(SourceCookedContentPath, CookPlatformString);

                foreach (var PakName in Directory.EnumerateFiles(SourceCookedContentPlatformPath, "*.pak"))
                {
                    string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(PakName ));
                    File.Copy(PakName , TemporaryPakFilename);
                    PakFiles.Add(TemporaryPakFilename);
                }
            }
            else if (SourceCookedContentPath.EndsWith(".pak"))
            {
                string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(SourceCookedContentPath));
                File.Copy(SourceCookedContentPath, TemporaryPakFilename);
                PakFiles.Add(TemporaryPakFilename);
            }


            string FullCookPath = CombinePaths(CookedSandboxesPath, CookPlatformString);

            var UnrealPakExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");

            
            foreach (var Name in PakFiles)
            {
                string UnrealPakParams = Name + " -Extract " + " " + TemporaryFilesPath;

                RunAndLog(CmdEnv, UnrealPakExe, UnrealPakParams, Options: ERunOptions.Default | ERunOptions.UTF8Output);
            }

            const string RootFailedContentDirectory = "\\\\epicgames.net\\root\\Developers\\Daniel.Lamb\\";

            string FailedContentDirectory = CombinePaths( RootFailedContentDirectory, CommandUtils.P4Env.BuildRootP4 + CommandUtils.P4Env.ChangelistString, Params.ShortProjectName, CookPlatformString );


            // diff the content
            List<string> AllFiles = Directory.EnumerateFiles(FullCookPath, "*.uasset", System.IO.SearchOption.AllDirectories).ToList();
            AllFiles.AddRange(Directory.EnumerateFiles(FullCookPath, "*.map", System.IO.SearchOption.AllDirectories).ToList());
            foreach (string SourceFilename in AllFiles)
            {
                // Filename.StartsWith( CookedSandboxesPath );
                string RelativeFilename = SourceFilename.Remove(0, FullCookPath.Length);
                
                string DestFilename = TemporaryFilesPath + RelativeFilename;

                byte[] SourceFile = File.ReadAllBytes(SourceFilename);

                byte[] DestFile = File.ReadAllBytes(DestFilename);

                if ( SourceFile.LongLength == DestFile.LongLength )
                {
                    for ( long Index = 0; Index < SourceFile.LongLength; ++Index )
                    {
                        if ( SourceFile[Index] != DestFile[Index] )
                        {
					        Log("Diff cooked content failed on file " +SourceFilename + " when comparing against "+DestFilename + " at offset " + Index.ToString() );
                            string SavedSourceFilename = CombinePaths( FailedContentDirectory, "Source" + Path.GetFileName(SourceFilename));
                            string SavedDestFilename = CombinePaths( FailedContentDirectory, "Dest" + Path.GetFileName(DestFilename));
                            File.Copy(SourceFilename, SavedSourceFilename);
                            File.Copy(DestFilename, SavedDestFilename);
                            Log("Content temporarily saved to " +SavedSourceFilename + " and "+SavedDestFilename + " at offset " + Index.ToString() );
                            break;
                        }
                    }
                }
                else
                {
                    Log("Diff cooked content failed on file " +SourceFilename + " when comparing against "+DestFilename + " files are different sizes " + SourceFile.LongLength.ToString() + " " + DestFile.LongLength.ToString() );
                }
            } 
        }
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

	#endregion */
}
