// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;


public abstract class BaseWinPlatform : Platform
{
	public BaseWinPlatform(UnrealTargetPlatform P)
		: base(P)
	{
	}

	private int StageExecutable(string Ext, DeploymentContext SC, string InPath, string Wildcard = "*", bool bRecursive = true, string[] ExcludeWildcard = null, string NewPath = null, bool bAllowNone = false, StagedFileType InStageFileType = StagedFileType.NonUFS, string NewName = null)
	{
		int Result = SC.StageFiles(InStageFileType, InPath, Wildcard + Ext, bRecursive, ExcludeWildcard, NewPath, bAllowNone, true, (NewName == null) ? null : (NewName + Ext));
		if (Result > 0)
		{
			SC.StageFiles(StagedFileType.DebugNonUFS, InPath, Wildcard + "pdb", bRecursive, ExcludeWildcard, NewPath, true, true, (NewName == null) ? null : (NewName + "pdb"));
			SC.StageFiles(StagedFileType.DebugNonUFS, InPath, Wildcard + "map", bRecursive, ExcludeWildcard, NewPath, true, true, (NewName == null) ? null : (NewName + "map"));
		}
		return Result;
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// Engine non-ufs (binaries)

		if (SC.bStageCrashReporter)
		{
			StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "CrashReportClient.");
		}

		// Stage all the build products
		foreach(BuildReceipt Receipt in SC.StageTargetReceipts)
		{
			SC.StageBuildProductsFromReceipt(Receipt);
		}

		// Copy the splash screen, windows specific
		SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Splash"), "Splash.bmp", false, null, null, true);

		// Stage the bootstrap executable
		if(!Params.NoBootstrapExe)
		{
			foreach(BuildReceipt Receipt in SC.StageTargetReceipts)
			{
				BuildProduct Executable = Receipt.BuildProducts.FirstOrDefault(x => x.Type == BuildProductType.Executable);
				if(Executable != null)
				{
					// only create bootstraps for executables
					if (SC.NonUFSStagingFiles.ContainsKey(Executable.Path) && Path.GetExtension(Executable.Path) == ".exe")
					{
						string BootstrapArguments = "";
						if (!SC.IsCodeBasedProject && !ShouldStageCommandLine(Params, SC))
						{
							BootstrapArguments = String.Format("..\\..\\..\\{0}\\{0}.uproject", SC.ShortProjectName);
						}

						string BootstrapExeName;
						if(SC.StageTargetConfigurations.Count > 1)
						{
							BootstrapExeName = Path.GetFileName(Executable.Path);
						}
						else if(Params.IsCodeBasedProject)
						{
							BootstrapExeName = Receipt.GetProperty("TargetName", SC.ShortProjectName) + ".exe";
						}
						else
						{
							BootstrapExeName = SC.ShortProjectName + ".exe";
						}

						StageBootstrapExecutable(SC, BootstrapExeName, Executable.Path, SC.NonUFSStagingFiles[Executable.Path], BootstrapArguments);
					}
				}
			}
		}
	}

	void StageBootstrapExecutable(DeploymentContext SC, string ExeName, string TargetFile, string StagedRelativeTargetPath, string StagedArguments)
	{
		string InputFile = CombinePaths(SC.LocalRoot, "Engine", "Binaries", SC.PlatformDir, String.Format("BootstrapPackagedGame-{0}-Shipping.exe", SC.PlatformDir));
		if(InternalUtils.SafeFileExists(InputFile))
		{
			// Create the new bootstrap program
			string IntermediateDir = CombinePaths(SC.ProjectRoot, "Intermediate", "Staging");
			InternalUtils.SafeCreateDirectory(IntermediateDir);

			string IntermediateFile = CombinePaths(IntermediateDir, ExeName);
			File.Copy(InputFile, IntermediateFile, true);
	
			// currently the icon updating doesn't run under mono
			if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 ||
				UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32)
			{
				// Get the icon from the build directory if possible
				GroupIconResource GroupIcon = null;
				if(InternalUtils.SafeFileExists(CombinePaths(SC.ProjectRoot, "Build/Windows/Application.ico")))
				{
					GroupIcon = GroupIconResource.FromIco(CombinePaths(SC.ProjectRoot, "Build/Windows/Application.ico"));
				}
				if(GroupIcon == null)
				{
					GroupIcon = GroupIconResource.FromExe(TargetFile);
				}

				// Update the resources in the new file
				using(ModuleResourceUpdate Update = new ModuleResourceUpdate(IntermediateFile, true))
				{
					const int IconResourceId = 101;
					if(GroupIcon != null) Update.SetIcons(IconResourceId, GroupIcon);

					const int ExecFileResourceId = 201;
					Update.SetData(ExecFileResourceId, ResourceType.RawData, Encoding.Unicode.GetBytes(StagedRelativeTargetPath + "\0"));

					const int ExecArgsResourceId = 202;
					Update.SetData(ExecArgsResourceId, ResourceType.RawData, Encoding.Unicode.GetBytes(StagedArguments + "\0"));
				}
			}

			// Copy it to the staging directory
			SC.StageFiles(StagedFileType.NonUFS, IntermediateDir, ExeName, false, null, "");
		}
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
	{
		const string NoEditorCookPlatform = "WindowsNoEditor";
		const string ServerCookPlatform = "WindowsServer";
		const string ClientCookPlatform = "WindowsClient";

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
		return "Windows";
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		// package up the program, potentially with an installer for Windows
		PrintRunTime();
	}

	public override bool CanHostPlatform(UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.Mac)
		{
			return false;
		}
		return true;
	}

	public override List<string> GetExecutableNames(DeploymentContext SC, bool bIsRun = false)
	{
		var ExecutableNames = new List<String>();
		string Ext = AutomationTool.Platform.GetExeExtension(TargetPlatformType);
		if (!String.IsNullOrEmpty(SC.CookPlatform))
		{
			if (SC.StageExecutables.Count() > 0)
			{
				foreach (var StageExecutable in SC.StageExecutables)
				{
					string ExeName = SC.StageTargetPlatform.GetPlatformExecutableName(StageExecutable);
					if(SC.IsCodeBasedProject)
					{
						ExecutableNames.Add(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir, ExeName + Ext));
					}
					else
					{
						ExecutableNames.Add(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir, ExeName + Ext));
					}
				}
			}
			//@todo, probably the rest of this can go away once everything passes it through
			else if (SC.DedicatedServer)
			{
				if (!SC.IsCodeBasedProject)
				{
					string ExeName = SC.StageTargetPlatform.GetPlatformExecutableName("UE4Server");
					ExecutableNames.Add(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir, ExeName + Ext));
				}
				else
				{
					string ExeName = SC.StageTargetPlatform.GetPlatformExecutableName(SC.ShortProjectName + "Server");
					string ClientApp = CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir, ExeName + Ext);
					var TestApp = CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir, SC.ShortProjectName + "Server" + Ext);
					string Game = "Game";
					//@todo, this is sketchy, someone might ask what the exe is before it is compiled
					if (!FileExists_NoExceptions(ClientApp) && !FileExists_NoExceptions(TestApp) && SC.ShortProjectName.EndsWith(Game, StringComparison.InvariantCultureIgnoreCase))
					{
						ExeName = SC.StageTargetPlatform.GetPlatformExecutableName(SC.ShortProjectName.Substring(0, SC.ShortProjectName.Length - Game.Length) + "Server");
						ClientApp = CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir, ExeName + Ext);
					}
					ExecutableNames.Add(ClientApp);
				}
			}
			else
			{
				if (!SC.IsCodeBasedProject && !bIsRun)
				{
					string ExeName = SC.StageTargetPlatform.GetPlatformExecutableName("UE4Game");
					ExecutableNames.Add(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir, ExeName + Ext));
				}
				else
				{
					string ExeName = SC.StageTargetPlatform.GetPlatformExecutableName(SC.ShortProjectName);
					ExecutableNames.Add(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir, ExeName + Ext));
				}
			}
		}
		else
		{
			string ExeName = SC.StageTargetPlatform.GetPlatformExecutableName("UE4Editor");
			ExecutableNames.Add(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir, ExeName + Ext));
		}
		return ExecutableNames;
	}

	public override bool ShouldStageCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		return !String.IsNullOrEmpty(Params.StageCommandline) || !String.IsNullOrEmpty(Params.RunCommandline) || (!Params.IsCodeBasedProject && Params.NoBootstrapExe);
	}

	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { ".pdb", ".map" };
	}
}

public class Win64Platform : BaseWinPlatform
{
	public Win64Platform()
		: base(UnrealTargetPlatform.Win64)
	{
	}

	public override bool IsSupported { get { return true; } }

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		base.GetFilesToDeployOrStage(Params, SC);
		
		if(Params.Prereqs)
		{
			string InstallerRelativePath = CombinePaths("Engine", "Extras", "Redist", "en-us");
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, InstallerRelativePath), "UE4PrereqSetup_x64.exe", false, null, InstallerRelativePath);
		}
	}
}

public class Win32Platform : BaseWinPlatform
{
	public Win32Platform()
		: base(UnrealTargetPlatform.Win32)
	{
	}

	public override bool IsSupported { get { return true; } }

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		base.GetFilesToDeployOrStage(Params, SC);
		
		if(Params.Prereqs)
		{
			string InstallerRelativePath = CombinePaths("Engine", "Extras", "Redist", "en-us");
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, InstallerRelativePath), "UE4PrereqSetup_x86.exe", false, null, InstallerRelativePath);
		}
	}
}
