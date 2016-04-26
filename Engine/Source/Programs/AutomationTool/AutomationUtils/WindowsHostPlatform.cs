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
		/// <returns>The path to Windows SDK directory for the specified version.</returns>
		private static string FindWindowsSDKInstallationFolder( string Version )
		{
			// Based on VCVarsQueryRegistry
			string WinSDKPath = (string)Microsoft.Win32.Registry.GetValue( @"HKEY_CURRENT_USER\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null );
			if( WinSDKPath != null )
			{
				return WinSDKPath;
			}
			WinSDKPath = (string)Microsoft.Win32.Registry.GetValue( @"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null );
			if( WinSDKPath != null )
			{
				return WinSDKPath;
			}
			WinSDKPath = (string)Microsoft.Win32.Registry.GetValue( @"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null );
			if( WinSDKPath != null )
			{
				return WinSDKPath;
			}

			throw new BuildException( "Can't find Windows SDK {0}.", Version );
		}

		/// <summary>
		/// Figures out the path to Visual Studio's /Common7/Tools directory.  Note that if Visual Studio is not
		/// installed, the Windows SDK path will be used, which also happens to be the same directory. (It installs
		/// the toolchain into the folder where Visual Studio would have installed it to.
		/// </summary>
		/// <returns>The path to Visual Studio's /Common7/Tools directory</returns>
		private bool FindBaseVSToolPaths(out string WindowsSDKDir, out string BaseVSToolPath)
		{
			WindowsSDKDir = "";
			BaseVSToolPath = WindowsPlatform.GetVSComnToolsPath();

			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
			{
				WindowsSDKDir = FindWindowsSDKInstallationFolder( "v8.1" );

				if (string.IsNullOrEmpty(BaseVSToolPath))
				{
					Log.TraceError("Visual Studio 2015 must be installed in order to build this target.");
					return false;
				}
			}
			else if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
			{
				WindowsSDKDir = FindWindowsSDKInstallationFolder( "v8.1" );

				if (string.IsNullOrEmpty(BaseVSToolPath))
				{
					Log.TraceError("Visual Studio 2013 must be installed in order to build this target.");
					return false;
				}
			}
			else
			{
				throw new BuildException("Unexpected WindowsPlatform.Compiler version");
			}
			return true;
		}

		public override string GetMsBuildExe()
		{
			return VCEnvironment.GetMSBuildToolPath();
		}

		public override string GetMsDevExe()
		{
			// Locate MsDevEnv executable
			string PotentialMsDevExe = Path.Combine(WindowsPlatform.GetVSComnToolsPath(), "..", "IDE", "Devenv.com");

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
