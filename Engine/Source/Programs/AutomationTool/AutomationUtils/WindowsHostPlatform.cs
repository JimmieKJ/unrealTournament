// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using UnrealBuildTool;

namespace AutomationTool
{
	class WindowsHostPlatform : HostPlatform
	{
		public override string GetMsBuildExe()
		{
			return VCEnvironment.GetMSBuildToolPath();
		}

		public override string GetMsDevExe()
		{
			WindowsCompiler Compiler = WindowsPlatform.GetDefaultCompiler(new string[0], null);

			// Locate MsDevEnv executable
			DirectoryReference VSInstallDir;
			if(!WindowsPlatform.TryGetVSInstallDir(Compiler, out VSInstallDir))
			{
				throw new NotSupportedException("Couldn't find installation of Visual Studio");
			}

			string PotentialMsDevExe = Path.Combine(VSInstallDir.FullName, "Common7", "IDE", "Devenv.com");

			var Info = new System.IO.FileInfo(PotentialMsDevExe);
			if (Info.Exists)
			{
				return PotentialMsDevExe;
			}
			else
			{				
				throw new NotSupportedException(String.Format("{0} does not exist.", PotentialMsDevExe));
			}
		}

		public override string RelativeBinariesFolder
		{
			get { return @"Engine/Binaries/Win64/"; }
		}

		public override string GetUE4ExePath(string UE4Exe)
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, RelativeBinariesFolder, UE4Exe);
		}

		public override string LocalBuildsLogFolder
		{
			get { return CommandUtils.CombinePaths(PathSeparator.Slash, Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Unreal Engine", "AutomationTool", "Logs"); }
		}

		public override string P4Exe
		{
			get { return "p4.exe"; }
		}

		public override Process CreateProcess(string AppName)
		{
			var NewProcess = new Process();
			return NewProcess;
		}

		public override void SetupOptionsForRun(ref string AppName, ref CommandUtils.ERunOptions Options, ref string CommandLine)
		{
		}

		public override void SetConsoleCtrlHandler(ProcessManager.CtrlHandlerDelegate Handler)
		{
			ProcessManager.SetConsoleCtrlHandler(Handler, true);
		}

		public override bool IsScriptModuleSupported(string ModuleName)
		{
			return true;
		}

		public override string UBTProjectName
		{
			get { return "UnrealBuildTool"; }
		}

		public override UnrealTargetPlatform HostEditorPlatform
		{
			get { return UnrealTargetPlatform.Win64; }
		}

		public override string PdbExtension
		{
			get { return ".pdb"; }
		}

		static string[] SystemServices = new string[]
		{
			"winlogon",
			"system idle process",
			"taskmgr",
			"spoolsv",
			"csrss",
			"smss",
			"svchost",
			"services",
			"lsass",
            "conhost",
            "oobe",
            "mmc"
		};
		public override string[] DontKillProcessList
		{
			get 
			{
				return SystemServices;
			}
		}
	}
}
