// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
			{
				WindowsSDKDir = FindWindowsSDKInstallationFolder( "v8.1" );

				if (string.IsNullOrEmpty(BaseVSToolPath))
				{
					Log.TraceError("Visual Studio 2013 must be installed in order to build this target.");
					return false;
				}
			}
			else if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2012)
			{
				WindowsSDKDir = FindWindowsSDKInstallationFolder( "v8.0" );

				if (string.IsNullOrEmpty(BaseVSToolPath))
				{
					Log.TraceError("Visual Studio 2012 must be installed in order to build this target.");
					return false;
				}
			}
			else
			{
				throw new BuildException("Unexpected WindowsPlatform.Compiler version");
			}
			return true;
		}

		public override void SetFrameworkVars()
		{
			var FrameworkDir = CommandUtils.GetEnvVar(EnvVarNames.NETFrameworkDir);
			var FrameworkVersion = CommandUtils.GetEnvVar(EnvVarNames.NETFrameworkVersion);

			if (String.IsNullOrEmpty(FrameworkDir) || String.IsNullOrEmpty(FrameworkVersion))
			{
				Log.TraceInformation("Setting .Net Framework environment variables.");

				// Determine 32 vs 64 bit. Worth noting that WOW64 will report x86.
				bool Supports64bitExecutables = Environment.GetEnvironmentVariable("PROCESSOR_ARCHITECTURE") == "AMD64";
				Log.TraceInformation("Supports64bitExecutables=" + Supports64bitExecutables.ToString());
				Log.TraceInformation("WindowsPlatform.Compiler" + WindowsPlatform.Compiler.ToString());

				string WindowsSDKDir, BaseVSToolPath;
				if (FindBaseVSToolPaths(out WindowsSDKDir, out BaseVSToolPath) == false)
				{
					throw new AutomationException("Unable to find VS paths.");
				}

				Log.TraceInformation("WindowsSDKDir=" + WindowsSDKDir);
				Log.TraceInformation("BaseVSToolPath=" + BaseVSToolPath);

				string VCVarsBatchFile = "";

				// 64 bit tool chain.
				if (Supports64bitExecutables)
				{
					VCVarsBatchFile = CommandUtils.CombinePaths(BaseVSToolPath, "../../VC/bin/x86_amd64/vcvarsx86_amd64.bat");
				}
				// The 32 bit vars batch file in the binary folder simply points to the one in the common tools folder.
				else
				{
					VCVarsBatchFile = CommandUtils.CombinePaths(BaseVSToolPath, "vsvars32.bat");
				}

				if (File.Exists(VCVarsBatchFile))
				{
					Log.TraceInformation("Setting VS environment variables via {0}.", VCVarsBatchFile);
					var BatchVars = CommandUtils.GetEnvironmentVariablesFromBatchFile(VCVarsBatchFile);
					// Note these env var names are hardcoded here because they're more
					// related to the values stored inside the batch files rather than the mapping equivalents.
					BatchVars.TryGetValue("FrameworkDir", out FrameworkDir);
					BatchVars.TryGetValue("FrameworkVersion", out FrameworkVersion);
					CommandUtils.SetEnvVar(EnvVarNames.NETFrameworkDir, FrameworkDir);
					CommandUtils.SetEnvVar(EnvVarNames.NETFrameworkVersion, FrameworkVersion);
				}
				else
				{
					throw new AutomationException("{0} does not exist.", VCVarsBatchFile);
				}
				Log.TraceInformation("{0}={1}", EnvVarNames.NETFrameworkDir, FrameworkDir);
				Log.TraceInformation("{0}={1}", EnvVarNames.NETFrameworkVersion, FrameworkVersion);
			}
		}

		public override string GetMsBuildExe()
		{
			var DotNETToolsFolder = CommandUtils.CombinePaths(CommandUtils.GetEnvVar(EnvVarNames.NETFrameworkDir), CommandUtils.GetEnvVar(EnvVarNames.NETFrameworkVersion));
			var MsBuildExe = CommandUtils.CombinePaths(DotNETToolsFolder, "MSBuild.exe");
			if (!CommandUtils.FileExists_NoExceptions(MsBuildExe))
			{				
				throw new NotSupportedException("MsBuild.exe does not exist.");
			}
			return MsBuildExe;
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
