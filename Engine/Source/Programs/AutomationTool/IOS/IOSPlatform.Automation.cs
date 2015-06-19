// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using Ionic.Zip;
using Ionic.Zlib;

static class IOSEnvVarNames
{
	// Should we code sign when staging?  (defaults to 1 if not present)
	static public readonly string CodeSignWhenStaging = "uebp_CodeSignWhenStaging";
}

public class IOSPlatform : Platform
{
	bool bCreatedIPA = false;

	public IOSPlatform()
		:base(UnrealTargetPlatform.IOS)
	{
	}

	// Run the integrated IPP code
	// @todo ipp: How to log the output in a nice UAT way?
	protected static int RunIPP(string IPPArguments)
	{
		List<string> Args = new List<string>();

		StringBuilder Token = null;
		int Index = 0;
		bool bInQuote = false;
		while (Index < IPPArguments.Length)
		{
			if (IPPArguments[Index] == '\"')
			{
				bInQuote = !bInQuote;
			}
			else if (IPPArguments[Index] == ' ' && !bInQuote)
			{
				if (Token != null)
				{
					Args.Add(Token.ToString());
					Token = null;
				}
			}
			else
			{
				if (Token == null)
				{
					Token = new StringBuilder();
				}
				Token.Append(IPPArguments[Index]);
			}
			Index++;
		}

		if (Token != null)
		{
			Args.Add(Token.ToString());
		}
	
		return RunIPP(Args.ToArray());
	}

	protected static int RunIPP(string[] Args)
	{
		return 5;//@TODO: Reintegrate IPP to IOS.Automation iPhonePackager.Program.RealMain(Args);
	}

	public override int RunCommand(string Command)
	{
		// run generic IPP commands through the interface
		if (Command.StartsWith("IPP:"))
		{
			return RunIPP(Command.Substring(4));
		}

		// non-zero error code
		return 4;
	}

	protected string MakeIPAFileName( UnrealTargetConfiguration TargetConfiguration, ProjectParams Params )
	{
		string ProjectIPA = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath), "Binaries", "IOS", (Params.Distribution ? "Distro_" : "") + Params.ShortProjectName);
		if (TargetConfiguration != UnrealTargetConfiguration.Development)
		{
			ProjectIPA += "-" + PlatformType.ToString() + "-" + TargetConfiguration.ToString();
		}
		ProjectIPA += ".ipa";
		return ProjectIPA;
	}

	// Determine if we should code sign
	protected bool GetCodeSignDesirability(ProjectParams Params)
	{
		//@TODO: Would like to make this true, as it's the common case for everyone else
		bool bDefaultNeedsSign = true;

		bool bNeedsSign = false;
		string EnvVar = InternalUtils.GetEnvironmentVariable(IOSEnvVarNames.CodeSignWhenStaging, bDefaultNeedsSign ? "1" : "0", /*bQuiet=*/ false);
		if (!bool.TryParse(EnvVar, out bNeedsSign))
		{
			int BoolAsInt;
			if (int.TryParse(EnvVar, out BoolAsInt))
			{
				bNeedsSign = BoolAsInt != 0;
			}
			else
			{
				bNeedsSign = bDefaultNeedsSign;
			}
		}

		if (!String.IsNullOrEmpty(Params.BundleName))
		{
			// Have to sign when a bundle name is specified
			bNeedsSign = true;
		}

		return bNeedsSign;
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		Log("Package {0}", Params.RawProjectPath);

		// ensure the ue4game binary exists, if applicable
		string FullExePath = CombinePaths(Path.GetDirectoryName(Params.ProjectGameExeFilename), SC.StageExecutables[0] + (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac ? ".stub" : ""));
		if (!SC.IsCodeBasedProject && !FileExists_NoExceptions(FullExePath))
		{
			Log("Failed to find game binary " + FullExePath);
			AutomationTool.ErrorReporter.Error("Stage Failed.", (int)AutomationTool.ErrorCodes.Error_MissingExecutable);
			throw new AutomationException("Could not find binary {0}. You may need to build the UE4 project with your target configuration and platform.", FullExePath);
		}

		//@TODO: We should be able to use this code on both platforms, when the following issues are sorted:
		//   - Raw executable is unsigned & unstripped (need to investigate adding stripping to IPP)
		//   - IPP needs to be able to codesign a raw directory
		//   - IPP needs to be able to take a .app directory instead of a Payload directory when doing RepackageFromStage (which would probably be renamed)
		//   - Some discrepancy in the loading screen pngs that are getting packaged, which needs to be investigated
		//   - Code here probably needs to be updated to write 0 byte files as 1 byte (difference with IPP, was required at one point when using Ionic.Zip to prevent issues on device, maybe not needed anymore?)
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			// copy in all of the artwork and plist
			var DeployHandler = UEBuildDeploy.GetBuildDeploy(UnrealTargetPlatform.IOS);

			DeployHandler.PrepForUATPackageOrDeploy(Params.ShortProjectName,
				Path.GetDirectoryName(Params.RawProjectPath),
				CombinePaths(Path.GetDirectoryName(Params.ProjectGameExeFilename), SC.StageExecutables[0]),
				CombinePaths(SC.LocalRoot, "Engine"),
				Params.Distribution, 
				"",
				false);

			// figure out where to pop in the staged files
			string AppDirectory = string.Format("{0}/Payload/{1}.app",
				Path.GetDirectoryName(Params.ProjectGameExeFilename),
				Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename));

			// delete the old cookeddata
			InternalUtils.SafeDeleteDirectory(AppDirectory + "/cookeddata", true);
			InternalUtils.SafeDeleteFile(AppDirectory + "/ue4commandline.txt", true);

			if (!Params.IterativeDeploy)
			{
				// copy the Staged files to the AppDirectory
				string[] StagedFiles = Directory.GetFiles (SC.StageDirectory, "*", SearchOption.AllDirectories);
				foreach (string Filename in StagedFiles)
				{
					string DestFilename = Filename.Replace (SC.StageDirectory, AppDirectory);
					Directory.CreateDirectory (Path.GetDirectoryName (DestFilename));
					InternalUtils.SafeCopyFile (Filename, DestFilename, true);
				}
			}
			else
			{
				// copy just the root stage directory files
				string[] StagedFiles = Directory.GetFiles (SC.StageDirectory, "*", SearchOption.TopDirectoryOnly);
				foreach (string Filename in StagedFiles)
				{
					string DestFilename = Filename.Replace (SC.StageDirectory, AppDirectory);
					Directory.CreateDirectory (Path.GetDirectoryName (DestFilename));
					InternalUtils.SafeCopyFile (Filename, DestFilename, true);
				}
			}
		}

		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}
		bCreatedIPA = false;
		bool bNeedsIPA = false;
		if (Params.IterativeDeploy)
		{
			String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath();
			// check to determine if we need to update the IPA
			if (File.Exists(NonUFSManifestPath))
			{
				string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
				string[] Lines = NonUFSFiles.Split('\n');
				bNeedsIPA = Lines.Length > 0 && !string.IsNullOrWhiteSpace(Lines[0]);
			}
		}

		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{
			var TargetConfiguration = SC.StageTargetConfigurations[0];
			var ProjectIPA = MakeIPAFileName(TargetConfiguration, Params);
			var ProjectStub = Path.GetFullPath(Params.ProjectGameExeFilename);

			// package a .ipa from the now staged directory
			var IPPExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/IPhonePackager.exe");

			Log("ProjectName={0}", Params.ShortProjectName);
			Log("ProjectStub={0}", ProjectStub);
			Log("ProjectIPA={0}", ProjectIPA);
			Log("IPPExe={0}", IPPExe);

			bool cookonthefly = Params.CookOnTheFly || Params.SkipCookOnTheFly;

			// if we are incremental check to see if we need to even update the IPA
			if (!Params.IterativeDeploy || !File.Exists(ProjectIPA) || bNeedsIPA)
			{
				// delete the .ipa to make sure it was made
				DeleteFile(ProjectIPA);

				bCreatedIPA = true;

				if (RemoteToolChain.bUseRPCUtil)
				{
					string IPPArguments = "RepackageFromStage \"" + (Params.IsCodeBasedProject ? Params.RawProjectPath : "Engine") + "\"";
					IPPArguments += " -config " + TargetConfiguration.ToString();

					if (TargetConfiguration == UnrealTargetConfiguration.Shipping)
					{
						IPPArguments += " -compress=best";
					}

					// Determine if we should sign
					bool bNeedToSign = GetCodeSignDesirability(Params);

					if (!String.IsNullOrEmpty(Params.BundleName))
					{
						// Have to sign when a bundle name is specified
						bNeedToSign = true;
						IPPArguments += " -bundlename " + Params.BundleName;
					}

					if (bNeedToSign)
					{
						IPPArguments += " -sign";
						if (Params.Distribution)
						{
							IPPArguments += " -distribution";
						}
					}

					IPPArguments += (cookonthefly ? " -cookonthefly" : "");
					IPPArguments += " -stagedir \"" + CombinePaths(Params.BaseStageDirectory, "IOS") + "\"";
					IPPArguments += " -project \"" + Params.RawProjectPath + "\"";
					if (Params.IterativeDeploy)
					{
						IPPArguments += " -iterate";
					}

					RunAndLog(CmdEnv, IPPExe, IPPArguments);
				}
				else
				{
					List<string> IPPArguments = new List<string>();
					IPPArguments.Add("RepackageFromStage");
					IPPArguments.Add(Params.IsCodeBasedProject ? Params.RawProjectPath : "Engine");
					IPPArguments.Add("-config");
					IPPArguments.Add(TargetConfiguration.ToString());

					if (TargetConfiguration == UnrealTargetConfiguration.Shipping)
					{
						IPPArguments.Add("-compress=best");
					}

					// Determine if we should sign
					bool bNeedToSign = GetCodeSignDesirability(Params);

					if (!String.IsNullOrEmpty(Params.BundleName))
					{
						// Have to sign when a bundle name is specified
						bNeedToSign = true;
						IPPArguments.Add("-bundlename");
						IPPArguments.Add(Params.BundleName);
					}

					if (bNeedToSign)
					{
						IPPArguments.Add("-sign");
					}

					if (cookonthefly)
					{
						IPPArguments.Add(" -cookonthefly");
					}
					IPPArguments.Add(" -stagedir");
					IPPArguments.Add(CombinePaths(Params.BaseStageDirectory, "IOS"));
					IPPArguments.Add(" -project");
					IPPArguments.Add(Params.RawProjectPath);
					if (Params.IterativeDeploy)
					{
						IPPArguments.Add(" -iterate");
					}

					if (RunIPP(IPPArguments.ToArray()) != 0)
					{
						throw new AutomationException("IPP Failed");
					}
				}
			}

			// verify the .ipa exists
			if (!FileExists(ProjectIPA))
			{
				ErrorReporter.Error(String.Format("PACKAGE FAILED - {0} was not created", ProjectIPA), (int)ErrorCodes.Error_FailedToCreateIPA);
				throw new AutomationException("PACKAGE FAILED - {0} was not created", ProjectIPA);
			}

			if (WorkingCL > 0)
			{
				// Open files for add or edit
				var ExtraFilesToCheckin = new List<string>
				{
					ProjectIPA
				};

				// check in the .ipa along with everything else
				UE4Build.AddBuildProductsToChangelist(WorkingCL, ExtraFilesToCheckin);
			}

			//@TODO: This automatically deploys after packaging, useful for testing on PC when iterating on IPP
			//Deploy(Params, SC);
		}
		else
		{
			// create the ipa
			string IPAName = CombinePaths(Path.GetDirectoryName(Params.RawProjectPath), "Binaries", "IOS", (Params.Distribution ? "Distro_" : "") + Params.ShortProjectName + (SC.StageTargetConfigurations[0] != UnrealTargetConfiguration.Development ? ("-IOS-" + SC.StageTargetConfigurations[0].ToString()) : "") + ".ipa");

			if (!Params.IterativeDeploy || !File.Exists(IPAName) || bNeedsIPA)
			{
				bCreatedIPA = true;

				// code sign the app
				CodeSign(Path.GetDirectoryName(Params.ProjectGameExeFilename), Params.IsCodeBasedProject ? Params.ShortProjectName : Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename), Params.RawProjectPath, SC.StageTargetConfigurations[0], SC.LocalRoot, Params.ShortProjectName, Path.GetDirectoryName(Params.RawProjectPath), SC.IsCodeBasedProject, Params.Distribution);

				// now generate the ipa
				PackageIPA(Path.GetDirectoryName(Params.ProjectGameExeFilename), Params.IsCodeBasedProject ? Params.ShortProjectName : Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename), Params.ShortProjectName, Path.GetDirectoryName(Params.RawProjectPath), SC.StageTargetConfigurations[0], Params.Distribution);
			}
		}

		PrintRunTime();
	}

	private string EnsureXcodeProjectExists(string RawProjectPath, string LocalRoot, string ShortProjectName, string ProjectRoot, bool IsCodeBasedProject, out bool bWasGenerated)
	{
		// first check for ue4.xcodeproj
		bWasGenerated = false;
		string XcodeProj = RawProjectPath.Replace(".uproject", "_IOS.xcodeproj");
		Console.WriteLine ("Project: " + XcodeProj);
		//		if (!Directory.Exists (XcodeProj))
		{
			// project.xcodeproj doesn't exist, so generate temp project
			string Arguments = "-project=\"" + RawProjectPath + "\"";
			Arguments += " -platforms=IOS -game -nointellisense -iosdeployonly -ignorejunk";
			string Script = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh");
			if (GlobalCommandLine.Rocket)
			{
				Script = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/BatchFiles/Mac/RocketGenerateProjectFiles.sh");
			}
			string CWD = Directory.GetCurrentDirectory ();
			Directory.SetCurrentDirectory (Path.GetDirectoryName (Script));
			Run (Script, Arguments, null, ERunOptions.Default);
			bWasGenerated = true;
			Directory.SetCurrentDirectory (CWD);

			if (!Directory.Exists (XcodeProj))
			{
				// something very bad happened
				throw new AutomationException("iOS couldn't find the appropriate Xcode Project");
			}
		}

		return XcodeProj;
	}

	private void CodeSign(string BaseDirectory, string GameName, string RawProjectPath, UnrealTargetConfiguration TargetConfig, string LocalRoot, string ProjectName, string ProjectDirectory, bool IsCode, bool Distribution = false)
	{
		// check for the proper xcodeproject
		bool bWasGenerated = false;
		string XcodeProj = EnsureXcodeProjectExists (RawProjectPath, LocalRoot, ProjectName, ProjectDirectory, IsCode, out bWasGenerated);

		string Arguments = "UBT_NO_POST_DEPLOY=true";
		Arguments += " /usr/bin/xcrun xcodebuild build -project \"" + XcodeProj + "\"";
		Arguments += " -scheme '";
		Arguments += GameName;
		Arguments += " - iOS'";
		Arguments += " -configuration " + TargetConfig.ToString();
		Arguments += " -sdk iphoneos";
		Arguments += " CODE_SIGN_IDENTITY=" + (Distribution ? "\"iPhone Distribution\"" : "\"iPhone Developer\"");
		ProcessResult Result = Run ("/usr/bin/env", Arguments, null, ERunOptions.Default);
		if (bWasGenerated)
		{
			InternalUtils.SafeDeleteDirectory( XcodeProj, true);
		}
		if (Result.ExitCode != 0)
		{
			ErrorReporter.Error("CodeSign Failed", (int)ErrorCodes.Error_FailedToCodeSign);
			throw new AutomationException("CodeSign Failed");
		}
	}

	private void PackageIPA(string BaseDirectory, string GameName, string ProjectName, string ProjectDirectory, UnrealTargetConfiguration TargetConfig, bool Distribution = false)
	{
		// create the ipa
		string IPAName = CombinePaths(ProjectDirectory, "Binaries", "IOS", (Distribution ? "Distro_" : "") + ProjectName + (TargetConfig != UnrealTargetConfiguration.Development ? ("-IOS-" + TargetConfig.ToString()) : "") + ".ipa");
		// delete the old one
		if (File.Exists(IPAName))
		{
			File.Delete(IPAName);
		}

		// make the subdirectory if needed
		string DestSubdir = Path.GetDirectoryName(IPAName);
		if (!Directory.Exists(DestSubdir))
		{
			Directory.CreateDirectory(DestSubdir);
		}

		// set up the directories
		string ZipWorkingDir = String.Format("Payload/{0}.app/", GameName);
		string ZipSourceDir = string.Format("{0}/Payload/{1}.app", BaseDirectory, GameName);

		// create the file
		using (ZipFile Zip = new ZipFile())
		{
            // Set encoding to support unicode filenames
            Zip.AlternateEncodingUsage = ZipOption.Always;
            Zip.AlternateEncoding = Encoding.UTF8;

			// set the compression level
			if (Distribution)
			{
				Zip.CompressionLevel = CompressionLevel.BestCompression;
			}

			// add the entire directory
			Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

			// Update permissions to be UNIX-style
			// Modify the file attributes of any added file to unix format
			foreach (ZipEntry E in Zip.Entries)
			{
				const byte FileAttributePlatform_NTFS = 0x0A;
				const byte FileAttributePlatform_UNIX = 0x03;
				const byte FileAttributePlatform_FAT = 0x00;

				const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
				//const int UNIX_FILETYPE_SOCKET = 0xC000;
				//const int UNIX_FILETYPE_SYMLINK = 0xA000;
				//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
				const int UNIX_FILETYPE_DIRECTORY = 0x4000;
				//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
				//const int UNIX_FILETYPE_FIFO = 0x1000;

				const int UNIX_EXEC = 1;
				const int UNIX_WRITE = 2;
				const int UNIX_READ = 4;


				int MyPermissions = UNIX_READ | UNIX_WRITE;
				int OtherPermissions = UNIX_READ;

				int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
				int LowerBits = 0;

				// Try to preserve read-only if it was set
				bool bIsDirectory = E.IsDirectory;

				// Check to see if this 
				bool bIsExecutable = false;
				if (Path.GetFileNameWithoutExtension(E.FileName).Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
				{
					bIsExecutable = true;
				}

				if (bIsExecutable)
				{
					// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
					// uncompressed gives a better indicator of IPA size for our distro builds
					E.CompressionLevel = CompressionLevel.None;
				}

				if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
				{
					FileAttributes OldAttributes = E.Attributes;
					//LowerBits = ((int)E.Attributes) & 0xFFFF;

					if ((OldAttributes & FileAttributes.Directory) != 0)
					{
						bIsDirectory = true;
					}

					// Permissions
					if ((OldAttributes & FileAttributes.ReadOnly) != 0)
					{
						MyPermissions &= ~UNIX_WRITE;
						OtherPermissions &= ~UNIX_WRITE;
					}
				}

				if (bIsDirectory || bIsExecutable)
				{
					MyPermissions |= UNIX_EXEC;
					OtherPermissions |= UNIX_EXEC;
				}

				// Re-jigger the external file attributes to UNIX style if they're not already that way
				if (PlatformEncodedBy != FileAttributePlatform_UNIX)
				{
					int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

					NewAttributes |= (MyPermissions << 6);
					NewAttributes |= (OtherPermissions << 3);
					NewAttributes |= (OtherPermissions << 0);

					// Now modify the properties
					E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
				}
			}

			// Save it out
			Zip.Save(IPAName);
		}
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		//		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{
			// copy the icons/launch screens from the engine
			{
				string SourcePath = CombinePaths(SC.LocalRoot, "Engine", "Build", "IOS", "Resources", "Graphics");
				SC.StageFiles(StagedFileType.NonUFS, SourcePath, "*.png", false, null, "", true, false);
			}

			// copy any additional framework assets that will be needed at runtime
			{
				string SourcePath = CombinePaths( ( SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "\\Engine" ), "Intermediate", "IOS", "FrameworkAssets" );
				if ( Directory.Exists( SourcePath ) )
				{
					SC.StageFiles( StagedFileType.NonUFS, SourcePath, "*.*", true, null, "", true, false );
				}
			}

			// copy the icons/launch screens from the game (may stomp the engine copies)
			{
				string SourcePath = CombinePaths(SC.ProjectRoot, "Build", "IOS", "Resources", "Graphics");
				SC.StageFiles(StagedFileType.NonUFS, SourcePath, "*.png", false, null, "", true, false);
			}

			// copy the plist (only if code signing, as it's protected by the code sign blob in the executable and can't be modified independently)
			if (GetCodeSignDesirability(Params))
			{
				string SourcePath = CombinePaths((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "/Engine"), "Intermediate", "IOS");
				string TargetPListFile = Path.Combine(SourcePath, (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + "-Info.plist");
//				if (!File.Exists(TargetPListFile))
				{
					// ensure the plist, entitlements, and provision files are properly copied
					UnrealBuildTool.IOS.UEDeployIOS.GeneratePList((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "/Engine"), !SC.IsCodeBasedProject, (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game"), SC.ShortProjectName, SC.LocalRoot + "/Engine", (SC.IsCodeBasedProject ? SC.ProjectRoot : SC.LocalRoot + "/Engine") + "/Binaries/IOS/Payload/" + (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + ".app");
				}

				SC.StageFiles(StagedFileType.NonUFS, SourcePath, Path.GetFileName(TargetPListFile), false, null, "", false, false, "Info.plist");
			}
		}

		// copy the movies from the project
		{
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Build/IOS/Resources/Movies"), "*", false, null, "", true, false);
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Movies"), "*", true, null, "", true, false);
		}
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}
		var TargetConfiguration = SC.StageTargetConfigurations[0];
		var ProjectIPA = MakeIPAFileName( TargetConfiguration, Params );

		// verify the .ipa exists
		if (!FileExists(ProjectIPA))
		{
			throw new AutomationException("ARCHIVE FAILED - {0} was not found", ProjectIPA);
		}

		SC.ArchiveFiles(Path.GetDirectoryName(ProjectIPA), Path.GetFileName(ProjectIPA));
	}

	public override bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, out List<string> UFSManifests, out List<string> NonUFSManifests)
	{
		bool Result = true;
		UFSManifests = new List<string>();
		NonUFSManifests = new List<string>();
		var DeployServer = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/DeploymentServer.exe");
		try
		{
			var TargetConfiguration = SC.StageTargetConfigurations[0];
			string BundleIdentifier = "";
			if (File.Exists(Params.BaseStageDirectory + "/IOS/Info.plist"))
			{
				string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}
			RunAndLog(CmdEnv, DeployServer, "Backup -file \"" + CombinePaths(Params.BaseStageDirectory, "IOS", DeploymentContext.UFSDeployedManifestFileName) + "\" -file \"" + CombinePaths(Params.BaseStageDirectory, "IOS", DeploymentContext.NonUFSDeployedManifestFileName) + "\"" + (String.IsNullOrEmpty(Params.Device) ? "" : " -device " + Params.Device.Substring(4)) + " -bundle " + BundleIdentifier);

			string[] ManifestFiles = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, "IOS"), "*_Manifest_UFS*.txt");
			UFSManifests.AddRange(ManifestFiles);

			ManifestFiles = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, "IOS"), "*_Manifest_NonUFS*.txt");
			NonUFSManifests.AddRange(ManifestFiles);
		}
		catch (System.Exception)
		{
			// delete any files that did get copied
			string[] Manifests = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, "IOS"), "*_Manifest_*.txt");
			foreach (string Manifest in Manifests)
			{
				File.Delete(Manifest);
			}
			Result = false;
		}

		return Result;
	}

	public override void Deploy(ProjectParams Params, DeploymentContext SC)
    {
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException ("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}
		if (Params.Distribution)
		{
			throw new AutomationException("iOS cannot deploy a package made for distribution.");
		}
		var TargetConfiguration = SC.StageTargetConfigurations[0];
		var ProjectIPA = MakeIPAFileName(TargetConfiguration, Params);
		var StagedIPA = SC.StageDirectory + "\\" + Path.GetFileName(ProjectIPA);

		// verify the .ipa exists
		if (!FileExists(StagedIPA))
		{
			StagedIPA = ProjectIPA;
			if(!FileExists(StagedIPA))
			{
				throw new AutomationException("DEPLOY FAILED - {0} was not found", ProjectIPA);
			}
		}

		// if iterative deploy, determine the file delta
		string BundleIdentifier = "";
		bool bNeedsIPA = true;
		if (Params.IterativeDeploy)
		{
			if (File.Exists(Params.BaseStageDirectory + "/IOS/Info.plist"))
			{
				string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}

			// check to determine if we need to update the IPA
			String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath();
			if (File.Exists(NonUFSManifestPath))
			{
				string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
				string[] Lines = NonUFSFiles.Split('\n');
				bNeedsIPA = Lines.Length > 0 && !string.IsNullOrWhiteSpace(Lines[0]);
			}
		}

		// Add a commandline for this deploy, if the config allows it.
		string AdditionalCommandline = (Params.FileServer || Params.CookOnTheFly) ? "" : (" -additionalcommandline " + "\"" + Params.RunCommandline + "\"");

		// deploy the .ipa
		var DeployServer = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/DeploymentServer.exe");

		// check for it in the stage directory
		string CurrentDir = Directory.GetCurrentDirectory();
		Directory.SetCurrentDirectory(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/"));
		if (!Params.IterativeDeploy || bCreatedIPA || bNeedsIPA)
		{
			RunAndLog(CmdEnv, DeployServer, "Install -ipa \"" + Path.GetFullPath(StagedIPA) + "\"" + (String.IsNullOrEmpty(Params.Device) ? "" : " -device " + Params.Device.Substring(4)) + AdditionalCommandline);
		}
		if (Params.IterativeDeploy)
		{
			// push over the changed files
			RunAndLog(CmdEnv, DeployServer, "Deploy -manifest \"" + CombinePaths(Params.BaseStageDirectory, "IOS", DeploymentContext.UFSDeployDeltaFileName) + "\"" + (String.IsNullOrEmpty(Params.Device) ? "" : " -device " + Params.Device.Substring(4)) + AdditionalCommandline + " -bundle " + BundleIdentifier);
		}
		Directory.SetCurrentDirectory (CurrentDir);
        PrintRunTime();
    }

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
	{
		return "IOS";
	}

	public override bool DeployPakInternalLowerCaseFilenames()
	{
		return false;
	}

	public override bool DeployLowerCaseFilenames(bool bUFSFile)
	{
		// we shouldn't modify the case on files like Info.plist or the icons
		return bUFSFile;
	}

	public override string LocalPathToTargetPath(string LocalPath, string LocalRoot)
	{
		return LocalPath.Replace("\\", "/").Replace(LocalRoot, "../../..");
	}

	public override bool IsSupported { get { return true; } }

	public override bool LaunchViaUFE { get { return UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac; } }

	public override bool UseAbsLog
	{
		get
		{
			return !LaunchViaUFE;
		}
	}

	public override string Remap(string Dest)
	{
		return "cookeddata/" + Dest;
	}
    public override List<string> GetDebugFileExtentions()
    {
        return new List<string> { ".dsym" };
    }

	public override ProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			/*			string AppDirectory = string.Format("{0}/Payload/{1}.app",
				Path.GetDirectoryName(Params.ProjectGameExeFilename), 
				Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename));
			string GameName = Path.GetFileNameWithoutExtension (ClientApp);
			if (GameName.Contains ("-IOS-"))
			{
				GameName = GameName.Substring (0, GameName.IndexOf ("-IOS-"));
			}
			string GameApp = AppDirectory + "/" + GameName;
			bWasGenerated = false;
			XcodeProj = EnsureXcodeProjectExists (Params.RawProjectPath, CmdEnv.LocalRoot, Params.ShortProjectName, GetDirectoryName(Params.RawProjectPath), Params.IsCodeBasedProject, out bWasGenerated);
			string Arguments = "UBT_NO_POST_DEPLOY=true /usr/bin/xcrun xcodebuild test -project \"" + XcodeProj + "\"";
			Arguments += " -scheme '";
			Arguments += GameName;
			Arguments += " - iOS'";
			Arguments += " -configuration " + Params.ClientConfigsToBuild [0].ToString();
			Arguments += " -destination 'platform=iOS,id=" + Params.Device.Substring(4) + "'";
			Arguments += " TEST_HOST=\"";
			Arguments += GameApp;
			Arguments += "\" BUNDLE_LOADER=\"";
			Arguments += GameApp + "\"";*/
			string BundleIdentifier = "";
			if (File.Exists(Params.BaseStageDirectory + "/IOS/Info.plist"))
			{
				string Contents = File.ReadAllText(Params.BaseStageDirectory + "/IOS/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}
			string Arguments = "/usr/bin/instruments";
			Arguments += " -w '" + Params.Device.Substring (4) + "'";
			Arguments += " -t 'Activity Monitor'";
			Arguments += " -D \"" + Params.BaseStageDirectory + "/IOS/launch.trace\"";
			Arguments += " '" + BundleIdentifier + "'";
			ProcessResult ClientProcess = Run ("/usr/bin/env", Arguments, null, ClientRunFlags | ERunOptions.NoWaitForExit);
			return ClientProcess;
		}
		else
		{
			ProcessResult Result = new ProcessResult("DummyApp", null, false, null);
			Result.ExitCode = 0;
			return Result;
		}
	}

	public override void PostRunClient(ProcessResult Result, ProjectParams Params)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			Console.WriteLine ("Deleting " + Params.BaseStageDirectory + "/IOS/launch.trace");
			if (Directory.Exists(Params.BaseStageDirectory + "/IOS/launch.trace"))
			{
				Directory.Delete (Params.BaseStageDirectory + "/IOS/launch.trace", true);
			}

			switch (Result.ExitCode)
			{
				case 253:
					ErrorReporter.Error("Launch Failure", (int)ErrorCodes.Error_DeviceNotSetupForDevelopment);
					break;

				case 255:
					ErrorReporter.Error("Launch Failure", (int)ErrorCodes.Error_DeviceOSNewerThanSDK);
					break;
			}
		}
	}

	public override bool StageMovies
	{
		get { return false; }
	}

	public override bool RequiresPackageToDeploy
	{
		get { return true; }
	}

	public override List<string> GetFilesForCRCCheck()
	{
		List<string> FileList = base.GetFilesForCRCCheck();
		FileList.Add("Info.plist");
		return FileList;
	}

	#region Hooks

	public override void PreBuildAgenda(UE4Build Build, UE4Build.BuildAgenda Agenda)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{
			Agenda.DotNetProjects.Add(@"Engine\Source\Programs\IOS\MobileDeviceInterface\MobileDeviceInterface.csproj");
			Agenda.DotNetProjects.Add(@"Engine\Source\Programs\IOS\iPhonePackager\iPhonePackager.csproj");
			Agenda.DotNetProjects.Add(@"Engine\Source\Programs\IOS\DeploymentServer\DeploymentServer.csproj");
		}
	}

	#endregion
}
