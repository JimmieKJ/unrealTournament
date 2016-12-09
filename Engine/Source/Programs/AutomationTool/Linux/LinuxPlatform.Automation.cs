// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Diagnostics;
using AutomationTool;
using UnrealBuildTool;


public abstract class BaseLinuxPlatform : Platform
{
	static string PScpPath = CombinePaths(CommandUtils.RootDirectory.FullName, "\\Engine\\Extras\\ThirdPartyNotUE\\putty\\PSCP.EXE");
	static string PlinkPath = CombinePaths(CommandUtils.RootDirectory.FullName, "\\Engine\\Extras\\ThirdPartyNotUE\\putty\\PLINK.EXE");
	static string LaunchOnHelperShellScriptName = "LaunchOnHelper.sh";

	public BaseLinuxPlatform(UnrealTargetPlatform P)
		: base(P)
	{
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.bStageCrashReporter)
		{
			string ReceiptFileName = TargetReceipt.GetDefaultPath(UnrealBuildTool.UnrealBuildTool.EngineDirectory.FullName, "CrashReportClient", SC.StageTargetPlatform.PlatformType, UnrealTargetConfiguration.Shipping, null);
			if (File.Exists(ReceiptFileName))
			{
				TargetReceipt Receipt = TargetReceipt.Read(ReceiptFileName);
				Receipt.ExpandPathVariables(UnrealBuildTool.UnrealBuildTool.EngineDirectory, (Params.RawProjectPath == null) ? UnrealBuildTool.UnrealBuildTool.EngineDirectory : Params.RawProjectPath.Directory);
				SC.StageBuildProductsFromReceipt(Receipt, true, false);
			}
		}

		// Stage all the build products
		Console.WriteLine("Staging all {0} build products", SC.StageTargets.Count);
		int BuildProductIdx = 0;
		foreach (StageTarget Target in SC.StageTargets)
		{
			Console.WriteLine(" Product {0}: {1}", BuildProductIdx, Target.Receipt.TargetName);
			SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.bTreatNonShippingBinariesAsDebugFiles);
			++BuildProductIdx;
        }

		SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Splash"), "Splash.bmp", false, null, null, true);

		// Stage the bootstrap executable
		if (!Params.NoBootstrapExe)
		{
			foreach (StageTarget Target in SC.StageTargets)
			{
				BuildProduct Executable = Target.Receipt.BuildProducts.FirstOrDefault(x => x.Type == BuildProductType.Executable);
				if (Executable != null)
				{
					// only create bootstraps for executables
					string FullExecutablePath = Path.GetFullPath(Executable.Path);
					if (Executable.Path.Replace("\\", "/").Contains("/" + TargetPlatformType.ToString() + "/"))
					{
						string BootstrapArguments = "";
						if (!SC.IsCodeBasedProject && !ShouldStageCommandLine(Params, SC))
						{
							BootstrapArguments = String.Format("\\\"../../../{0}/{0}.uproject\\\"", SC.ShortProjectName);
						}

						string BootstrapExeName;
						if (SC.StageTargetConfigurations.Count > 1)
						{
							BootstrapExeName = Path.GetFileName(Executable.Path);
						}
						else if (Params.IsCodeBasedProject)
						{
							BootstrapExeName = Target.Receipt.TargetName;
						}
						else
						{
							BootstrapExeName = SC.ShortProjectName;
						}

						foreach (string StagePath in SC.NonUFSStagingFiles[FullExecutablePath])
						{
							StageBootstrapExecutable(SC, BootstrapExeName + ".sh", FullExecutablePath, StagePath, BootstrapArguments);
						}
					}
				}
			}
		}
	}

	public override bool ShouldStageCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		return false;
	}

	void StageBootstrapExecutable(DeploymentContext SC, string ExeName, string TargetFile, string StagedRelativeTargetPath, string StagedArguments)
	{
		// create a temp script file location
		string IntermediateDir = CombinePaths(SC.ProjectRoot, "Intermediate", "Staging");
		string IntermediateFile = CombinePaths(IntermediateDir, ExeName);
		InternalUtils.SafeCreateDirectory(IntermediateDir);

		// make sure slashes are good
		StagedRelativeTargetPath = StagedRelativeTargetPath.Replace("\\", "/");

		// make contents
		StringBuilder Script = new StringBuilder();
		string EOL = "\n";
		Script.Append("#!/bin/sh" + EOL);
		Script.AppendFormat("chmod +x \"$(dirname $0)/{0}\"" + EOL, StagedRelativeTargetPath);
		Script.AppendFormat("\"$(dirname $0)/{0}\" {1} $@" + EOL, StagedRelativeTargetPath, StagedArguments);

		// write out the 
		File.WriteAllText(IntermediateFile, Script.ToString());

		if (Utils.IsRunningOnMono)
		{
			var Result = CommandUtils.Run("sh", string.Format("-c 'chmod +x \\\"{0}\\\"'", IntermediateFile));
			if (Result.ExitCode != 0)
			{
				throw new AutomationException(string.Format("Failed to chmod \"{0}\"", IntermediateFile));
			}
		}

		SC.StageFiles(StagedFileType.NonUFS, IntermediateDir, ExeName, false, null, "");
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		const string NoEditorCookPlatform = "LinuxNoEditor";
		const string ServerCookPlatform = "LinuxServer";
		const string ClientCookPlatform = "LinuxClient";

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
		return "Linux";
	}

	/// <summary>
	/// return true if we need to change the case of filenames outside of pak files
	/// </summary>
	/// <returns></returns>
	public override bool DeployLowerCaseFilenames(bool bUFSFile)
	{
		return false;
	}

	/// <summary>
	/// Deploy the application on the current platform
	/// </summary>
	/// <param name="Params"></param>
	/// <param name="SC"></param>
	public override void Deploy(ProjectParams Params, DeploymentContext SC)
	{
		if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux)
		{
			foreach (string DeviceAddress in Params.DeviceNames)
			{
				string CookPlatformName = GetCookPlatform(Params.DedicatedServer, Params.Client);
				string SourcePath = CombinePaths(Params.BaseStageDirectory, CookPlatformName);
				string DestPath = "./" + Params.ShortProjectName;
				List<string> Exes = GetExecutableNames(SC);
				string BinaryName = "";
				if (Exes.Count > 0)
				{
					BinaryName = Exes[0].Replace(Params.BaseStageDirectory, DestPath);
					BinaryName = BinaryName.Replace("\\", "/");
				}

				// stage a shell script that makes running easy
				string Script = String.Format(@"#!/bin/bash
export DISPLAY=:0
chmod +x {0}
{0} $@
", BinaryName, BinaryName);
				Script = Script.Replace("\r\n", "\n");
				string ScriptFile = Path.Combine(SourcePath, "..", LaunchOnHelperShellScriptName);
				File.WriteAllText(ScriptFile, Script);

				if (!Params.IterativeDeploy)
				{
					// non-null input is essential, since RedirectStandardInput=true is needed for PLINK, see http://stackoverflow.com/questions/1910592/process-waitforexit-on-console-vs-windows-forms
					RunAndLog(CmdEnv, PlinkPath, String.Format("-batch -l {0} -pw {1} {2} rm -rf {3} && mkdir -p {3}", Params.DeviceUsername, Params.DevicePassword, Params.DeviceNames[0], DestPath), Input: "");
				}

				// copy the contents
				RunAndLog(CmdEnv, PScpPath, String.Format("-batch -pw {0} -r {1} {2}", Params.DevicePassword, SourcePath, Params.DeviceUsername + "@" + DeviceAddress + ":" + DestPath));

				// copy the helper script
				RunAndLog(CmdEnv, PScpPath, String.Format("-batch -pw {0} -r {1} {2}", Params.DevicePassword, ScriptFile, Params.DeviceUsername + "@" + DeviceAddress + ":" + DestPath));

				string RemoteScriptFile = DestPath + "/" + LaunchOnHelperShellScriptName;
				// non-null input is essential, since RedirectStandardInput=true is needed for PLINK, see http://stackoverflow.com/questions/1910592/process-waitforexit-on-console-vs-windows-forms
				RunAndLog(CmdEnv, PlinkPath, String.Format(" -batch -l {0} -pw {1} {2} chmod +x {3}", Params.DeviceUsername, Params.DevicePassword, Params.DeviceNames[0], RemoteScriptFile), Input: "");
			}
		}
	}

	// -abslog only makes sense when running on the native platform and not remotely
	public override bool UseAbsLog { get { return BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux; } }

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		// package up the program
		PrintRunTime();
	}

	public override bool CanHostPlatform(UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64)
		{
			return false;
		}
		return true;
	}

	/// <summary>
	/// Allow the platform to alter the ProjectParams
	/// </summary>
	/// <param name="ProjParams"></param>
	public override void PlatformSetupParams(ref ProjectParams ProjParams)
	{
		if ((ProjParams.Deploy || ProjParams.Run) && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux)
		{ 
			// Prompt for username if not already set
			while (String.IsNullOrEmpty(ProjParams.DeviceUsername))
			{
				Console.Write("Username: ");
				ProjParams.DeviceUsername = Console.ReadLine();
			}

			// Prompty for password if not already set
			while (String.IsNullOrEmpty(ProjParams.DevicePassword))
			{
				ProjParams.DevicePassword = String.Empty;
				Console.Write("Password: ");
				ConsoleKeyInfo key;
				do
				{
					key = Console.ReadKey(true);
					if (key.Key != ConsoleKey.Backspace && key.Key != ConsoleKey.Enter)
					{
						ProjParams.DevicePassword += key.KeyChar;
						Console.Write("*");
					}
					else
					{
						if (key.Key == ConsoleKey.Backspace && ProjParams.DevicePassword.Length > 0)
						{
							ProjParams.DevicePassword = ProjParams.DevicePassword.Substring(0, (ProjParams.DevicePassword.Length - 1));
							Console.Write("\b \b");
						}
					}

				} while (key.Key != ConsoleKey.Enter);
				Console.WriteLine();
			}

			// try contacting the device(s) and cache the key(s)
			foreach(string DeviceAddress in ProjParams.DeviceNames)
			{
				RunAndLog(CmdEnv, "cmd.exe", String.Format("/c \"echo y | {0} -ssh -t -l {1} -pw {2} {3} echo All Ok\"", PlinkPath, ProjParams.DeviceUsername, ProjParams.DevicePassword, DeviceAddress));
			}
		}
	}
	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux)
		{
			IProcessResult Result = null;
			foreach (string DeviceAddress in Params.DeviceNames)
			{
				// Use the helper script (should already be +x - see Deploy)
				string RemoteBasePath = "./" + Params.ShortProjectName;
				string RemotePathToBinary = RemoteBasePath + "/" + LaunchOnHelperShellScriptName;
				// non-null input is essential, since RedirectStandardInput=true is needed for PLINK, see http://stackoverflow.com/questions/1910592/process-waitforexit-on-console-vs-windows-forms
				Result = Run(PlinkPath, String.Format("-batch -ssh -t -l {0} -pw {1} {2}  {3} {4}", Params.DeviceUsername, Params.DevicePassword, Params.DeviceNames[0], RemotePathToBinary, ClientCmdLine), "");
			}
			return Result;
		}
		else
		{
			return base.RunClient(ClientRunFlags, ClientApp, ClientCmdLine, Params);
		}
	}


	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { };
	}

	public override bool IsSupported { get { return true; } }

}


public class GenericLinuxPlatform : BaseLinuxPlatform
{
	public GenericLinuxPlatform()
		: base(UnrealTargetPlatform.Linux)
	{
	}
}
