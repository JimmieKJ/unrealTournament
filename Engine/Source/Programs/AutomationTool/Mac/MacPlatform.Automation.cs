// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

public class MacPlatform : Platform
{
	public MacPlatform()
		: base(UnrealTargetPlatform.Mac)
	{
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
	{
		const string NoEditorCookPlatform = "MacNoEditor";
		const string ServerCookPlatform = "MacServer";
		const string ClientCookPlatform = "MacClient";

		if (bDedicatedServer)
		{
			return ServerCookPlatform;
		}
		else if (bIsClientOnly)
		{
			return ClientCookPlatform;
		}
		else
		{
			return NoEditorCookPlatform;
		}
	}

	public override string GetEditorCookPlatform()
	{
		return "Mac";
	}

	private void StageAppBundle(DeploymentContext SC, StagedFileType InStageFileType, string InPath, string NewName)
	{
		SC.StageFiles(InStageFileType, InPath, "*", true, null, NewName, false, true, null);
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
	
		if (Params.StageNonMonolithic)
		{
			if (SC.DedicatedServer)
			{
				if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Development))
				{
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "UE4Server.app"));
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4Server-" + SC.ShortProjectName + ".dylib");
				}
				if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Test))
				{
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "UE4Server-Mac-Test.app"));
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4Server-" + SC.ShortProjectName + "-Mac-Test.dylib");
				}
				if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Shipping))
				{
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "UE4Server-Mac-Shipping.app"));
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4Server-" + SC.ShortProjectName + "-Mac-Shipping.dylib");
				}

				SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins"), "UE4Server-*.dylib", true);
				SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins"), "UE4Server-*.dylib", true);
			}
			else
			{
				if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Development))
				{
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "UE4.app"));
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4-" + SC.ShortProjectName + ".dylib");
				}
				if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Test))
				{
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "UE4-Mac-Test.app"));
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4-" + SC.ShortProjectName + "-Mac-Test.dylib");
				}
				if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Shipping))
				{
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "UE4-Mac-Shipping.app"));
					SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4-" + SC.ShortProjectName + "-Mac-Shipping.dylib");
				}

				SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins"), "UE4-*.dylib", true);
				SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins"), "UE4-*.dylib", true, null, null, true);
			}
		}
		else
		{
			// the first app is the "main" one, the rest are marked as debug files for exclusion from chunking/distribution
			StagedFileType WorkingFileType = StagedFileType.NonUFS;

			List<string> Exes = GetExecutableNames(SC);
			foreach (var Exe in Exes)
			{
				string AppBundlePath = "";
				if (Exe.StartsWith(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
				{
					AppBundlePath = CombinePaths(SC.ShortProjectName, "Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe) + ".app");
					StageAppBundle(SC, WorkingFileType, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe) + ".app"), AppBundlePath);
				}
				else if (Exe.StartsWith(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
				{
					AppBundlePath = CombinePaths("Engine/Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe) + ".app");

					string AbsoluteBundlePath = CombinePaths (SC.LocalRoot, AppBundlePath);
					// ensure the ue4game binary exists, if applicable
					if (!SC.IsCodeBasedProject && !Directory.Exists(AbsoluteBundlePath) && !SC.bIsCombiningMultiplePlatforms)
					{
						Log("Failed to find app bundle " + AbsoluteBundlePath);
						AutomationTool.ErrorReporter.Error("Stage Failed.", (int)AutomationTool.ErrorCodes.Error_MissingExecutable);
						throw new AutomationException("Could not find app bundle {0}. You may need to build the UE4 project with your target configuration and platform.", AbsoluteBundlePath);
					}

					StageAppBundle(SC, WorkingFileType, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe) + ".app"), AppBundlePath);
				}

				if (!string.IsNullOrEmpty(AppBundlePath))
				{
					SC.StageFiles(WorkingFileType, CombinePaths(SC.ProjectRoot, "Build/Mac"), "Application.icns", false, null, CombinePaths(AppBundlePath, "Contents/Resources"), true);

					if (Params.bUsesSteam)
					{
						SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Source/ThirdParty/Steamworks/Steamv132/sdk/redistributable_bin/osx32"), "libsteam_api.dylib", false, null, CombinePaths(AppBundlePath, "Contents/MacOS"), true);
					}
				}

				// the first app is the "main" one, the rest are marked as debug files for exclusion from chunking/distribution
				WorkingFileType = StagedFileType.DebugNonUFS;
			}
		}
		// Copy the splash screen, Mac specific
		SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Splash"), "Splash.bmp", false, null, null, true);

		// CEF3 files
		if(Params.bUsesCEF3)
		{
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/CEF3/Mac/"), "*", true, null, null, true);
			string UnrealCEFSubProcessPath = CombinePaths("Engine/Binaries", SC.PlatformDir, "UnrealCEFSubProcess.app");
			StageAppBundle(SC, StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "UnrealCEFSubProcess.app"), UnrealCEFSubProcessPath);
		}
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		// package up the program, potentially with an installer for Mac
		PrintRunTime();
	}

	public override void ProcessArchivedProject(ProjectParams Params, DeploymentContext SC)
	{
		string ExeName = SC.StageExecutables[0];
		string BundlePath = CombinePaths(SC.ArchiveDirectory, SC.ShortProjectName + ".app");

		if (SC.bIsCombiningMultiplePlatforms)
		{
			// when combining multiple platforms, don't merge the content into the .app, use the one in the Binaries directory
			BundlePath = CombinePaths(SC.ArchiveDirectory, SC.ShortProjectName, "Binaries", "Mac", ExeName + ".app");
			if (!Directory.Exists(BundlePath))
			{
				// if the .app wasn't there, just skip out (we don't require executables when combining)
				return;
			}
		}

		string TargetPath = CombinePaths(BundlePath, "Contents", "UE4");
		if (!SC.bIsCombiningMultiplePlatforms)
		{
			if (!Directory.Exists(BundlePath))
			{
				string SourceBundlePath = CombinePaths(SC.ArchiveDirectory, SC.ShortProjectName, "Binaries", "Mac", ExeName + ".app");
				if (!Directory.Exists(SourceBundlePath))
				{
					SourceBundlePath = CombinePaths(SC.ArchiveDirectory, "Engine", "Binaries", "Mac", ExeName + ".app");
				}
				Directory.Move(SourceBundlePath, BundlePath);
			}

			if (DirectoryExists(TargetPath))
			{
				Directory.Delete(TargetPath, true);
			}

			// First, move all files and folders inside he app bundle
			string[] StagedFiles = Directory.GetFiles(SC.ArchiveDirectory, "*", SearchOption.TopDirectoryOnly);
			foreach (string FilePath in StagedFiles)
			{
				string TargetFilePath = CombinePaths(TargetPath, Path.GetFileName(FilePath));
				Directory.CreateDirectory(Path.GetDirectoryName(TargetFilePath));
				File.Move(FilePath, TargetFilePath);
			}

			string[] StagedDirectories = Directory.GetDirectories(SC.ArchiveDirectory, "*", SearchOption.TopDirectoryOnly);
			foreach (string DirPath in StagedDirectories)
			{
				string DirName = Path.GetFileName(DirPath);
				if (!DirName.EndsWith(".app"))
				{
					string TargetDirPath = CombinePaths(TargetPath, DirName);
					Directory.CreateDirectory(Path.GetDirectoryName(TargetDirPath));
					Directory.Move(DirPath, TargetDirPath);
				}
			}
		}

		// Update executable name, icon and entry in Info.plist
		string UE4GamePath = CombinePaths(BundlePath, "Contents", "MacOS", ExeName);
		if (ExeName != SC.ShortProjectName && File.Exists(UE4GamePath))
		{
			string GameExePath = CombinePaths(BundlePath, "Contents", "MacOS", SC.ShortProjectName);
			File.Delete(GameExePath);
			File.Move(UE4GamePath, GameExePath);

			string DefaultIconPath = CombinePaths(BundlePath, "Contents", "Resources", "UE4.icns");
			string CustomIconSrcPath = CombinePaths(BundlePath, "Contents", "Resources", "Application.icns");
			string CustomIconDestPath = CombinePaths(BundlePath, "Contents", "Resources", SC.ShortProjectName + ".icns");
			if (File.Exists(CustomIconSrcPath))
			{
				File.Delete(DefaultIconPath);
				File.Move(CustomIconSrcPath, CustomIconDestPath);
			}
			else
			{
				File.Move(DefaultIconPath, CustomIconDestPath);
			}

			string InfoPlistPath = CombinePaths(BundlePath, "Contents", "Info.plist");
			string InfoPlistContents = File.ReadAllText(InfoPlistPath);
			InfoPlistContents = InfoPlistContents.Replace(ExeName, SC.ShortProjectName);
			InfoPlistContents = InfoPlistContents.Replace("<string>UE4</string>", "<string>" + SC.ShortProjectName + "</string>");
			File.Delete(InfoPlistPath);
			File.WriteAllText(InfoPlistPath, InfoPlistContents);
		}

		if (!SC.bIsCombiningMultiplePlatforms)
		{
			// creating this directory when the content isn't moved into the application causes it 
			// to fail to load, and isn't needed
			Directory.CreateDirectory(CombinePaths(TargetPath, ExeName.StartsWith("UE4Game") ? "Engine" : SC.ShortProjectName, "Binaries", "Mac"));
		}
	}

	public override ProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (!File.Exists(ClientApp))
		{
			if (Directory.Exists(ClientApp + ".app"))
			{
				ClientApp += ".app/Contents/MacOS/" + Path.GetFileName(ClientApp);
			}
			else
			{
				Int32 BaseDirLen = Params.BaseStageDirectory.Length;
				string StageSubDir = ClientApp.Substring(BaseDirLen, ClientApp.IndexOf("/", BaseDirLen + 1) - BaseDirLen);
				ClientApp = CombinePaths(Params.BaseStageDirectory, StageSubDir, Params.ShortProjectName + ".app/Contents/MacOS/" + Params.ShortProjectName);
			}
		}

		PushDir(Path.GetDirectoryName(ClientApp));
		// Always start client process and don't wait for exit.
		ProcessResult ClientProcess = Run(ClientApp, ClientCmdLine, null, ClientRunFlags | ERunOptions.NoWaitForExit);
		PopDir();

		return ClientProcess;
	}

	public override bool IsSupported { get { return true; } }

	public override bool ShouldUseManifestForUBTBuilds(string AddArgs)
	{
		// don't use the manifest to set up build products if we are compiling Mac under Windows and we aren't going to copy anything back to the PC
		bool bIsBuildingRemotely = UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac;
		bool bUseManifest = !bIsBuildingRemotely || AddArgs.IndexOf("-CopyAppBundleBackToDevice", StringComparison.InvariantCultureIgnoreCase) > 0;
		return bUseManifest;
	}
	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { ".dsym" };
	}
	public override bool CanHostPlatform(UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.IOS || Platform == UnrealTargetPlatform.Mac)
		{
			return true;
		}
		return false;
	}
}
