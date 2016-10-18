// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using Ionic.Zip;
using System.IO;
using System.Web;
using System.Net;
using System.Linq;
using System.Text;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text.RegularExpressions;


public class HTML5Platform : Platform
{
	public HTML5Platform()
		: base(UnrealTargetPlatform.HTML5)
	{
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		Log("Package {0}", Params.RawProjectPath);

		Log("Setting Emscripten SDK for packaging..");
		HTML5SDKInfo.SetupEmscriptenTemp();
		HTML5SDKInfo.SetUpEmscriptenConfigFile();

		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5");
		if (!Directory.Exists(PackagePath))
		{
			Directory.CreateDirectory(PackagePath);
		}
		string FinalDataLocation = Path.Combine(PackagePath, Params.ShortProjectName) + ".data";

		var ConfigCache = new UnrealBuildTool.ConfigCacheIni(UnrealTargetPlatform.HTML5, "Engine", Path.GetDirectoryName(Params.RawProjectPath.FullName), CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine"));

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
			string PythonPath = HTML5SDKInfo.Python();
			string PackagerPath = HTML5SDKInfo.EmscriptenPackager();

			using (new ScopedEnvVar("EM_CONFIG", HTML5SDKInfo.DOT_EMSCRIPTEN))
			{
				using (new PushedDirectory(Path.Combine(Params.BaseStageDirectory, "HTML5")))
				{
					string CmdLine = string.Format("\"{0}\" \"{1}\" --preload . --js-output=\"{1}.js\"", PackagerPath, FinalDataLocation);
					RunAndLog(CmdEnv, PythonPath, CmdLine);
				}
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
	        throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find application {0}. You may need to build the UE4 project with your target configuration and platform.", FullGameExePath);
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
		string TemplateFileName = "GameX.html.template"; 
		string TemplateFile = Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "HTML5", TemplateFileName);
		string OutputFile = Path.Combine(PackagePath, (Params.ClientConfigsToBuild[0].ToString() != "Development" ? (Params.ShortProjectName + "-HTML5-" + Params.ClientConfigsToBuild[0].ToString()) : Params.ShortProjectName)) + ".html";

		// find Heap Size.
		ulong HeapSize;

		int ConfigHeapSize = 0;
		// Valuer set by Editor UI
		var bGotHeapSize = ConfigCache.GetInt32("/Script/HTML5PlatformEditor.HTML5TargetSettings", "HeapSize" + Params.ClientConfigsToBuild[0].ToString(), out ConfigHeapSize);

		// Fallback if the previous method failed
		if (!bGotHeapSize && !ConfigCache.GetInt32("/Script/BuildSettings.BuildSettings", "HeapSize" + Params.ClientConfigsToBuild[0].ToString(), out ConfigHeapSize)) // in Megs.
		{
			// we couldn't find a per config heap size, look for a common one.
			if (!ConfigCache.GetInt32("/Script/BuildSettings.BuildSettings", "HeapSize", out ConfigHeapSize))
			{
				ConfigHeapSize = Params.IsCodeBasedProject ? 1024 : 512;
				Log("Could not find Heap Size setting in .ini for Client config {0}", Params.ClientConfigsToBuild[0].ToString());
			}
		}

		HeapSize = (ulong)ConfigHeapSize * 1024L * 1024L; // convert to bytes.
		Log("Setting Heap size to {0} Mb ", ConfigHeapSize);


		GenerateFileFromTemplate(TemplateFile, OutputFile, Params.ShortProjectName, Params.ClientConfigsToBuild[0].ToString(), Params.StageCommandline, !Params.IsCodeBasedProject, HeapSize);
		
		string MacBashTemplateFile = Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "HTML5", "RunMacHTML5LaunchHelper.command.template");
		string MacBashOutputFile = Path.Combine(PackagePath, "RunMacHTML5LaunchHelper.command");
		string MonoPath = Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "BatchFiles", "Mac", "SetupMono.sh");
		GenerateMacCommandFromTemplate(MacBashTemplateFile, MacBashOutputFile, MonoPath);

		string htaccessTemplate = Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "HTML5", "htaccess.template");
		string htaccesspath = Path.Combine(PackagePath, ".htaccess");
		if ( File.Exists(htaccesspath) )
		{
			FileAttributes attributes = File.GetAttributes(htaccesspath);
			if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
			{
				attributes &= ~FileAttributes.ReadOnly;
				File.SetAttributes(htaccesspath, attributes);
			}
		}
		File.Copy(htaccessTemplate, htaccesspath, true);

		string JSDir = Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "HTML5");
		string OutDir = PackagePath;

		// Gather utlity .js files and combine into one file
		string[] UtilityJavaScriptFiles = Directory.GetFiles(JSDir, "*.js");

		string DestinationFile = OutDir + "/Utility.js";
        File.Delete(DestinationFile);
		foreach( var UtilityFile in UtilityJavaScriptFiles)
		{
			string Data = File.ReadAllText(UtilityFile);
			File.AppendAllText(DestinationFile, Data);
		}

		// Compress all files. These are independent tasks which can be threaded. 
		Task[] CompressionTasks = new Task[6];
		//data file.
		CompressionTasks[0] = Task.Factory.StartNew( () => CompressFile(FinalDataLocation, FinalDataLocation + "gz"));		
		// data file .js driver.
		CompressionTasks[1] = Task.Factory.StartNew( () => CompressFile(FinalDataLocation + ".js" , FinalDataLocation + ".jsgz"));
		// main js.
		CompressionTasks[2] = Task.Factory.StartNew(() => CompressFile(Path.Combine(PackagePath, GameExe), Path.Combine(PackagePath, GameExe) + "gz"));
		// mem init file.
		CompressionTasks[3] = Task.Factory.StartNew(() => CompressFile(Path.Combine(PackagePath, GameExe) + ".mem", Path.Combine(PackagePath, GameExe) + ".memgz"));
		// symbols file.
		CompressionTasks[4] = Task.Factory.StartNew(() => CompressFile(Path.Combine(PackagePath, GameExe) + ".symbols", Path.Combine(PackagePath, GameExe) + ".symbolsgz"));
		// Utility 
		CompressionTasks[5] = Task.Factory.StartNew(() => CompressFile(OutDir + "/Utility.js", OutDir + "/Utility.jsgz"));

		File.Copy(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/HTML5LaunchHelper.exe"),CombinePaths(OutDir, "HTML5LaunchHelper.exe"),true);
		Task.WaitAll(CompressionTasks);
		PrintRunTime();
	}

	void CompressFile(string Source, string Destination)
	{
		Log(" Compressing " + Source); 
		bool DeleteSource = false; 

		if(  Source == Destination )
		{
			string CopyOrig = Source + ".Copy";
			File.Copy(Source, CopyOrig);
			Source = CopyOrig;
			DeleteSource = true; 
		}

		using (System.IO.Stream input = System.IO.File.OpenRead(Source))
		{
			using (var raw = System.IO.File.Create(Destination))
				{
					using (Stream compressor = new Ionic.Zlib.GZipStream(raw, Ionic.Zlib.CompressionMode.Compress,Ionic.Zlib.CompressionLevel.BestCompression))
					{
						byte[] buffer = new byte[2048];
						int SizeRead = 0;
						while ((SizeRead = input.Read(buffer, 0, buffer.Length)) != 0)
						{
							compressor.Write(buffer, 0, SizeRead);
						}
					}
				}
		}

		if (DeleteSource && File.Exists(Source))
		{
			File.Delete(Source);
		}
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

	protected void GenerateMacCommandFromTemplate(string InTemplateFile, string InOutputFile, string InMonoPath)
	{
		StringBuilder outputContents = new StringBuilder();
		using (StreamReader reader = new StreamReader(InTemplateFile))
		{
			string InMonoPathParent = Path.GetDirectoryName(InMonoPath);
			string LineStr = null;
			while (reader.Peek() != -1)
			{
				LineStr = reader.ReadLine();
				if (LineStr.Contains("${unreal_mono_pkg_path}"))
				{
					LineStr = LineStr.Replace("${unreal_mono_pkg_path}", InMonoPath);
				}
				if (LineStr.Contains("${unreal_mono_pkg_path_base}"))
				{
					LineStr = LineStr.Replace("${unreal_mono_pkg_path_base}", InMonoPathParent);
				}

				outputContents.Append(LineStr + '\n');
			}
		}

		if (outputContents.Length > 0)
		{
			// Save the file. We Copy the template file to keep any permissions set to it.
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(InOutputFile));
				if (File.Exists(InOutputFile))
				{
					File.SetAttributes(InOutputFile, File.GetAttributes(InOutputFile) & ~FileAttributes.ReadOnly);
					File.Delete(InOutputFile);
				}
				File.Copy(InTemplateFile, InOutputFile);
				File.SetAttributes(InOutputFile, File.GetAttributes(InOutputFile) & ~FileAttributes.ReadOnly);
				using (var CmdFile = File.Open(InOutputFile, FileMode.OpenOrCreate | FileMode.Truncate)) 
				{
					Byte[] BytesToWrite = new UTF8Encoding(true).GetBytes(outputContents.ToString());
					CmdFile.Write(BytesToWrite, 0, BytesToWrite.Length);
				}
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

		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5");
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

		// data file 
		SC.ArchiveFiles(PackagePath, Path.GetFileName(FinalDataLocation));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(FinalDataLocation + "gz"));
		// data file js driver 
		SC.ArchiveFiles(PackagePath, Path.GetFileName(FinalDataLocation + ".js"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(FinalDataLocation + ".jsgz"));
		// main js file
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe + "gz"));
		// memory init file
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe + ".mem"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe + ".memgz"));
		// symbols file
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe + ".symbols"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(GameExe + ".symbolsgz"));
		// utilities
		SC.ArchiveFiles(PackagePath, Path.GetFileName("Utility.js"));
		SC.ArchiveFiles(PackagePath, Path.GetFileName("Utility.jsgz"));
		// landing page.
		SC.ArchiveFiles(PackagePath, Path.GetFileName(OutputFile));

		// Archive HTML5 Server and a Readme. 
		var LaunchHelperPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/");
		SC.ArchiveFiles(LaunchHelperPath, "HTML5LaunchHelper.exe");
		SC.ArchiveFiles(Path.Combine(CombinePaths(CmdEnv.LocalRoot, "Engine"), "Build", "HTML5"), "Readme.txt");
		SC.ArchiveFiles(PackagePath, Path.GetFileName(Path.Combine(PackagePath, "RunMacHTML5LaunchHelper.command")));
		SC.ArchiveFiles(PackagePath, Path.GetFileName(Path.Combine(PackagePath, ".htaccess")));

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

		UploadToS3(SC);
	}

	public override ProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		// look for browser
		string BrowserPath = Params.Devices[0].Replace("HTML5@", "");

		// open the webpage
		Int32 ServerPort = 8000;

		var ConfigCache = new UnrealBuildTool.ConfigCacheIni(UnrealTargetPlatform.HTML5, "Engine", Path.GetDirectoryName(Params.RawProjectPath.FullName), CombinePaths(CmdEnv.LocalRoot, "Engine"));
		ConfigCache.GetInt32("/Script/HTML5PlatformEditor.HTML5TargetSettings", "DeployServerPort", out ServerPort);
		string WorkingDirectory = Path.GetDirectoryName(ClientApp);
		string url = Path.GetFileName(ClientApp) +".html";
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
			url = String.Format("http://localhost:{0}/{1}", ServerPort, url);
		}

		// Check HTML5LaunchHelper source for command line args

		var LowerBrowserPath = BrowserPath.ToLower();
		var ProfileDirectory = Path.Combine(Utils.GetUserSettingDirectory().FullName, "UE4_HTML5", "user");

		string BrowserCommandline = url; 

		if (LowerBrowserPath.Contains("chrome"))
		{
			BrowserCommandline  += "  " + String.Format("--user-data-dir=\\\"{0}\\\" --enable-logging --no-first-run", Path.Combine(ProfileDirectory, "chrome"));
		}
		else if (LowerBrowserPath.Contains("firefox"))
		{
			BrowserCommandline += "  " +  String.Format("-no-remote -profile \\\"{0}\\\"", Path.Combine(ProfileDirectory, "firefox"));
		}

		string LauncherArguments = string.Format(" -Browser=\"{0}\" + -BrowserCommandLine=\"{1}\" -ServerPort=\"{2}\" -ServerRoot=\"{3}\" ", new object[] { BrowserPath, BrowserCommandline, ServerPort, WorkingDirectory });

		var LaunchHelperPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/HTML5LaunchHelper.exe");
		ProcessResult BrowserProcess = Run(LaunchHelperPath, LauncherArguments, null, ClientRunFlags | ERunOptions.NoWaitForExit);
	
		return BrowserProcess;
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return "HTML5";
	}

	public override string GetCookExtraCommandLine(ProjectParams Params)
	{
		return HTMLPakAutomation.CanCreateMapPaks(Params) ? " -GenerateDependenciesForMaps " : ""; 
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

#region AMAZON S3 
	public void UploadToS3(DeploymentContext SC)
	{
		ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(SC.StageTargetPlatform.PlatformType, "Engine", DirectoryReference.FromFile(SC.RawProjectPath));
		bool Upload = false;

		string KeyId = "";
		string AccessKey = "";
		string BucketName = "";
		string FolderName = "";

		if (Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "UploadToS3", out Upload))
		{
			if (!Upload)
				return;
		}
		else
		{
			return; 
		}
		bool AmazonIdentity = Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3KeyID", out KeyId) &&
								Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3SecretAccessKey", out AccessKey) &&
								Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3BucketName", out BucketName) &&
								Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3FolderName", out FolderName);

		if ( !AmazonIdentity )
		{
			Log("Amazon S3 Incorrectly configured"); 
			return; 
		}

		if ( FolderName == "" )
		{
			FolderName = SC.ShortProjectName;
		}

		List<Task> UploadTasks = new List<Task>();
		foreach(KeyValuePair<string, string> Entry in SC.ArchivedFiles)
		{
			FileInfo Info = new FileInfo(Entry.Key);
			UploadTasks.Add (Task.Factory.StartNew(() => UploadToS3Worker(Info,KeyId,AccessKey,BucketName,FolderName))); 
		}

		Task.WaitAll(UploadTasks.ToArray());

		string URL = "http://" + BucketName + ".s3.amazonaws.com/" + FolderName + "/" + SC.ShortProjectName + ".html";
		Log("Your project's shareable link is: " + URL);

		Log("Upload Tasks finished.");
	}

	private static IDictionary<string, string> MimeTypeMapping = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase) 
		{
			{ ".html", "text/html"},
			{ ".jsgz", "application/x-javascript" },  // upload compressed javascript. 
			{ ".datagz", "appication/octet-stream"}
		}; 

	public void UploadToS3Worker(FileInfo Info, string KeyId, string AccessKey, string BucketName, string FolderName )
	{
		Log(" Uploading " + Info.Name); 

			// force upload files even if the timestamps haven't changed. 
			string TimeStamp = string.Format("{0:r}", DateTime.UtcNow);
			string ContentType = ""; 
			if (MimeTypeMapping.ContainsKey(Info.Extension))
			{
				ContentType = MimeTypeMapping[Info.Extension];
			}
			else 
			{
				// default
				ContentType = "application/octet-stream";
			}

		
			// URL to put things. 
			string URL = "http://" + BucketName + ".s3.amazonaws.com/" + FolderName + "/" + Info.Name;

			HttpWebRequest Request = (HttpWebRequest)WebRequest.Create(URL); 

			// Upload. 
			Request.Method = "PUT";
			Request.Headers["x-amz-date"] = TimeStamp;
			Request.Headers["x-amz-acl"] = "public-read"; // we probably want to make public read by default. 

			// set correct content encoding for compressed javascript. 
			if ( Info.Extension.EndsWith("gz") )
			{
				Request.Headers["Content-Encoding"] = "gzip";
			}

			Request.ContentType = ContentType;
			Request.ContentLength = Info.Length;

			//http://docs.aws.amazon.com/AmazonS3/latest/dev/RESTAuthentication.html
			// Find Base64Encoded data. 
			UTF8Encoding EncodingMethod = new UTF8Encoding();
			HMACSHA1 Signature = new HMACSHA1 { Key = EncodingMethod.GetBytes(AccessKey) };
			// don't change this string.
			string RequestString = "PUT\n\n" + ContentType + "\n\nx-amz-acl:public-read\nx-amz-date:" + TimeStamp + "\n/"+ BucketName + "/" + FolderName + "/" + Info.Name; 
			Byte[] ComputedHash = Signature.ComputeHash(EncodingMethod.GetBytes(RequestString));
			var Base64Encoded = Convert.ToBase64String(ComputedHash);
			
			// final amz auth header. 
			Request.Headers["Authorization"] = "AWS " + KeyId + ":" + Base64Encoded;

			try
			{
				// may fail for really big stuff. YMMV. May need Multi part approach, we will see. 
				Byte[] FileData = File.ReadAllBytes(Info.FullName);
				var requestStream = Request.GetRequestStream();
				requestStream.Write(FileData, 0, FileData.Length);
				requestStream.Close();

				using (var response = Request.GetResponse() as HttpWebResponse)
				{
					var reader = new StreamReader(response.GetResponseStream());
					reader.ReadToEnd();
				}
			}
			catch (Exception ex)
			{
				Log("Could not connect to S3, incorrect S3 Keys? " + ex.ToString());
				throw ex;
			}

			Log(Info.Name + " has been uploaded "); 
	}

#endregion 
}
