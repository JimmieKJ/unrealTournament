// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Diagnostics;
using System.Threading;

public class HTML5Platform : Platform
{
    public HTML5Platform()
		: base(UnrealTargetPlatform.HTML5)
	{
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
        Log("Package {0}", Params.RawProjectPath);
        
        string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath), "Binaries", "HTML5");
        if (!Directory.Exists(PackagePath))
        {
            Directory.CreateDirectory(PackagePath);
        }
        string FinalDataLocation = Path.Combine(PackagePath, Params.ShortProjectName) + ".data";

        var ConfigCache = new UnrealBuildTool.ConfigCacheIni(UnrealTargetPlatform.HTML5, "Engine", Path.GetDirectoryName(Params.RawProjectPath), CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine"));

        if (HTMLPakAutomation.CanCreateMapPaks(Params))
        {
            HTMLPakAutomation PakAutomation = new HTMLPakAutomation(Params, SC);

            // Create Necessary Paks. 
            PakAutomation.CreateEnginePak();
            PakAutomation.CreateGamePak();
            PakAutomation.CreateContentDirectoryPak();

            // Create Emscripten Package from Necessary Paks. - This will be the VFS. 
            PakAutomation.CreateEmscriptenDataPackage(PackagePath, FinalDataLocation);

            // Create All Map Paks which  will be downloaded on the fly. 
            PakAutomation.CreateMapPak();

            // Create Delta Paks if setup.
            List<string> Paks = new List<string>();
            ConfigCache.GetArray("/Script/HTML5PlatformEditor.HTML5TargetSettings", "LevelTransitions", out Paks);

            if (Paks != null)
            {
                foreach (var Pak in Paks)
                {
                    var Matched = Regex.Matches(Pak, "\"[^\"]+\"", RegexOptions.IgnoreCase);
                    string MapFrom = Path.GetFileNameWithoutExtension(Matched[0].ToString().Replace("\"", ""));
                    string MapTo = Path.GetFileNameWithoutExtension(Matched[1].ToString().Replace("\"", ""));
                    PakAutomation.CreateDeltaMapPaks(MapFrom, MapTo);
                }
            }
        }
        else
        {
            // we need to operate in the root
            using (new PushedDirectory(Path.Combine(Params.BaseStageDirectory, "HTML5")))
            {
                string PythonPath = HTML5SDKInfo.PythonPath();
                string PackagerPath = HTML5SDKInfo.EmscriptenPackager();

                string CmdLine = string.Format("\"{0}\" \"{1}\" --preload . --js-output=\"{1}.js\"", PackagerPath, FinalDataLocation);
                RunAndLog(CmdEnv, PythonPath, CmdLine);
            }
        }

        // copy the "Executable" to the package directory
        string GameExe = Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename);
        if (Params.ClientConfigsToBuild[0].ToString() != "Development")
        {
            GameExe += "-HTML5-" + Params.ClientConfigsToBuild[0].ToString();
        }
        GameExe += ".js";
			
        // ensure the ue4game binary exists, if applicable
        string FullGameExePath = Path.Combine(Path.GetDirectoryName(Params.ProjectGameExeFilename), GameExe);
        if (!SC.IsCodeBasedProject && !FileExists_NoExceptions(FullGameExePath))
        {
	        Log("Failed to find game application " + FullGameExePath);
	        AutomationTool.ErrorReporter.Error("Stage Failed.", (int)AutomationTool.ErrorCodes.Error_MissingExecutable);
	        throw new AutomationException("Could not find application {0}. You may need to build the UE4 project with your target configuration and platform.", FullGameExePath);
        }

        if (Path.Combine(Path.GetDirectoryName(Params.ProjectGameExeFilename), GameExe) != Path.Combine(PackagePath, GameExe))
        {
            File.Copy(Path.Combine(Path.GetDirectoryName(Params.ProjectGameExeFilename), GameExe), Path.Combine(PackagePath, GameExe), true);
            File.Copy(Path.Combine(Path.GetDirectoryName(Params.ProjectGameExeFilename), GameExe) + ".mem", Path.Combine(PackagePath, GameExe) + ".mem", true);
			File.Copy(Path.Combine(Path.GetDirectoryName(Params.ProjectGameExeFilename), GameExe) + ".symbols", Path.Combine(PackagePath, GameExe) + ".symbols", true);
        }
        File.SetAttributes(Path.Combine(PackagePath, GameExe), FileAttributes.Normal);
        File.SetAttributes(Path.Combine(PackagePath, GameExe) + ".mem", FileAttributes.Normal);
		File.SetAttributes(Path.Combine(PackagePath, GameExe) + ".symbols", FileAttributes.Normal);


        // put the HTML file to the package directory

        bool UseExperimentalTemplate = false;
        ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "UseExperimentalTemplate", out UseExperimentalTemplate);
        string TemplateFileName = UseExperimentalTemplate ? "GameX.html.template" : "Game.html.template";

        string TemplateFile = Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "HTML5", TemplateFileName);
        string OutputFile = Path.Combine(PackagePath, (Params.ClientConfigsToBuild[0].ToString() != "Development" ? (Params.ShortProjectName + "-HTML5-" + Params.ClientConfigsToBuild[0].ToString()) : Params.ShortProjectName)) + ".html";

        // find Heap Size.
        ulong HeapSize;

        int ConfigHeapSize = 0;
        // Valuer set by Editor UI
        var bGotHeapSize = ConfigCache.GetInt32("/Script/HTML5PlatformEditor.HTML5TargetSettings", "HeapSize" + Params.ClientConfigsToBuild[0].ToString(), out ConfigHeapSize);

        // Fallback if the previous method failed
        if (!bGotHeapSize && !ConfigCache.GetInt32("BuildSettings", "HeapSize" + Params.ClientConfigsToBuild[0].ToString(), out ConfigHeapSize)) // in Megs.
        {
            // we couldn't find a per config heap size, look for a common one.
            if (!ConfigCache.GetInt32("BuildSettings", "HeapSize", out ConfigHeapSize))
            {
                ConfigHeapSize = Params.IsCodeBasedProject ? 1024 : 512;
                Log("Could not find Heap Size setting in .ini for Client config {0}", Params.ClientConfigsToBuild[0].ToString());
            }
        }

        HeapSize = (ulong)ConfigHeapSize * 1024L * 1024L; // convert to bytes.
        Log("Setting Heap size to {0} Mb ", ConfigHeapSize);


        GenerateFileFromTemplate(TemplateFile, OutputFile, Params.ShortProjectName, Params.ClientConfigsToBuild[0].ToString(), Params.StageCommandline, !Params.IsCodeBasedProject, HeapSize);

        // copy the jstorage files to the binaries directory
        string JSDir = Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "HTML5");
        string OutDir = PackagePath;
        File.Copy(JSDir + "/json2.js", OutDir + "/json2.js", true);
        File.SetAttributes(OutDir + "/json2.js", FileAttributes.Normal);
        File.Copy(JSDir + "/jStorage.js", OutDir + "/jStorage.js", true);
        File.SetAttributes(OutDir + "/jStorage.js", FileAttributes.Normal);
        File.Copy(JSDir + "/moz_binarystring.js", OutDir + "/moz_binarystring.js", true);
        File.SetAttributes(OutDir + "/moz_binarystring.js", FileAttributes.Normal);
        PrintRunTime();
	}


    public override bool RequiresPackageToDeploy
    {
        get { return true; }
    }


	protected void GenerateFileFromTemplate(string InTemplateFile, string InOutputFile, string InGameName, string InGameConfiguration, string InArguments, bool IsContentOnly, ulong HeapSize)
	{
		StringBuilder outputContents = new StringBuilder();
		using (StreamReader reader = new StreamReader(InTemplateFile))
		{
			string LineStr = null;
			while (reader.Peek() != -1)
			{
				LineStr = reader.ReadLine();
				if (LineStr.Contains("%GAME%"))
				{
					LineStr = LineStr.Replace("%GAME%", InGameName);
				}

				if (LineStr.Contains("%HEAPSIZE%"))
				{
					LineStr = LineStr.Replace("%HEAPSIZE%", HeapSize.ToString());
				}

				if (LineStr.Contains("%CONFIG%"))
				{
					string TempGameName = InGameName;
					if (IsContentOnly)
						TempGameName = "UE4Game";
					LineStr = LineStr.Replace("%CONFIG%", (InGameConfiguration != "Development" ? (TempGameName + "-HTML5-" + InGameConfiguration) : TempGameName));
				}

				if (LineStr.Contains("%UE4CMDLINE%"))
				{
					InArguments = InArguments.Replace ("\"", "");
					string[] Arguments = InArguments.Split(' ');
					string ArgumentString = IsContentOnly ? "'../../../" + InGameName + "/" + InGameName + ".uproject '," : "";
					for (int i = 0; i < Arguments.Length - 1; ++i)
					{
						ArgumentString += "'";
			   		    ArgumentString += Arguments[i];
						ArgumentString += "'";
						ArgumentString += ",' ',";
					}
					if (Arguments.Length > 0)
					{
						ArgumentString += "'";
						ArgumentString += Arguments[Arguments.Length - 1];
						ArgumentString += "'";
					}
					LineStr = LineStr.Replace("%UE4CMDLINE%", ArgumentString);

				}

				outputContents.AppendLine(LineStr);
			}
		}

		if (outputContents.Length > 0)
		{
			// Save the file
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(InOutputFile));
				File.WriteAllText(InOutputFile, outputContents.ToString(), Encoding.UTF8);
			}
			catch (Exception)
			{
				// Unable to write to the project file.
			}
		}
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath), "Binaries", "HTML5");
		string FinalDataLocation = Path.Combine(PackagePath, Params.ShortProjectName) + ".data";

		// copy the "Executable" to the archive directory
		string GameExe = Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename);
		if (Params.ClientConfigsToBuild[0].ToString() != "Development")
		{
			GameExe += "-HTML5-" + Params.ClientConfigsToBuild[0].ToString();
		}
		GameExe += ".js";

		// put the HTML file to the package directory
		string OutputFile = Path.Combine(PackagePath, (Params.ClientConfigsToBuild[0].ToString() != "Development" ? (Params.ShortProjectName + "-HTML5-" + Params.ClientConfigsToBuild[0].ToString()) : Params.ShortProjectName)) + ".html";

		SC.ArchiveFiles(PackagePath, Path.GetFileName(FinalDataLocation));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(FinalDataLocation + ".js"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe + ".mem"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe + ".symbols"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName("json2.js"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName("jStorage.js"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName("moz_binarystring.js"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(OutputFile));
        

        if (HTMLPakAutomation.CanCreateMapPaks(Params))
        {
            // find all paks.
            string[] Files = Directory.GetFiles(Path.Combine(PackagePath, Params.ShortProjectName), "*",SearchOption.AllDirectories);
            foreach(string PakFile in Files)
            {
                var DestPak = PakFile.Replace(PackagePath,"");
                SC.ArchivedFiles.Add(PakFile, DestPak);
            }
        }

	}

	public override ProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		// look for browser
		var ConfigCache = new UnrealBuildTool.ConfigCacheIni(UnrealTargetPlatform.HTML5, "Engine", Path.GetDirectoryName(Params.RawProjectPath), CombinePaths(CmdEnv.LocalRoot, "Engine"));
		bool ok = false;
		List<string> Devices;
		string browserPath = "";
		string DeviceName = Params.Device.Split('@')[1];
		DeviceName = DeviceName.Substring(0, DeviceName.LastIndexOf(" on "));

		if (ConfigCache.GetArray("/Script/HTML5PlatformEditor.HTML5SDKSettings", "DeviceMap", out Devices))
		{
			foreach (var Dev in Devices)
			{
				var Matched = Regex.Match(Dev, "\\(DeviceName=\"(.*)\",DevicePath=\\(FilePath=\"(.*)\"\\)\\)", RegexOptions.IgnoreCase);
				if (Matched.Success && Matched.Groups[1].ToString() == DeviceName)
				{
					browserPath = Matched.Groups[2].ToString();
					ok = true;
					break;
				}
			}
		}

		if (!ok && HTML5SDKInfo.bAllowFallbackSDKSettings)
		{
			string DeviceSection;

			if (Utils.IsRunningOnMono)
			{
				DeviceSection = "HTML5DevicesMac";
			}
			else
			{
				DeviceSection = "HTML5DevicesWindows";
			}

			ok = ConfigCache.GetString(DeviceSection, DeviceName, out browserPath);
		}

		if (!ok)
		{
			throw new System.Exception("Incorrect browser configuration in HTML5Engine.ini ");
		}

		// open the webpage
		Int32 ServerPort = 8000;
		ConfigCache.GetInt32("/Script/HTML5PlatformEditor.HTML5TargetSettings", "DeployServerPort", out ServerPort);
		string WorkingDirectory = Path.GetDirectoryName(ClientApp);
		string url = Path.GetFileName(ClientApp) +".html";
		string args = "-m ";
		// Are we running via cook on the fly server?
		// find our http url - This is awkward because RunClient doesn't have real information that NFS is running or not.
		bool IsCookOnTheFly = false;

		// 9/24/2014 @fixme - All this is convoluted, clean up.
		// looks like cookonthefly commandline stopped adding protocol or the port :/ hard coding to DEFAULT_TCP_FILE_SERVING_PORT+1 (DEFAULT_HTTP_FILE_SERVING_PORT)
		// This will fail if the NFS server is started with a different port - we need to modify driver .cs script to pass in IP/Port data correctly. 

		if (ClientCmdLine.Contains("filehostip"))
		{
			IsCookOnTheFly = true; 
			url = "http://127.0.0.1:41898/" + url; 
		}

		if (IsCookOnTheFly)
		{
			url += "?cookonthefly=true";
		}
		else
		{
			url = String.Format("http://127.0.0.1:{0}/{1}", ServerPort, url);
			args += String.Format("-h -s {0} ", ServerPort);
		}

		// Check HTML5LaunchHelper source for command line args
		var LowerBrowserPath = browserPath.ToLower();
		args += String.Format("-b \"{0}\" -p \"{1}\" -w \"{2}\" ", browserPath, HTML5SDKInfo.PythonPath(), WorkingDirectory);
		args += url + " ";
		var ProfileDirectory = Path.Combine(Utils.GetUserSettingDirectory(), "UE4_HTML5", "user");
		if (LowerBrowserPath.Contains("chrome"))
		{
			args += String.Format("--user-data-dir=\"{0}\" --enable-logging --no-first-run", Path.Combine(ProfileDirectory, "chrome"));
		}
		else if (LowerBrowserPath.Contains("firefox"))
		{
			args += String.Format("-no-remote -profile \"{0}\"", Path.Combine(ProfileDirectory, "firefox"));
		}
		//else if (browserPath.Contains("Safari")) {}

		var LaunchHelperPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/HTML5LaunchHelper.exe");
		ProcessResult BrowserProcess = Run(LaunchHelperPath, args, null, ClientRunFlags | ERunOptions.NoWaitForExit);
	
		return BrowserProcess;

	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
	{
		return "HTML5";
	}

    public override string GetCookExtraCommandLine(ProjectParams Params)
    {
        return HTMLPakAutomation.CanCreateMapPaks(Params) ? " -GenerateDependenciesForMaps " : ""; 
    }

    public override List<string> GetCookExtraMaps()
    {
        var Maps = new List<string>();
        Maps.Add("/Engine/Maps/Entry");
        return Maps; 
    }

	public override bool DeployPakInternalLowerCaseFilenames()
	{
		return false;
	}

    public override PakType RequiresPak(ProjectParams Params)
    {
        return HTMLPakAutomation.CanCreateMapPaks(Params) ? PakType.Never : PakType.Always;
    }

	public override string GetPlatformPakCommandLine()
	{
		return " -compress";
	}

	public override bool DeployLowerCaseFilenames(bool bUFSFile)
	{
		return false;
	}

	public override string LocalPathToTargetPath(string LocalPath, string LocalRoot)
	{
		return LocalPath;//.Replace("\\", "/").Replace(LocalRoot, "../../..");
	}

	public override bool IsSupported { get { return true; } }

	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { ".pdb" };
	}
	#region Hooks

	public override void PreBuildAgenda(UE4Build Build, UE4Build.BuildAgenda Agenda)
	{
	}

	public override List<string> GetExecutableNames(DeploymentContext SC, bool bIsRun = false)
	{
		var ExecutableNames = new List<String>();
		ExecutableNames.Add(Path.Combine(SC.ProjectRoot, "Binaries", "HTML5", SC.ShortProjectName));
		return ExecutableNames;
	}
	#endregion
}
