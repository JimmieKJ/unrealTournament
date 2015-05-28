// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// This software is provided "as-is," without any express or implied warranty. 
// In no event shall the author, nor Epic Games, Inc. be held liable for any damages arising from the use of this software.
// This software will not be supported.
// Use at your own risk.
using System;
using System.Threading;
using System.Diagnostics;
using UnrealBuildTool;
using System.Reflection;

namespace AutomationTool
{
	// NOTE: this needs to be kept in sync with EditorAnalytics.h and iPhonePackager.cs
	public enum ErrorCodes
	{
		Error_UATNotFound = -1,
		Error_Success = 0,
		Error_Unknown = 1,
		Error_Arguments = 2,
		Error_UnknownCommand = 3,
		Error_SDKNotFound = 10,
		Error_ProvisionNotFound = 11,
		Error_CertificateNotFound = 12,
		Error_ProvisionAndCertificateNotFound = 13,
		Error_InfoPListNotFound = 14,
		Error_KeyNotFoundInPList = 15,
		Error_ProvisionExpired = 16,
		Error_CertificateExpired = 17,
		Error_CertificateProvisionMismatch = 18,
		Error_CodeUnsupported = 19,
		Error_PluginsUnsupported = 20,
		Error_UnknownCookFailure = 25,
		Error_UnknownDeployFailure = 26,
		Error_UnknownBuildFailure = 27,
		Error_UnknownPackageFailure = 28,
		Error_UnknownLaunchFailure = 29,
		Error_StageMissingFile = 30,
		Error_FailedToCreateIPA = 31,
		Error_FailedToCodeSign = 32,
		Error_DeviceBackupFailed = 33,
		Error_AppUninstallFailed = 34,
		Error_AppInstallFailed = 35,
		Error_AppNotFound = 36,
		Error_StubNotSignedCorrectly = 37,
		Error_IPAMissingInfoPList = 38,
		Error_DeleteFile = 39,
		Error_DeleteDirectory = 40,
		Error_CreateDirectory = 41,
		Error_CopyFile = 42,
        Error_OnlyOneObbFileSupported = 50,
        Error_FailureGettingPackageInfo = 51,
        Error_OnlyOneTargetConfigurationSupported = 52,
        Error_ObbNotFound = 53,
        Error_AndroidBuildToolsPathNotFound = 54,
        Error_NoApkSuitableForArchitecture = 55,
		Error_FilesInstallFailed = 56,
		Error_RemoteCertificatesNotFound = 57,
		Error_LauncherFailed = 100,
		Error_UATLaunchFailure = 101,
		Error_FailedToDeleteStagingDirectory = 102,
		Error_MissingExecutable = 103,
		Error_DeviceNotSetupForDevelopment = 150,
		Error_DeviceOSNewerThanSDK = 151,
	};

	public class ErrorReporter
	{
		static public void Error(string Line, int Code = 1)
		{
			if (Program.ReturnCode == 0)
			{
				Program.ReturnCode = Code;
			}

			Console.ForegroundColor = ConsoleColor.Red;
			Log.WriteLine(TraceEventType.Error, "AutomationTool error: " + Line);
			Console.ResetColor();
		}
	};

	public class Program
	{
		// This needs to be static, otherwise SetConsoleCtrlHandler will result in a crash on exit.
		static ProcessManager.CtrlHandlerDelegate ProgramCtrlHandler = new ProcessManager.CtrlHandlerDelegate(CtrlHandler);
		static public int ReturnCode = 0;

		[STAThread]
		public static int Main()
		{
			var CommandLine = SharedUtils.ParseCommandLine();
			HostPlatform.Initialize();
			
			LogUtils.InitLogging(CommandLine);
			Log.WriteLine(TraceEventType.Information, "Running on {0}", HostPlatform.Current.GetType().Name);

			XmlConfigLoader.Init();

			// Log if we're running from the launcher
			var ExecutingAssemblyLocation = CommandUtils.CombinePaths(Assembly.GetExecutingAssembly().Location);
			if (String.Compare(ExecutingAssemblyLocation, CommandUtils.CombinePaths(InternalUtils.ExecutingAssemblyLocation), true) != 0)
			{
				Log.WriteLine(TraceEventType.Information, "Executed from AutomationToolLauncher ({0})", ExecutingAssemblyLocation);
			}
			Log.WriteLine(TraceEventType.Information, "CWD={0}", Environment.CurrentDirectory);

			// Hook up exit callbacks
			var Domain = AppDomain.CurrentDomain;
			Domain.ProcessExit += Domain_ProcessExit;
			Domain.DomainUnload += Domain_ProcessExit;
			HostPlatform.Current.SetConsoleCtrlHandler(ProgramCtrlHandler);

			var Version = InternalUtils.ExecutableVersion;
			Log.WriteLine(TraceEventType.Verbose, "{0} ver. {1}", Version.ProductName, Version.ProductVersion);

			try
			{
				// Don't allow simultaneous execution of AT (in the same branch)
				ReturnCode = InternalUtils.RunSingleInstance(MainProc, CommandLine);
			}
			catch (Exception Ex)
			{
				Log.WriteLine(TraceEventType.Error, "AutomationTool terminated with exception:");
				Log.WriteLine(TraceEventType.Error, LogUtils.FormatException(Ex));
				Log.WriteLine(TraceEventType.Error, Ex.Message);
				if (ReturnCode == 0)
				{
					ReturnCode = (int)ErrorCodes.Error_Unknown;
				}
			}

			// Make sure there's no directiories on the stack.
			CommandUtils.ClearDirStack();
			Environment.ExitCode = ReturnCode;

			// Try to kill process before app domain exits to leave the other KillAll call to extreme edge cases
			if (ShouldKillProcesses && !Utils.IsRunningOnMono)
			{
				ProcessManager.KillAll();
			}

			Log.WriteLine(TraceEventType.Information, "AutomationTool exiting with ExitCode={0}", ReturnCode);
			LogUtils.CloseFileLogging();

			return ReturnCode;
		}

		static bool CtrlHandler(CtrlTypes EventType)
		{
			Domain_ProcessExit(null, null);
			if (EventType == CtrlTypes.CTRL_C_EVENT)
			{
				// Force exit
				Environment.Exit(3);
			}			
			return true;
		}

		static void Domain_ProcessExit(object sender, EventArgs e)
		{
			// Kill all spawned processes (Console instead of Log because logging is closed at this time anyway)
			Console.WriteLine("Domain_ProcessExit");
			if (ShouldKillProcesses && !Utils.IsRunningOnMono)
			{			
				ProcessManager.KillAll();
			}
			Trace.Close();
		}

		static int MainProc(object Param)
		{
			Automation.Process((string[])Param);
			ShouldKillProcesses = Automation.ShouldKillProcesses;
			return 0;
		}

		static bool ShouldKillProcesses = true;
	}
}
