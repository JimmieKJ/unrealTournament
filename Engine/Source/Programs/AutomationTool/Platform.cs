// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Reflection;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Platform abstraction layer.
	/// </summary>
	public class Platform : CommandUtils
	{
		#region Intialization

		private static Dictionary<UnrealTargetPlatform, Platform> AllPlatforms = new Dictionary<UnrealTargetPlatform, Platform>();
		internal static void InitializePlatforms(Assembly[] AssembliesWithPlatforms = null)
		{
			LogVerbose("Creating platforms.");

			// Create all available platforms.
			foreach (var ScriptAssembly in (AssembliesWithPlatforms != null ? AssembliesWithPlatforms : AppDomain.CurrentDomain.GetAssemblies()))
			{
				CreatePlatformsFromAssembly(ScriptAssembly);
			}
			// Create dummy platforms for platforms we don't support
			foreach (var PlatformType in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				var TargetType = (UnrealTargetPlatform)PlatformType;
				Platform ExistingInstance;
				if (AllPlatforms.TryGetValue(TargetType, out ExistingInstance) == false)
				{
					Log("Creating placeholder platform for target: {0}", TargetType);
					AllPlatforms.Add(TargetType, new Platform(TargetType));
				}
			}
		}

		private static void CreatePlatformsFromAssembly(Assembly ScriptAssembly)
		{
			Log("Looking for platforms in {0}", ScriptAssembly.Location);
			Type[] AllTypes = null;
			try
			{
				AllTypes = ScriptAssembly.GetTypes();
			}
			catch (Exception Ex)
			{
				LogError("Failed to get assembly types for {0}", ScriptAssembly.Location);
				if (Ex is ReflectionTypeLoadException)
				{					
					var TypeLoadException = (ReflectionTypeLoadException)Ex;
					if (!IsNullOrEmpty(TypeLoadException.LoaderExceptions))
					{
						LogError("Loader Exceptions:");
						foreach (var LoaderException in TypeLoadException.LoaderExceptions)
						{
							Log(System.Diagnostics.TraceEventType.Error, LoaderException);
						}
					}
					else
					{
						Log("No Loader Exceptions available.");
					}
				}
				// Re-throw, this is still a critical error!
				throw Ex;
			}
			foreach (var PotentialPlatformType in AllTypes)
			{
				if (PotentialPlatformType != typeof(Platform) && typeof(Platform).IsAssignableFrom(PotentialPlatformType) && !PotentialPlatformType.IsAbstract)
				{
					Log("Creating platform {0} from {1}.", PotentialPlatformType.Name, ScriptAssembly.Location);
					var PlatformInstance = Activator.CreateInstance(PotentialPlatformType) as Platform;
					Platform ExistingInstance;
					if (!AllPlatforms.TryGetValue(PlatformInstance.PlatformType, out ExistingInstance))
					{
						AllPlatforms.Add(PlatformInstance.PlatformType, PlatformInstance);
					}
					else
					{
						LogWarning("Platform {0} already exists", PotentialPlatformType.Name);
					}
				}
			}
		}

		#endregion

		protected UnrealTargetPlatform TargetPlatformType = UnrealTargetPlatform.Unknown;

		public Platform(UnrealTargetPlatform PlatformType)
		{
			TargetPlatformType = PlatformType;
		}

		/// <summary>
		/// Allow the platform to alter the ProjectParams
		/// </summary>
		/// <param name="ProjParams"></param>
		public virtual void PlatformSetupParams(ref ProjectParams ProjParams)
		{

		}

		/// <summary>
		/// Package files for the current platform.
		/// </summary>
		/// <param name="ProjectPath"></param>
		/// <param name="ProjectExeFilename"></param>
		public virtual void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
		{
			throw new AutomationException("{0} does not yet implement Packaging.", PlatformType);
		}

		/// <summary>
		/// Allow platform to do platform specific work on archived project before it's deployed.
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		public virtual void ProcessArchivedProject(ProjectParams Params, DeploymentContext SC)
		{
		}

        /// <summary>
        /// Get all connected device names for this platform
        /// </summary>
        /// <param name="Params"></param>
        /// <param name="SC"></param>
        public virtual void GetConnectedDevices(ProjectParams Params, out List<string> Devices)
        {
            Devices = null;
            Log(System.Diagnostics.TraceEventType.Warning, "{0} does not implement GetConnectedDevices", PlatformType);
        }



		/// <summary>
		/// Deploy the application on the current platform
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		public virtual void Deploy(ProjectParams Params, DeploymentContext SC)
		{
			Log(System.Diagnostics.TraceEventType.Warning, "{0} does not implement Deploy...", PlatformType);
		}

		/// <summary>
		/// Run the client application on the platform
		/// </summary>
		/// <param name="ClientRunFlags"></param>
		/// <param name="ClientApp"></param>
		/// <param name="ClientCmdLine"></param>
		public virtual ProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		{
			PushDir(Path.GetDirectoryName(ClientApp));
			// Always start client process and don't wait for exit.
			ProcessResult ClientProcess = Run(ClientApp, ClientCmdLine, null, ClientRunFlags | ERunOptions.NoWaitForExit);
			PopDir();

			return ClientProcess;
		}

		/// <summary>
		/// Allow platform specific clean-up or detection after client has run
		/// </summary>
		/// <param name="ClientRunFlags"></param>
		public virtual void PostRunClient(ProcessResult Result, ProjectParams Params)
		{
			// do nothing in the default case
		}

		/// <summary>
		/// Get the platform-specific name for the executable (with out the file extension)
		/// </summary>
		/// <param name="InExecutableName"></param>
		/// <returns></returns>
		public virtual string GetPlatformExecutableName(string InExecutableName)
		{
			return InExecutableName;
		}

		public virtual List<string> GetExecutableNames(DeploymentContext SC, bool bIsRun = false)
		{
			var ExecutableNames = new List<String>();
			string Ext = AutomationTool.Platform.GetExeExtension(SC.StageTargetPlatform.TargetPlatformType);
			if (!String.IsNullOrEmpty(SC.CookPlatform))
			{
				if (SC.StageExecutables.Count() > 0)
				{
					foreach (var StageExecutable in SC.StageExecutables)
					{
						string ExeName = SC.StageTargetPlatform.GetPlatformExecutableName(StageExecutable);
						if (!SC.IsCodeBasedProject)
						{
							ExecutableNames.Add(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir, ExeName + Ext));
						}
						else
						{
							ExecutableNames.Add(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir, ExeName + Ext));
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
                    if (!SC.IsCodeBasedProject)
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

		/// <summary>
		/// Get the files to deploy, specific to this platform, typically binaries
		/// </summary>
		/// <param name="SC">Deployment Context</param>
		public virtual void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
		{
			throw new AutomationException("{0} does not yet implement GetFilesToDeployOrStage.", PlatformType);
		}

		/// <summary>
		/// Called after CopyUsingStagingManifest.  Does anything platform specific that requires a final list of staged files.
		/// e.g.  PlayGo emulation control file generation for PS4.
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		public virtual void PostStagingFileCopy(ProjectParams Params, DeploymentContext SC)
		{
		}

		/// <summary>
		/// Get the files to deploy, specific to this platform, typically binaries
		/// </summary>
		/// <param name="SC">Deployment Context</param>
		public virtual void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
		{
			SC.ArchiveFiles(SC.StageDirectory);
		}

		/// <summary>
		/// Gets cook platform name for this platform.
		/// </summary>
		/// <param name="bDedicatedServer">True if cooking for dedicated server</param>
		/// <param name="bIsClientOnly">True if cooking for client only</param>
		/// <param name="CookFlavor">Additional parameter used to indicate special sub-target platform</param>
		/// <returns>Cook platform string.</returns>
		public virtual string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
		{
			throw new AutomationException("{0} does not yet implement GetCookPlatform.", PlatformType);
		}

        /// <summary>
        /// Gets extra cook commandline arguments for this platform.
        /// </summary>
        /// <param name="Params"> ProjectParams </param>
        /// <returns>Cook platform string.</returns>
        public virtual string GetCookExtraCommandLine(ProjectParams Params)
        {
            return ""; 
        }

        /// <summary>
        /// Gets extra maps needed on this platform.
        /// </summary>
        /// <returns>extra maps</returns>
        public virtual List<string> GetCookExtraMaps()
        {
            return new List<string>();
        }



        /// <summary>
        /// Gets editor cook platform name for this platform. Cooking the editor is not useful, but this is used to fill the derived data cache
        /// </summary>
        /// <returns>Cook platform string.</returns>
        public virtual string GetEditorCookPlatform()
        {
            return GetCookPlatform(false, false, "");
        }

		/// <summary>
		/// return true if we need to change the case of filenames inside of pak files
		/// </summary>
		/// <returns></returns>
		public virtual bool DeployPakInternalLowerCaseFilenames()
		{
			return false;
		}

		/// <summary>
		/// return true if we need to change the case of filenames outside of pak files
		/// </summary>
		/// <returns></returns>
		public virtual bool DeployLowerCaseFilenames(bool bUFSFile)
		{
			return false;
		}

		/// <summary>
		/// Converts local path to target platform path.
		/// </summary>
		/// <param name="LocalPath">Local path.</param>
		/// <param name="LocalRoot">Local root.</param>
		/// <returns>Local path converted to device format path.</returns>
		public virtual string LocalPathToTargetPath(string LocalPath, string LocalRoot)
		{
			return LocalPath;
		}
        /// <summary>
        /// Returns a list of the compiler produced debug file extensions
        /// </summary>
        /// <returns>a list of the compiler produced debug file extensions</returns>
        public virtual List<string> GetDebugFileExtentions()
        {
            return new List<string>();
        }

		/// <summary>
		/// Remaps movie directory for platforms that need a remap
		/// </summary>
		public virtual bool StageMovies
		{
			get { return true; }
		}

		/// <summary>
		/// UnrealTargetPlatform type for this platform.
		/// </summary>
		public UnrealTargetPlatform PlatformType
		{
			get { return TargetPlatformType; }
		}

		/// <summary>
		/// True if this platform is supported.
		/// </summary>
		public virtual bool IsSupported
		{
			get { return false; }
		}

		/// <summary>
		/// True if this platform requires UFE for deploying
		/// </summary>
		public virtual bool DeployViaUFE
		{
			get { return false; }
		}

		/// <summary>
		/// True if this platform requires UFE for launching
		/// </summary>
		public virtual bool LaunchViaUFE
		{
			get { return false; }
		}

		/// <summary>
		/// True if this platform can write to the abslog path that's on the host desktop.
		/// </summary>
		public virtual bool UseAbsLog
		{
			get { return true; }
		}

		/// <summary>
		/// Remaps the given content directory to its final location
		/// </summary>
		public virtual string Remap(string Dest)
		{
			return Dest;
		}

		/// <summary>
		/// Tri-state - The intent is to override command line parameters for pak if needed per platform.
		/// </summary>
        /// 
        public enum PakType { Always, Never, DontCare };

        public virtual PakType RequiresPak(ProjectParams Params)
		{
            return PakType.DontCare;
		}

        /// <summary>
        /// Returns platform specific command line options for UnrealPak
        /// </summary>
        public virtual string GetPlatformPakCommandLine()
        {
            return "";
        }

		/// <summary>
		///  Returns whether the platform requires a package to deploy to a device
		/// </summary>
		public virtual bool RequiresPackageToDeploy
		{
			get { return false; }
		}

		public virtual List<string> GetFilesForCRCCheck()
		{
			string CmdLine = "UE4CommandLine.txt";
			if (DeployLowerCaseFilenames(true))
			{
				CmdLine = CmdLine.ToLowerInvariant();
			}
			return new List<string>() { CmdLine };
		}

		#region Hooks

		public virtual void PreBuildAgenda(UE4Build Build, UE4Build.BuildAgenda Agenda)
		{

		}

		public virtual void PostBuildTarget(UE4Build Build, string ProjectName, string UProjectPath, string Config)
		{

		}

		/// <summary>
		/// General purpose command to run generic string commands inside the platform interfeace
		/// </summary>
		/// <param name="Command"></param>
		public virtual int RunCommand(string Command)
		{
			return 0;
		}

		public virtual bool ShouldUseManifestForUBTBuilds(string AddArgs)
		{
			return true;
		}

		/// <summary>
		/// Determines whether we should stage a UE4CommandLine.txt for this platform 
		/// </summary>
		public virtual bool ShouldStageCommandLine(ProjectParams Params, DeploymentContext SC)
		{
			return true;
		}

        /// <summary>
        /// Only relevant for the mac and PC at the moment. Example calling the Mac platform with PS4 as an arg will return false. Can't compile or cook for the PS4 on the mac.
        /// </summary>
        public virtual bool CanHostPlatform(UnrealTargetPlatform Platform)
        {
            return false;
        }

		/// <summary>
		/// Allows some platforms to not be compiled, for instance when BuildCookRun -build is performed
		/// </summary>
		/// <returns><c>true</c> if this instance can be compiled; otherwise, <c>false</c>.</returns>
		public virtual bool CanBeCompiled()
		{
			return true;
		}

		public virtual bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, out List<string> UFSManifests, out List<string> NonUFSManifests)
		{
			UFSManifests = null;
			NonUFSManifests = null;
			return false;
		}

		public virtual UnrealTargetPlatform[] GetStagePlatforms()
		{
			return new UnrealTargetPlatform[] { PlatformType };
		}

		#endregion

		#region Utilities

		public static string GetExeExtension(UnrealTargetPlatform Target)
		{
			switch (Target)
			{
				case UnrealTargetPlatform.Win32:
				case UnrealTargetPlatform.Win64:
				case UnrealTargetPlatform.WinRT:
				case UnrealTargetPlatform.WinRT_ARM:
				case UnrealTargetPlatform.XboxOne:
					return ".exe";
				case UnrealTargetPlatform.PS4:
					return ".self";
				case UnrealTargetPlatform.IOS:
					return ".stub";
				case UnrealTargetPlatform.Linux:
					return "";
				case UnrealTargetPlatform.HTML5:
					return ".js";
			}
			return String.Empty;
		}

		public static Dictionary<UnrealTargetPlatform, Platform> Platforms
		{
			get { return AllPlatforms; }
		}

		#endregion
	}
}
