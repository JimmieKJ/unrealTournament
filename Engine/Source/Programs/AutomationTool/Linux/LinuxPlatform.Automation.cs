// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
	static Regex deviceRegex = new Regex(@"linux@([A-Za-z0-9\.\-]+)[\+]?");
	static Regex usernameRegex = new Regex(@"\\([a-z_][a-z0-9_]{0,30})@");
	static String keyPath = CombinePaths(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "/UE4SSHKeys");
	static String pscpPath = CombinePaths(Environment.GetEnvironmentVariable("LINUX_ROOT"), "bin/PSCP.exe");
	static String plinkPath = CombinePaths(Environment.GetEnvironmentVariable("LINUX_ROOT"), "bin/PLINK.exe");

	public BaseLinuxPlatform(UnrealTargetPlatform P)
		: base(P)
	{
	}

	public override List<string> GetExecutableNames(DeploymentContext SC, bool bIsRun = false)
	{
		List<string> Exes = base.GetExecutableNames(SC, bIsRun);
		// replace the binary name to match what was staged
		if (bIsRun && !SC.IsCodeBasedProject)
		{
			Exes[0] = CommandUtils.CombinePaths(SC.StageProjectRoot, "Binaries", SC.PlatformDir, SC.ShortProjectName);
		}
		return Exes;
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// FIXME: use build architecture
		string BuildArchitecture = "x86_64-unknown-linux-gnu";

		if (SC.bStageCrashReporter)
		{
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "CrashReportClient", false, null, null, true);
		}

		{
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/ICU/icu4c-53_1/", SC.PlatformDir, BuildArchitecture), Params.bDebugBuildsActuallyUseDebugCRT ? "*d.so*" : "*.so*", false, new[] { Params.bDebugBuildsActuallyUseDebugCRT ? "*.so*" : "*d.so*" }, CombinePaths("Engine/Binaries", SC.PlatformDir));
		}

		// assume that we always have to deploy Steam (FIXME: should be automatic - UEPLAT-807)
		{
			string SteamVersion = "Steamv132";

			// Check if the Steam directory exists. We need it for Steam controller support, so we include it whenever we can.
			if (Directory.Exists(CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion)))
			{
				SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "libsteam_api.so", false, null, CombinePaths("Engine/Binaries", SC.PlatformDir));
			}

			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Config"), "controller.vdf", false, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Saved/Config"));
			// copy optional perfcounters definition file
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Config"), "PerfCounters.json", false, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Saved/Config"), true);
		}

		// stage libLND (omit it for dedservers and Rocket - proper resolution is to use build receipts, see UEPLAT-807)
		if (!SC.DedicatedServer && !GlobalCommandLine.Rocket)
		{
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/LinuxNativeDialogs/", SC.PlatformDir, BuildArchitecture), "libLND*.so");
		}

		// assume that we always have to deploy OpenAL (FIXME: should be automatic - UEPLAT-807)
		{
			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/OpenAL/", SC.PlatformDir), "libopenal.so.1", false, null, CombinePaths("Engine/Binaries", SC.PlatformDir));
		}

		SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Splash"), "Splash.bmp", false, null, null, true);

        if (Params.StageNonMonolithic)
        {
            if (SC.DedicatedServer)
            {
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4Server*");
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "libUE4Server-*.so");
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins"), "libUE4Server-*.so", true, null, null, true);
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "libUE4Server-*.so");
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins"), "libUE4Server-*.so", true, null, null, true);
            }
            else
            {
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "UE4*");
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "libUE4-*.so");
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins"), "libUE4-*.so", true, null, null, true);
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), "libUE4-*.so");
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins"), "libUE4-*.so", true, null, null, true);
            }
        }
        else
        {
	    	List<string> Exes = GetExecutableNames(SC);

		    foreach (var Exe in Exes)
		    {

			    if (Exe.StartsWith(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
			    {
				    // remap the project root. For content-only projects, rename the executable to the project name.
				    if (!Params.IsCodeBasedProject && Exe == Exes[0])
				    {
					    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir), false, true, SC.ShortProjectName);
				    }
				    else
				    {
					    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir), false);
				    }
			    }
			    else if (Exe.StartsWith(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
			    {
				    // Move the executable for content-only projects into the project directory, using the project name, so it can figure out the UProject to look for and is consistent with code projects.
				    if (!Params.IsCodeBasedProject && Exe == Exes[0])
				    {
						// ensure the ue4game binary exists, if applicable
						if (!SC.IsCodeBasedProject && !FileExists_NoExceptions(Params.ProjectGameExeFilename) && !SC.bIsCombiningMultiplePlatforms)
						{
							Log("Failed to find game binary " + Params.ProjectGameExeFilename);
							AutomationTool.ErrorReporter.Error("Stage Failed.", (int)AutomationTool.ErrorCodes.Error_MissingExecutable);
							throw new AutomationException("Could not find game binary {0}. You may need to build the UE4 project with your target configuration and platform.", Params.ProjectGameExeFilename);
						}

					    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir), false, true, SC.ShortProjectName);
				    }
				    else
				    {
					    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, null, false);
				    }
			    }
			    else
			    {
				    throw new AutomationException("Can't stage the exe {0} because it doesn't start with {1} or {2}", Exe, CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir), CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir));
                }
            }
		}
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
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
		if (!String.IsNullOrEmpty(Params.ServerDeviceAddress))
		{
			string sourcePath = CombinePaths(Params.BaseStageDirectory, GetCookPlatform(Params.DedicatedServer, false, ""));
			string destPath = Params.DeviceUsername + "@" + Params.ServerDeviceAddress + ":.";
			RunAndLog(CmdEnv, pscpPath, String.Format("-batch -i {0} -r {1} {2}", Params.DevicePassword, sourcePath, destPath));

			List<string> Exes = GetExecutableNames(SC);

			string binPath = CombinePaths(GetCookPlatform(Params.DedicatedServer, false, ""), SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir, Path.GetFileName(Exes[0])).Replace("\\", "/");
			string iconPath = CombinePaths(GetCookPlatform(Params.DedicatedServer, false, ""), SC.RelativeProjectRootForStage, SC.ShortProjectName + ".png").Replace("\\", "/");

			string DesiredGLVersion = "4.3";

			// Begin Bash Shell Script
			string script = String.Format(@"#!/bin/bash
# Check for OpenGL4 support
glarg=''
if command -v glxinfo >/dev/null 2>&1 ; then
    export DISPLAY="":0""
    glversion=$(glxinfo | grep ""OpenGL version string:"" | sed 's/[^0-9.]*\([0-9.]*\).*/\1/')
    glmatch=$(echo -e ""$glversion\n{0}"" | sort -Vr | head -1)
    [[ ""$glmatch"" = ""$glversion"" ]] && glarg=' -opengl4'
fi

# Create .desktop file
cat > $HOME/Desktop/{1}.desktop << EOF
[Desktop Entry]
Type=Application
Name={2}
Comment=UE4 Game
Exec=$HOME/{3}{4}$glarg
Icon=$HOME/{5}
Terminal=false
Categories=Game;
EOF

# Set permissions
chmod 755 $HOME/{3}
chmod 700 $HOME/Desktop/{1}.desktop", DesiredGLVersion, SC.ShortProjectName, SC.ShortProjectName, binPath, (Params.UsePak(SC.StageTargetPlatform) ? " -pak" : ""), iconPath);
			// End Bash Shell Script

			string scriptFile = Path.GetTempFileName();
			File.WriteAllText(scriptFile, script);
			RunAndLog(CmdEnv, plinkPath, String.Format("-ssh -t -batch -l {0} -i {1} {2} -m {3}", Params.DeviceUsername, Params.DevicePassword, Params.ServerDeviceAddress, scriptFile));
			File.Delete(scriptFile);
		}
	}

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
		Match match = deviceRegex.Match(ProjParams.ServerDevice);
		if (match.Success)
		{
			ProjParams.ServerDeviceAddress = match.Groups[1].Value;

			string linuxKey = null;
			string keyID = ProjParams.ServerDeviceAddress;

			// If username specified use key for that user
			if (!String.IsNullOrEmpty(ProjParams.DeviceUsername))
			{
				keyID = ProjParams.DeviceUsername + "@" + keyID;
			}

			if (!DirectoryExists(keyPath))
			{
				Directory.CreateDirectory(keyPath);
			}

			linuxKey = Directory.EnumerateFiles(keyPath).Where(f => f.Contains(keyID)).FirstOrDefault();

			// Generate key if it doesn't exist
			if (String.IsNullOrEmpty(linuxKey)
				&& ((!String.IsNullOrEmpty(ProjParams.DeviceUsername) && !String.IsNullOrEmpty(ProjParams.DevicePassword))
					|| !ProjParams.Unattended)) // Skip key generation in unattended mode if information is missing
			{
				Log("Configuring Linux host");

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
				String Command = @"/bin/sh";

				// Convienence names
				String keyName = String.Format("UE4:{0}@{1}", Environment.UserName, Environment.MachineName);
				String idRSA = String.Format(".ssh/id_rsa/{0}", keyName);
				String ppk = String.Format("{0}@{1}.ppk", ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress);

				// Generate keys including the .ppk on Linux device
				String Script = String.Format("mkdir -p .ssh/id_rsa && if [ ! -f {0} ] ; then ssh-keygen -t rsa -C {1} -N '' -f {0} && cat {0}.pub >> .ssh/authorized_keys ; fi && chmod -R go-w ~/.ssh && puttygen {0} -o {2}", idRSA, keyName, ppk);
				String Args = String.Format("/c ECHO y | {0} -ssh -t -pw {1} {2}@{3} \"{4}\"", plinkPath, ProjParams.DevicePassword, ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress, Script);
				RunAndLog(CmdEnv, Command, Args);

				// Retrive the .ppk
				Command = pscpPath;
				Args = String.Format("-batch -pw {0} -l {1} {2}:{3} {4}", ProjParams.DevicePassword, ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress, ppk, keyPath);
				RunAndLog(CmdEnv, Command, Args);

				linuxKey = CombinePaths(keyPath, ppk);

				// Remove the .ppk from the Linux device
				Command = plinkPath;
				Script = String.Format("rm {0}", ppk);
				Args = String.Format("-batch -i {0} {1}@{2} \"{3}\"", linuxKey, ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress, Script);
				RunAndLog(CmdEnv, Command, Args);
			}

			// Fail if a key couldn't be found or generated
			if (String.IsNullOrEmpty(linuxKey))
			{
				throw new AutomationException("can't deploy/run using a Linux device without a valid SSH key");
			}

			// Set username from key
			if (String.IsNullOrEmpty(ProjParams.DeviceUsername))
			{
				match = usernameRegex.Match(linuxKey);
				if (!match.Success)
				{
					throw new AutomationException("can't determine username from SSH key");
				}
				ProjParams.DeviceUsername = match.Groups[1].Value;
			}

			// Use key as password
			ProjParams.DevicePassword = linuxKey;
		}
		else if ((ProjParams.Deploy || ProjParams.Run) && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux)
		{
			throw new AutomationException("must specify device IP for remote Linux target (-serverdevice=<ip>)");
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
