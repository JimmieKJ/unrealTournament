// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	public partial class UnrealBuildTool
	{
		/// <summary>
		/// Time at which control fist passes to UBT.
		/// </summary>
		static public DateTime StartTime;

		static public string BuildGuid = Guid.NewGuid().ToString();

		/// <summary>
		/// Total time spent building projects.
		/// </summary>
		static public double TotalBuildProjectTime = 0;

		/// <summary>
		/// Total time spent compiling.
		/// </summary>
		static public double TotalCompileTime = 0;

		/// <summary>
		/// Total time spent creating app bundles.
		/// </summary>
		static public double TotalCreateAppBundleTime = 0;

		/// <summary>
		/// Total time spent generating debug information.
		/// </summary>
		static public double TotalGenerateDebugInfoTime = 0;

		/// <summary>
		/// Total time spent linking.
		/// </summary>
		static public double TotalLinkTime = 0;

		/// <summary>
		/// Total time spent on other actions.
		/// </summary>
		static public double TotalOtherActionsTime = 0;

		/// <summary>
		/// How much time was spent scanning for include dependencies for outdated C++ files
		/// </summary>
		public static double TotalDeepIncludeScanTime = 0.0;

		/// <summary>
		/// The command line
		/// </summary>
		static public List<string> CmdLine = new List<string>();

		/// <summary>
		/// The environment at boot time.
		/// </summary>
		static public System.Collections.IDictionary InitialEnvironment;

		/// <summary>
		/// Should we skip all extra modules for platforms?
		/// </summary>
		static bool bSkipNonHostPlatforms = false;

		static UnrealTargetPlatform OnlyPlatformSpecificFor = UnrealTargetPlatform.Unknown;

		/// <summary>
		/// Whether we're running with engine installed
		/// </summary>
		static private bool? bIsEngineInstalled;

		/// <summary>
		/// The full name of the Root UE4 directory
		/// </summary>
		public static readonly DirectoryReference RootDirectory = new DirectoryReference(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetOriginalLocation()), "..", "..", ".."));

		/// <summary>
		/// The full name of the Engine directory
		/// </summary>
		public static readonly DirectoryReference EngineDirectory = DirectoryReference.Combine(RootDirectory, "Engine");

		/// <summary>
		/// The full name of the Engine/Source directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceDirectory = DirectoryReference.Combine(EngineDirectory, "Source");

		/// <summary>
		/// Full path to the Engine/Source/Runtime directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceRuntimeDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Runtime");

		/// <summary>
		/// Full path to the Engine/Source/Developer directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceDeveloperDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Developer");

		/// <summary>
		/// Full path to the Engine/Source/Editor directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceEditorDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Editor");

		/// <summary>
		/// Full path to the Engine/Source/Programs directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceProgramsDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Programs");

		/// <summary>
		/// Full path to the Engine/Source/ThirdParty directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceThirdPartyDirectory = DirectoryReference.Combine(EngineSourceDirectory, "ThirdParty");

		/// <summary>
		/// The Remote Ini directory.  This should always be valid when compiling using a remote server.
		/// </summary>
		static string RemoteIniPath = null;

		/// <summary>
		/// This is set to true during UBT startup, after it is safe to be accessing the IsGatheringBuild and IsAssemblingBuild properties.  Never access this directly.
		/// </summary>
		private static bool bIsSafeToCheckIfGatheringOrAssemblingBuild = false;

		/// <summary>
		/// True if this run of UBT should only invalidate Makefile.
		/// </summary>
		static public bool bIsInvalidatingMakefilesOnly = false;

		/// <summary>
		/// Cached array of all the supported target platforms
		/// </summary>
		static public UnrealTargetPlatform[] AllPlatforms = (UnrealTargetPlatform[])Enum.GetValues(typeof(UnrealTargetPlatform));

		/// <summary>
		/// True if we should gather module dependencies for building in this run.  If this is false, then we'll expect to be loading information from disk about
		/// the target's modules before we'll be able to build anything.  One or both of IsGatheringBuild or IsAssemblingBuild must be true.
		/// </summary>
		static public bool IsGatheringBuild
		{
			get
			{
				if (bIsSafeToCheckIfGatheringOrAssemblingBuild)
				{
					return bIsGatheringBuild_Unsafe;
				}
				else
				{
					throw new BuildException("Accessed UnrealBuildTool.IsGatheringBuild before it was fully initialized at startup (bIsSafeToCheckIfGatheringOrAssemblingBuild)");
				}
			}
		}
		static private bool bIsGatheringBuild_Unsafe = true;

		/// <summary>
		/// True if we should go ahead and actually compile and link in this run.  If this is false, no actions will be executed and we'll stop after gathering
		/// and saving information about dependencies.  One or both of IsGatheringBuild or IsAssemblingBuild must be true.
		/// </summary>
		static public bool IsAssemblingBuild
		{
			get
			{
				if (bIsSafeToCheckIfGatheringOrAssemblingBuild)
				{
					return bIsAssemblingBuild_Unsafe;
				}
				else
				{
					throw new BuildException("Accessed UnrealBuildTool.IsGatheringBuild before it was fully initialized at startup (bIsSafeToCheckIfGatheringOrAssemblingBuild)");
				}
			}
		}
		static private bool bIsAssemblingBuild_Unsafe = true;

		/// <summary>
		/// Used when BuildConfiguration.bUseUBTMakefiles is enabled.  If true, it means that our cached includes may not longer be
		/// valid (or never were), and we need to recover by forcibly scanning included headers for all build prerequisites to make sure that our
		/// cached set of includes is actually correct, before determining which files are outdated.
		/// </summary>
		static public bool bNeedsFullCPPIncludeRescan = false;


		/// <summary>
		/// Returns true if UnrealBuildTool should skip everything other than the host platform as far as finding other modules that need to be compiled.
		/// </summary>
		/// <returns>True if running with "-skipnonhostplatforms"</returns>
		static public bool SkipNonHostPlatforms()
		{
			return bSkipNonHostPlatforms;
		}

		/// <summary>
		/// Returns the platform we should compile extra modules for, in addition to the host
		/// </summary>
		/// <returns>The extra platfrom to compile for with "-onlyhostplatformand="</returns>
		static public UnrealTargetPlatform GetOnlyPlatformSpecificFor()
		{
			return OnlyPlatformSpecificFor;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		static public bool IsEngineInstalled()
		{
			if (!bIsEngineInstalled.HasValue)
			{
				bIsEngineInstalled = UnrealBuildTool.CommandLineContains("-Installed");
				FileReference InstalledBuildFile = FileReference.Combine(RootDirectory, "Engine", "Build", "InstalledBuild.txt");
				bIsEngineInstalled |= InstalledBuildFile.Exists();
				if (bIsEngineInstalled.Value)
				{
					bIsEngineInstalled = !UnrealBuildTool.CommandLineContains("-NotInstalledEngine");
				}
				else
				{
					bIsEngineInstalled = UnrealBuildTool.CommandLineContains("-InstalledEngine");
				}
			}

			return bIsEngineInstalled.Value;
		}

		static public string GetUBTPath()
		{
			string UnrealBuildToolPath = Assembly.GetExecutingAssembly().GetOriginalLocation();
			return UnrealBuildToolPath;
		}

		/// <summary>
		/// The Unreal remote tool ini directory.  This should be valid if compiling using a remote server
		/// </summary>
		/// <returns>The directory path</returns>
		static public string GetRemoteIniPath()
		{
			return RemoteIniPath;
		}

		static public void SetRemoteIniPath(string Path)
		{
			RemoteIniPath = Path;
		}

		// @todo projectfiles: Move this into the ProjectPlatformGeneration class?
		/// <summary>
		/// IsDesktopPlatform
		/// </summary>
		/// <param name="InPlatform">The platform of interest</param>
		/// <returns>True if the given platform is a 'desktop' platform</returns>
		static public bool IsDesktopPlatform(UnrealTargetPlatform InPlatform)
		{
			// Windows and Mac are desktop platforms.
			return (
				(InPlatform == UnrealTargetPlatform.Win64) ||
				(InPlatform == UnrealTargetPlatform.Win32) ||
				(InPlatform == UnrealTargetPlatform.Linux) ||
				(InPlatform == UnrealTargetPlatform.Mac)
				);
		}

		// @todo projectfiles: Move this into the ProjectPlatformGeneration class?
		/// <summary>
		/// IsEditorPlatform
		/// </summary>
		/// <param name="InPlatform">The platform of interest</param>
		/// <returns>True if the given platform is a platform that supports the editor.</returns>
		static public bool IsEditorPlatform(UnrealTargetPlatform InPlatform)
		{
			// Windows 64, Linux and Mac are editor platforms.
			return (
				(InPlatform == UnrealTargetPlatform.Win64) ||
				(InPlatform == UnrealTargetPlatform.Linux) ||
				(InPlatform == UnrealTargetPlatform.Mac)
				);
		}

		// @todo projectfiles: Move this into the ProjectPlatformGeneration class?
		/// <summary>
		/// IsServerPlatform
		/// </summary>
		/// <param name="InPlatform">The platform of interest</param>
		/// <returns>True if the given platform is a 'server' platform</returns>
		static public bool IsServerPlatform(UnrealTargetPlatform InPlatform)
		{
			// Windows, Mac, and Linux are desktop platforms.
			return (
				(InPlatform == UnrealTargetPlatform.Win64) ||
				(InPlatform == UnrealTargetPlatform.Win32) ||
				(InPlatform == UnrealTargetPlatform.Mac) ||
				(InPlatform == UnrealTargetPlatform.Linux)
				);
		}

		// @todo projectfiles: Move this into the ProjectPlatformGeneration class?
		/// <summary>
		/// PlatformSupportsCrashReporter
		/// </summary>
		/// <param name="InPlatform">The platform of interest</param>
		/// <returns>True if the given platform supports a crash reporter client (i.e. it can be built for it)</returns>
		static public bool PlatformSupportsCrashReporter(UnrealTargetPlatform InPlatform)
		{
			return (
				(InPlatform == UnrealTargetPlatform.Win64) ||
				(InPlatform == UnrealTargetPlatform.Win32) ||
				(InPlatform == UnrealTargetPlatform.Linux) ||
				(InPlatform == UnrealTargetPlatform.Mac)
				);
		}

		/// <summary>
		/// Gets all platforms that satisfies the predicate.
		/// </summary>
		/// <param name="OutPlatforms">The list of platform to fill in.</param>
		/// <param name="Predicate">The predicate to filter the platforms.</param>
		/// <param name="bCheckValidity">If true, ensure it is a valid platform.</param>
		/// <returns>True if successful and platforms are found, false if not.</returns>
		static private bool GetPlatforms(ref List<UnrealTargetPlatform> OutPlatforms, Func<UnrealTargetPlatform, bool> Predicate, bool bCheckValidity = true)
		{
			OutPlatforms.Clear();
			foreach (UnrealTargetPlatform Platform in AllPlatforms)
			{
				if (Predicate(Platform))
				{
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
					if ((bCheckValidity == false) || (BuildPlatform != null))
					{
						OutPlatforms.Add(Platform);
					}
				}
			}

			return (OutPlatforms.Count > 0) ? true : false;
		}

		/// <summary>
		/// Gets all platforms.
		/// </summary>
		/// <param name="OutPlatforms">The list of platform to fill in.</param>
		/// <param name="bCheckValidity">If true, ensure it is a valid platform.</param>
		/// <returns>True if successful and platforms are found, false if not.</returns>
		static public bool GetAllPlatforms(ref List<UnrealTargetPlatform> OutPlatforms, bool bCheckValidity = true)
		{
			return GetPlatforms(ref OutPlatforms, (Platform) => { return true; }, bCheckValidity);
		}

		/// <summary>
		/// Gets all desktop platforms.
		/// </summary>
		/// <param name="OutPlatforms">The list of platform to fill in.</param>
		/// <param name="bCheckValidity">If true, ensure it is a valid platform.</param>
		/// <returns>True if successful and platforms are found, false if not.</returns>
		static public bool GetAllDesktopPlatforms(ref List<UnrealTargetPlatform> OutPlatforms, bool bCheckValidity = true)
		{
			return GetPlatforms(ref OutPlatforms, UnrealBuildTool.IsDesktopPlatform, bCheckValidity);
		}

		/// <summary>
		/// Gets all editor platforms.
		/// </summary>
		/// <param name="OutPlatforms">The list of platform to fill in.</param>
		/// <param name="bCheckValidity">If true, ensure it is a valid platform.</param>
		/// <returns>True if successful and platforms are found, false if not.</returns>
		static public bool GetAllEditorPlatforms(ref List<UnrealTargetPlatform> OutPlatforms, bool bCheckValidity = true)
		{
			return GetPlatforms(ref OutPlatforms, UnrealBuildTool.IsEditorPlatform, bCheckValidity);
		}

		/// <summary>
		/// Gets all server platforms.
		/// </summary>
		/// <param name="OutPlatforms">The list of platform to fill in.</param>
		/// <param name="bCheckValidity">If true, ensure it is a valid platform.</param>
		/// <returns>True if successful and platforms are found, false if not.</returns>
		static public bool GetAllServerPlatforms(ref List<UnrealTargetPlatform> OutPlatforms, bool bCheckValidity = true)
		{
			return GetPlatforms(ref OutPlatforms, UnrealBuildTool.IsServerPlatform, bCheckValidity);
		}

		/// <summary>
		/// Is this a valid configuration. Used primarily for Installed vs non-Installed.
		/// </summary>
		/// <param name="InConfiguration"></param>
		/// <returns>true if valid, false if not</returns>
		static public bool IsValidConfiguration(UnrealTargetConfiguration InConfiguration)
		{
			return InstalledPlatformInfo.Current.IsValidConfiguration(InConfiguration, EProjectType.Code);
		}

		/// <summary>
		/// Is this a valid platform. Used primarily for Installed vs non-Installed.
		/// </summary>
		/// <param name="InPlatform"></param>
		/// <returns>true if valid, false if not</returns>
		static public bool IsValidPlatform(UnrealTargetPlatform InPlatform)
		{
			return InstalledPlatformInfo.Current.IsValidPlatform(InPlatform, EProjectType.Code);
		}


		/// <summary>
		/// Is this a valid platform. Used primarily for Installed vs non-Installed builds.
		/// </summary>
		/// <param name="InPlatform"></param>
		/// <returns>true if valid, false if not</returns>
		static public bool IsValidDesktopPlatform(UnrealTargetPlatform InPlatform)
		{
			switch (BuildHostPlatform.Current.Platform)
			{
				case UnrealTargetPlatform.Linux:
					return InPlatform == UnrealTargetPlatform.Linux;
				case UnrealTargetPlatform.Mac:
					return InPlatform == UnrealTargetPlatform.Mac;
				case UnrealTargetPlatform.Win64:
					return ((InPlatform == UnrealTargetPlatform.Win32) || (InPlatform == UnrealTargetPlatform.Win64));
				default:
					throw new BuildException("Invalid RuntimePlatform:" + BuildHostPlatform.Current.Platform);
			}
		}

		/// <summary>
		/// See if the given string was pass in on the command line
		/// </summary>
		/// <param name="InString">The argument to check for (must include '-' if expected)</param>
		/// <returns>true if the argument is found, false if not</returns>
		static public bool CommandLineContains(string InString)
		{
			string LowercaseInString = InString.ToLowerInvariant();
			foreach (string Arg in CmdLine)
			{
				string LowercaseArg = Arg.ToLowerInvariant();
				if (LowercaseArg == LowercaseInString)
				{
					return true;
				}
			}

			return false;
		}

		public static void RegisterAllUBTClasses(bool bSkipBuildPlatforms = false, bool bValidatingPlatforms = false)
		{
			// Find and register all tool chains and build platforms that are present
			Assembly UBTAssembly = Assembly.GetExecutingAssembly();
			if (UBTAssembly != null)
			{
				Log.TraceVerbose("Searching for ToolChains, BuildPlatforms, BuildDeploys and ProjectGenerators...");

				List<System.Type> ProjectGeneratorList = new List<System.Type>();
				Type[] AllTypes = UBTAssembly.GetTypes();
				if (!bSkipBuildPlatforms)
				{
					// register all build platforms first, since they implement SDK-switching logic that can set environment variables
					foreach (Type CheckType in AllTypes)
					{
						if (CheckType.IsClass && !CheckType.IsAbstract)
						{
							if (CheckType.IsSubclassOf(typeof(UEBuildPlatformFactory)))
							{
								Log.TraceVerbose("    Registering build platform: {0}", CheckType.ToString());
								UEBuildPlatformFactory TempInst = (UEBuildPlatformFactory)(UBTAssembly.CreateInstance(CheckType.FullName, true));
								TempInst.TryRegisterBuildPlatforms(bValidatingPlatforms);
							}
						}
					}
				}
				// register all the other classes
				foreach (Type CheckType in AllTypes)
				{
					if (CheckType.IsClass && !CheckType.IsAbstract)
					{
						if (CheckType.IsSubclassOf(typeof(UEPlatformProjectGenerator)))
						{
							ProjectGeneratorList.Add(CheckType);
						}
					}
				}

				// We need to process the ProjectGenerators AFTER all BuildPlatforms have been 
				// registered as they use the BuildPlatform to verify they are legitimate.
				foreach (Type ProjType in ProjectGeneratorList)
				{
					Log.TraceVerbose("    Registering project generator: {0}", ProjType.ToString());
					UEPlatformProjectGenerator TempInst = (UEPlatformProjectGenerator)(UBTAssembly.CreateInstance(ProjType.FullName, true));
					TempInst.RegisterPlatformProjectGenerator();
				}
			}
		}

		private static bool ParseRocketCommandlineArg(string InArg, ref string OutGameName, ref FileReference ProjectFile)
		{
			string LowercaseArg = InArg.ToLowerInvariant();

			string ProjectArg = null;
            FileReference TryProjectFile;
			if (LowercaseArg.StartsWith("-project="))
			{
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
				{
					ProjectArg = InArg.Substring(9).Trim(new Char[] { ' ', '"', '\'' });
				}
				else
				{
					ProjectArg = InArg.Substring(9);
				}
			}
			else if (LowercaseArg.EndsWith(".uproject"))
			{
				ProjectArg = InArg;
			}
			else if (LowercaseArg.StartsWith("-remoteini="))
			{
				RemoteIniPath = InArg.Substring(11);
			}
			else
            if (UProjectInfo.TryGetProjectForTarget(LowercaseArg, out TryProjectFile))
            {
                ProjectFile = TryProjectFile;
                OutGameName = InArg;
                return true;
            }
            else
            {
				return false;
			}

			if (ProjectArg != null && ProjectArg.Length > 0)
			{
				string ProjectPath = Path.GetDirectoryName(ProjectArg);
				OutGameName = Path.GetFileNameWithoutExtension(ProjectArg);

				if (Path.IsPathRooted(ProjectPath) == false)
				{
					if (Directory.Exists(ProjectPath) == false)
					{
						// If it is *not* rooted, then we potentially received a path that is 
						// relative to Engine/Binaries/[PLATFORM] rather than Engine/Source
						if (ProjectPath.StartsWith("../") || ProjectPath.StartsWith("..\\"))
						{
							string AlternativeProjectpath = ProjectPath.Substring(3);
							if (Directory.Exists(AlternativeProjectpath))
							{
								ProjectPath = AlternativeProjectpath;
								ProjectArg = ProjectArg.Substring(3);
								Debug.Assert(ProjectArg.Length > 0);
							}
						}
					}
				}

				ProjectFile = new FileReference(ProjectArg);
			}

			return true;
		}

		/// <summary>
		/// This is a safeguard to our initialization order, which tends to be fragile.
		/// It is not really safe to reference a configuration value until the XmlConfig system loads it (and command lines override it), but non-trivial
		/// code needs to run before the XmlConfig can load, so it can be hard to tell if a function you are calling in these early 
		/// stages might unsafely reference a config value.
		/// 
		/// This bool is set RIGHT BEFORE the XmlConfig system loads configs, so any references to the config class before that will be trapped.
		/// If you think you need to change where this value is set, you need to reconsider how soon you are trying to reference config values.
		/// </summary>
		public static bool bIsSafeToReferenceConfigurationValues = false;

		/// <summary>
		/// Paranoid init of the logging system in case someone tries to log super early in startup.
		/// However, no configuration files have been loaded, and the command line has not been parsed, so 
		/// certain features like verbose and log file cannot be initialized yet.
		/// </summary>
		private static void PreInitLogging()
		{
			Log.InitLogging(
				bLogTimestamps: false,
				InLogLevel: LogEventType.Log,
				bLogSeverity: false,
				bLogSources: false,
				bColorConsoleOutput: true,
				TraceListeners: new[] { new ConsoleTraceListener() });
		}

		/// <summary>
		/// UBT startup order is fairly fragile, and relies on globals that may or may not be safe to use yet.
		/// This function is for super early startup stuff that should not access Configuration classes (anything loaded by XmlConfg).
		/// This should be very minimal startup code.
		/// </summary>
		/// <param name="Arguments">Cmdline arguments</param>
		private static void DoStartupStuffThatCannotAccessConfigurationClasses(string[] Arguments)
		{
			// Helpers used for stats tracking.
			StartTime = DateTime.UtcNow;

			// Do super early log init as a safeguard. We'll re-init with proper config options later.
			PreInitLogging();

			// ensure we can resolve any external assemblies that are not in the same folder as our assembly.
			AssemblyUtils.InstallAssemblyResolver(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()));

			// Copy off the arguments to allow checking for command-line arguments elsewhere
			CmdLine = new List<string>(Arguments);

			// Grab the environment.
			InitialEnvironment = Environment.GetEnvironmentVariables();
			if (InitialEnvironment.Count < 1)
			{
				throw new BuildException("Environment could not be read");
			}

			// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
			// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
			// UEBuildConfiguration.PostReset is susceptible to this, so we must do this before configs are loaded.
			string EngineSourceDirectory = Path.Combine(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()), "..", "..", "..", "Engine", "Source");

			//@todo.Rocket: This is a workaround for recompiling game code in editor
			// The working directory when launching is *not* what we would expect
			if (Directory.Exists(EngineSourceDirectory) == false)
			{
				// We are assuming UBT always runs from <>/Engine/Binaries/DotNET/...
				EngineSourceDirectory = Assembly.GetExecutingAssembly().GetOriginalLocation();
				EngineSourceDirectory = EngineSourceDirectory.Replace("\\", "/");
				Int32 EngineIdx = EngineSourceDirectory.IndexOf("/Engine/Binaries/DotNET/", StringComparison.InvariantCultureIgnoreCase);
				if (EngineIdx != 0)
				{
					EngineSourceDirectory = Path.Combine(EngineSourceDirectory.Substring(0, EngineIdx), "Engine", "Source");
				}
			}
			if (Directory.Exists(EngineSourceDirectory)) // only set the directory if it exists, this should only happen if we are launching the editor from an artist sync
			{
				Directory.SetCurrentDirectory(EngineSourceDirectory);
			}
		}

		/// <summary>
		/// Enough startup work has been done to load configuration classes (like setting the proper current directory).
		/// It is still not safe to log. Mostly this relates to XmlConfigLoader, but any other code added here cannot log yet either.
		/// </summary>
		/// <param name="Arguments"></param>
		private static void InitConfigurationClasses(string[] Arguments)
		{
			// Load the Config XML files first.
			XmlConfigLoader.Init();

			// Then let the command lines override any configs necessary.
			const string UCAUsageThresholdString = "-ucausagethreshold=";
			const string UCAModuleToAnalyzeString = "-ucamoduletoanalyze=";
			foreach (string LowercaseArg in Arguments.Select(arg => arg.ToLowerInvariant()))
			{
				if (LowercaseArg == "-verbose")
				{
					BuildConfiguration.bPrintDebugInfo = true;
					BuildConfiguration.LogLevel = "Verbose";
				}
				else if (LowercaseArg.StartsWith("-log="))
				{
					BuildConfiguration.LogFilename = LowercaseArg.Replace("-log=", "");
				}
				else if (LowercaseArg.StartsWith("-logdetailedcompilertiminginfo="))
				{
					WindowsPlatform.bLogDetailedCompilerTimingInfo = bool.Parse(LowercaseArg.Replace("-logdetailedcompilertiminginfo=", ""));
				}
				else if (LowercaseArg == "-forceheadergeneration")
				{
					UEBuildConfiguration.bForceHeaderGeneration = true;
				}
				else if (LowercaseArg == "-nobuilduht")
				{
					UEBuildConfiguration.bDoNotBuildUHT = true;
				}
				else if (LowercaseArg == "-failifgeneratedcodechanges")
				{
					UEBuildConfiguration.bFailIfGeneratedCodeChanges = true;
				}
				else if (LowercaseArg == "-clean")
				{
					UEBuildConfiguration.bCleanProject = true;
				}
				else if (LowercaseArg == "-prepfordeploy")
				{
					UEBuildConfiguration.bPrepForDeployment = true;
				}
				else if (LowercaseArg == "-generateexternalfilelist")
				{
					UEBuildConfiguration.bGenerateExternalFileList = true;
				}
				else if (LowercaseArg == "-mergeexternalfilelist")
				{
					UEBuildConfiguration.bMergeExternalFileList = true;
				}
				else if (LowercaseArg == "-listbuildfolders")
				{
					UEBuildConfiguration.bListBuildFolders = true;
				}
				else if (LowercaseArg == "-generatemanifest")
				{
					// Generate a manifest file containing all the files required to be in Perforce
					UEBuildConfiguration.bGenerateManifest = true;
				}
				else if (LowercaseArg == "-mergemanifests")
				{
					// Whether to add to the existing manifest (if it exists), or start afresh
					UEBuildConfiguration.bMergeManifests = true;
				}
				else if (LowercaseArg == "-noxge")
				{
					BuildConfiguration.bAllowXGE = false;
				}
				else if (LowercaseArg == "-noxgemonitor")
				{
					BuildConfiguration.bShowXGEMonitor = false;
				}
				else if (LowercaseArg == "-xgeexport")
				{
					BuildConfiguration.bXGEExport = true;
					BuildConfiguration.bAllowXGE = true;
				}
				else if (LowercaseArg == "-stresstestunity")
				{
					BuildConfiguration.bStressTestUnity = true;
				}
				else if (LowercaseArg == "-disableunity")
				{
					BuildConfiguration.bUseUnityBuild = false;
				}
				else if (LowercaseArg == "-forceunity")
				{
					BuildConfiguration.bForceUnityBuild = true;
				}
				else if (LowercaseArg == "-shadowvariableerrors")
				{
					BuildConfiguration.bShadowVariableErrors = true;
				}
				// New Monolithic Graphics drivers have optional "fast calls" replacing various D3d functions
				else if (LowercaseArg == "-fastmonocalls")
				{
					BuildConfiguration.bUseFastMonoCalls = true;
				}
				else if (LowercaseArg == "-nofastmonocalls")
				{
					BuildConfiguration.bUseFastMonoCalls = false;
				}
				else if (LowercaseArg == "-uniqueintermediate")
				{
					BuildConfiguration.BaseIntermediateFolder = "Intermediate/Build/" + BuildGuid + "/";
				}
				else if (LowercaseArg == "-nopch")
				{
					BuildConfiguration.bUsePCHFiles = false;
				}
				else if (LowercaseArg == "-nosharedpch")
				{
					BuildConfiguration.bUseSharedPCHs = false;
				}
				else if (LowercaseArg == "-skipActionHistory")
				{
					BuildConfiguration.bUseActionHistory = false;
				}
				else if (LowercaseArg == "-noltcg")
				{
					BuildConfiguration.bAllowLTCG = false;
				}
				else if (LowercaseArg == "-nopdb")
				{
					BuildConfiguration.bUsePDBFiles = false;
				}
				else if (LowercaseArg == "-deploy")
				{
					BuildConfiguration.bDeployAfterCompile = true;
				}
				else if (LowercaseArg == "-copyappbundlebacktodevice")
				{
					BuildConfiguration.bCopyAppBundleBackToDevice = true;
				}
				else if (LowercaseArg == "-flushmac")
				{
					BuildConfiguration.bFlushBuildDirOnRemoteMac = true;
				}
				else if (LowercaseArg == "-nodebuginfo")
				{
					BuildConfiguration.bDisableDebugInfo = true;
				}
				else if (LowercaseArg == "-forcedebuginfo")
				{
					BuildConfiguration.bDisableDebugInfo = false;
					BuildConfiguration.bOmitPCDebugInfoInDevelopment = false;
				}
				else if (LowercaseArg == "-noubtmakefiles")
				{
					BuildConfiguration.bUseUBTMakefiles = false;
				}
				else if (LowercaseArg == "-uhtmakefiles")
				{
					BuildConfiguration.bUseUHTMakefiles = true;
				}
				else if (LowercaseArg == "-nosimplygon")
				{
					UEBuildConfiguration.bCompileSimplygon = false;
                    UEBuildConfiguration.bCompileSimplygonSSF = false;
				}
				else if (LowercaseArg == "-nospeedtree")
				{
					UEBuildConfiguration.bCompileSpeedTree = false;
				}
				else if (LowercaseArg == "-canskiplink")
				{
					UEBuildConfiguration.bSkipLinkingWhenNothingToCompile = true;
				}
				else if (LowercaseArg == "-nocef3")
				{
					UEBuildConfiguration.bCompileCEF3 = false;
				}
				else if (LowercaseArg == "-winxp")
				{
					UEBuildConfiguration.PreferredSubPlatform = "WindowsXP";
				}
				else if (LowercaseArg == "-rununrealcodeanalyzer")
				{
					BuildConfiguration.bRunUnrealCodeAnalyzer = true;
				}
				else if (LowercaseArg.StartsWith(UCAUsageThresholdString))
				{
					string UCAUsageThresholdValue = LowercaseArg.Substring(UCAUsageThresholdString.Length);
					BuildConfiguration.UCAUsageThreshold = float.Parse(UCAUsageThresholdValue.Replace(',', '.'), System.Globalization.CultureInfo.InvariantCulture);
				}
				else if (LowercaseArg.StartsWith(UCAModuleToAnalyzeString))
				{
					BuildConfiguration.UCAModuleToAnalyze = LowercaseArg.Substring(UCAModuleToAnalyzeString.Length).Trim();
				}
				else if (LowercaseArg == "-ucacheckuobjectthreadsafety")
				{
					BuildConfiguration.bUCACheckUObjectThreadSafety = true;
				}
				else if (LowercaseArg == "-ucacheckpchfiles")
				{
					BuildConfiguration.bUCACheckPCHFiles = true;
				}
				else if (LowercaseArg.StartsWith("-singlefile="))
				{
					BuildConfiguration.bUseUBTMakefiles = false;
					BuildConfiguration.SingleFileToCompile = LowercaseArg.Replace("-singlefile=", "");
				}
				else if (LowercaseArg.StartsWith("-fastpdb"))
				{
					BuildConfiguration.bUseFastPDBLinking = true;
				}
				else if (LowercaseArg == "-mapfile")
				{
					BuildConfiguration.bCreateMapFile = true;
				}
				else if (LowercaseArg.StartsWith("-architectures="))
				{
					UEBuildConfiguration.Architectures = LowercaseArg.Replace("-architectures=", "").Split(new char[] { '+' }, StringSplitOptions.RemoveEmptyEntries);
				}
				else if (LowercaseArg.StartsWith("-gpuarchitectures="))
				{
					UEBuildConfiguration.GPUArchitectures = LowercaseArg.Replace("-gpuarchitectures=", "").Split(new char[] { '+' }, StringSplitOptions.RemoveEmptyEntries);
				}
				else if(LowercaseArg == "-enablecodeanalysis")
				{
					BuildConfiguration.bEnableCodeAnalysis = true;
				}
			}
		}

		/// <summary>
		/// Sets up UBT Trace listeners and filters.
		/// </summary>
		private static void InitLogging()
		{
			// Initialize the shared log system using UBT specific listeners.
			Log.InitLogging(
				bLogTimestamps: false,
				InLogLevel: (LogEventType)Enum.Parse(typeof(LogEventType), BuildConfiguration.LogLevel),
				bLogSeverity: false,
				bLogSources: false,
				bColorConsoleOutput: true,
				TraceListeners: new[] 
                {
                    new ConsoleTraceListener(),
                    !string.IsNullOrEmpty(BuildConfiguration.LogFilename) ? new TextWriterTraceListener(new StreamWriter(new FileStream(BuildConfiguration.LogFilename, FileMode.Create, FileAccess.ReadWrite, FileShare.Read)) { AutoFlush = true }) : null,
                });
		}

		private static int DoPostStartupStuffThatCanAccessConfigs(string[] Arguments)
		{
			// We can now do a full initialization of the logging system with proper configuration values.
			InitLogging();

			// Parse rocket-specific arguments.
			FileReference ProjectFile = null;
			foreach (string Arg in Arguments)
			{
				string TempGameName = null;
				if (ParseRocketCommandlineArg(Arg, ref TempGameName, ref ProjectFile) == true)
				{
					// This is to allow relative paths for the project file
					Log.TraceVerbose("UBT Running for Rocket: " + ProjectFile);
                    break;
				}
			}

			// Build the list of game projects that we know about. When building from the editor (for hot-reload) or for projects from installed builds, we require the 
			// project file to be passed in. Otherwise we scan for projects in directories named in UE4Games.uprojectdirs.
			if (ProjectFile != null)
			{
				UProjectInfo.AddProject(ProjectFile);
			}
			else
			{
				UProjectInfo.FillProjectInfo();
			}

			DateTime BasicInitStartTime = DateTime.UtcNow;

			ECompilationResult Result = ECompilationResult.Succeeded;

			Log.TraceVerbose("UnrealBuildTool (DEBUG OUTPUT MODE)");
			Log.TraceVerbose("Command-line: {0}", String.Join(" ", Arguments));

			Telemetry.Initialize();

			bool bCheckThirdPartyHeaders = false;
			bool bIgnoreJunk = false;

			// @todo: Ideally we never need to Mutex unless we are invoked with the same target project,
			// in the same branch/path!  This would allow two clientspecs to build at the same time (even though we need
			// to make sure that XGE is only used once at a time)
			bool bUseMutex = true;
			{
				int NoMutexArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-NoMutex", out NoMutexArgumentIndex))
				{
					bUseMutex = false;
				}
			}
			bool bWaitMutex = false;
			{
				int WaitMutexArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-WaitMutex", out WaitMutexArgumentIndex))
				{
					bWaitMutex = true;
				}
			}

			bool bAutoSDKOnly = false;
			{
				int AutoSDKOnlyArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-autosdkonly", out AutoSDKOnlyArgumentIndex))
				{
					bAutoSDKOnly = true;
					bIgnoreJunk = true;
				}
			}
			bool bValidatePlatforms = false;
			{
				int ValidatePlatformsArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-validateplatform", out ValidatePlatformsArgumentIndex))
				{
					bValidatePlatforms = true;
					bIgnoreJunk = true;
				}
			}


			// Don't allow simultaneous execution of Unreal Built Tool. Multi-selection in the UI e.g. causes this and you want serial
			// execution in this case to avoid contention issues with shared produced items.
			bool bCreatedMutex = false;
			{
				Mutex SingleInstanceMutex = null;
				if (bUseMutex)
				{
					int LocationHash = Assembly.GetEntryAssembly().GetOriginalLocation().GetHashCode();

					String MutexName;
					if (bAutoSDKOnly || bValidatePlatforms)
					{
						// this mutex has to be truly global because AutoSDK installs may change global state (like a console copying files into VS directories,
						// or installing Target Management services
						MutexName = "Global/UnrealBuildTool_Mutex_AutoSDKS";
					}
					else
					{
						MutexName = "Global/UnrealBuildTool_Mutex_" + LocationHash.ToString();
					}
					SingleInstanceMutex = new Mutex(true, MutexName, out bCreatedMutex);
					if (!bCreatedMutex)
					{
						if (bWaitMutex)
						{
							try
							{
								SingleInstanceMutex.WaitOne();
							}
							catch (AbandonedMutexException)
							{
							}
						}
						else if (bAutoSDKOnly || bValidatePlatforms)
						{
							throw new BuildException("A conflicting instance of UnrealBuildTool is already running. Either -autosdkonly or -validateplatform was passed, which allows only a single instance to be run globally. Therefore, the conflicting instance may be in another location or the current location: {0}. A process manager may be used to determine the conflicting process and what tool may have launched it.", Assembly.GetEntryAssembly().GetOriginalLocation());
						}
						else
						{
							throw new BuildException("A conflicting instance of UnrealBuildTool is already running at this location: {0}. A process manager may be used to determine the conflicting process and what tool may have launched it.", Assembly.GetEntryAssembly().GetOriginalLocation());
						}
					}
				}

				try
				{
					string GameName = null;
					bool bGenerateVCProjectFiles = false;
					bool bGenerateXcodeProjectFiles = false;
					bool bGenerateMakefiles = false;
					bool bGenerateCMakefiles = false;
					bool bGenerateQMakefiles = false;
					bool bGenerateKDevelopFiles = false;
					bool bGenerateCodeLiteFiles = false;
					bool bValidPlatformsOnly = false;
					bool bSpecificModulesOnly = false;

					// We need to be able to identify if one of the arguments is the platform...
					// Leverage the existing parser function in UEBuildTarget to get this information.
					UnrealTargetPlatform CheckPlatform;
					UnrealTargetConfiguration CheckConfiguration;
					UEBuildTarget.ParsePlatformAndConfiguration(Arguments, out CheckPlatform, out CheckConfiguration, false);

					// @todo ubtmake: remove this when building with RPCUtility works
					// @todo tvos merge: Check the change to this line, not clear why. Is TVOS needed here?
					if (CheckPlatform == UnrealTargetPlatform.Mac || CheckPlatform == UnrealTargetPlatform.IOS || CheckPlatform == UnrealTargetPlatform.TVOS)
					{
						BuildConfiguration.bUseUBTMakefiles = false;
					}

					// If we were asked to enable fast build iteration, we want the 'gather' phase to default to off (unless it is overridden below
					// using a command-line option.)
					if (BuildConfiguration.bUseUBTMakefiles)
					{
						bIsGatheringBuild_Unsafe = false;
					}

					foreach (string Arg in Arguments)
					{
						string LowercaseArg = Arg.ToLowerInvariant();
						const string OnlyPlatformSpecificForArg = "-onlyplatformspecificfor=";
						if (ProjectFile == null && ParseRocketCommandlineArg(Arg, ref GameName, ref ProjectFile))
						{
							// Already handled at startup. Calling now just to properly set the game name
							continue;
						}
						else if (LowercaseArg == "-skipnonhostplatforms")
						{
							bSkipNonHostPlatforms = true;
						}
						else if (LowercaseArg.StartsWith(OnlyPlatformSpecificForArg))
						{
							string OtherPlatform = LowercaseArg.Substring(OnlyPlatformSpecificForArg.Length);
							foreach (UnrealTargetPlatform platform in Enum.GetValues(typeof(UnrealTargetPlatform)))
							{
								if (OtherPlatform == platform.ToString().ToLowerInvariant())
								{
									OnlyPlatformSpecificFor = platform;
								}
							}
							if (OnlyPlatformSpecificFor == UnrealTargetPlatform.Unknown)
							{
								throw new BuildException("Could not parse {0}", LowercaseArg);
							}
						}
						else if (LowercaseArg.StartsWith("-makefile"))
						{
							bGenerateMakefiles = true;
							ProjectFileGenerator.bGenerateProjectFiles = true;
						}
						else if (LowercaseArg.StartsWith("-cmakefile"))
						{
							bGenerateCMakefiles = true;
							ProjectFileGenerator.bGenerateProjectFiles = true;
						}
						else if (LowercaseArg.StartsWith("-qmakefile"))
						{
							bGenerateQMakefiles = true;
							ProjectFileGenerator.bGenerateProjectFiles = true;
						}
						else if (LowercaseArg.StartsWith("-kdevelopfile"))
						{
							bGenerateKDevelopFiles = true;
							ProjectFileGenerator.bGenerateProjectFiles = true;
						}
						else if (LowercaseArg.StartsWith("-codelitefile"))
						{
							bGenerateCodeLiteFiles = true;
							ProjectFileGenerator.bGenerateProjectFiles = true;
						}
						else if (LowercaseArg.StartsWith("-projectfile"))
						{
							bGenerateVCProjectFiles = true;
							ProjectFileGenerator.bGenerateProjectFiles = true;
						}
						else if (LowercaseArg.StartsWith("-xcodeprojectfile"))
						{
							bGenerateXcodeProjectFiles = true;
							ProjectFileGenerator.bGenerateProjectFiles = true;
						}
						else if (LowercaseArg == "-validplatformsonly")
						{
							bValidPlatformsOnly = true;
						}
						else if (LowercaseArg == "development" || LowercaseArg == "debug" || LowercaseArg == "shipping" || LowercaseArg == "test" || LowercaseArg == "debuggame")
						{
							//ConfigName = Arg;
						}
						else if (LowercaseArg == "-modulewithsuffix")
						{
							bSpecificModulesOnly = true;
							continue;
						}
						else if (LowercaseArg == "-check3rdpartyheaders")
						{
							bCheckThirdPartyHeaders = true;
						}
						else if (LowercaseArg == "-invalidatemakefilesonly")
						{
							UnrealBuildTool.bIsInvalidatingMakefilesOnly = true;
						}
						else if (LowercaseArg == "-gather")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = true;
						}
						else if (LowercaseArg == "-nogather")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = false;
						}
						else if (LowercaseArg == "-gatheronly")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = true;
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = false;
						}
						else if (LowercaseArg == "-assemble")
						{
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = true;
						}
						else if (LowercaseArg == "-noassemble")
						{
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = false;
						}
						else if (LowercaseArg == "-assembleonly")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = false;
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = true;
						}
						else if (LowercaseArg == "-ignorejunk")
						{
							bIgnoreJunk = true;
						}
						else if (LowercaseArg == "-define")
						{
							// Skip -define
						}
						else if (LowercaseArg == "-progress")
						{
							ProgressWriter.bWriteMarkup = true;
						}
						else if (CheckPlatform.ToString().ToLowerInvariant() == LowercaseArg)
						{
							// It's the platform set...
							//PlatformName = Arg;
						}
						else
						{
							// This arg may be a game name. Check for the existence of a game folder with this name.
							// "Engine" is not a valid game name.
							if (LowercaseArg != "engine" && Arg.IndexOfAny(Path.GetInvalidPathChars()) == -1 && Arg.IndexOfAny(new char[]{ ':', Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar}) == -1 &&
								Directory.Exists(Path.Combine(ProjectFileGenerator.RootRelativePath, Arg, "Config")))
							{
								GameName = Arg;
								Log.TraceVerbose("CommandLine: Found game name '{0}'", GameName);
							}
						}
					}

					if (!bIsGatheringBuild_Unsafe && !bIsAssemblingBuild_Unsafe)
					{
						throw new BuildException("UnrealBuildTool: At least one of either IsGatheringBuild or IsAssemblingBuild must be true.  Did you pass '-NoGather' with '-NoAssemble'?");
					}

					if (BuildConfiguration.bRunUnrealCodeAnalyzer)
					{
						if (BuildConfiguration.UCAModuleToAnalyze == null || BuildConfiguration.UCAModuleToAnalyze.Length == 0)
						{
							throw new BuildException("When running UnrealCodeAnalyzer, please specify module to analyze in UCAModuleToAnalyze field in BuildConfiguration.xml");
						}

						if ((BuildConfiguration.bUCACheckPCHFiles && BuildConfiguration.bUCACheckUObjectThreadSafety)
							|| (!BuildConfiguration.bUCACheckPCHFiles && !BuildConfiguration.bUCACheckUObjectThreadSafety))
						{
							throw new BuildException("When running UnrealCodeAnalyzer, please specify exactly one action to execute");
						}
					}

					// Get the build version
					BuildVersion Version;
					if (!BuildVersion.TryRead("../Build/Build.version", out Version))
					{
						Log.TraceWarning("Could not read build.version");
					}

					// Send an event with basic usage dimensions
					// [RCL] 2014-11-03 FIXME: this is incorrect since we can have more than one action
					Telemetry.SendEvent("CommonAttributes.2",
						// @todo No platform independent way to do processor speed and count. Defer for now. Mono actually has ways to do this but needs to be tested.
						"ProcessorCount", Environment.ProcessorCount.ToString(),
						"ComputerName", Environment.MachineName,
						"User", Environment.UserName,
						"Domain", Environment.UserDomainName,
						"CommandLine", string.Join("|", Arguments),
						"UBT Action", bGenerateVCProjectFiles
							? "GenerateVCProjectFiles"
							: bGenerateXcodeProjectFiles
							? "GenerateXcodeProjectFiles"
							: bGenerateMakefiles
							? "GenerateMakefiles"
							: bGenerateCMakefiles
							? "GenerateCMakeFiles"
							: bGenerateQMakefiles
							? "GenerateQMakefiles"
							: bGenerateKDevelopFiles
							? "GenerateKDevelopFiles"
							: bGenerateCodeLiteFiles
							? "GenerateCodeLiteFiles"
							: bValidatePlatforms
							? "ValidatePlatforms"
							: "Build",
						"Platform", CheckPlatform.ToString(),
						"Configuration", CheckConfiguration.ToString(),
						"EngineVersion", (Version == null) ? "0" : Version.Changelist.ToString(),
						"Branch", (Version == null) ? "" : Version.BranchName
						);



					if (bValidPlatformsOnly == true)
					{
						// We turned the flag on if generating projects so that the
						// build platforms would know we are doing so... In this case,
						// turn it off to only generate projects for valid platforms.
						ProjectFileGenerator.bGenerateProjectFiles = false;
					}

					// Find and register all tool chains, build platforms, etc. that are present
					RegisterAllUBTClasses(bValidatingPlatforms:bValidatePlatforms);
					ProjectFileGenerator.bGenerateProjectFiles = false;

					if (BuildConfiguration.bPrintPerformanceInfo)
					{
						double BasicInitTime = (DateTime.UtcNow - BasicInitStartTime).TotalSeconds;
						Log.TraceInformation("Basic UBT initialization took " + BasicInitTime + "s");
					}

					// now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
					if (!bSpecificModulesOnly && !bIgnoreJunk)
					{
						JunkDeleter.DeleteJunk();
					}

					if (bGenerateCodeLiteFiles || bGenerateVCProjectFiles || bGenerateXcodeProjectFiles || bGenerateMakefiles || bGenerateCMakefiles || bGenerateQMakefiles || bGenerateKDevelopFiles)
					{
						bool bGenerationSuccess = true;
						if (bGenerateVCProjectFiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new VCProjectFileGenerator(ProjectFile), Arguments);
						}
						if (bGenerateXcodeProjectFiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new XcodeProjectFileGenerator(ProjectFile), Arguments);
						}
						if (bGenerateMakefiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new MakefileGenerator(ProjectFile), Arguments);
						}
						if (bGenerateCMakefiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new CMakefileGenerator(ProjectFile), Arguments);
						}
						if (bGenerateQMakefiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new QMakefileGenerator(ProjectFile), Arguments);
						}
						if (bGenerateKDevelopFiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new KDevelopGenerator(ProjectFile), Arguments);
						}
						if (bGenerateCodeLiteFiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new CodeLiteGenerator(ProjectFile), Arguments);
						}
						if (!bGenerationSuccess)
						{
							Result = ECompilationResult.OtherCompilationError;
						}
					}
					else if (bAutoSDKOnly)
					{
						// RegisterAllUBTClasses has already done all the SDK setup.
						Result = ECompilationResult.Succeeded;
					}
					else if (bValidatePlatforms)
					{
						ValidatePlatforms(Arguments);
					}
					else
					{
						// Check if any third party headers are included from public engine headers.
						if (bCheckThirdPartyHeaders)
						{
							ThirdPartyHeaderFinder.FindThirdPartyIncludes(CheckPlatform, CheckConfiguration, "");
						}

						// Build our project
						if (Result == ECompilationResult.Succeeded)
						{
							if (UEBuildConfiguration.bPrepForDeployment == false)
							{
								// If we are only prepping for deployment, assume the build already occurred.
								Result = RunUBT(Arguments, ProjectFile);
							}
							else
							{
								UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(CheckPlatform, true);
								if (BuildPlatform != null)
								{
									// Setup environment wasn't called, so set the flag
									BuildConfiguration.bDeployAfterCompile = BuildPlatform.RequiresDeployPrepAfterCompile();
									BuildConfiguration.PlatformIntermediateFolder = Path.Combine(BuildConfiguration.BaseIntermediateFolder, CheckPlatform.ToString(), BuildPlatform.CreateContext(ProjectFile).GetActiveArchitectureFolderName());
								}
							}

							// If we build w/ bXGEExport true, we didn't REALLY build at this point, 
							// so don't bother with doing the PrepTargetForDeployment call. 
							if ((Result == ECompilationResult.Succeeded) && (BuildConfiguration.bDeployAfterCompile == true) && (BuildConfiguration.bXGEExport == false) && String.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile) && 
								(UEBuildConfiguration.bGenerateManifest == false) && (UEBuildConfiguration.bGenerateExternalFileList == false) && (UEBuildConfiguration.bCleanProject == false) && (UEBuildConfiguration.bListBuildFolders == false))
							{
								List<TargetDescriptor> TargetDescs = UEBuildTarget.ParseTargetCommandLine(Arguments, ref ProjectFile);
								if (TargetDescs[0].OnlyModules.Count == 0)
								{
									UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(CheckPlatform);
									UEBuildPlatformContext PlatformContext = BuildPlatform.CreateContext(TargetDescs[0].ProjectFile);

									UEBuildDeploy DeploymentHandler = PlatformContext.CreateDeploymentHandler();
									if (DeploymentHandler != null)
									{
										// We need to be able to identify the Target.Type we can derive it from the Arguments.
										BuildConfiguration.bFlushBuildDirOnRemoteMac = false;
										UEBuildTarget CheckTarget = UEBuildTarget.CreateTarget(TargetDescs[0]); // @todo ubtmake: This may not work in assembler only mode.  We don't want to be loading target rules assemblies here either.
										UEToolChain ToolChain = PlatformContext.CreateToolChainForDefaultCppPlatform();
										CheckTarget.SetupGlobalEnvironment(ToolChain);
										if ((CheckTarget.TargetType == TargetRules.TargetType.Game) ||
											(CheckTarget.TargetType == TargetRules.TargetType.Server) ||
											(CheckTarget.TargetType == TargetRules.TargetType.Client))
										{
											CheckTarget.AppName = CheckTarget.TargetName;
										}
										DeploymentHandler.PrepTargetForDeployment(CheckTarget);
									}
								}
							}
						}
					}
					// Print some performance info
					double BuildDuration = (DateTime.UtcNow - StartTime).TotalSeconds;
					if (BuildConfiguration.bPrintPerformanceInfo)
					{
						Log.TraceInformation("GetIncludes time: " + CPPEnvironment.TotalTimeSpentGettingIncludes + "s (" + CPPEnvironment.TotalIncludesRequested + " includes)");
						Log.TraceInformation("DirectIncludes cache miss time: " + CPPEnvironment.DirectIncludeCacheMissesTotalTime + "s (" + CPPEnvironment.TotalDirectIncludeCacheMisses + " misses)");
						Log.TraceInformation("FindIncludePaths calls: " + CPPEnvironment.TotalFindIncludedFileCalls + " (" + CPPEnvironment.IncludePathSearchAttempts + " searches)");
						Log.TraceInformation("PCH gen time: " + UEBuildModuleCPP.TotalPCHGenTime + "s");
						Log.TraceInformation("PCH cache time: " + UEBuildModuleCPP.TotalPCHCacheTime + "s");
						Log.TraceInformation("Deep C++ include scan time: " + UnrealBuildTool.TotalDeepIncludeScanTime + "s");
						Log.TraceInformation("Include Resolves: {0} ({1} misses, {2:0.00}%)", CPPEnvironment.TotalDirectIncludeResolves, CPPEnvironment.TotalDirectIncludeResolveCacheMisses, (float)CPPEnvironment.TotalDirectIncludeResolveCacheMisses / (float)CPPEnvironment.TotalDirectIncludeResolves * 100);
						Log.TraceInformation("Total FileItems: {0} ({1} missing)", FileItem.TotalFileItemCount, FileItem.MissingFileItemCount);

						Log.TraceInformation("Execution time: {0}s", BuildDuration);
					}
				}
				catch (Exception Exception)
				{
					Log.TraceError("UnrealBuildTool Exception: " + Exception);
					Result = ECompilationResult.OtherCompilationError;
				}

				if (bUseMutex)
				{
					// Release the mutex to avoid the abandoned mutex timeout.
					SingleInstanceMutex.ReleaseMutex();
					SingleInstanceMutex.Dispose();
					SingleInstanceMutex = null;
				}
			}

			Telemetry.Shutdown();

			// Print some performance info
			Log.TraceVerbose("Execution time: {0}", (DateTime.UtcNow - StartTime).TotalSeconds);

			if (ExtendedErrorCode != 0)
			{
				return ExtendedErrorCode;
			}
			return (int)Result;
		}

		public static int ExtendedErrorCode = 0;
		private static int Main(string[] Arguments)
		{
            // make sure we catch any exceptions and return an appropriate error code.
            // Some inner code already does this (to ensure the Mutex is released),
            // but we need something to cover all outer code as well.

            // Console.WriteLine("UBT Cmd Line: " + string.Join(" ", Arguments));

			try
			{
				// Make it more explicit what startup code should not be accessing config data.
				// The Configuration classes will also assert, but this clarifies where the line is.
				DoStartupStuffThatCannotAccessConfigurationClasses(Arguments);

				// Now we can load the configuration files.
				InitConfigurationClasses(Arguments);

				// Finally, we do execute code that relies on config classes.
				return DoPostStartupStuffThatCanAccessConfigs(Arguments);
			}
			catch (Exception Exception)
			{
				if (Log.IsInitialized())
				{
					Log.TraceError("UnrealBuildTool Exception: " + Exception.ToString());
				}
				if (ExtendedErrorCode != 0)
				{
					return ExtendedErrorCode;
				}
				return (int)ECompilationResult.OtherCompilationError;
			}
		}

		/// <summary>
		/// Generates project files.  Can also be used to generate projects "in memory", to allow for building
		/// targets without even having project files at all.
		/// </summary>
		/// <param name="Generator">Project generator to use</param>
		/// <param name="Arguments">Command-line arguments</param>
		/// <returns>True if successful</returns>
		public static bool GenerateProjectFiles(ProjectFileGenerator Generator, string[] Arguments)
		{
			bool bSuccess;
			ProjectFileGenerator.bGenerateProjectFiles = true;
			Generator.GenerateProjectFiles(Arguments, out bSuccess);
			ProjectFileGenerator.bGenerateProjectFiles = false;
			return bSuccess;
		}



		/// <summary>
		/// Validates the various platforms to determine if they are ready for building
		/// </summary>
		public static void ValidatePlatforms(string[] Arguments)
		{
			List<UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
			foreach (string CurArgument in Arguments)
			{
				if (CurArgument.StartsWith("-"))
				{
					if (CurArgument.StartsWith("-Platforms=", StringComparison.InvariantCultureIgnoreCase))
					{
						// Parse the list... will be in Foo+Bar+New format
						string PlatformList = CurArgument.Substring(11);
						while (PlatformList.Length > 0)
						{
							string PlatformString = PlatformList;
							Int32 PlusIdx = PlatformList.IndexOf("+");
							if (PlusIdx != -1)
							{
								PlatformString = PlatformList.Substring(0, PlusIdx);
								PlatformList = PlatformList.Substring(PlusIdx + 1);
							}
							else
							{
								// We are on the last platform... clear the list to exit the loop
								PlatformList = "";
							}

							// Is the string a valid platform? If so, add it to the list
							UnrealTargetPlatform SpecifiedPlatform = UnrealTargetPlatform.Unknown;
							foreach (UnrealTargetPlatform PlatformParam in Enum.GetValues(typeof(UnrealTargetPlatform)))
							{
								if (PlatformString.Equals(PlatformParam.ToString(), StringComparison.InvariantCultureIgnoreCase))
								{
									SpecifiedPlatform = PlatformParam;
									break;
								}
							}

							if (SpecifiedPlatform != UnrealTargetPlatform.Unknown)
							{
								if (Platforms.Contains(SpecifiedPlatform) == false)
								{
									Platforms.Add(SpecifiedPlatform);
								}
							}
							else
							{
								Log.TraceWarning("ValidatePlatforms invalid platform specified: {0}", PlatformString);
							}
						}
					}
					else switch (CurArgument.ToUpperInvariant())
						{
							case "-ALLPLATFORMS":
								foreach (UnrealTargetPlatform platform in Enum.GetValues(typeof(UnrealTargetPlatform)))
								{
									if (platform != UnrealTargetPlatform.Unknown)
									{
										if (Platforms.Contains(platform) == false)
										{
											Platforms.Add(platform);
										}
									}
								}
								break;
						}
				}
			}

			foreach (UnrealTargetPlatform platform in Platforms)
			{
				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(platform, true);
				if (BuildPlatform != null && BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
				{
					Console.WriteLine("##PlatformValidate: {0} VALID", platform.ToString());
				}
				else
				{
					Console.WriteLine("##PlatformValidate: {0} INVALID", platform.ToString());
				}
			}
		}


		public static ECompilationResult RunUBT(string[] Arguments, FileReference ProjectFile)
		{
			bool bSuccess = true;


			DateTime RunUBTInitStartTime = DateTime.UtcNow;


			// Reset global configurations
			ActionGraph.ResetAllActions();

			// We need to allow the target platform to perform the 'reset' as well...
			UnrealTargetPlatform ResetPlatform = UnrealTargetPlatform.Unknown;
			UnrealTargetConfiguration ResetConfiguration;
			UEBuildTarget.ParsePlatformAndConfiguration(Arguments, out ResetPlatform, out ResetConfiguration);
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(ResetPlatform);
			UEBuildPlatformContext BuildPlatformContext = BuildPlatform.CreateContext(ProjectFile);
			BuildPlatformContext.ResetBuildConfiguration(ResetConfiguration);

			// now that we have the platform, we can set the intermediate path to include the platform/architecture name
			BuildConfiguration.PlatformIntermediateFolder = Path.Combine(BuildConfiguration.BaseIntermediateFolder, ResetPlatform.ToString(), BuildPlatformContext.GetActiveArchitectureFolderName());

			string ExecutorName = "Unknown";
			ECompilationResult BuildResult = ECompilationResult.Succeeded;

			UEToolChain ToolChain = BuildPlatformContext.CreateToolChainForDefaultCppPlatform();

			Thread CPPIncludesThread = null;

			try
			{
				List<string[]> TargetSettings = ParseCommandLineFlags(Arguments);

				int ArgumentIndex;
				// action graph implies using the dependency resolve cache
				bool GeneratingActionGraph = Utils.ParseCommandLineFlag(Arguments, "-graph", out ArgumentIndex);
				if (GeneratingActionGraph)
				{
					BuildConfiguration.bUseIncludeDependencyResolveCache = true;
				}

				bool CreateStub = Utils.ParseCommandLineFlag(Arguments, "-nocreatestub", out ArgumentIndex);
				if (CreateStub || (String.IsNullOrEmpty(Environment.GetEnvironmentVariable("uebp_LOCAL_ROOT")) && BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac))
				{
					BuildConfiguration.bCreateStubIPA = false;
				}


				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					double RunUBTInitTime = (DateTime.UtcNow - RunUBTInitStartTime).TotalSeconds;
					Log.TraceInformation("RunUBT initialization took " + RunUBTInitTime + "s");
				}

				List<TargetDescriptor> TargetDescs = new List<TargetDescriptor>();
				{
					DateTime TargetDescstStartTime = DateTime.UtcNow;

					foreach (string[] TargetSetting in TargetSettings)
					{
						TargetDescs.AddRange(UEBuildTarget.ParseTargetCommandLine(TargetSetting, ref ProjectFile));
					}

					if (BuildConfiguration.bPrintPerformanceInfo)
					{
						double TargetDescsTime = (DateTime.UtcNow - TargetDescstStartTime).TotalSeconds;
						Log.TraceInformation("Target descriptors took " + TargetDescsTime + "s");
					}
				}

				if (UnrealBuildTool.bIsInvalidatingMakefilesOnly)
				{
					Log.TraceInformation("Invalidating makefiles only in this run.");
					if (TargetDescs.Count != 1)
					{
						Log.TraceError("You have to provide one target name for makefile invalidation.");
						return ECompilationResult.OtherCompilationError;
					}

					InvalidateMakefiles(TargetDescs[0]);
					return ECompilationResult.Succeeded;
				}

				bool bNoHotReload = UnrealBuildTool.CommandLineContains("-NoHotReload");
				UEBuildConfiguration.bHotReloadFromIDE = !bNoHotReload && UEBuildConfiguration.bAllowHotReloadFromIDE && TargetDescs.Count == 1 && !TargetDescs[0].bIsEditorRecompile && ShouldDoHotReloadFromIDE(TargetDescs[0]);
				bool bIsHotReload = !bNoHotReload && (UEBuildConfiguration.bHotReloadFromIDE || (TargetDescs.Count == 1 && TargetDescs[0].OnlyModules.Count > 0 && TargetDescs[0].ForeignPlugins.Count == 0));
				TargetDescriptor HotReloadTargetDesc = bIsHotReload ? TargetDescs[0] : null;

				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					// Create empty timestamp file to record when was the last time we regenerated projects.
					Directory.CreateDirectory(Path.GetDirectoryName(ProjectFileGenerator.ProjectTimestampFile));
					File.Create(ProjectFileGenerator.ProjectTimestampFile).Dispose();
				}

				if (!ProjectFileGenerator.bGenerateProjectFiles)
				{
					if (BuildConfiguration.bUseUBTMakefiles)
					{
						// Only the modular editor and game targets will share build products.  Unfortunately, we can't determine at
						// at this point whether we're dealing with modular or monolithic game binaries, so we opt to always invalidate
						// cached includes if the target we're switching to is either a game target (has project file) or "UE4Editor".
						bool bMightHaveSharedBuildProducts = 
							ProjectFile != null ||	// Is this a game? (has a .uproject file for the target)
							TargetDescs[ 0 ].TargetName.Equals( "UE4Editor", StringComparison.InvariantCultureIgnoreCase );	// Is the engine?
						if( bMightHaveSharedBuildProducts )
						{
							bool bIsBuildingSameTargetsAsLastTime = false;

							string TargetCollectionName = MakeTargetCollectionName(TargetDescs);

							string LastBuiltTargetsFileName = bIsHotReload ? "HotReloadLastBuiltTargets.txt" : "LastBuiltTargets.txt";
							string LastBuiltTargetsFilePath = Path.Combine(BuildConfiguration.BaseIntermediatePath, LastBuiltTargetsFileName);
							if (File.Exists(LastBuiltTargetsFilePath) && Utils.ReadAllText(LastBuiltTargetsFilePath) == TargetCollectionName)
							{
								// @todo ubtmake: Because we're using separate files for hot reload vs. full compiles, it's actually possible that includes will
								// become out of date without us knowing if the developer ping-pongs between hot reloading target A and building target B normally.
								// To fix this we can not use a different file name for last built targets, but the downside is slower performance when
								// performing the first hot reload after compiling normally (forces full include dependency scan)
								bIsBuildingSameTargetsAsLastTime = true;
							}

							// Save out the name of the targets we're building
							if (!bIsBuildingSameTargetsAsLastTime)
							{
								Directory.CreateDirectory(Path.GetDirectoryName(LastBuiltTargetsFilePath));
								File.WriteAllText(LastBuiltTargetsFilePath, TargetCollectionName, Encoding.UTF8);
							}

							if (!bIsBuildingSameTargetsAsLastTime)
							{
								// Can't use super fast include checking unless we're building the same set of targets as last time, because
								// we might not know about all of the C++ include dependencies for already-up-to-date shared build products
								// between the targets
								bNeedsFullCPPIncludeRescan = true;
								Log.TraceInformation("Performing full C++ include scan ({0} a new target)", bIsHotReload ? "hot reloading" : "building");
							}
						}
					}
				}



				UBTMakefile UBTMakefile = null;
				{
					// If we're generating project files, then go ahead and wipe out the existing UBTMakefile for every target, to make sure that
					// it gets a full dependency scan next time.
					// NOTE: This is just a safeguard and doesn't have to be perfect.  We also check for newer project file timestamps in LoadUBTMakefile()
					FileReference UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath(TargetDescs);
					if (ProjectFileGenerator.bGenerateProjectFiles)	// @todo ubtmake: This is only hit when generating IntelliSense for project files.  Probably should be done right inside ProjectFileGenerator.bat
					{													// @todo ubtmake: Won't catch multi-target cases as GPF always builds one target at a time for Intellisense
						// Delete the UBTMakefile
						if (UBTMakefilePath.Exists())
						{
							UEBuildTarget.CleanFile(UBTMakefilePath.FullName);
						}
					}

					// Make sure the gather phase is executed if we're not actually building anything
					if (ProjectFileGenerator.bGenerateProjectFiles || UEBuildConfiguration.bGenerateManifest || UEBuildConfiguration.bCleanProject || BuildConfiguration.bXGEExport || UEBuildConfiguration.bGenerateExternalFileList || GeneratingActionGraph)
					{
						UnrealBuildTool.bIsGatheringBuild_Unsafe = true;
					}

					// Were we asked to run in 'assembler only' mode?  If so, let's check to see if that's even possible by seeing if
					// we have a valid UBTMakefile already saved to disk, ready for us to load.
					if (UnrealBuildTool.bIsAssemblingBuild_Unsafe && !UnrealBuildTool.bIsGatheringBuild_Unsafe)
					{
						// @todo ubtmake: Mildly terrified of BuildConfiguration/UEBuildConfiguration globals that were set during the Prepare phase but are not set now.  We may need to save/load all of these, otherwise
						//		we'll need to call SetupGlobalEnvironment on all of the targets (maybe other stuff, too.  See PreBuildStep())

						// Try to load the UBTMakefile.  It will only be loaded if it has valid content and is not determined to be out of date.    
						string ReasonNotLoaded;
						UBTMakefile = LoadUBTMakefile(UBTMakefilePath, ProjectFile, out ReasonNotLoaded);

						// Invalid makefile if only modules have changed
						if (UBTMakefile != null && !TargetDescs.SelectMany(x => x.OnlyModules)
								.Select(x => new Tuple<string, bool>(x.OnlyModuleName.ToLower(), string.IsNullOrWhiteSpace(x.OnlyModuleSuffix)))
								.SequenceEqual(
									UBTMakefile.Targets.SelectMany(x => x.OnlyModules)
									.Select(x => new Tuple<string, bool>(x.OnlyModuleName.ToLower(), string.IsNullOrWhiteSpace(x.OnlyModuleSuffix)))
								)
							)
						{
							UBTMakefile = null;
							ReasonNotLoaded = "modules to compile have changed";
						}

						// Invalid makefile if foreign plugins have changed
						if (UBTMakefile != null && !TargetDescs.SelectMany(x => x.ForeignPlugins)
								.Select(x => new Tuple<string, bool>(x.FullName.ToLower(), string.IsNullOrWhiteSpace(x.FullName)))
								.SequenceEqual(
									UBTMakefile.Targets.SelectMany(x => x.ForeignPlugins)
									.Select(x => new Tuple<string, bool>(x.FullName.ToLower(), string.IsNullOrWhiteSpace(x.FullName)))
								)
							)
						{
							UBTMakefile = null;
							ReasonNotLoaded = "foreign plugins have changed";
						}

						if (UBTMakefile == null)
						{
							// If the Makefile couldn't be loaded, then we're not going to be able to continue in "assembler only" mode.  We'll do both
							// a 'gather' and 'assemble' in the same run.  This will take a while longer, but subsequent runs will be fast!
							UnrealBuildTool.bIsGatheringBuild_Unsafe = true;

							FileItem.ClearCaches();

							Log.TraceInformation("Creating makefile for {0}{1}{2} ({3})",
								bIsHotReload ? "hot reloading " : "",
								TargetDescs[0].TargetName,
								TargetDescs.Count > 1 ? (" (and " + (TargetDescs.Count - 1).ToString() + " more)") : "",
								ReasonNotLoaded);

							// Invalidate UHT makefiles too
							ExternalExecution.bInvalidateUHTMakefile = true;
						}
					}

					// OK, after this point it is safe to access the UnrealBuildTool.IsGatheringBuild and UnrealBuildTool.IsAssemblingBuild properties.
					// These properties will not be changing again during this session/
					bIsSafeToCheckIfGatheringOrAssemblingBuild = true;
				}


				List<UEBuildTarget> Targets;
				if (UBTMakefile != null && !IsGatheringBuild && IsAssemblingBuild)
				{
					// If we've loaded a makefile, then we can fill target information from this file!
					Targets = UBTMakefile.Targets;
				}
				else
				{
					DateTime TargetInitStartTime = DateTime.UtcNow;

					Targets = new List<UEBuildTarget>();
					foreach (TargetDescriptor TargetDesc in TargetDescs)
					{
						UEBuildTarget Target = UEBuildTarget.CreateTarget(TargetDesc);
						if ((Target == null) && (UEBuildConfiguration.bCleanProject))
						{
							continue;
						}
						Targets.Add(Target);
					}

					if (BuildConfiguration.bPrintPerformanceInfo)
					{
						double TargetInitTime = (DateTime.UtcNow - TargetInitStartTime).TotalSeconds;
						Log.TraceInformation("Target init took " + TargetInitTime + "s");
					}
				}

				// Build action lists for all passed in targets.
				List<FileItem> OutputItemsForAllTargets = new List<FileItem>();
				Dictionary<string, List<UHTModuleInfo>> TargetNameToUObjectModules = new Dictionary<string, List<UHTModuleInfo>>(StringComparer.InvariantCultureIgnoreCase);
				foreach (UEBuildTarget Target in Targets)
				{
					if (bIsHotReload)
					{
						// Don't produce new DLLs if there's been no code changes
						UEBuildConfiguration.bSkipLinkingWhenNothingToCompile = true;
						Log.TraceInformation("Compiling game modules for hot reload");
					}

					// When in 'assembler only' mode, we'll load this cache later on a worker thread.  It takes a long time to load!
					if (!(!UnrealBuildTool.IsGatheringBuild && UnrealBuildTool.IsAssemblingBuild))
					{
						// Load the direct include dependency cache.
						CPPEnvironment.IncludeDependencyCache.Add(Target, DependencyCache.Create(DependencyCache.GetDependencyCachePathForTarget(Target)));
					}

					// We don't need this dependency cache in 'gather only' mode
					if (BuildConfiguration.bUseUBTMakefiles &&
						!(UnrealBuildTool.IsGatheringBuild && !UnrealBuildTool.IsAssemblingBuild))
					{
						// Load the cache that contains the list of flattened resolved includes for resolved source files
						// @todo ubtmake: Ideally load this asynchronously at startup and only block when it is first needed and not finished loading
						CPPEnvironment.FlatCPPIncludeDependencyCache.Add(Target, FlatCPPIncludeDependencyCache.Create(Target));
					}

					if (UnrealBuildTool.IsGatheringBuild)
					{
						List<FileItem> TargetOutputItems;
						List<UHTModuleInfo> TargetUObjectModules;
						BuildResult = Target.Build(ToolChain, out TargetOutputItems, out TargetUObjectModules);
						if (BuildResult != ECompilationResult.Succeeded)
						{
							break;
						}

						OutputItemsForAllTargets.AddRange(TargetOutputItems);

						// Update mapping of the target name to the list of UObject modules in this target
						TargetNameToUObjectModules[Target.GetTargetName()] = TargetUObjectModules;

						if ((BuildConfiguration.bXGEExport && UEBuildConfiguration.bGenerateManifest) || (!ProjectFileGenerator.bGenerateProjectFiles && !UEBuildConfiguration.bGenerateManifest && !UEBuildConfiguration.bCleanProject))
						{
							// Generate an action graph if we were asked to do that.  The graph generation needs access to the include dependency cache, so
							// we generate it before saving and cleaning that up.
							if (GeneratingActionGraph)
							{
								// The graph generation feature currently only works with a single target at a time.  This is because of how we need access
								// to include dependencies for the target, but those aren't kept around as we process multiple targets
								if (TargetSettings.Count != 1)
								{
									throw new BuildException("ERROR: The '-graph' option only works with a single target at a time");
								}
								ActionGraph.FinalizeActionGraph();
								List<Action> ActionsToExecute = ActionGraph.AllActions;

								ActionGraph.ActionGraphVisualizationType VisualizationType = ActionGraph.ActionGraphVisualizationType.OnlyCPlusPlusFilesAndHeaders;
								ActionGraph.SaveActionGraphVisualization(Target, Path.Combine(BuildConfiguration.BaseIntermediatePath, Target.GetTargetName() + ".gexf"), Target.GetTargetName(), VisualizationType, ActionsToExecute);
							}
						}
					}
				}

				if (BuildResult == ECompilationResult.Succeeded &&
					(
						(BuildConfiguration.bXGEExport && UEBuildConfiguration.bGenerateManifest) ||
						(!GeneratingActionGraph && !ProjectFileGenerator.bGenerateProjectFiles && !UEBuildConfiguration.bGenerateManifest && !UEBuildConfiguration.bCleanProject && !UEBuildConfiguration.bGenerateExternalFileList && !UEBuildConfiguration.bListBuildFolders)
					))
				{
					if (UnrealBuildTool.IsGatheringBuild)
					{
						ActionGraph.FinalizeActionGraph();

						UBTMakefile = new UBTMakefile();
						UBTMakefile.AllActions = ActionGraph.AllActions;

						// For now simply treat all object files as the root target.
						{
							HashSet<Action> PrerequisiteActionsSet = new HashSet<Action>();
							foreach (FileItem OutputItem in OutputItemsForAllTargets)
							{
								ActionGraph.GatherPrerequisiteActions(OutputItem, ref PrerequisiteActionsSet);
							}
							UBTMakefile.PrerequisiteActions = PrerequisiteActionsSet.ToArray();
						}

						foreach (System.Collections.DictionaryEntry EnvironmentVariable in Environment.GetEnvironmentVariables())
						{
							UBTMakefile.EnvironmentVariables.Add(Tuple.Create((string)EnvironmentVariable.Key, (string)EnvironmentVariable.Value));
						}
						UBTMakefile.TargetNameToUObjectModules = TargetNameToUObjectModules;
						UBTMakefile.Targets = Targets;
						UBTMakefile.SourceFileWorkingSet = Unity.SourceFileWorkingSet;

						if (BuildConfiguration.bUseUBTMakefiles)
						{
							// We've been told to prepare to build, so let's go ahead and save out our action graph so that we can use in a later invocation 
							// to assemble the build.  Even if we are configured to assemble the build in this same invocation, we want to save out the
							// Makefile so that it can be used on subsequent 'assemble only' runs, for the fastest possible iteration times
							// @todo ubtmake: Optimization: We could make 'gather + assemble' mode slightly faster by saving this while busy compiling (on our worker thread)
							SaveUBTMakefile(TargetDescs, UBTMakefile);
						}
					}

					if (UnrealBuildTool.IsAssemblingBuild)
					{
						// If we didn't build the graph in this session, then we'll need to load a cached one
						if (!UnrealBuildTool.IsGatheringBuild && BuildResult.Succeeded())
						{
							ActionGraph.AllActions = UBTMakefile.AllActions;

							// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
							// the same for every invocation on that makefile, but we need a new suffix each time.
							if (bIsHotReload)
							{
								PatchActionHistoryForHotReloadAssembling(HotReloadTargetDesc.OnlyModules);
							}


							foreach (Tuple<string, string> EnvironmentVariable in UBTMakefile.EnvironmentVariables)
							{
								// @todo ubtmake: There may be some variables we do NOT want to clobber.
								Environment.SetEnvironmentVariable(EnvironmentVariable.Item1, EnvironmentVariable.Item2);
							}

							// Execute all the pre-build steps
							foreach(UEBuildTarget Target in Targets)
							{
								if(!Target.ExecuteCustomPreBuildSteps())
								{
									BuildResult = ECompilationResult.OtherCompilationError;
									break;
								}
							}

							// If any of the targets need UHT to be run, we'll go ahead and do that now
							if(BuildResult.Succeeded())
							{
								foreach (UEBuildTarget Target in Targets)
								{
									List<UHTModuleInfo> TargetUObjectModules;
									if (UBTMakefile.TargetNameToUObjectModules.TryGetValue(Target.GetTargetName(), out TargetUObjectModules))
									{
										if (TargetUObjectModules.Count > 0)
										{
											// Execute the header tool
											FileReference ModuleInfoFileName = FileReference.Combine(Target.ProjectIntermediateDirectory, Target.GetTargetName() + ".uhtmanifest");
											ECompilationResult UHTResult = ECompilationResult.OtherCompilationError;
											if (!ExternalExecution.ExecuteHeaderToolIfNecessary(ToolChain, Target, GlobalCompileEnvironment: null, UObjectModules: TargetUObjectModules, ModuleInfoFileName: ModuleInfoFileName, UHTResult: ref UHTResult))
											{
												Log.TraceInformation("UnrealHeaderTool failed for target '" + Target.GetTargetName() + "' (platform: " + Target.Platform.ToString() + ", module info: " + ModuleInfoFileName + ").");
												BuildResult = UHTResult;
												break;
											}
										}
									}
								}
							}
						}

						if (BuildResult.Succeeded())
						{
							// Make sure any old DLL files from in-engine recompiles aren't lying around.  Must be called after the action graph is finalized.
							ActionGraph.DeleteStaleHotReloadDLLs();

							if (!bIsHotReload && String.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile))
							{
								// clean up any stale modules
								foreach (UEBuildTarget Target in Targets)
								{
									if (Target.OnlyModules == null || Target.OnlyModules.Count == 0)
									{
										Target.CleanStaleModules();
									}
								}
							}

							ToolChain.PreBuildSync();

                            // Plan the actions to execute for the build.
                            Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap;
                            List<Action> ActionsToExecute = ActionGraph.GetActionsToExecute(UBTMakefile.PrerequisiteActions, Targets, out TargetToOutdatedPrerequisitesMap);

                            // Display some stats to the user.
                            Log.TraceVerbose(
                                    "{0} actions, {1} outdated and requested actions",
                                    ActionGraph.AllActions.Count,
                                    ActionsToExecute.Count
                                    );

							// Cache indirect includes for all outdated C++ files.  We kick this off as a background thread so that it can
							// perform the scan while we're compiling.  It usually only takes up to a few seconds, but we don't want to hurt
							// our best case UBT iteration times for this task which can easily be performed asynchronously
							if (BuildConfiguration.bUseUBTMakefiles && TargetToOutdatedPrerequisitesMap.Count > 0)
							{
								CPPIncludesThread = CreateThreadForCachingCPPIncludes(TargetToOutdatedPrerequisitesMap);
								CPPIncludesThread.Start();
							}

							// If we're not touching any shared files (ie. anything under Engine), allow the build ids to be recycled between applications.
							bool bModifiedEngineFiles = ActionsToExecute.Any(x => x.ProducedItems.Any(y => y.Reference.IsUnderDirectory(EngineDirectory)));
							foreach (UEBuildTarget Target in Targets)
							{
								Target.RecycleVersionManifests(bModifiedEngineFiles);
							}

							// Execute the actions.
							string TargetInfoForTelemetry = String.Join("|", Targets.Select(x => String.Format("{0} {1} {2}{3}", x.TargetName, x.Platform, x.Configuration, BuildConfiguration.bUseUnityBuild? "" : " NonUnity")));
							bSuccess = ActionGraph.ExecuteActions(ActionsToExecute, out ExecutorName, TargetInfoForTelemetry, bIsHotReload);

							// if the build succeeded, write the receipts and do any needed syncing
							if (bSuccess)
							{
								foreach (UEBuildTarget Target in Targets)
								{
									Target.WriteReceipts();
									ToolChain.PostBuildSync(Target);
								}
								if (ActionsToExecute.Count == 0 && UEBuildConfiguration.bSkipLinkingWhenNothingToCompile)
								{
									BuildResult = ECompilationResult.UpToDate;
								}
							}
							else
							{
								BuildResult = ECompilationResult.OtherCompilationError;
							}
						}

						// Execute all the post-build steps
						if(BuildResult.Succeeded())
						{
							foreach(UEBuildTarget Target in Targets)
							{
								if(!Target.ExecuteCustomPostBuildSteps())
								{
									BuildResult = ECompilationResult.OtherCompilationError;
									break;
								}
							}
						}
					}
				}
			}
			catch (BuildException Exception)
			{
				// Output the message only, without the call stack
				Log.TraceInformation(Exception.Message);
				BuildResult = ECompilationResult.OtherCompilationError;
			}
			catch (Exception Exception)
			{
				Log.TraceInformation("ERROR: {0}", Exception);
				BuildResult = ECompilationResult.OtherCompilationError;
			}

			// Wait until our CPPIncludes dependency scanner thread has finished
			if (CPPIncludesThread != null)
			{
				CPPIncludesThread.Join();
			}

			// Save the include dependency cache.
			{
				// NOTE: It's very important that we save the include cache, even if a build exception was thrown (compile error, etc), because we need to make sure that
				//    any C++ include dependencies that we computed for out of date source files are saved.  Remember, the build may fail *after* some build products
				//    are successfully built.  If we didn't save our dependency cache after build failures, source files for those build products that were successsfully
				//    built before the failure would not be considered out of date on the next run, so this is our only chance to cache C++ includes for those files!

				foreach (DependencyCache DependencyCache in CPPEnvironment.IncludeDependencyCache.Values)
				{
					DependencyCache.Save();
				}
				CPPEnvironment.IncludeDependencyCache.Clear();

				foreach (FlatCPPIncludeDependencyCache FlatCPPIncludeDependencyCache in CPPEnvironment.FlatCPPIncludeDependencyCache.Values)
				{
					FlatCPPIncludeDependencyCache.Save();
				}
				CPPEnvironment.FlatCPPIncludeDependencyCache.Clear();
			}

			// Figure out how long we took to execute.
			double BuildDuration = (DateTime.UtcNow - StartTime).TotalSeconds;
			if (ExecutorName == "Local" || ExecutorName == "Distcc" || ExecutorName == "SNDBS")
			{
				Log.WriteLineIf(BuildConfiguration.bLogDetailedActionStats || BuildConfiguration.bPrintDebugInfo,
					LogEventType.Console,
					"Cumulative action seconds ({0} processors): {1:0.00} building projects, {2:0.00} compiling, {3:0.00} creating app bundles, {4:0.00} generating debug info, {5:0.00} linking, {6:0.00} other",
					System.Environment.ProcessorCount,
					TotalBuildProjectTime,
					TotalCompileTime,
					TotalCreateAppBundleTime,
					TotalGenerateDebugInfoTime,
					TotalLinkTime,
					TotalOtherActionsTime
				);

				Log.TraceInformation("Total build time: {0:0.00} seconds", BuildDuration);

				// reset statistics
				TotalBuildProjectTime = 0;
				TotalCompileTime = 0;
				TotalCreateAppBundleTime = 0;
				TotalGenerateDebugInfoTime = 0;
				TotalLinkTime = 0;
				TotalOtherActionsTime = 0;
			}
			else
			{
				if (ExecutorName == "XGE")
				{
					Log.TraceInformation("XGE execution time: {0:0.00} seconds", BuildDuration);
				}
			}

			return BuildResult;
		}

		/// <summary>
		/// Invalidates makefiles for given target.
		/// </summary>
		/// <param name="Target">Target</param>
		private static void InvalidateMakefiles(TargetDescriptor Target)
		{
			string[] MakefileNames = new string[] { "HotReloadMakefile.ubt", "Makefile.ubt" };
			DirectoryReference BaseDir = GetUBTMakefileDirectoryPathForSingleTarget(Target);

			foreach (string MakefileName in MakefileNames)
			{
				FileReference MakefileRef = FileReference.Combine(BaseDir, MakefileName);
				if (MakefileRef.Exists())
				{
					MakefileRef.Delete();
				}
			}
		}
		/// <summary>
		/// Checks if the editor is currently running and this is a hot-reload
		/// </summary>
		/// <param name="ResetPlatform"></param>
		/// <param name="ResetConfiguration"></param>
		/// <param name="TargetDesc"></param>
		/// <returns></returns>
		private static bool ShouldDoHotReloadFromIDE(TargetDescriptor TargetDesc)
		{
			if (UnrealBuildTool.CommandLineContains("-NoHotReloadFromIDE"))
			{
				// Hot reload disabled through command line, possibly running from UAT
				return false;
			}

			bool bIsRunning = false;

			// @todo ubtmake: Kind of cheating here to figure out if an editor target.  At this point we don't have access to the actual target description, and
			// this code must be able to execute before we create or load module rules DLLs so that hot reload can work with bUseUBTMakefiles
			bool bIsEditorTarget = TargetDesc.TargetName.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase) || TargetDesc.bIsEditorRecompile;

			if (!ProjectFileGenerator.bGenerateProjectFiles && !UEBuildConfiguration.bGenerateManifest && bIsEditorTarget)
			{
				List<FileReference> EditorProcessFilenames = UEBuildTarget.MakeExecutablePaths(UnrealBuildTool.EngineDirectory, "UE4Editor", TargetDesc.Platform, TargetDesc.Configuration, "", UnrealTargetConfiguration.Development, false, null, null);
				if (EditorProcessFilenames.Count != 1)
				{
					throw new BuildException("ShouldDoHotReload cannot handle multiple binaries returning from UEBuildTarget.MakeExecutablePaths");
				}

				string EditorProcessFilename = EditorProcessFilenames[0].CanonicalName;
				if (TargetDesc.Platform == UnrealTargetPlatform.Mac && !EditorProcessFilename.Contains(".app/contents/macos/"))
				{
					EditorProcessFilename += ".app/contents/macos/" + Path.GetFileNameWithoutExtension(EditorProcessFilename);
				}
					
				BuildHostPlatform.ProcessInfo[] Processes = BuildHostPlatform.Current.GetProcesses();
				string EditorRunsDir = Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Intermediate", "EditorRuns");

				if (!Directory.Exists(EditorRunsDir))
				{
					return false;
				}

				FileInfo[] EditorRunsFiles = new DirectoryInfo(EditorRunsDir).GetFiles();

				foreach(FileInfo File in EditorRunsFiles)
				{
					int PID;
					BuildHostPlatform.ProcessInfo Proc = null;
					if (!Int32.TryParse(File.Name, out PID) || (Proc = Processes.FirstOrDefault(P => P.PID == PID)) == default(BuildHostPlatform.ProcessInfo))
					{
						// Delete stale files (it may happen if editor crashes).
						File.Delete();
						continue;
					}

					// Don't break here to allow clean-up of other stale files.
					if (!bIsRunning)
					{
						// Otherwise check if the path matches.
						bIsRunning = new FileReference (Proc.Filename).CanonicalName == EditorProcessFilename;
					}
				}
			}
			return bIsRunning;
		}

		/// <summary>
		/// Parses the passed in command line for build configuration overrides.
		/// </summary>
		/// <param name="Arguments">List of arguments to parse</param>
		/// <returns>List of build target settings</returns>
		private static List<string[]> ParseCommandLineFlags(string[] Arguments)
		{
			List<string[]> TargetSettings = new List<string[]>();
			int ArgumentIndex = 0;

			if (Utils.ParseCommandLineFlag(Arguments, "-targets", out ArgumentIndex))
			{
				if (ArgumentIndex + 1 >= Arguments.Length)
				{
					throw new BuildException("Expected filename after -targets argument, but found nothing.");
				}
				// Parse lines from the referenced file into target settings.
				string[] Lines = File.ReadAllLines(Arguments[ArgumentIndex + 1]);
				foreach (string Line in Lines)
				{
					if (Line != "" && Line[0] != ';')
					{
						TargetSettings.Add(Line.Split(' '));
					}
				}
			}
			// Simply use full command line arguments as target setting if not otherwise overridden.
			else
			{
				TargetSettings.Add(Arguments);
			}

			return TargetSettings;
		}


		/// <summary>
		/// Returns a Thread object that can be kicked off to update C++ include dependency cache
		/// </summary>
		/// <param name="TargetToOutdatedPrerequisitesMap">Maps each target to a list of outdated C++ files that need indirect dependencies cached</param>
		/// <returns>The thread object</returns>
		private static Thread CreateThreadForCachingCPPIncludes(Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap)
		{
			return new Thread(new ThreadStart(() =>
				{
					// @todo ubtmake: This thread will access data structures that are also used on the main UBT thread, but during this time UBT
					// is only invoking the build executor, so should not be touching this stuff.  However, we need to at some guards to make sure.

					foreach (KeyValuePair<UEBuildTarget, List<FileItem>> TargetAndOutdatedPrerequisites in TargetToOutdatedPrerequisitesMap)
					{
						UEBuildTarget Target = TargetAndOutdatedPrerequisites.Key;
						List<FileItem> OutdatedPrerequisites = TargetAndOutdatedPrerequisites.Value;

						foreach (FileItem PrerequisiteItem in OutdatedPrerequisites)
						{
							// Invoke our deep include scanner to figure out whether any of the files included by this source file have
							// changed since the build product was built
							UEBuildPlatform FileBuildPlatform = UEBuildPlatform.GetBuildPlatform(Target.Platform);
							CPPEnvironment.FindAndCacheAllIncludedFiles(Target, PrerequisiteItem, FileBuildPlatform, PrerequisiteItem.CachedCPPIncludeInfo, bOnlyCachedDependencies: false);
						}
					}
				}));
		}



		/// <summary>
		/// A special Makefile that UBT is able to create in "-gather" mode, then load in "-assemble" mode to accelerate iterative compiling and linking
		/// </summary>
		[Serializable]
		class UBTMakefile : ISerializable
		{
			public const int CurrentVersion = 4;

			public int Version;

			/// <summary>
			/// Every action in the action graph
			/// </summary>
			public List<Action> AllActions;

			/// <summary>
			/// List of the actions that need to be run in order to build the targets' final output items
			/// </summary>
			public Action[] PrerequisiteActions;

			/// <summary>
			/// Environment variables that we'll need in order to invoke the platform's compiler and linker
			/// </summary>
			// @todo ubtmake: Really we want to allow a different set of environment variables for every Action.  This would allow for targets on multiple platforms to be built in a single assembling phase.  We'd only have unique variables for each platform that has actions, so we'd want to make sure we only store the minimum set.
			public readonly List<Tuple<string, string>> EnvironmentVariables = new List<Tuple<string, string>>();

			/// <summary>
			/// Maps each target to a list of UObject module info structures
			/// </summary>
			public Dictionary<string, List<UHTModuleInfo>> TargetNameToUObjectModules;

			/// <summary>
			/// List of targets being built
			/// </summary>
			public List<UEBuildTarget> Targets;


			/// <summary>
			/// Current working set of source files, for when bUseAdaptiveUnityBuild is enabled
			/// </summary>
			public HashSet<FileItem> SourceFileWorkingSet = new HashSet<FileItem>();

			public UBTMakefile()
			{
				Version = CurrentVersion;
			}

			public UBTMakefile(SerializationInfo Info, StreamingContext Context)
			{
				Version = Info.GetInt32("ve");
				if (Version != CurrentVersion)
				{
					throw new Exception(string.Format("Makefile version does not match - found {0}, expected: {1}", Version, CurrentVersion));
				}

				AllActions = (List<Action>)Info.GetValue("ac", typeof(List<Action>));
				PrerequisiteActions = (Action[])Info.GetValue("pa", typeof(Action[]));
				EnvironmentVariables = ((string[])Info.GetValue("e1", typeof(string[]))).Zip((string[])Info.GetValue("e2", typeof(string[])), (i1, i2) => new Tuple<string, string>(i1, i2)).ToList();
				TargetNameToUObjectModules = (Dictionary<string, List<UHTModuleInfo>>)Info.GetValue("nu", typeof(Dictionary<string, List<UHTModuleInfo>>));
				Targets = (List<UEBuildTarget>)Info.GetValue("ta", typeof(List<UEBuildTarget>));
				SourceFileWorkingSet = (HashSet<FileItem>)Info.GetValue("ws", typeof(HashSet<FileItem>));
			}

			public void GetObjectData(SerializationInfo Info, StreamingContext Context)
			{
				Info.AddValue("ve", Version);
				Info.AddValue("ac", AllActions);
				Info.AddValue("pa", PrerequisiteActions);
				Info.AddValue("e1", EnvironmentVariables.Select(x => x.Item1).ToArray());
				Info.AddValue("e2", EnvironmentVariables.Select(x => x.Item2).ToArray());
				Info.AddValue("nu", TargetNameToUObjectModules);
				Info.AddValue("ta", Targets);
				Info.AddValue("ws", SourceFileWorkingSet);
			}


			/// <returns> True if this makefile's contents look valid.  Called after loading the file to make sure it is legit.</returns>
			public bool IsValidMakefile()
			{
				return
					AllActions != null && AllActions.Count > 0 &&
					PrerequisiteActions != null && PrerequisiteActions.Length > 0 &&
					EnvironmentVariables != null &&
					TargetNameToUObjectModules != null && TargetNameToUObjectModules.Count > 0 &&
					Targets != null && Targets.Count > 0 &&
					SourceFileWorkingSet != null;
			}
		}


		/// <summary>
		/// Saves a UBTMakefile to disk
		/// </summary>
		/// <param name="TargetDescs">List of targets.  Order is not important</param>
		static void SaveUBTMakefile(List<TargetDescriptor> TargetDescs, UBTMakefile UBTMakefile)
		{
			if (!UBTMakefile.IsValidMakefile())
			{
				throw new BuildException("Can't save a makefile that has invalid contents.  See UBTMakefile.IsValidMakefile()");
			}

			DateTime TimerStartTime = DateTime.UtcNow;

			FileItem UBTMakefileItem = FileItem.GetItemByFileReference(GetUBTMakefilePath(TargetDescs));

			// @todo ubtmake: Optimization: The UBTMakefile saved for game projects is upwards of 9 MB.  We should try to shrink its content if possible
			// @todo ubtmake: Optimization: C# Serialization may be too slow for these big Makefiles.  Loading these files often shows up as the slower part of the assembling phase.

			// Serialize the cache to disk.
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(UBTMakefileItem.AbsolutePath));
				using (FileStream Stream = new FileStream(UBTMakefileItem.AbsolutePath, FileMode.Create, FileAccess.Write))
				{
					BinaryFormatter Formatter = new BinaryFormatter();
					Formatter.Serialize(Stream, UBTMakefile);
				}
			}
			catch (Exception Ex)
			{
				Console.Error.WriteLine("Failed to write makefile: {0}", Ex.Message);
			}

			if (BuildConfiguration.bPrintPerformanceInfo)
			{
				TimeSpan TimerDuration = DateTime.UtcNow - TimerStartTime;
				Log.TraceInformation("Saving makefile took " + TimerDuration.TotalSeconds + "s");
			}
		}


		/// <summary>
		/// Loads a UBTMakefile from disk
		/// </summary>
		/// <param name="MakefilePath">Path to the makefile to load</param>
		/// <param name="ReasonNotLoaded">If the function returns null, this string will contain the reason why</param>
		/// <returns>The loaded makefile, or null if it failed for some reason.  On failure, the 'ReasonNotLoaded' variable will contain information about why</returns>
		static UBTMakefile LoadUBTMakefile(FileReference MakefilePath, FileReference ProjectFile, out string ReasonNotLoaded)
		{
			// Check the directory timestamp on the project files directory.  If the user has generated project files more
			// recently than the UBTMakefile, then we need to consider the file to be out of date
			FileInfo UBTMakefileInfo = new FileInfo(MakefilePath.FullName);
			if (!UBTMakefileInfo.Exists)
			{
				// UBTMakefile doesn't even exist, so we won't bother loading it
				ReasonNotLoaded = "no existing makefile";
				return null;
			}

			// @todo ubtmake: This will only work if the directory timestamp actually changes with every single GPF.  Force delete existing files before creating new ones?  Eh... really we probably just want to delete + create a file in that folder
			//			-> UPDATE: Seems to work OK right now though on Windows platform, maybe due to GUID changes
			// @todo ubtmake: Some platforms may not save any files into this folder.  We should delete + generate a "touch" file to force the directory timestamp to be updated (or just check the timestamp file itself.  We could put it ANYWHERE, actually)

			// Installed Build doesn't need to check engine projects for outdatedness
			if (!UnrealBuildTool.IsEngineInstalled())
			{
				if (ProjectFileGenerator.IntermediateProjectFilesPath.Exists())
				{
					DateTime EngineProjectFilesLastUpdateTime = new FileInfo(ProjectFileGenerator.ProjectTimestampFile).LastWriteTime;
					if (UBTMakefileInfo.LastWriteTime.CompareTo(EngineProjectFilesLastUpdateTime) < 0)
					{
						// Engine project files are newer than UBTMakefile
						Log.TraceVerbose("Existing makefile is older than generated engine project files, ignoring it");
						ReasonNotLoaded = "project files are newer";
						return null;
					}
				}
			}

			// Check the game project directory too
			if (ProjectFile != null)
			{
				string ProjectFilename = ProjectFile.FullName;
				FileInfo ProjectFileInfo = new FileInfo(ProjectFilename);
				if (!ProjectFileInfo.Exists || UBTMakefileInfo.LastWriteTime.CompareTo(ProjectFileInfo.LastWriteTime) < 0)
				{
					// .uproject file is newer than UBTMakefile
					Log.TraceVerbose("Makefile is older than .uproject file, ignoring it");
					ReasonNotLoaded = ".uproject file is newer";
					return null;
				}

				DirectoryReference MasterProjectRelativePath = ProjectFile.Directory;
				string GameIntermediateProjectFilesPath = Path.Combine(MasterProjectRelativePath.FullName, "Intermediate", "ProjectFiles");
				if (Directory.Exists(GameIntermediateProjectFilesPath))
				{
					DateTime GameProjectFilesLastUpdateTime = new DirectoryInfo(GameIntermediateProjectFilesPath).LastWriteTime;
					if (UBTMakefileInfo.LastWriteTime.CompareTo(GameProjectFilesLastUpdateTime) < 0)
					{
						// Game project files are newer than UBTMakefile
						Log.TraceVerbose("Makefile is older than generated game project files, ignoring it");
						ReasonNotLoaded = "game project files are newer";
						return null;
					}
				}
			}

			// Check to see if UnrealBuildTool.exe was compiled more recently than the UBTMakefile
			DateTime UnrealBuildToolTimestamp = new FileInfo(Assembly.GetExecutingAssembly().Location).LastWriteTime;
			if (UBTMakefileInfo.LastWriteTime.CompareTo(UnrealBuildToolTimestamp) < 0)
			{
				// UnrealBuildTool.exe was compiled more recently than the UBTMakefile
				Log.TraceVerbose("Makefile is older than UnrealBuildTool.exe, ignoring it");
				ReasonNotLoaded = "UnrealBuildTool.exe is newer";
				return null;
			}

			// Check to see if any BuildConfiguration files have changed since the last build
			if (XmlConfigLoader.NewestXmlTimestamp > UBTMakefileInfo.LastWriteTime)
			{
				Log.TraceVerbose("Makefile is older than BuildConfiguration.xml, ignoring it" );
				ReasonNotLoaded = "BuildConfiguration.xml is newer";
				return null;
			}

			UBTMakefile LoadedUBTMakefile = null;

			try
			{
				DateTime LoadUBTMakefileStartTime = DateTime.UtcNow;

				using (FileStream Stream = new FileStream(UBTMakefileInfo.FullName, FileMode.Open, FileAccess.Read))
				{
					BinaryFormatter Formatter = new BinaryFormatter();
					LoadedUBTMakefile = Formatter.Deserialize(Stream) as UBTMakefile;
				}

				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					double LoadUBTMakefileTime = (DateTime.UtcNow - LoadUBTMakefileStartTime).TotalSeconds;
					Log.TraceInformation("LoadUBTMakefile took " + LoadUBTMakefileTime + "s");
				}
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Failed to read makefile: {0}", Ex.Message);
				ReasonNotLoaded = "couldn't read existing makefile";
				return null;
			}

			if (!LoadedUBTMakefile.IsValidMakefile())
			{
				Log.TraceWarning("Loaded makefile appears to have invalid contents, ignoring it ({0})", UBTMakefileInfo.FullName);
				ReasonNotLoaded = "existing makefile appears to be invalid";
				return null;
			}

			// Check if any of the target's Build.cs files are newer than the makefile
			foreach (UEBuildTarget Target in LoadedUBTMakefile.Targets)
			{
				string TargetCsFilename = Target.TargetCsFilename.FullName;
				if (TargetCsFilename != null)
				{
					FileInfo TargetCsFile = new FileInfo(TargetCsFilename);
					bool bTargetCsFileExists = TargetCsFile.Exists;
					if (!bTargetCsFileExists || TargetCsFile.LastWriteTime > UBTMakefileInfo.LastWriteTime)
					{
						Log.TraceVerbose("{0} has been {1} since makefile was built, ignoring it ({2})", TargetCsFilename, bTargetCsFileExists ? "changed" : "deleted", UBTMakefileInfo.FullName);
						ReasonNotLoaded = string.Format("changes to target files");
						return null;
					}
				}

				IEnumerable<string> BuildCsFilenames = Target.GetAllModuleBuildCsFilenames();
				foreach (string BuildCsFilename in BuildCsFilenames)
				{
					if (BuildCsFilename != null)
					{
						FileInfo BuildCsFile = new FileInfo(BuildCsFilename);
						bool bBuildCsFileExists = BuildCsFile.Exists;
						if (!bBuildCsFileExists || BuildCsFile.LastWriteTime > UBTMakefileInfo.LastWriteTime)
						{
							Log.TraceVerbose("{0} has been {1} since makefile was built, ignoring it ({2})", BuildCsFilename, bBuildCsFileExists ? "changed" : "deleted", UBTMakefileInfo.FullName);
							ReasonNotLoaded = string.Format("changes to module files");
							return null;
						}
					}
				}
			}

			// We do a check to see if any modules' headers have changed which have
			// acquired or lost UHT types.  If so, which should be rare,
			// we'll just invalidate the entire makefile and force it to be rebuilt.
			foreach (UEBuildTarget Target in LoadedUBTMakefile.Targets)
			{
				// Get all H files in processed modules newer than the makefile itself
				HashSet<string> HFilesNewerThanMakefile =
					new HashSet<string>(
						Target.FlatModuleCsData
						.SelectMany(x => x.Value.ModuleSourceFolder != null ? Directory.EnumerateFiles(x.Value.ModuleSourceFolder.FullName, "*.h", SearchOption.AllDirectories) : Enumerable.Empty<string>())
						.Where(y => Directory.GetLastWriteTimeUtc(y) > UBTMakefileInfo.LastWriteTimeUtc)
						.OrderBy(z => z).Distinct()
					);

				// Get all H files in all modules processed in the last makefile build
				HashSet<string> AllUHTHeaders = new HashSet<string>(Target.FlatModuleCsData.Select(x => x.Value).SelectMany(x => x.UHTHeaderNames));

				// Check whether any headers have been deleted. If they have, we need to regenerate the makefile since the module might now be empty. If we don't,
				// and the file has been moved to a different module, we may include stale generated headers.
				foreach (string FileName in AllUHTHeaders)
				{
					if (!File.Exists(FileName))
					{
						Log.TraceVerbose("File processed by UHT was deleted ({0}); invalidating makefile", FileName);
						ReasonNotLoaded = string.Format("UHT file was deleted");
						return null;
					}
				}

				// Makefile is invalid if:
				// * There are any newer files which contain no UHT data, but were previously in the makefile
				// * There are any newer files contain data which needs processing by UHT, but weren't not previously in the makefile
				foreach (string Filename in HFilesNewerThanMakefile)
				{
					bool bContainsUHTData = CPPEnvironment.DoesFileContainUObjects(Filename);
					bool bWasProcessed = AllUHTHeaders.Contains(Filename);
					if (bContainsUHTData != bWasProcessed)
					{
						Log.TraceVerbose("{0} {1} contain UHT types and now {2} , ignoring it ({3})", Filename, bWasProcessed ? "used to" : "didn't", bWasProcessed ? "doesn't" : "does", UBTMakefileInfo.FullName);
						ReasonNotLoaded = string.Format("new files with reflected types");
						return null;
					}
				}
			}

			// If adaptive unity build is enabled, do a check to see if there are any source files that became part of the
			// working set since the Makefile was created (or, source files were removed from the working set.)  If anything
			// changed, then we'll force a new Makefile to be created so that we have fresh unity build blobs.  We always
			// want to make sure that source files in the working set are excluded from those unity blobs (for fastest possible
			// iteration times.)
			if (BuildConfiguration.bUseAdaptiveUnityBuild)
			{
				// Get all .cpp files in processed modules newer than the makefile itself
				foreach (UEBuildTarget Target in LoadedUBTMakefile.Targets)
				{
					foreach (FlatModuleCsDataType FlatModule in Target.FlatModuleCsData.Values)
					{
						if (FlatModule != null && FlatModule.ModuleSourceFolder != null)
						{
							// Has the directory been touched?  Don't bother checking folders that haven't been written to since the
							// Makefile was created. 
							//if( Directory.GetLastWriteTimeUtc( FlatModule.ModuleSourceFolder ) > UBTMakefileInfo.LastWriteTimeUtc )
							{
								foreach (FileReference SourceFilePath in FlatModule.ModuleSourceFolder.EnumerateFileReferences("*.cpp", SearchOption.AllDirectories))
								{
									// Was the file written to since the Makefile was created?  We're not able to do a fully exhaustive
									// check for working sets when in assembler mode, so we might get it wrong occasionally.  This is
									// why we only want to check newly-modified or created files.  Otherwise, we could potentially
									// invalidate the Makefile every single run.
									if (File.GetLastWriteTimeUtc(SourceFilePath.FullName) > UBTMakefileInfo.LastWriteTimeUtc)
									{
										bool bShouldBePartOfWorkingSet = UnrealBuildTool.ShouldSourceFileBePartOfWorkingSet(SourceFilePath.FullName);
										bool bIsAlreadyPartOfWorkingSet = LoadedUBTMakefile.SourceFileWorkingSet.Contains(FileItem.GetItemByFileReference(SourceFilePath));
										if (bShouldBePartOfWorkingSet != bIsAlreadyPartOfWorkingSet)
										{
											Log.TraceVerbose("{0} {1} part of source working set and now {2}; invalidating makefile ({3})", SourceFilePath, bIsAlreadyPartOfWorkingSet ? "was" : "was not", bShouldBePartOfWorkingSet ? "is" : "is not", UBTMakefileInfo.FullName);
											ReasonNotLoaded = string.Format("working set of source files changed");
											return null;
										}
									}
								}
							}
						}
					}
				}
			}

			ReasonNotLoaded = null;
			return LoadedUBTMakefile;
		}


		/// <summary>
		/// Gets the file path for a UBTMakefile
		/// </summary>
		/// <param name="Targets">List of targets.  Order is not important</param>
		/// <returns>UBTMakefile path</returns>
		public static FileReference GetUBTMakefilePath(List<TargetDescriptor> TargetDescs)
		{
			FileReference UBTMakefilePath;

			if (TargetDescs.Count == 1)
			{
				TargetDescriptor TargetDesc = TargetDescs[0];

				bool bIsHotReload = (UEBuildConfiguration.bHotReloadFromIDE || (TargetDesc.OnlyModules != null && TargetDesc.OnlyModules.Count > 0));
				string UBTMakefileName = bIsHotReload ? "HotReloadMakefile.ubt" : "Makefile.ubt";

				UBTMakefilePath = FileReference.Combine(GetUBTMakefileDirectoryPathForSingleTarget(TargetDesc), UBTMakefileName);
			}
			else
			{
				// For Makefiles that contain multiple targets, we'll make up a file name that contains all of the targets, their
				// configurations and platforms, and save it into the base intermediate folder
				string TargetCollectionName = MakeTargetCollectionName(TargetDescs);

				TargetDescriptor DescriptorWithProject = TargetDescs.FirstOrDefault(x => x.ProjectFile != null);

				DirectoryReference ProjectIntermediatePath;
				if (DescriptorWithProject != null)
				{
					ProjectIntermediatePath = DirectoryReference.Combine(DescriptorWithProject.ProjectFile.Directory, BuildConfiguration.BaseIntermediateFolder);
				}
				else
				{
					ProjectIntermediatePath = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, BuildConfiguration.BaseIntermediateFolder);
				}

				// @todo ubtmake: The TargetCollectionName string could be really long if there is more than one target!  Hash it?
				UBTMakefilePath = FileReference.Combine(ProjectIntermediatePath, TargetCollectionName + ".ubt");
			}

			return UBTMakefilePath;
		}

		/// <summary>
		/// Gets the file path for a UBTMakefile for single target.
		/// </summary>
		/// <param name="Target">The target.</param>
		/// <returns>UBTMakefile path</returns>
		private static DirectoryReference GetUBTMakefileDirectoryPathForSingleTarget(TargetDescriptor Target)
		{
			// If there's only one target, just save the UBTMakefile in the target's build intermediate directory
			// under a folder for that target (and platform/config combo.)
			DirectoryReference PlatformIntermediatePath;
			if (Target.ProjectFile != null)
			{
				PlatformIntermediatePath = DirectoryReference.Combine(Target.ProjectFile.Directory, BuildConfiguration.PlatformIntermediateFolder);
			}
			else
			{
				PlatformIntermediatePath = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, BuildConfiguration.PlatformIntermediateFolder);
			}

			// @todo ubtmake: If this is a compile triggered from the editor it will have passed along the game's target name, not the editor target name.
			// At this point in Unreal Build Tool, we can't possibly know what the actual editor target name is, but we can take a guess.
			// Even if we get it wrong, this won't have any side effects aside from not being able to share the exact same cached UBT Makefile
			// between hot reloads invoked from the editor and hot reloads invoked from within the IDE.
			string TargetName = Target.TargetName;
			if (!TargetName.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase) && Target.bIsEditorRecompile)
			{
				TargetName += "Editor";
			}

			return DirectoryReference.Combine(PlatformIntermediatePath, TargetName, Target.Configuration.ToString());
		}

		/// <summary>
		/// Makes up a name for a set of targets that we can use for file or directory names
		/// </summary>
		/// <param name="TargetDescs">List of targets.  Order is not important</param>
		/// <returns>The name to use</returns>
		private static string MakeTargetCollectionName(List<TargetDescriptor> TargetDescs)
		{
			if (TargetDescs.Count == 0)
			{
				throw new BuildException("Expecting at least one Target to be passed to MakeTargetCollectionName");
			}

			List<TargetDescriptor> SortedTargets = new List<TargetDescriptor>();
			SortedTargets.AddRange(TargetDescs);
			SortedTargets.Sort((x, y) => { return x.TargetName.CompareTo(y.TargetName); });

			// Figure out what to call our action graph based on everything we're building
			StringBuilder TargetCollectionName = new StringBuilder();
			foreach (TargetDescriptor Target in SortedTargets)
			{
				if (TargetCollectionName.Length > 0)
				{
					TargetCollectionName.Append("_");
				}

				// @todo ubtmake: If this is a compile triggered from the editor it will have passed along the game's target name, not the editor target name.
				// At this point in Unreal Build Tool, we can't possibly know what the actual editor target name is, but we can take a guess.
				// Even if we get it wrong, this won't have any side effects aside from not being able to share the exact same cached UBT Makefile
				// between hot reloads invoked from the editor and hot reloads invoked from within the IDE.
				string TargetName = Target.TargetName;
				if (!TargetName.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase) && Target.bIsEditorRecompile)
				{
					TargetName += "Editor";
				}

				// @todo ubtmake: Should we also have the platform Architecture in this string?
				TargetCollectionName.Append(TargetName + "-" + Target.Platform.ToString() + "-" + Target.Configuration.ToString());
			}

			return TargetCollectionName.ToString();
		}

		/// <summary>
		/// Sets up UBT when running from UAT
		/// </summary>
		/// <param name="UProjectDir"></param>
		public static void SetupUBTFromUAT()
		{
			// when running UAT, the working directory is the root UE4 dir
			BuildConfiguration.RelativeEnginePath = "Engine";
		}


		/// <summary>
		/// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
		/// the same for every invocation on that makefile, but we need a new suffix each time.
		/// </summary>
		protected static void PatchActionHistoryForHotReloadAssembling(List<OnlyModule> OnlyModules)
		{
			// Gather all of the response files for link actions.  We're going to need to patch 'em up after we figure out new
			// names for all of the output files and import libraries
			List<string> ResponseFilePaths = new List<string>();

			// Keep a map of the original file names and their new file names, so we can fix up response files after
			List<Tuple<string, string>> OriginalFileNameAndNewFileNameList_NoExtensions = new List<Tuple<string, string>>();

			// Finally, we'll keep track of any file items that we had to create counterparts for change file names, so we can fix those up too
			Dictionary<FileItem, FileItem> AffectedOriginalFileItemAndNewFileItemMap = new Dictionary<FileItem, FileItem>();

			foreach (Action Action in ActionGraph.AllActions)
			{
				if (Action.ActionType == ActionType.Link)
				{
					// Assume that the first produced item (with no extension) is our output file name
					string OriginalFileNameWithoutExtension = Utils.GetFilenameWithoutAnyExtensions(Action.ProducedItems[0].AbsolutePath);

					// Remove the numbered suffix from the end of the file
					int IndexOfLastHyphen = OriginalFileNameWithoutExtension.LastIndexOf('-');
					string OriginalNumberSuffix = OriginalFileNameWithoutExtension.Substring(IndexOfLastHyphen);
					int NumberSuffix;
					bool bHasNumberSuffix = int.TryParse(OriginalNumberSuffix, out NumberSuffix);

					string OriginalFileNameWithoutNumberSuffix = null;
					string PlatformConfigSuffix = string.Empty;
					if (bHasNumberSuffix)
					{
						// Remove "-####" suffix in Development configuration
						OriginalFileNameWithoutNumberSuffix = OriginalFileNameWithoutExtension.Substring(0, OriginalFileNameWithoutExtension.Length - OriginalNumberSuffix.Length);
					}
					else
					{
						OriginalFileNameWithoutNumberSuffix = OriginalFileNameWithoutExtension;

						// Remove "-####-Platform-Configuration" suffix in Debug configuration
						int IndexOfSecondLastHyphen = OriginalFileNameWithoutExtension.LastIndexOf('-', IndexOfLastHyphen - 1, IndexOfLastHyphen);
						if (IndexOfSecondLastHyphen >= 0)
						{
							int IndexOfThirdLastHyphen = OriginalFileNameWithoutExtension.LastIndexOf('-', IndexOfSecondLastHyphen - 1, IndexOfSecondLastHyphen);
							if (IndexOfThirdLastHyphen >= 0)
							{
								if (int.TryParse(OriginalFileNameWithoutExtension.Substring(IndexOfThirdLastHyphen + 1, IndexOfSecondLastHyphen - IndexOfThirdLastHyphen - 1), out NumberSuffix))
								{
									OriginalFileNameWithoutNumberSuffix = OriginalFileNameWithoutExtension.Substring(0, IndexOfThirdLastHyphen);
									PlatformConfigSuffix = OriginalFileNameWithoutExtension.Substring(IndexOfSecondLastHyphen);
									bHasNumberSuffix = true;
								}
							}
						}
					}

					// Figure out which suffix to use
					string UniqueSuffix = null;
					if (bHasNumberSuffix)
					{
						int FirstHyphenIndex = OriginalFileNameWithoutNumberSuffix.IndexOf('-');
						if (FirstHyphenIndex == -1)
						{
							throw new BuildException("Expected produced item file name '{0}' to start with a prefix (such as 'UE4Editor-') when hot reloading", OriginalFileNameWithoutExtension);
						}
						string OutputFileNamePrefix = OriginalFileNameWithoutNumberSuffix.Substring(0, FirstHyphenIndex + 1);

						// Get the module name from the file name
						string ModuleName = OriginalFileNameWithoutNumberSuffix.Substring(OutputFileNamePrefix.Length);

						// Add a new random suffix
						if (OnlyModules != null)
						{
							foreach (OnlyModule OnlyModule in OnlyModules)
							{
								if (OnlyModule.OnlyModuleName.Equals(ModuleName, StringComparison.InvariantCultureIgnoreCase))
								{
									// OK, we found the module!
									UniqueSuffix = "-" + OnlyModule.OnlyModuleSuffix;
									break;
								}
							}
						}
						if (UniqueSuffix == null)
						{
							UniqueSuffix = "-" + (new Random((int)(DateTime.Now.Ticks % Int32.MaxValue)).Next(10000)).ToString();
						}
					}
					else
					{
						UniqueSuffix = string.Empty;
					}
					string NewFileNameWithoutExtension = OriginalFileNameWithoutNumberSuffix + UniqueSuffix + PlatformConfigSuffix;

					// Find the response file in the command line.  We'll need to make a copy of it with our new file name.
					if (bHasNumberSuffix)
					{
						string ResponseFileExtension = ".response";
						int ResponseExtensionIndex = Action.CommandArguments.IndexOf(ResponseFileExtension, StringComparison.InvariantCultureIgnoreCase);
						if (ResponseExtensionIndex != -1)
						{
							int ResponseFilePathIndex = Action.CommandArguments.LastIndexOf("@\"", ResponseExtensionIndex);
							if (ResponseFilePathIndex == -1)
							{
								throw new BuildException("Couldn't find response file path in action's command arguments when hot reloading");
							}

							string OriginalResponseFilePathWithoutExtension = Action.CommandArguments.Substring(ResponseFilePathIndex + 2, (ResponseExtensionIndex - ResponseFilePathIndex) - 2);
							string OriginalResponseFilePath = OriginalResponseFilePathWithoutExtension + ResponseFileExtension;

							string NewResponseFilePath = OriginalResponseFilePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

							// Copy the old response file to the new path
							File.Copy(OriginalResponseFilePath, NewResponseFilePath, overwrite: true);

							// Keep track of the new response file name.  We'll have to do some edits afterwards.
							ResponseFilePaths.Add(NewResponseFilePath);
						}
					}


					// Go ahead and replace all occurrences of our file name in the command-line (ignoring extensions)
					Action.CommandArguments = Action.CommandArguments.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);


					// Update this action's list of produced items too
					{
						for (int ItemIndex = 0; ItemIndex < Action.ProducedItems.Count; ++ItemIndex)
						{
							FileItem OriginalProducedItem = Action.ProducedItems[ItemIndex];

							string OriginalProducedItemFilePath = OriginalProducedItem.AbsolutePath;
							string NewProducedItemFilePath = OriginalProducedItemFilePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

							if (OriginalProducedItemFilePath != NewProducedItemFilePath)
							{
								// OK, the produced item's file name changed so we'll update it to point to our new file
								FileItem NewProducedItem = FileItem.GetItemByPath(NewProducedItemFilePath);
								Action.ProducedItems[ItemIndex] = NewProducedItem;

								// Copy the other important settings from the original file item
								NewProducedItem.bNeedsHotReloadNumbersDLLCleanUp = OriginalProducedItem.bNeedsHotReloadNumbersDLLCleanUp;
								NewProducedItem.ProducingAction = OriginalProducedItem.ProducingAction;
								NewProducedItem.bIsRemoteFile = OriginalProducedItem.bIsRemoteFile;

								// Keep track of it so we can fix up dependencies in a second pass afterwards
								AffectedOriginalFileItemAndNewFileItemMap.Add(OriginalProducedItem, NewProducedItem);
							}
						}
					}

					// The status description of the item has the file name, so we'll update it too
					Action.StatusDescription = Action.StatusDescription.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);


					// Keep track of the file names, so we can fix up response files afterwards.
					OriginalFileNameAndNewFileNameList_NoExtensions.Add(Tuple.Create(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension));
				}
			}


			// Do another pass and update any actions that depended on the original file names that we changed
			foreach (Action Action in ActionGraph.AllActions)
			{
				for (int ItemIndex = 0; ItemIndex < Action.PrerequisiteItems.Count; ++ItemIndex)
				{
					FileItem OriginalFileItem = Action.PrerequisiteItems[ItemIndex];

					FileItem NewFileItem;
					if (AffectedOriginalFileItemAndNewFileItemMap.TryGetValue(OriginalFileItem, out NewFileItem))
					{
						// OK, looks like we need to replace this file item because we've renamed the file
						Action.PrerequisiteItems[ItemIndex] = NewFileItem;
					}
				}
			}


			if (OriginalFileNameAndNewFileNameList_NoExtensions.Count > 0)
			{
				foreach (string ResponseFilePath in ResponseFilePaths)
				{
					// Load the file up
					StringBuilder FileContents = new StringBuilder(Utils.ReadAllText(ResponseFilePath));

					// Replace all of the old file names with new ones
					foreach (Tuple<string, string> FileNameTuple in OriginalFileNameAndNewFileNameList_NoExtensions)
					{
						string OriginalFileNameWithoutExtension = FileNameTuple.Item1;
						string NewFileNameWithoutExtension = FileNameTuple.Item2;

						FileContents.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);
					}

					// Overwrite the original file
					string FileContentsString = FileContents.ToString();
					File.WriteAllText(ResponseFilePath, FileContentsString, new System.Text.UTF8Encoding(false));
				}
			}
		}

		/// <summary>
		/// Given a path to a source file, determines whether it should be considered part of the programmer's working
		/// file set.  That is, a file that is currently being iterated on and is likely to be recompiled soon.  This
		/// is only used when bUseAdaptiveUnityBuild is enabled.
		/// </summary>
		/// <param name="SourceFileAbsolutePath">Path to the source file on disk</param>
		/// <returns>True if file is part of the working set</returns>
		public static bool ShouldSourceFileBePartOfWorkingSet(string SourceFileAbsolutePath)
		{
			// @todo: This logic needs work.  Currently its designed for users working with Perforce, or a similar
			// source control system that keeps source files read-only when not actively checked out for editing.
			bool bShouldBePartOfWorkingSourceFileSet =
				!System.IO.File.GetAttributes(SourceFileAbsolutePath).HasFlag(FileAttributes.ReadOnly);
			return bShouldBePartOfWorkingSourceFileSet;
		}
	}
}
