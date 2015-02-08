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

		//todo we need to support shipping and test executables
		//todo this should all be partially based on UBT manifests and not hard coded
		//monolithic assumption
		StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Ogg", SC.PlatformDir), "*.", true);
		StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Vorbis", SC.PlatformDir), "*.", true);
		string PhysXVer = "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
		string ApexVer = "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
		string PhysXMaskForDebugConfiguration = Params.bDebugBuildsActuallyUseDebugCRT ? "*DEBUG*.*" : "*PROFILE*.*";
		if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Debug) && !Params.Rocket)
		{
			StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/PhysX/APEX-1.3", SC.PlatformDir, ApexVer), PhysXMaskForDebugConfiguration, true);
			StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/PhysX/PhysX-3.3", SC.PlatformDir, PhysXVer), PhysXMaskForDebugConfiguration, true);
			StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/PhysX/PhysX-3.3", SC.PlatformDir, PhysXVer), "nvToolsExt*.", true);
		}
		if (SC.StageTargetConfigurations.Any(x => x != UnrealTargetConfiguration.Debug) || Params.Rocket)
		{
			StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/PhysX/APEX-1.3", SC.PlatformDir, ApexVer), "*.", true, new string[] { "*DEBUG*.*", "*CHECKED*.*" });
			StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/PhysX/PhysX-3.3", SC.PlatformDir, PhysXVer), "*.", true, new string[] { "*DEBUG*.*", "*CHECKED*.*" });
		}

		if (Params.bUsesSteam)
		{
			string SteamVersion = "Steamv130";

			// Check that the TPS directory exists. We don't distribute binaries for Steam in Rocket.
			if (Directory.Exists(CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion)))
			{
				if (SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Win32)
				{
					StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "steam_api.");
					if (SC.DedicatedServer)
					{
						StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "steamclient.");
						StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "tier0_s.");
						StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "vstdlib_s.");
					}
				}
				else
				{
					StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "steam_api64.");
					if (SC.DedicatedServer)
					{
						StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "steamclient64.");
						StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "tier0_s64.");
						StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "vstdlib_s64.");
					}
				}
			}
		}

		// Copy the splash screen, windows specific
		SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Splash"), "Splash.bmp", false, null, null, true);

		// CEF3 files
		SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/CEF3", SC.PlatformDir), "*", true, null, null, true);
		StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UnrealCEFSubProcess.", false, null, null, true);

        if (Params.StageNonMonolithic)
        {
            StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/ICU/icu4c-53_1", SC.PlatformDir, "VS2013"), "*.");

            if (SC.DedicatedServer)
            {
                if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Development))
                {
                    StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4Server.");
                }

                if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Test))
                {
                    StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4Server*-Test.");
                }

                if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Shipping))
                {
                    StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4Server*-Shipping.");
                }

                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4Server-*.");
                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Plugins"), "UE4Server-*.", true);
                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4Server-*.");
                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.ProjectRoot, "Plugins"), "UE4Server-*.", true);
            }
            else
            {
                if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Development))
                {
                    StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4.");
                }

                if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Test))
                {
                    StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4*-Test.");
                }

                if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Shipping))
                {
                    StageExecutable("exe", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4*-Shipping.");
                }

                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4-*.");
                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Plugins"), "UE4-*.", true);
                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "UE4-*.");
                StageExecutable("dll", SC, CommandUtils.CombinePaths(SC.ProjectRoot, "Plugins"), "UE4-*.", true);
            }
        }
        else
        {
			List<string> Exes = GetExecutableNames(SC);

			// the first exe is the "main" one, the rest are marked as debug files
			StagedFileType WorkingFileType = StagedFileType.NonUFS;
				
		    foreach (var Exe in Exes)
            {
				if (Exe.StartsWith(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
				{
					string BaseExeFileName = Path.GetFileName(Exe);
					if (!String.IsNullOrEmpty(Params.OverrideMinimumOS) && (Params.OverrideMinimumOS == "WinXP"))
					{
						BaseExeFileName = Path.GetFileNameWithoutExtension(Exe) + "_xp" + Path.GetExtension(Exe);
					}

					// remap the project root.
					string SourceFile = CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir, BaseExeFileName);
					StageExecutable("exe", SC, Path.GetDirectoryName(SourceFile), Path.GetFileNameWithoutExtension(SourceFile) + ".", true, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir), false, WorkingFileType);
					if(Exe == Exes[0] && !Params.NoBootstrapExe)
					{
						StageBootstrapExecutable(SC, SourceFile, CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir, BaseExeFileName), "");
					}
				}
				else if (Exe.StartsWith(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
				{
					// Select the appropriate windows executable to stage, xp support or not.
					string ExeFileName = Path.GetFileName(Exe);
					if (!String.IsNullOrEmpty(Params.OverrideMinimumOS) && (Params.OverrideMinimumOS == "WinXP"))
					{
						ExeFileName = Path.GetFileNameWithoutExtension(ExeFileName) + "_xp" + Path.GetExtension(ExeFileName);
					}

					// keep it in the engine directory.
					string SourceFile = CombinePaths(SC.LocalRoot, "Engine", "Binaries", SC.PlatformDir, ExeFileName);

					// ensure the ue4game binary exists, if applicable
					if (!SC.IsCodeBasedProject && !FileExists_NoExceptions(SourceFile))
					{
						Log("Failed to find game executable " + SourceFile);
						AutomationTool.ErrorReporter.Error("Stage Failed.", (int)AutomationTool.ErrorCodes.Error_MissingExecutable);
						throw new AutomationException("Could not find exe {0}. You may need to build the UE4 project with your target configuration and platform.", SourceFile);
					}

					StageExecutable("exe", SC, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(SourceFile) + ".", true, null, null, false, WorkingFileType);
					if(Exe == Exes[0] && !Params.NoBootstrapExe)
					{
						StageBootstrapExecutable(SC, SourceFile, CombinePaths("Engine", "Binaries", SC.PlatformDir, ExeFileName), String.Format("..\\..\\..\\{0}\\{0}.uproject", SC.ShortProjectName));
					}
				}
				else
				{
					throw new AutomationException("Can't stage the exe {0} because it doesn't start with {1} or {2}", Exe, CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir), CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir));
				}
				// the first exe is the "main" one, the rest are marked as debug files
				WorkingFileType = StagedFileType.DebugNonUFS;
			}
		}
	}

	void StageBootstrapExecutable(DeploymentContext SC, string TargetFile, string StagedRelativeTargetPath, string StagedArguments)
	{
		string InputFile = CombinePaths(SC.LocalRoot, "Engine", "Binaries", SC.PlatformDir, String.Format("BootstrapPackagedGame-{0}-Shipping.exe", SC.PlatformDir));
		if(InternalUtils.SafeFileExists(InputFile))
		{
			// Create the new bootstrap program
			string ExeName = String.Format("{0}.exe", SC.ShortProjectName);

			string IntermediateDir = CombinePaths(SC.ProjectRoot, "Intermediate", "Staging");
			InternalUtils.SafeCreateDirectory(IntermediateDir);

			string IntermediateFile = CombinePaths(IntermediateDir, ExeName);
			File.Copy(InputFile, IntermediateFile, true);
	
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
