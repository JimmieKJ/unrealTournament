// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;

namespace UnrealBuildTool
{
    public partial class UnrealBuildTool
    {
        /** Time at which control fist passes to UBT. */
        static public DateTime StartTime;

        /** Time we waited for the mutex. Tracked for proper active execution time collection. */
        static public TimeSpan MutexWaitTime = TimeSpan.Zero;

        static public string BuildGuid = Guid.NewGuid().ToString();

        /** Total time spent building projects. */
        static public double TotalBuildProjectTime = 0;

        /** Total time spent compiling. */
        static public double TotalCompileTime = 0;

        /** Total time spent creating app bundles. */
        static public double TotalCreateAppBundleTime = 0;

        /** Total time spent generating debug information. */
        static public double TotalGenerateDebugInfoTime = 0;

        /** Total time spent linking. */
        static public double TotalLinkTime = 0;

        /** Total time spent on other actions. */
        static public double TotalOtherActionsTime = 0;

        /// How much time was spent scanning for include dependencies for outdated C++ files
        public static double TotalDeepIncludeScanTime = 0.0;

        /** The command line */
        static public List<string> CmdLine = new List<string>();

        /** The environment at boot time. */
        static public System.Collections.IDictionary InitialEnvironment;

        /** Should we skip all extra modules for platforms? */
        static bool bSkipNonHostPlatforms = false;

        static UnrealTargetPlatform OnlyPlatformSpecificFor = UnrealTargetPlatform.Unknown;

        /** Are we running for Rocket */
        static public bool bRunningRocket = false;

        /** The project file */
        static string UProjectFile = "";

        /** The Unreal project file path.  This should always be valid when compiling game projects.  Engine targets and program targets may not have a UProject set. */
        static bool bHasUProjectFile = false;

        /** The Unreal project directory.  This should always be valid when compiling game projects.  Engine targets and program targets may not have a UProject set. */
        static string UProjectPath = "";

        /** This is set to true during UBT startup, after it is safe to be accessing the IsGatheringBuild and IsAssemblingBuild properties.  Never access this directly. */
        private static bool bIsSafeToCheckIfGatheringOrAssemblingBuild = false;

		/** True if this run of UBT should only invalidate Makefile. */
		static public bool bIsInvalidatingMakefilesOnly = false;

        /** True if we should gather module dependencies for building in this run.  If this is false, then we'll expect to be loading information from disk about
            the target's modules before we'll be able to build anything.  One or both of IsGatheringBuild or IsAssemblingBuild must be true. */
        static public bool IsGatheringBuild
        {
            get
            {
                if( bIsSafeToCheckIfGatheringOrAssemblingBuild )
                { 
                    return bIsGatheringBuild_Unsafe;
                }
                else
                {
                    throw new BuildException( "Accessed UnrealBuildTool.IsGatheringBuild before it was fully initialized at startup (bIsSafeToCheckIfGatheringOrAssemblingBuild)" );
                }
            }
        }
        static private bool bIsGatheringBuild_Unsafe = true;

        /** True if we should go ahead and actually compile and link in this run.  If this is false, no actions will be executed and we'll stop after gathering
            and saving information about dependencies.  One or both of IsGatheringBuild or IsAssemblingBuild must be true. */
        static public bool IsAssemblingBuild
        {
            get
            {
                if( bIsSafeToCheckIfGatheringOrAssemblingBuild )
                { 
                    return bIsAssemblingBuild_Unsafe;
                }
                else
                {
                    throw new BuildException( "Accessed UnrealBuildTool.IsGatheringBuild before it was fully initialized at startup (bIsSafeToCheckIfGatheringOrAssemblingBuild)" );
                }
            }
        }
        static private bool bIsAssemblingBuild_Unsafe = true;

        /** Used when BuildConfiguration.bUseUBTMakefiles is enabled.  If true, it means that our cached includes may not longer be
            valid (or never were), and we need to recover by forcibly scanning included headers for all build prerequisites to make sure that our
            cached set of includes is actually correct, before determining which files are outdated. */
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
        /// Returns true if UnrealBuildTool is running with the "-rocket" option (Rocket mode)
        /// </summary>
        /// <returns>True if running with "-rocket"</returns>
        static public bool RunningRocket()
        {
            return bRunningRocket;
        }

		/// <summary>
		/// Returns true if UnrealBuildTool is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		static public bool IsEngineInstalled()
		{
			// For now this is only true when running Rocket
			return RunningRocket();
		}

        /// <summary>
        /// Returns true if a UProject file is available.  This should always be the case when working with game projects.  For engine or program targets, a UProject will not be available.
        /// </summary>
        /// <returns></returns>
        static public bool HasUProjectFile()
        {
            return bHasUProjectFile;
        }

        /// <summary>
        /// The Unreal project file path.  This should always be valid when compiling game projects.  Engine targets and program targets may not have a UProject set.
        /// </summary>
        /// <returns>The path to the file</returns>
        static public string GetUProjectFile()
        {
            return UProjectFile;
        }

        /// <summary>
        /// The Unreal project directory.  This should always be valid when compiling game projects.  Engine targets and program targets may not have a UProject set. */
        /// </summary>
        /// <returns>The directory path</returns>
        static public string GetUProjectPath()
        {
            return UProjectPath;
        }

        /// <summary>
        /// The Unreal project source directory.  This should always be valid when compiling game projects.  Engine targets and program targets may not have a UProject set. */
        /// </summary>
        /// <returns>Relative path to the project's source directory</returns>
        static public string GetUProjectSourcePath()
        {
            string ProjectPath = GetUProjectPath();
            if (ProjectPath != null)
            {
                ProjectPath = Path.Combine(ProjectPath, "Source");
            }
            return ProjectPath;
        }

        static public string GetUBTPath()
        {
            string UnrealBuildToolPath = Path.Combine(Utils.GetExecutingAssemblyDirectory(), "UnrealBuildTool.exe");
            return UnrealBuildToolPath;
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
                (InPlatform == UnrealTargetPlatform.WinRT) ||
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
            foreach (UnrealTargetPlatform Platform in Enum.GetValues(typeof(UnrealTargetPlatform)))
            {
                if (Predicate(Platform))
                {
                    var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
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
        /// Is this a valid configuration
        /// Used primarily for Rocket vs non-Rocket
        /// </summary>
        /// <param name="InConfiguration"></param>
        /// <returns>true if valid, false if not</returns>
        static public bool IsValidConfiguration(UnrealTargetConfiguration InConfiguration)
        {
            if (RunningRocket())
            {
                if (
                    (InConfiguration != UnrealTargetConfiguration.Development)
                    && (InConfiguration != UnrealTargetConfiguration.DebugGame)
                    && (InConfiguration != UnrealTargetConfiguration.Shipping)
                    )
                {
                    return false;
                }
            }
            return true;
        }

        /// <summary>
        /// Is this a valid platform
        /// Used primarily for Rocket vs non-Rocket
        /// </summary>
        /// <param name="InPlatform"></param>
        /// <returns>true if valid, false if not</returns>
        static public bool IsValidPlatform(UnrealTargetPlatform InPlatform)
        {
            if (RunningRocket())
            {
                if(Utils.IsRunningOnMono)
                {
                    if (InPlatform != UnrealTargetPlatform.Mac && InPlatform != UnrealTargetPlatform.IOS && InPlatform != UnrealTargetPlatform.Linux)
                    {
                        return false;
                    }
                }
                else
                {
                    if(InPlatform != UnrealTargetPlatform.Win32 && InPlatform != UnrealTargetPlatform.Win64 && InPlatform != UnrealTargetPlatform.Android)
                    {
                        return false;
                    }
                }
            }
            return true;
        }


        /// <summary>
        /// Is this a valid platform
        /// Used primarily for Rocket vs non-Rocket
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

        public static void RegisterAllUBTClasses(bool bSkipBuildPlatforms = false)
        {
            // Find and register all tool chains and build platforms that are present
            Assembly UBTAssembly = Assembly.GetExecutingAssembly();
            if (UBTAssembly != null)
            {
                Log.TraceVerbose("Searching for ToolChains, BuildPlatforms, BuildDeploys and ProjectGenerators...");

                List<System.Type> ProjectGeneratorList = new List<System.Type>();
                var AllTypes = UBTAssembly.GetTypes();
                if (!bSkipBuildPlatforms)
                {
                    // register all build platforms first, since they implement SDK-switching logic that can set environment variables
                    foreach (var CheckType in AllTypes)
                    {
                        if (CheckType.IsClass && !CheckType.IsAbstract)
                        {
                            if (Utils.ImplementsInterface<IUEBuildPlatform>(CheckType))
                            {
                                Log.TraceVerbose("    Registering build platform: {0}", CheckType.ToString());
                                var TempInst = (UEBuildPlatform)(UBTAssembly.CreateInstance(CheckType.FullName, true));
                                TempInst.RegisterBuildPlatform();
                            }
                        }
                    }
                }
                // register all the other classes
                foreach (var CheckType in AllTypes)
                {
                    if (CheckType.IsClass && !CheckType.IsAbstract)
                    {
                        if (Utils.ImplementsInterface<IUEToolChain>(CheckType))
                        {
                            Log.TraceVerbose("    Registering tool chain    : {0}", CheckType.ToString());
                            var TempInst = (UEToolChain)(UBTAssembly.CreateInstance(CheckType.FullName, true));
                            TempInst.RegisterToolChain();
                        }
                        else if (Utils.ImplementsInterface<IUEBuildDeploy>(CheckType))
                        {
                            Log.TraceVerbose("    Registering build deploy  : {0}", CheckType.ToString());
                            var TempInst = (UEBuildDeploy)(UBTAssembly.CreateInstance(CheckType.FullName, true));
                            TempInst.RegisterBuildDeploy();
                        }
                        else if (CheckType.IsSubclassOf(typeof(UEPlatformProjectGenerator)))
                        {
                            ProjectGeneratorList.Add(CheckType);
                        }
                    }
                }

                // We need to process the ProjectGenerators AFTER all BuildPlatforms have been 
                // registered as they use the BuildPlatform to verify they are legitimate.
                foreach (var ProjType in ProjectGeneratorList)
                {
                    Log.TraceVerbose("    Registering project generator: {0}", ProjType.ToString());
                    UEPlatformProjectGenerator TempInst = (UEPlatformProjectGenerator)(UBTAssembly.CreateInstance(ProjType.FullName, true));
                    TempInst.RegisterPlatformProjectGenerator();
                }
            }
        }

        public static void SetProjectFile(string InProjectFile)
        {
            if (HasUProjectFile() == false || String.IsNullOrEmpty(InProjectFile))
            {
                UProjectFile = InProjectFile;
				if (!String.IsNullOrEmpty(UProjectFile))
                {
                    bHasUProjectFile = true;
                    UProjectPath = Path.GetDirectoryName(UProjectFile);
                }
                else
                {
                    bHasUProjectFile = false;
					UProjectPath = String.Empty;
                }
            }
            else
            {
                throw new BuildException("Trying to set the project file to {0} when it is already set to {1}", InProjectFile, UProjectFile);
            }
        }

		public static void ResetProjectFile()
		{
			// Reset the project settings to their original state. Used by project file generator, since it mutates the active project setting.
			UProjectFile = "";
			bHasUProjectFile = false;
			UProjectPath = "";
		}

        private static bool ParseRocketCommandlineArg(string InArg, ref string OutGameName)
        {
            string LowercaseArg = InArg.ToLowerInvariant();
            bool bSetupProject = false;
            
            if (LowercaseArg == "-rocket")
            {
                bRunningRocket = true;
            }
            else if (LowercaseArg.StartsWith("-project="))
            {
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux) 
				{
					UProjectFile = InArg.Substring(9).Trim(new Char[] { ' ', '"', '\'' });
				}
				else 
				{
					UProjectFile = InArg.Substring(9);
				}

				bSetupProject = true;
            }
            else if (LowercaseArg.EndsWith(".uproject"))
            {
                UProjectFile = InArg;
                bSetupProject = true;
            }
            else
            {
                return false;
            }

            if (bSetupProject && (UProjectFile.Length > 0))
            {
                bHasUProjectFile = true;
    
                string ProjectPath = Path.GetDirectoryName(UProjectFile);
                OutGameName = Path.GetFileNameWithoutExtension(UProjectFile);

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
                                UProjectFile = UProjectFile.Substring(3);
                                Debug.Assert(UProjectFile.Length > 0);
                            }
                        }
                    }
                }

                UProjectPath = Path.GetFullPath(ProjectPath);
            }

            return true;
        }

        /// <summary>
        /// Sets up UBT Trace listeners and filters.
        /// </summary>
        private static void InitLogging()
        {
            Trace.Listeners.Add(new ConsoleListener());
            var Filter = new VerbosityFilter();
            foreach (TraceListener Listener in Trace.Listeners)
            {
                Listener.Filter = Filter;
            }
        }

        public static int ExtendedErrorCode = 0;
        private static int Main(string[] Arguments)
        {
            InitLogging();

            try
            {
                XmlConfigLoader.Init();
            }
            catch (BuildException Exception)
            {
                Log.TraceError("UnrealBuildTool Exception: " + Exception);
                return (int) ECompilationResult.OtherCompilationError;
            }

			// Because XmlConfigLoader.Init() will reset all parameter by call LoadDefaults, 
			// so need to parse verbose argument here.
			int Verbose;
			if (Utils.ParseCommandLineFlag(Arguments, "-verbose", out Verbose))
			{
				BuildConfiguration.bPrintDebugInfo = true;
			}

			Log.TraceVerbose(Environment.CommandLine);

            InitialEnvironment = Environment.GetEnvironmentVariables();
            if (InitialEnvironment.Count < 1)
            {
                throw new BuildException("Environment could not be read");
            }
            // Helpers used for stats tracking.
            StartTime = DateTime.UtcNow;

            Telemetry.Initialize();

            // Copy off the arguments to allow checking for command-line arguments elsewhere
            CmdLine = new List<string>(Arguments);

            foreach (string Arg in Arguments)
            {
                string TempGameName = null;
                if (ParseRocketCommandlineArg(Arg, ref TempGameName) == true)
                {
                    // This is to allow relative paths for the project file
                    Log.TraceVerbose("UBT Running for Rocket: " + UProjectFile);
                }
            }

            // Change the working directory to be the Engine/Source folder. We are running from Engine/Binaries/DotNET
            string EngineSourceDirectory = Path.Combine(Utils.GetExecutingAssemblyDirectory(), "..", "..", "..", "Engine", "Source");

            //@todo.Rocket: This is a workaround for recompiling game code in editor
            // The working directory when launching is *not* what we would expect
            if (Directory.Exists(EngineSourceDirectory) == false)
            {
                // We are assuming UBT always runs from <>/Engine/...
                EngineSourceDirectory = Path.GetDirectoryName(Utils.GetExecutingAssemblyLocation());
                EngineSourceDirectory = EngineSourceDirectory.Replace("\\", "/");
                Int32 EngineIdx = EngineSourceDirectory.IndexOf("/Engine/");
                if (EngineIdx != 0)
                {
                    EngineSourceDirectory = EngineSourceDirectory.Substring(0, EngineIdx + 8);
                    EngineSourceDirectory += "Source";
                }
            }
            if (Directory.Exists(EngineSourceDirectory)) // only set the directory if it exists, this should only happen if we are launching the editor from an artist sync
            {
                Directory.SetCurrentDirectory(EngineSourceDirectory);
            }

            // Build the list of game projects that we know about. When building from the editor (for hot-reload) or for Rocket projects, we require the 
            // project file to be passed in. Otherwise we scan for projects in directories named in UE4Games.uprojectdirs.
            if (HasUProjectFile())
            {
                string SourceFolder = Path.Combine(Path.GetDirectoryName(GetUProjectFile()), "Source");
				string IntermediateSourceFolder = Path.Combine(Path.GetDirectoryName(GetUProjectFile()), "Intermediate", "Source");
                UProjectInfo.AddProject(GetUProjectFile(), Directory.Exists(SourceFolder) || Directory.Exists(IntermediateSourceFolder));
            }
            else
            {
                UProjectInfo.FillProjectInfo();
            }

			var BasicInitStartTime = DateTime.UtcNow;

            ECompilationResult Result = ECompilationResult.Succeeded;

            // Reset early so we can access BuildConfiguration even before RunUBT() is called
            XmlConfigLoader.Reset<BuildConfiguration>();
            XmlConfigLoader.Reset<UEBuildConfiguration>();

			if (Utils.ParseCommandLineFlag(Arguments, "-verbose", out Verbose))
			{
				BuildConfiguration.bPrintDebugInfo = true;
			}

            Log.TraceVerbose("UnrealBuildTool (DEBUG OUTPUT MODE)");
            Log.TraceVerbose("Command-line: {0}", String.Join(" ", Arguments));

            bool bRunCopyrightVerification = false;
            bool bDumpToFile = false;
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
                    int LocationHash = Utils.GetExecutingAssemblyLocation().GetHashCode();

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
                        // If this instance didn't create the mutex, wait for the existing mutex to be released by the mutex's creator.
                        DateTime MutexWaitStartTime = DateTime.UtcNow;
                        SingleInstanceMutex.WaitOne();
                        MutexWaitTime = DateTime.UtcNow - MutexWaitStartTime;
                    }
                }

                try
                {
                    string GameName = null;
                    var bGenerateVCProjectFiles = false;
                    var bGenerateXcodeProjectFiles = false;
                    var bGenerateMakefiles = false;
					var bGenerateCMakefiles = false;
					var bGenerateQMakefiles = false;
					var bGenerateKDevelopFiles = false;
                    var bValidPlatformsOnly = false;
                    var bSpecificModulesOnly = false;

                    // We need to be able to identify if one of the arguments is the platform...
                    // Leverage the existing parser function in UEBuildTarget to get this information.
                    UnrealTargetPlatform CheckPlatform;
                    UnrealTargetConfiguration CheckConfiguration;
                    UEBuildTarget.ParsePlatformAndConfiguration(Arguments, out CheckPlatform, out CheckConfiguration, false);

					// @todo ubtmake: remove this when building with RPCUtility works
					if (CheckPlatform == UnrealTargetPlatform.Mac || CheckPlatform == UnrealTargetPlatform.IOS)
					{
						BuildConfiguration.bUseUBTMakefiles = false;
					}

                    // If we were asked to enable fast build iteration, we want the 'gather' phase to default to off (unless it is overridden below
                    // using a command-line option.)
                    if( BuildConfiguration.bUseUBTMakefiles )
                    {
                        bIsGatheringBuild_Unsafe = false;
                    }

                    foreach (var Arg in Arguments)
                    {
                        string LowercaseArg = Arg.ToLowerInvariant();
                        const string OnlyPlatformSpecificForArg = "-onlyplatformspecificfor=";
						const string UCAUsageThresholdString = "-ucausagethreshold=";
						const string UCAModuleToAnalyzeString = "-ucamoduletoanalyze=";
                        if (ParseRocketCommandlineArg(Arg, ref GameName))
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
                        else if (LowercaseArg == "-forceheadergeneration")
                        {
                            // Don't reset this as we don't recheck the args in RunUBT
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
                        else if (LowercaseArg == "-copyrightverification")
                        {
                            bRunCopyrightVerification = true;
                        }
                        else if (LowercaseArg == "-dumptofile")
                        {
                            bDumpToFile = true;
                        }
                        else if (LowercaseArg == "-check3rdpartyheaders")
                        {
                            bCheckThirdPartyHeaders = true;
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
                        else if (LowercaseArg == "-xgeexport")
                        {
                            BuildConfiguration.bXGEExport = true;
                            BuildConfiguration.bAllowXGE = true;
                        }
						else if (LowercaseArg == "-noubtmakefiles")
						{
							BuildConfiguration.bUseUBTMakefiles = false;
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
                        else if (LowercaseArg == "-nosimplygon")
                        {
                            UEBuildConfiguration.bCompileSimplygon = false;
                        }
                        else if (LowercaseArg == "-nospeedtree")
                        {
                            UEBuildConfiguration.bCompileSpeedTree = false;
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
						else if (LowercaseArg == "-canskiplink")
						{
							UEBuildConfiguration.bSkipLinkingWhenNothingToCompile = true;
						}
						else if (LowercaseArg == "-nocef3")
						{
							UEBuildConfiguration.bCompileCEF3 = false;
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
                        else if (CheckPlatform.ToString().ToLowerInvariant() == LowercaseArg)
                        {
                            // It's the platform set...
                            //PlatformName = Arg;
                        }						
                        else
                        {
                            // This arg may be a game name. Check for the existence of a game folder with this name.
                            // "Engine" is not a valid game name.
                            if (LowercaseArg != "engine" && Arg.IndexOfAny(Path.GetInvalidPathChars()) == -1 &&
                                Directory.Exists(Path.Combine(ProjectFileGenerator.RootRelativePath, Arg, "Config")))
                            {
                                GameName = Arg;
                                Log.TraceVerbose("CommandLine: Found game name '{0}'", GameName);
                            }
                            else if (LowercaseArg == "rocket")
                            {
                                GameName = Arg;
                                Log.TraceVerbose("CommandLine: Found game name '{0}'", GameName);
                            }
                        }
                    }

                    if( !bIsGatheringBuild_Unsafe && !bIsAssemblingBuild_Unsafe )
                    {
                        throw new BuildException( "UnrealBuildTool: At least one of either IsGatheringBuild or IsAssemblingBuild must be true.  Did you pass '-NoGather' with '-NoAssemble'?" );
                    }

					if (BuildConfiguration.bRunUnrealCodeAnalyzer && (BuildConfiguration.UCAModuleToAnalyze == null || BuildConfiguration.UCAModuleToAnalyze.Length == 0))
					{
						throw new BuildException("When running UnrealCodeAnalyzer, please specify module to analyze in UCAModuleToAnalyze field in BuildConfiguration.xml");
					}

                    // Send an event with basic usage dimensions
                    // [RCL] 2014-11-03 FIXME: this is incorrect since we can have more than one action
                    Telemetry.SendEvent("CommonAttributes.2",
                        // @todo No platform independent way to do processor speed and count. Defer for now. Mono actually has ways to do this but needs to be tested.
                        "ProcessorCount", Environment.ProcessorCount.ToString(),
                        "ComputerName", Environment.MachineName,
                        "User", Environment.UserName,
                        "Domain", Environment.UserDomainName,
                        "CommandLine", string.Join(" ", Arguments),
                        "UBT Action", bGenerateVCProjectFiles 
                            ? "GenerateVCProjectFiles" 
                            : bGenerateXcodeProjectFiles 
                                ? "GenerateXcodeProjectFiles" 
                                : bRunCopyrightVerification
                                    ? "RunCopyrightVerification"
                                    : bGenerateMakefiles
                                        ? "GenerateMakefiles"
										: bGenerateCMakefiles
										    ? "GenerateCMakeFiles"
											: bGenerateQMakefiles
											    ? "GenerateQMakefiles"
												: bGenerateKDevelopFiles
													? "GenerateKDevelopFiles"
													: bValidatePlatforms 
                                                   		? "ValidatePlatforms"
                                                		: "Build",
                        "Platform", CheckPlatform.ToString(),
                        "Configuration", CheckConfiguration.ToString(),
                        "IsRocket", bRunningRocket.ToString(),
                        "EngineVersion", Utils.GetEngineVersionFromObjVersionCPP().ToString()
                        );



                    if (bValidPlatformsOnly == true)
                    {
                        // We turned the flag on if generating projects so that the
                        // build platforms would know we are doing so... In this case,
                        // turn it off to only generate projects for valid platforms.
                        ProjectFileGenerator.bGenerateProjectFiles = false;
                    }

                    // Find and register all tool chains, build platforms, etc. that are present
                    RegisterAllUBTClasses();
                    ProjectFileGenerator.bGenerateProjectFiles = false;

					if( BuildConfiguration.bPrintPerformanceInfo )
					{ 
						var BasicInitTime = (DateTime.UtcNow - BasicInitStartTime).TotalSeconds;
						Log.TraceInformation( "Basic UBT initialization took " + BasicInitTime + "s" );
					}

                    // now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
                    if (!bSpecificModulesOnly && !bIgnoreJunk)
                    {
                        JunkDeleter.DeleteJunk();
                    }

					if (bGenerateVCProjectFiles || bGenerateXcodeProjectFiles || bGenerateMakefiles || bGenerateCMakefiles || bGenerateQMakefiles || bGenerateKDevelopFiles)
                    {
                        bool bGenerationSuccess = true;
                        if (bGenerateVCProjectFiles)
                        {
                            bGenerationSuccess &= GenerateProjectFiles(new VCProjectFileGenerator(), Arguments);
                        }
                        if (bGenerateXcodeProjectFiles)
                        {
                            bGenerationSuccess &= GenerateProjectFiles(new XcodeProjectFileGenerator(), Arguments);
                        }
                        if (bGenerateMakefiles)
                        {
                            bGenerationSuccess &= GenerateProjectFiles(new MakefileGenerator(), Arguments);
                        }
						if (bGenerateCMakefiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new CMakefileGenerator(), Arguments);
						}
						if (bGenerateQMakefiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new QMakefileGenerator(), Arguments);
						}
						if (bGenerateKDevelopFiles)
						{
							bGenerationSuccess &= GenerateProjectFiles(new KDevelopGenerator(), Arguments);
						}

                        if(!bGenerationSuccess)
                        {
                            Result = ECompilationResult.OtherCompilationError;
                        }
                    }
                    else if (bRunCopyrightVerification)
                    {
                        CopyrightVerify.RunCopyrightVerification(EngineSourceDirectory, GameName, bDumpToFile);
                        Log.TraceInformation("Completed... exiting.");
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
                            ThirdPartyHeaderFinder.FindThirdPartyIncludes(CheckPlatform, CheckConfiguration);
                        }

                        // Build our project
                        if (Result == ECompilationResult.Succeeded)
                        {
                            if (UEBuildConfiguration.bPrepForDeployment == false)
                            {
								// If we are only prepping for deployment, assume the build already occurred.
								Result = RunUBT(Arguments);
                            }
                            else
                            {
                                var BuildPlatform = UEBuildPlatform.GetBuildPlatform(CheckPlatform, true);
                                if (BuildPlatform != null)
                                {
                                    // Setup environment wasn't called, so set the flag
                                    BuildConfiguration.bDeployAfterCompile = BuildPlatform.RequiresDeployPrepAfterCompile();
                                    BuildConfiguration.PlatformIntermediateFolder = Path.Combine(BuildConfiguration.BaseIntermediateFolder, CheckPlatform.ToString(), BuildPlatform.GetActiveArchitecture());
                                }
                            }

                            // If we build w/ bXGEExport true, we didn't REALLY build at this point, 
                            // so don't bother with doing the PrepTargetForDeployment call. 
                            if ((Result == ECompilationResult.Succeeded) && (BuildConfiguration.bDeployAfterCompile == true) && (BuildConfiguration.bXGEExport == false) &&
                                (UEBuildConfiguration.bGenerateManifest == false) && (UEBuildConfiguration.bGenerateExternalFileList == false) && (UEBuildConfiguration.bCleanProject == false))
                            {
                                var DeployHandler = UEBuildDeploy.GetBuildDeploy(CheckPlatform);
                                if (DeployHandler != null)
                                {
                                    // We need to be able to identify the Target.Type we can derive it from the Arguments.
                                    BuildConfiguration.bFlushBuildDirOnRemoteMac = false;
									var TargetDescs = UEBuildTarget.ParseTargetCommandLine( Arguments );
			                        UEBuildTarget CheckTarget = UEBuildTarget.CreateTarget( TargetDescs[0] );	// @todo ubtmake: This may not work in assembler only mode.  We don't want to be loading target rules assemblies here either.
                                    CheckTarget.SetupGlobalEnvironment();
                                    if ((CheckTarget.TargetType == TargetRules.TargetType.Game) ||
                                        (CheckTarget.TargetType == TargetRules.TargetType.Server) ||
                                        (CheckTarget.TargetType == TargetRules.TargetType.Client))
                                    {
                                        CheckTarget.AppName = CheckTarget.TargetName;
                                    }
                                    DeployHandler.PrepTargetForDeployment(CheckTarget);
                                }
                            }
                        }
                    }
                    // Print some performance info
                    var BuildDuration = (DateTime.UtcNow - StartTime - MutexWaitTime).TotalSeconds;
                    if (BuildConfiguration.bPrintPerformanceInfo)
                    {
                        Log.TraceInformation("GetIncludes time: " + CPPEnvironment.TotalTimeSpentGettingIncludes + "s (" + CPPEnvironment.TotalIncludesRequested + " includes)" );
                        Log.TraceInformation("DirectIncludes cache miss time: " + CPPEnvironment.DirectIncludeCacheMissesTotalTime + "s (" + CPPEnvironment.TotalDirectIncludeCacheMisses + " misses)" );
                        Log.TraceInformation("FindIncludePaths calls: " + CPPEnvironment.TotalFindIncludedFileCalls + " (" + CPPEnvironment.IncludePathSearchAttempts + " searches)" );
                        Log.TraceInformation("PCH gen time: " + UEBuildModuleCPP.TotalPCHGenTime + "s" );
                        Log.TraceInformation("PCH cache time: " + UEBuildModuleCPP.TotalPCHCacheTime + "s" );
                        Log.TraceInformation("Deep C++ include scan time: " + UnrealBuildTool.TotalDeepIncludeScanTime + "s" );
                        Log.TraceInformation("Include Resolves: {0} ({1} misses, {2:0.00}%)", CPPEnvironment.TotalDirectIncludeResolves, CPPEnvironment.TotalDirectIncludeResolveCacheMisses, (float)CPPEnvironment.TotalDirectIncludeResolveCacheMisses / (float)CPPEnvironment.TotalDirectIncludeResolves * 100);
                        Log.TraceInformation("Total FileItems: {0} ({1} missing)", FileItem.TotalFileItemCount, FileItem.MissingFileItemCount);

                        Log.TraceInformation("Execution time: {0}s", BuildDuration);
                    }

                    Telemetry.SendEvent("PerformanceInfo.2",
                        "TotalExecutionTimeSec", BuildDuration.ToString("0.00"),
                        "MutexWaitTimeSec", MutexWaitTime.TotalSeconds.ToString("0.00"),
                        "TotalTimeSpentGettingIncludesSec", CPPEnvironment.TotalTimeSpentGettingIncludes.ToString("0.00"),
                        "TotalIncludesRequested", CPPEnvironment.TotalIncludesRequested.ToString(),
                        "DirectIncludeCacheMissesTotalTimeSec", CPPEnvironment.DirectIncludeCacheMissesTotalTime.ToString("0.00"),
                        "TotalDirectIncludeCacheMisses", CPPEnvironment.TotalDirectIncludeCacheMisses.ToString(),
                        "TotalFindIncludedFileCalls", CPPEnvironment.TotalFindIncludedFileCalls.ToString(),
                        "IncludePathSearchAttempts", CPPEnvironment.IncludePathSearchAttempts.ToString(),
                        "TotalFileItemCount", FileItem.TotalFileItemCount.ToString(),
                        "MissingFileItemCount", FileItem.MissingFileItemCount.ToString()
                        );
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
            Log.TraceVerbose("Execution time: {0}", (DateTime.UtcNow - StartTime - MutexWaitTime).TotalSeconds);

			if (ExtendedErrorCode != 0)
			{
				return ExtendedErrorCode;
			}
            return (int)Result; 
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
            foreach (var CurArgument in Arguments)
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
                var BuildPlatform = UEBuildPlatform.GetBuildPlatform(platform, true);
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

        
        public static ECompilationResult RunUBT(string[] Arguments)
        {
            bool bSuccess = true;

			
			var RunUBTInitStartTime = DateTime.UtcNow;


            // Reset global configurations
            ActionGraph.ResetAllActions();

            ParseBuildConfigurationFlags(Arguments);

            // We need to allow the target platform to perform the 'reset' as well...
            UnrealTargetPlatform ResetPlatform = UnrealTargetPlatform.Unknown;
            UnrealTargetConfiguration ResetConfiguration;
            UEBuildTarget.ParsePlatformAndConfiguration(Arguments, out ResetPlatform, out ResetConfiguration);
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(ResetPlatform);
			BuildPlatform.ResetBuildConfiguration(ResetPlatform, ResetConfiguration);

            // now that we have the platform, we can set the intermediate path to include the platform/architecture name
            BuildConfiguration.PlatformIntermediateFolder = Path.Combine(BuildConfiguration.BaseIntermediateFolder, ResetPlatform.ToString(), BuildPlatform.GetActiveArchitecture());

            string ExecutorName = "Unknown";
            ECompilationResult BuildResult = ECompilationResult.Succeeded;

            var ToolChain = UEToolChain.GetPlatformToolChain(BuildPlatform.GetCPPTargetPlatform(ResetPlatform));

            string EULAViolationWarning = null;
			Thread CPPIncludesThread = null;

            try
            {
                List<string[]> TargetSettings = ParseCommandLineFlags(Arguments);

                int ArgumentIndex;
                // action graph implies using the dependency resolve cache
                bool GeneratingActionGraph = Utils.ParseCommandLineFlag(Arguments, "-graph", out ArgumentIndex );
                if (GeneratingActionGraph)
                {
                    BuildConfiguration.bUseIncludeDependencyResolveCache = true;
                }

                bool CreateStub = Utils.ParseCommandLineFlag(Arguments, "-nocreatestub", out ArgumentIndex);
                if (CreateStub || (String.IsNullOrEmpty(Environment.GetEnvironmentVariable("uebp_LOCAL_ROOT")) && BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac))
                {
                    BuildConfiguration.bCreateStubIPA = false;
                }


				if( BuildConfiguration.bPrintPerformanceInfo )
				{ 
					var RunUBTInitTime = (DateTime.UtcNow - RunUBTInitStartTime).TotalSeconds;
					Log.TraceInformation( "RunUBT initialization took " + RunUBTInitTime + "s" );
				}

	            var TargetDescs = new List<TargetDescriptor>();
				{ 
					var TargetDescstStartTime = DateTime.UtcNow;

					foreach (string[] TargetSetting in TargetSettings)
					{
						TargetDescs.AddRange( UEBuildTarget.ParseTargetCommandLine( TargetSetting ) );
					}

					if( BuildConfiguration.bPrintPerformanceInfo )
					{ 
						var TargetDescsTime = (DateTime.UtcNow - TargetDescstStartTime).TotalSeconds;
						Log.TraceInformation( "Target descriptors took " + TargetDescsTime + "s" );
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

				UEBuildConfiguration.bHotReloadFromIDE = UEBuildConfiguration.bAllowHotReloadFromIDE && TargetDescs.Count == 1 && !TargetDescs[0].bIsEditorRecompile && ShouldDoHotReloadFromIDE(TargetDescs[0]);
				bool bIsHotReload = UEBuildConfiguration.bHotReloadFromIDE || ( TargetDescs.Count == 1 && TargetDescs[0].OnlyModules.Count > 0 && TargetDescs[0].ForeignPlugins.Count == 0 );
				TargetDescriptor HotReloadTargetDesc = bIsHotReload ? TargetDescs[0] : null;

				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					// Create empty timestamp file to record when was the last time we regenerated projects.
					Directory.CreateDirectory( Path.GetDirectoryName( ProjectFileGenerator.ProjectTimestampFile ) );
					File.Create(ProjectFileGenerator.ProjectTimestampFile).Dispose();
				}

                if( !ProjectFileGenerator.bGenerateProjectFiles )
                {
					if( BuildConfiguration.bUseUBTMakefiles )
                    {
                        // If we're building UHT without a Mutex, we'll need to assume that we're building the same targets and that no caches
                        // should be invalidated for this run.  This is important when UBT is invoked from within UBT in order to compile
                        // UHT.  In that (very common) case we definitely don't want to have to rebuild our cache from scratch.
                        bool bMustAssumeSameTargets = false;

                        if( TargetDescs[0].TargetName.Equals( "UnrealHeaderTool", StringComparison.InvariantCultureIgnoreCase ) )
                        {
                            int NoMutexArgumentIndex;
                            if (Utils.ParseCommandLineFlag(Arguments, "-NoMutex", out NoMutexArgumentIndex))
                            {
                                bMustAssumeSameTargets = true;
                            }
                        }

	                    bool bIsBuildingSameTargetsAsLastTime = false;
                        if( bMustAssumeSameTargets )
                        {
                            bIsBuildingSameTargetsAsLastTime = true;
                        }
                        else
                        {
                            string TargetCollectionName = MakeTargetCollectionName( TargetDescs );

							string LastBuiltTargetsFileName = bIsHotReload ? "HotReloadLastBuiltTargets.txt" : "LastBuiltTargets.txt";
                            string LastBuiltTargetsFilePath = Path.Combine( BuildConfiguration.BaseIntermediatePath, LastBuiltTargetsFileName );
                            if( File.Exists( LastBuiltTargetsFilePath ) && Utils.ReadAllText( LastBuiltTargetsFilePath ) == TargetCollectionName )
                            {
								// @todo ubtmake: Because we're using separate files for hot reload vs. full compiles, it's actually possible that includes will
								// become out of date without us knowing if the developer ping-pongs between hot reloading target A and building target B normally.
								// To fix this we can not use a different file name for last built targets, but the downside is slower performance when
								// performing the first hot reload after compiling normally (forces full include dependency scan)
                                bIsBuildingSameTargetsAsLastTime = true;
                            }

                            // Save out the name of the targets we're building
                            if( !bIsBuildingSameTargetsAsLastTime )
                            {
                                Directory.CreateDirectory( Path.GetDirectoryName( LastBuiltTargetsFilePath ) );
                                File.WriteAllText( LastBuiltTargetsFilePath, TargetCollectionName, Encoding.UTF8 );
                            }


                            if( !bIsBuildingSameTargetsAsLastTime )
                            {
                                // Can't use super fast include checking unless we're building the same set of targets as last time, because
                                // we might not know about all of the C++ include dependencies for already-up-to-date shared build products
                                // between the targets
                                bNeedsFullCPPIncludeRescan = true;
								Log.TraceInformation( "Performing full C++ include scan ({0} a new target)", bIsHotReload ? "hot reloading" : "building" );
                            }
                        }
                    }
                }
            


                UBTMakefile UBTMakefile = null;
                { 
                    // If we're generating project files, then go ahead and wipe out the existing UBTMakefile for every target, to make sure that
                    // it gets a full dependency scan next time.
                    // NOTE: This is just a safeguard and doesn't have to be perfect.  We also check for newer project file timestamps in LoadUBTMakefile()
                    if( ProjectFileGenerator.bGenerateProjectFiles )	// @todo ubtmake: This is only hit when generating IntelliSense for project files.  Probably should be done right inside ProjectFileGenerator.bat
                    {													// @todo ubtmake: Won't catch multi-target cases as GPF always builds one target at a time for Intellisense
                        // Delete the UBTMakefile
                        var UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath( TargetDescs );
                        if (File.Exists(UBTMakefilePath))
                        {
                            UEBuildTarget.CleanFile(UBTMakefilePath);
                        }
                    }

                    // Make sure the gather phase is executed if we're not actually building anything
                    if( ProjectFileGenerator.bGenerateProjectFiles || UEBuildConfiguration.bGenerateManifest || UEBuildConfiguration.bCleanProject || BuildConfiguration.bXGEExport || UEBuildConfiguration.bGenerateExternalFileList || GeneratingActionGraph)
                    {
                        UnrealBuildTool.bIsGatheringBuild_Unsafe = true;						
                    }

                    // Were we asked to run in 'assembler only' mode?  If so, let's check to see if that's even possible by seeing if
                    // we have a valid UBTMakefile already saved to disk, ready for us to load.
                    if( UnrealBuildTool.bIsAssemblingBuild_Unsafe && !UnrealBuildTool.bIsGatheringBuild_Unsafe )
                    {
                        // @todo ubtmake: Mildly terrified of BuildConfiguration/UEBuildConfiguration globals that were set during the Prepare phase but are not set now.  We may need to save/load all of these, otherwise
                        //		we'll need to call SetupGlobalEnvironment on all of the targets (maybe other stuff, too.  See PreBuildStep())

                        // Try to load the UBTMakefile.  It will only be loaded if it has valid content and is not determined to be out of date.    
						string ReasonNotLoaded;
                        UBTMakefile = LoadUBTMakefile( TargetDescs, out ReasonNotLoaded );

                        if( UBTMakefile == null )
                        { 
                            // If the Makefile couldn't be loaded, then we're not going to be able to continue in "assembler only" mode.  We'll do both
                            // a 'gather' and 'assemble' in the same run.  This will take a while longer, but subsequent runs will be fast!
                            UnrealBuildTool.bIsGatheringBuild_Unsafe = true;

                            Log.TraceInformation( "Creating makefile for {0}{1}{2} ({3})", 
								bIsHotReload ? "hot reloading " : "", 
								TargetDescs[0].TargetName, 
								TargetDescs.Count > 1 ? ( " (and " + ( TargetDescs.Count - 1 ).ToString() + " more)" ) : "", 
								ReasonNotLoaded );
                        }
                    }

                    // OK, after this point it is safe to access the UnrealBuildTool.IsGatheringBuild and UnrealBuildTool.IsAssemblingBuild properties.
                    // These properties will not be changing again during this session/
                    bIsSafeToCheckIfGatheringOrAssemblingBuild = true;
                }


				List<UEBuildTarget> Targets;
				if( UBTMakefile != null && !IsGatheringBuild && IsAssemblingBuild )
				{
					// If we've loaded a makefile, then we can fill target information from this file!
					Targets = UBTMakefile.Targets;
				}
				else
				{ 
					var TargetInitStartTime = DateTime.UtcNow;

					Targets = new List<UEBuildTarget>();
					foreach( var TargetDesc in TargetDescs )
					{
						var Target = UEBuildTarget.CreateTarget( TargetDesc );
						if ((Target == null) && (UEBuildConfiguration.bCleanProject))
						{
							continue;
						}
						Targets.Add(Target);
					}

					if( BuildConfiguration.bPrintPerformanceInfo )
					{ 
						var TargetInitTime = (DateTime.UtcNow - TargetInitStartTime).TotalSeconds;
						Log.TraceInformation( "Target init took " + TargetInitTime + "s" );
					}
				}

                // Build action lists for all passed in targets.
                var OutputItemsForAllTargets = new List<FileItem>();
                var TargetNameToUObjectModules = new Dictionary<string, List<UHTModuleInfo>>( StringComparer.InvariantCultureIgnoreCase );
                foreach (var Target in Targets)
                {
                    var TargetStartTime = DateTime.UtcNow;

                    if (bIsHotReload)
                    {
						// Don't produce new DLLs if there's been no code changes
						UEBuildConfiguration.bSkipLinkingWhenNothingToCompile = true;
                        Log.TraceInformation("Compiling game modules for hot reload");
                    }

                    // When in 'assembler only' mode, we'll load this cache later on a worker thread.  It takes a long time to load!
                    if( !( !UnrealBuildTool.IsGatheringBuild && UnrealBuildTool.IsAssemblingBuild ) )
                    {	
                        // Load the direct include dependency cache.
                        CPPEnvironment.IncludeDependencyCache.Add( Target, DependencyCache.Create( DependencyCache.GetDependencyCachePathForTarget(Target) ) );
                    }

                    // We don't need this dependency cache in 'gather only' mode
                    if( BuildConfiguration.bUseUBTMakefiles &&
                        !( UnrealBuildTool.IsGatheringBuild && !UnrealBuildTool.IsAssemblingBuild ) )
                    { 
                        // Load the cache that contains the list of flattened resolved includes for resolved source files
                        // @todo ubtmake: Ideally load this asynchronously at startup and only block when it is first needed and not finished loading
                        CPPEnvironment.FlatCPPIncludeDependencyCache.Add( Target, FlatCPPIncludeDependencyCache.Create( Target ) );
                    }

                    if( UnrealBuildTool.IsGatheringBuild )
                    { 
                        List<FileItem> TargetOutputItems;
                        List<UHTModuleInfo> TargetUObjectModules;
                        BuildResult = Target.Build(ToolChain, out TargetOutputItems, out TargetUObjectModules, out EULAViolationWarning);
                        if(BuildResult != ECompilationResult.Succeeded)
                        {
                            break;
                        }

                        OutputItemsForAllTargets.AddRange( TargetOutputItems );

                        // Update mapping of the target name to the list of UObject modules in this target
                        TargetNameToUObjectModules[ Target.GetTargetName() ] = TargetUObjectModules;

                        if ( (BuildConfiguration.bXGEExport && UEBuildConfiguration.bGenerateManifest) || (!ProjectFileGenerator.bGenerateProjectFiles && !UEBuildConfiguration.bGenerateManifest && !UEBuildConfiguration.bCleanProject))
                        {
                            // Generate an action graph if we were asked to do that.  The graph generation needs access to the include dependency cache, so
                            // we generate it before saving and cleaning that up.
                            if( GeneratingActionGraph )
                            {
                                // The graph generation feature currently only works with a single target at a time.  This is because of how we need access
                                // to include dependencies for the target, but those aren't kept around as we process multiple targets
                                if( TargetSettings.Count != 1 )
                                {
                                    throw new BuildException( "ERROR: The '-graph' option only works with a single target at a time" );
                                }
                                ActionGraph.FinalizeActionGraph();
                                var ActionsToExecute = ActionGraph.AllActions;

                                var VisualizationType = ActionGraph.ActionGraphVisualizationType.OnlyCPlusPlusFilesAndHeaders;
                                ActionGraph.SaveActionGraphVisualization( Target, Path.Combine( BuildConfiguration.BaseIntermediatePath, Target.GetTargetName() + ".gexf" ), Target.GetTargetName(), VisualizationType, ActionsToExecute );
                            }
                        }

                        var TargetBuildTime = (DateTime.UtcNow - TargetStartTime).TotalSeconds;

                        // Send out telemetry for this target
                        Telemetry.SendEvent("TargetBuildStats.2",
                            "AppName", Target.AppName,
                            "GameName", Target.TargetName,
                            "Platform", Target.Platform.ToString(),
                            "Configuration", Target.Configuration.ToString(),
                            "CleanTarget", UEBuildConfiguration.bCleanProject.ToString(),
                            "Monolithic", Target.ShouldCompileMonolithic().ToString(),
                            "CreateDebugInfo", Target.IsCreatingDebugInfo().ToString(),
                            "TargetType", Target.TargetType.ToString(),
                            "TargetCreateTimeSec", TargetBuildTime.ToString("0.00")
                            );
                    }
                }

                if (BuildResult == ECompilationResult.Succeeded &&
                    (
                        (BuildConfiguration.bXGEExport && UEBuildConfiguration.bGenerateManifest) ||
                        (!GeneratingActionGraph && !ProjectFileGenerator.bGenerateProjectFiles && !UEBuildConfiguration.bGenerateManifest && !UEBuildConfiguration.bCleanProject && !UEBuildConfiguration.bGenerateExternalFileList)
                    ))
                {
                    if( UnrealBuildTool.IsGatheringBuild )
                    {
                        ActionGraph.FinalizeActionGraph();

                        UBTMakefile = new UBTMakefile();
                        UBTMakefile.AllActions = ActionGraph.AllActions;

                        // For now simply treat all object files as the root target.
                        { 
                            var PrerequisiteActionsSet = new HashSet<Action>();
                            foreach (var OutputItem in OutputItemsForAllTargets)
                            {
                                ActionGraph.GatherPrerequisiteActions(OutputItem, ref PrerequisiteActionsSet);
                            }
                            UBTMakefile.PrerequisiteActions = PrerequisiteActionsSet.ToArray();
                        }			

                        foreach( System.Collections.DictionaryEntry EnvironmentVariable in Environment.GetEnvironmentVariables() )
                        {
                            UBTMakefile.EnvironmentVariables.Add( Tuple.Create( (string)EnvironmentVariable.Key, (string)EnvironmentVariable.Value ) );
                        }
                        UBTMakefile.TargetNameToUObjectModules = TargetNameToUObjectModules;
						UBTMakefile.Targets = Targets;

						if( BuildConfiguration.bUseUBTMakefiles )
						{ 
							// We've been told to prepare to build, so let's go ahead and save out our action graph so that we can use in a later invocation 
							// to assemble the build.  Even if we are configured to assemble the build in this same invocation, we want to save out the
							// Makefile so that it can be used on subsequent 'assemble only' runs, for the fastest possible iteration times
							// @todo ubtmake: Optimization: We could make 'gather + assemble' mode slightly faster by saving this while busy compiling (on our worker thread)
							SaveUBTMakefile( TargetDescs, UBTMakefile );
						}
                    }

                    if( UnrealBuildTool.IsAssemblingBuild )
                    {
                        // If we didn't build the graph in this session, then we'll need to load a cached one
                        if( !UnrealBuildTool.IsGatheringBuild )
                        {
                            ActionGraph.AllActions = UBTMakefile.AllActions;

							// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
							// the same for every invocation on that makefile, but we need a new suffix each time.
							if( bIsHotReload )
							{
								PatchActionHistoryForHotReloadAssembling( HotReloadTargetDesc.OnlyModules );
							}


                            foreach( var EnvironmentVariable in UBTMakefile.EnvironmentVariables )
                            {
                                // @todo ubtmake: There may be some variables we do NOT want to clobber.
                                Environment.SetEnvironmentVariable( EnvironmentVariable.Item1, EnvironmentVariable.Item2 );
                            }

                            // If any of the targets need UHT to be run, we'll go ahead and do that now
                            foreach( var Target in Targets )
                            {
                                List<UHTModuleInfo> TargetUObjectModules;
                                if( UBTMakefile.TargetNameToUObjectModules.TryGetValue( Target.GetTargetName(), out TargetUObjectModules ) )
                                {
                                    if( TargetUObjectModules.Count > 0 )
                                    {
                                        // Execute the header tool
                                        string ModuleInfoFileName = Path.GetFullPath( Path.Combine( Target.ProjectIntermediateDirectory, "UnrealHeaderTool.manifest" ) );
                                        ECompilationResult UHTResult = ECompilationResult.OtherCompilationError;
                                        if (!ExternalExecution.ExecuteHeaderToolIfNecessary(Target, GlobalCompileEnvironment:null, UObjectModules:TargetUObjectModules, ModuleInfoFileName:ModuleInfoFileName, UHTResult:ref UHTResult))
                                        {
                                            Log.TraceInformation("UnrealHeaderTool failed for target '" + Target.GetTargetName() + "' (platform: " + Target.Platform.ToString() + ", module info: " + ModuleInfoFileName + ").");
                                            BuildResult = UHTResult;
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        if( BuildResult.Succeeded() )
                        { 
                            // Make sure any old DLL files from in-engine recompiles aren't lying around.  Must be called after the action graph is finalized.
                            ActionGraph.DeleteStaleHotReloadDLLs();

                            // Plan the actions to execute for the build.
                            Dictionary<UEBuildTarget,List<FileItem>> TargetToOutdatedPrerequisitesMap;
                            List<Action> ActionsToExecute = ActionGraph.GetActionsToExecute(UBTMakefile.PrerequisiteActions, Targets, out TargetToOutdatedPrerequisitesMap);

                            // Display some stats to the user.
                            Log.TraceVerbose(
                                    "{0} actions, {1} outdated and requested actions",
                                    ActionGraph.AllActions.Count,
                                    ActionsToExecute.Count
                                    );

                            ToolChain.PreBuildSync();

                    
                            // Cache indirect includes for all outdated C++ files.  We kick this off as a background thread so that it can
                            // perform the scan while we're compiling.  It usually only takes up to a few seconds, but we don't want to hurt
                            // our best case UBT iteration times for this task which can easily be performed asynchronously
                            if( BuildConfiguration.bUseUBTMakefiles && TargetToOutdatedPrerequisitesMap.Count > 0 )
                            {
                                CPPIncludesThread = CreateThreadForCachingCPPIncludes( TargetToOutdatedPrerequisitesMap );
                                CPPIncludesThread.Start();
                            }

                            // Execute the actions.
                            bSuccess = ActionGraph.ExecuteActions(ActionsToExecute, out ExecutorName);

                            // if the build succeeded, write the receipts and do any needed syncing
                            if (bSuccess)
                            {
                                foreach (UEBuildTarget Target in Targets)
                                {
									Target.WriteReceipt();
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
			if( CPPIncludesThread != null )
			{ 
			    CPPIncludesThread.Join();
			}

            // Save the include dependency cache.
            { 
				// NOTE: It's very important that we save the include cache, even if a build exception was thrown (compile error, etc), because we need to make sure that
				//    any C++ include dependencies that we computed for out of date source files are saved.  Remember, the build may fail *after* some build products
				//    are successfully built.  If we didn't save our dependency cache after build failures, source files for those build products that were successsfully
				//    built before the failure would not be considered out of date on the next run, so this is our only chance to cache C++ includes for those files!

                foreach( var DependencyCache in CPPEnvironment.IncludeDependencyCache.Values )
                { 
                    DependencyCache.Save();
                }
                CPPEnvironment.IncludeDependencyCache.Clear();

                foreach( var FlatCPPIncludeDependencyCache in CPPEnvironment.FlatCPPIncludeDependencyCache.Values )
                { 
                    FlatCPPIncludeDependencyCache.Save();
                }
                CPPEnvironment.FlatCPPIncludeDependencyCache.Clear();
            }

            if(EULAViolationWarning != null)
            {
                Log.TraceWarning("WARNING: {0}", EULAViolationWarning);
            }

            // Figure out how long we took to execute.
            double BuildDuration = (DateTime.UtcNow - StartTime - MutexWaitTime).TotalSeconds;
            if (ExecutorName == "Local" || ExecutorName == "Distcc" || ExecutorName == "SNDBS")
            {
                Log.WriteLineIf(BuildConfiguration.bLogDetailedActionStats || BuildConfiguration.bPrintDebugInfo,
					TraceEventType.Information, 
					"Cumulative action seconds ({0} processors): {1:0.00} building projects, {2:0.00} compiling, {3:0.00} creating app bundles, {4:0.00} generating debug info, {5:0.00} linking, {6:0.00} other",
                    System.Environment.ProcessorCount,
                    TotalBuildProjectTime,
                    TotalCompileTime,
                    TotalCreateAppBundleTime,
                    TotalGenerateDebugInfoTime,
                    TotalLinkTime,
                    TotalOtherActionsTime
                );
                Telemetry.SendEvent("BuildStatsTotal.2",
                    "ExecutorName", ExecutorName,
                    "TotalUBTWallClockTimeSec", BuildDuration.ToString("0.00"),
                    "TotalBuildProjectThreadTimeSec", TotalBuildProjectTime.ToString("0.00"),
                    "TotalCompileThreadTimeSec", TotalCompileTime.ToString("0.00"),
                    "TotalCreateAppBundleThreadTimeSec", TotalCreateAppBundleTime.ToString("0.00"),
                    "TotalGenerateDebugInfoThreadTimeSec", TotalGenerateDebugInfoTime.ToString("0.00"),
                    "TotalLinkThreadTimeSec", TotalLinkTime.ToString("0.00"),
                    "TotalOtherActionsThreadTimeSec", TotalOtherActionsTime.ToString("0.00")
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
                Telemetry.SendEvent("BuildStatsTotal.2",
                    "ExecutorName", ExecutorName,
                    "TotalUBTWallClockTimeSec", BuildDuration.ToString("0.00")
                    );
            }

            return BuildResult;
        }

		/// <summary>
		/// Invalidates makefiles for given target.
		/// </summary>
		/// <param name="Target">Target</param>
		private static void InvalidateMakefiles(TargetDescriptor Target)
		{
			var MakefileNames = new string[] { "HotReloadMakefile.ubt", "Makefile.ubt" };
			var BaseDir = GetUBTMakefileDirectoryPathForSingleTarget(Target);

			foreach (var MakefileName in MakefileNames)
			{
				var MakefilePath = Path.Combine(BaseDir, MakefileName);

				if (File.Exists(MakefilePath))
				{
					File.Delete(MakefilePath);
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
			bool bIsEditorTarget = TargetDesc.TargetName.EndsWith( "Editor", StringComparison.InvariantCultureIgnoreCase ) || TargetDesc.bIsEditorRecompile;	

			if (!ProjectFileGenerator.bGenerateProjectFiles && !UEBuildConfiguration.bGenerateManifest && bIsEditorTarget )
			{
				var EditorProcessFilenames = UEBuildTarget.MakeExecutablePaths("..", "UE4Editor", TargetDesc.Platform, TargetDesc.Configuration, UnrealTargetConfiguration.Development, false, null);
				if (EditorProcessFilenames.Length != 1)
				{
					throw new BuildException("ShouldDoHotReload cannot handle multiple binaries returning from UEBuildTarget.MakeExecutablePaths");
				}

				var EditorProcessFilename = EditorProcessFilenames[0];
				var EditorProcessName = Path.GetFileNameWithoutExtension(EditorProcessFilename);
				var EditorProcesses = BuildHostPlatform.Current.GetProcessesByName(EditorProcessName);
				var BinariesPath = Path.GetFullPath(Path.GetDirectoryName(EditorProcessFilename));
				var PerProjectBinariesPath = Path.Combine(UnrealBuildTool.GetUProjectPath(), BinariesPath.Substring(BinariesPath.LastIndexOf("Binaries")));
				bIsRunning = EditorProcesses.FirstOrDefault(EditorProc =>
					{
						if(!Path.GetFullPath(EditorProc.Filename).StartsWith(BinariesPath, StringComparison.InvariantCultureIgnoreCase))
						{
							return false;
						}

						if(PerProjectBinariesPath.Equals(BinariesPath, StringComparison.InvariantCultureIgnoreCase))
						{
							return false;
						}

						var Modules = BuildHostPlatform.Current.GetProcessModules(EditorProc.PID, EditorProc.Filename);
						if(!Modules.Any(Module => Module.StartsWith(PerProjectBinariesPath)))
						{
							return false;
						}

						return true;
					}) != default(BuildHostPlatform.ProcessInfo);
			}
			return bIsRunning;
		}

        private static void ParseBuildConfigurationFlags(string[] Arguments)
        {
            int ArgumentIndex = 0;

            // Parse optional command-line flags.
            if (Utils.ParseCommandLineFlag(Arguments, "-verbose", out ArgumentIndex))
            {
                BuildConfiguration.bPrintDebugInfo = true;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-noxge", out ArgumentIndex))
            {
                BuildConfiguration.bAllowXGE = false;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-noxgemonitor", out ArgumentIndex))
            {
                BuildConfiguration.bShowXGEMonitor = false;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-stresstestunity", out ArgumentIndex))
            {
                BuildConfiguration.bStressTestUnity = true;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-disableunity", out ArgumentIndex))
            {
                BuildConfiguration.bUseUnityBuild = false;
            }
            if (Utils.ParseCommandLineFlag(Arguments, "-forceunity", out ArgumentIndex))
            {
                BuildConfiguration.bForceUnityBuild = true;
            }

            // New Monolithic Graphics drivers have optional "fast calls" replacing various D3d functions
            if (Utils.ParseCommandLineFlag(Arguments, "-fastmonocalls", out ArgumentIndex))
            {
                BuildConfiguration.bUseFastMonoCalls = true;
            }
            if (Utils.ParseCommandLineFlag(Arguments, "-nofastmonocalls", out ArgumentIndex))
            {
                BuildConfiguration.bUseFastMonoCalls = false;
            }
            
            if (Utils.ParseCommandLineFlag(Arguments, "-uniqueintermediate", out ArgumentIndex))
            {
                BuildConfiguration.BaseIntermediateFolder = "Intermediate/Build/" + BuildGuid + "/";
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-nopch", out ArgumentIndex))
            {
                BuildConfiguration.bUsePCHFiles = false;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-nosharedpch", out ArgumentIndex))
            {
                BuildConfiguration.bUseSharedPCHs = false;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-skipActionHistory", out ArgumentIndex))
            {
                BuildConfiguration.bUseActionHistory = false;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-noLTCG", out ArgumentIndex))
            {
                BuildConfiguration.bAllowLTCG = false;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-nopdb", out ArgumentIndex))
            {
                BuildConfiguration.bUsePDBFiles = false;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-deploy", out ArgumentIndex))
            {
                BuildConfiguration.bDeployAfterCompile = true;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-CopyAppBundleBackToDevice", out ArgumentIndex))
            {
                BuildConfiguration.bCopyAppBundleBackToDevice = true;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-flushmac", out ArgumentIndex))
            {
                BuildConfiguration.bFlushBuildDirOnRemoteMac = true;
            }

            if (Utils.ParseCommandLineFlag(Arguments, "-nodebuginfo", out ArgumentIndex))
            {
                BuildConfiguration.bDisableDebugInfo = true;
            }
            else if (Utils.ParseCommandLineFlag(Arguments, "-forcedebuginfo", out ArgumentIndex))
            {
                BuildConfiguration.bDisableDebugInfo = false;
                BuildConfiguration.bOmitPCDebugInfoInDevelopment = false;
            }
        }

        /**
         * Parses the passed in command line for build configuration overrides.
         * 
         * @param	Arguments	List of arguments to parse
         * @return	List of build target settings
         */
        private static List<string[]> ParseCommandLineFlags(string[] Arguments)
        {
            var TargetSettings = new List<string[]>();
            int ArgumentIndex = 0;

            // Log command-line arguments.
            if (BuildConfiguration.bPrintDebugInfo)
            {
                Console.Write("Command-line arguments: ");
                foreach (string Argument in Arguments)
                {
                    Console.Write("{0} ", Argument);
                }
                Log.TraceInformation("");
            }

            ParseBuildConfigurationFlags(Arguments);

            if (Utils.ParseCommandLineFlag(Arguments, "-targets", out ArgumentIndex))
            {
                if (ArgumentIndex + 1 >= Arguments.Length)
                {
                    throw new BuildException("Expected filename after -targets argument, but found nothing.");
                }
                // Parse lines from the referenced file into target settings.
                var Lines = File.ReadAllLines(Arguments[ArgumentIndex + 1]);
                foreach (string Line in Lines)
                {
                    if (Line != "" && Line[0] != ';')
                    {
                        TargetSettings.Add(Line.Split(' '));
                    }
                }
            }
            // Simply use full command line arguments as target setting if not otherwise overriden.
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
        private static Thread CreateThreadForCachingCPPIncludes( Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap)
        {
            return new Thread( new ThreadStart( () =>
                {
                    // @todo ubtmake: This thread will access data structures that are also used on the main UBT thread, but during this time UBT
                    // is only invoking the build executor, so should not be touching this stuff.  However, we need to at some guards to make sure.

                    foreach( var TargetAndOutdatedPrerequisites in TargetToOutdatedPrerequisitesMap )
                    {
                        var Target = TargetAndOutdatedPrerequisites.Key;
                        var OutdatedPrerequisites = TargetAndOutdatedPrerequisites.Value;

                        foreach( var PrerequisiteItem in OutdatedPrerequisites )
                        {
                            // Invoke our deep include scanner to figure out whether any of the files included by this source file have
                            // changed since the build product was built
                            var FileBuildPlatform = UEBuildPlatform.GetBuildPlatform( Target.Platform );
                            CPPEnvironment.FindAndCacheAllIncludedFiles( Target, PrerequisiteItem, FileBuildPlatform, PrerequisiteItem.CachedCPPIncludeInfo, bOnlyCachedDependencies:false );
                        }
                    }
                } ) );
        }



        /// <summary>
        /// A special Makefile that UBT is able to create in "-gather" mode, then load in "-assemble" mode to accelerate iterative compiling and linking
        /// </summary>
        [Serializable]
        class UBTMakefile
        {
            /** Every action in the action graph */
            public List<Action> AllActions;

            /** List of the actions that need to be run in order to build the targets' final output items */
            public Action[] PrerequisiteActions;			

            /** Environment variables that we'll need in order to invoke the platform's compiler and linker */
			// @todo ubtmake: Really we want to allow a different set of environment variables for every Action.  This would allow for targets on multiple platforms to be built in a single assembling phase.  We'd only have unique variables for each platform that has actions, so we'd want to make sure we only store the minimum set.
            public readonly List<Tuple<string,string>> EnvironmentVariables = new List<Tuple<string,string>>();

            /** Maps each target to a list of UObject module info structures */
            public Dictionary<string,List<UHTModuleInfo>> TargetNameToUObjectModules;

			/** List of targets being built */
			public List<UEBuildTarget> Targets;


            /** @return	 True if this makefile's contents look valid.  Called after loading the file to make sure it is legit. */
            public bool IsValidMakefile()
            {
                return 
                    AllActions != null && AllActions.Count > 0 &&
                    PrerequisiteActions != null && PrerequisiteActions.Length > 0 &&
                    EnvironmentVariables != null &&
                    TargetNameToUObjectModules != null && TargetNameToUObjectModules.Count > 0 &&
					Targets != null && Targets.Count > 0;
            }
        }


        /// <summary>
        /// Saves a UBTMakefile to disk
        /// </summary>
        /// <param name="TargetDescs">List of targets.  Order is not important</param>
        static void SaveUBTMakefile( List<TargetDescriptor> TargetDescs, UBTMakefile UBTMakefile )
        {
            if( !UBTMakefile.IsValidMakefile() )
            {
                throw new BuildException( "Can't save a makefile that has invalid contents.  See UBTMakefile.IsValidMakefile()" );
            }

            var TimerStartTime = DateTime.UtcNow;

            var UBTMakefileItem = FileItem.GetItemByFullPath( GetUBTMakefilePath( TargetDescs ) );

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
                var TimerDuration = DateTime.UtcNow - TimerStartTime;
                Log.TraceInformation("Saving makefile took " + TimerDuration.TotalSeconds + "s");
            }
        }


        /// <summary>
        /// Loads a UBTMakefile from disk
        /// </summary>
        /// <param name="TargetDescs">List of targets.  Order is not important</param>
		/// <param name="ReasonNotLoaded">If the function returns null, this string will contain the reason why</param>
		/// <returns>The loaded makefile, or null if it failed for some reason.  On failure, the 'ReasonNotLoaded' variable will contain information about why</returns>
        static UBTMakefile LoadUBTMakefile( List<TargetDescriptor> TargetDescs, out string ReasonNotLoaded )
        {
            var UBTMakefileItem = FileItem.GetItemByFullPath( GetUBTMakefilePath( TargetDescs ) );
			ReasonNotLoaded = null;

            // Check the directory timestamp on the project files directory.  If the user has generated project files more
            // recently than the UBTMakefile, then we need to consider the file to be out of date
            bool bForceOutOfDate = false;
            if( UBTMakefileItem.bExists )
            {
                // @todo ubtmake: This will only work if the directory timestamp actually changes with every single GPF.  Force delete existing files before creating new ones?  Eh... really we probably just want to delete + create a file in that folder
                //			-> UPDATE: Seems to work OK right now though on Windows platform, maybe due to GUID changes
                // @todo ubtmake: Some platforms may not save any files into this folder.  We should delete + generate a "touch" file to force the directory timestamp to be updated (or just check the timestamp file itself.  We could put it ANYWHERE, actually)

				if( !UnrealBuildTool.RunningRocket() )
                {
                    if( Directory.Exists( ProjectFileGenerator.IntermediateProjectFilesPath ) )
                    {
                        var EngineProjectFilesLastUpdateTime = new FileInfo(ProjectFileGenerator.ProjectTimestampFile).LastWriteTime;
						if( UBTMakefileItem.LastWriteTime.CompareTo( EngineProjectFilesLastUpdateTime ) < 0 )
						{
							// Engine project files are newer than UBTMakefile
							bForceOutOfDate = true;
							Log.TraceVerbose("Existing makefile is older than generated engine project files, ignoring it" );

							ReasonNotLoaded = "project files are newer";
						}
                    }
                }
                else
                {
                    // Rocket doesn't need to check engine projects for outdatedness
                }

				if( !bForceOutOfDate )
				{
					// Check the game project directory too
					if( UnrealBuildTool.HasUProjectFile() )
					{ 
						var MasterProjectRelativePath = UnrealBuildTool.GetUProjectPath();
						var GameIntermediateProjectFilesPath = Path.Combine( MasterProjectRelativePath, "Intermediate", "ProjectFiles" );
						if( Directory.Exists( GameIntermediateProjectFilesPath ) )
						{
							var GameProjectFilesLastUpdateTime = new DirectoryInfo( GameIntermediateProjectFilesPath ).LastWriteTime;
							if( UBTMakefileItem.LastWriteTime.CompareTo( GameProjectFilesLastUpdateTime ) < 0 )
							{
								// Game project files are newer than UBTMakefile
								bForceOutOfDate = true;
								Log.TraceVerbose("Makefile is older than generated game project files, ignoring it" );

								ReasonNotLoaded = "game project files are newer";
							}
						}	
					}
				}

				if( !bForceOutOfDate )
				{
					// Check to see if UnrealBuildTool.exe was compiled more recently than the UBTMakefile
					var UnrealBuildToolTimestamp = new FileInfo( Assembly.GetExecutingAssembly().Location ).LastWriteTime;
					if( UBTMakefileItem.LastWriteTime.CompareTo( UnrealBuildToolTimestamp ) < 0 )
					{
						// UnrealBuildTool.exe was compiled more recently than the UBTMakefile
						Log.TraceVerbose("Makefile is older than UnrealBuildTool.exe, ignoring it" );

						ReasonNotLoaded = "UnrealBuildTool.exe is newer";
						bForceOutOfDate = true;
					}
				}
			}
			else
			{
				// UBTMakefile doesn't even exist, so we won't bother loading it
				bForceOutOfDate = true;

				ReasonNotLoaded = "no existing makefile";
			}



            UBTMakefile LoadedUBTMakefile = null;
            if( !bForceOutOfDate )
            { 
                try
                {
					var LoadUBTMakefileStartTime = DateTime.UtcNow;

                    using (FileStream Stream = new FileStream(UBTMakefileItem.AbsolutePath, FileMode.Open, FileAccess.Read))
                    {	
                        BinaryFormatter Formatter = new BinaryFormatter();
                        LoadedUBTMakefile = Formatter.Deserialize(Stream) as UBTMakefile;
                    }

					if( BuildConfiguration.bPrintPerformanceInfo )
					{ 
						var LoadUBTMakefileTime = (DateTime.UtcNow - LoadUBTMakefileStartTime).TotalSeconds;
						Log.TraceInformation( "LoadUBTMakefile took " + LoadUBTMakefileTime + "s" );
					}
                }
                catch (Exception Ex)
                {
                    Log.TraceWarning("Failed to read makefile: {0}", Ex.Message);
					ReasonNotLoaded = "couldn't read existing makefile";
                }

                if( LoadedUBTMakefile != null && !LoadedUBTMakefile.IsValidMakefile() )
                {
                    Log.TraceWarning("Loaded makefile appears to have invalid contents, ignoring it ({0})", UBTMakefileItem.AbsolutePath );
                    LoadedUBTMakefile = null;
					ReasonNotLoaded = "existing makefile appears to be invalid";
                }

				if( LoadedUBTMakefile != null )
				{
					// Check if any of the target's Build.cs files are newer than the makefile
					foreach (var Target in LoadedUBTMakefile.Targets)
					{
						string TargetCsFilename = Target.TargetCsFilename;
						if (TargetCsFilename != null)
						{
							var TargetCsFile = new FileInfo(TargetCsFilename);
							bool bTargetCsFileExists = TargetCsFile.Exists;
							if (!bTargetCsFileExists || TargetCsFile.LastWriteTime > UBTMakefileItem.LastWriteTime)
							{
								Log.TraceWarning("{0} has been {1} since makefile was built, ignoring it ({2})", TargetCsFilename, bTargetCsFileExists ? "changed" : "deleted", UBTMakefileItem.AbsolutePath);
								LoadedUBTMakefile = null;
								ReasonNotLoaded = string.Format("changes to target files");
								goto SkipRemainingTimestampChecks;
							}
						}

						List<string> BuildCsFilenames = Target.GetAllModuleBuildCsFilenames();
						foreach (var BuildCsFilename in BuildCsFilenames)
						{
							if (BuildCsFilename != null)
							{
								var BuildCsFile = new FileInfo(BuildCsFilename);
								bool bBuildCsFileExists = BuildCsFile.Exists;
								if (!bBuildCsFileExists || BuildCsFile.LastWriteTime > UBTMakefileItem.LastWriteTime)
								{
									Log.TraceWarning("{0} has been {1} since makefile was built, ignoring it ({2})", BuildCsFilename, bBuildCsFileExists ? "changed" : "deleted", UBTMakefileItem.AbsolutePath);
									LoadedUBTMakefile = null;
									ReasonNotLoaded = string.Format("changes to module files");
									goto SkipRemainingTimestampChecks;
								}
							}
						}
					}

				SkipRemainingTimestampChecks:;
				}
            }

            return LoadedUBTMakefile;
        }


        /// <summary>
        /// Gets the file path for a UBTMakefile
        /// </summary>
        /// <param name="Targets">List of targets.  Order is not important</param>
        /// <returns>UBTMakefile path</returns>
        public static string GetUBTMakefilePath( List<TargetDescriptor> TargetDescs )
        {
            string UBTMakefilePath;

            if( TargetDescs.Count == 1 )
            {
				var TargetDesc = TargetDescs[0];

				bool bIsHotReload = (UEBuildConfiguration.bHotReloadFromIDE || (TargetDesc.OnlyModules != null && TargetDesc.OnlyModules.Count > 0));
				string UBTMakefileName = bIsHotReload ? "HotReloadMakefile.ubt" : "Makefile.ubt";

				UBTMakefilePath = Path.Combine(GetUBTMakefileDirectoryPathForSingleTarget(TargetDesc), UBTMakefileName);
            }
            else
            {
                // For Makefiles that contain multiple targets, we'll make up a file name that contains all of the targets, their
                // configurations and platforms, and save it into the base intermediate folder
                var TargetCollectionName = MakeTargetCollectionName( TargetDescs );

                string ProjectIntermediatePath = BuildConfiguration.BaseIntermediatePath;
                if (UnrealBuildTool.HasUProjectFile())
                {
                    ProjectIntermediatePath = Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.BaseIntermediatePath);
                }

                // @todo ubtmake: The TargetCollectionName string could be really long if there is more than one target!  Hash it?
                UBTMakefilePath = Path.Combine(ProjectIntermediatePath, TargetCollectionName + ".ubt" );
            }

            return UBTMakefilePath;
        }

		/// <summary>
		/// Gets the file path for a UBTMakefile for single target.
		/// </summary>
		/// <param name="Target">The target.</param>
		/// <returns>UBTMakefile path</returns>
		private static string GetUBTMakefileDirectoryPathForSingleTarget(TargetDescriptor Target)
		{
			// If there's only one target, just save the UBTMakefile in the target's build intermediate directory
			// under a folder for that target (and platform/config combo.)
			string PlatformIntermediatePath = BuildConfiguration.PlatformIntermediatePath;
			if (UnrealBuildTool.HasUProjectFile())
			{
				PlatformIntermediatePath = Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.PlatformIntermediateFolder);
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

			return Path.Combine(PlatformIntermediatePath, TargetName, Target.Configuration.ToString());
		}

        /// <summary>
        /// Makes up a name for a set of targets that we can use for file or directory names
        /// </summary>
        /// <param name="TargetDescs">List of targets.  Order is not important</param>
        /// <returns>The name to use</returns>
        private static string MakeTargetCollectionName( List<TargetDescriptor> TargetDescs )
        {
            if( TargetDescs.Count == 0 )
            {
                throw new BuildException( "Expecting at least one Target to be passed to MakeTargetCollectionName" );
            }

            var SortedTargets = new List<TargetDescriptor>();
            SortedTargets.AddRange( TargetDescs );
            SortedTargets.Sort( ( x, y ) => { return x.TargetName.CompareTo( y.TargetName ); } );

            // Figure out what to call our action graph based on everything we're building
            var TargetCollectionName = new StringBuilder();
            foreach( var Target in SortedTargets )
            {
                if( TargetCollectionName.Length > 0 )
                {
                    TargetCollectionName.Append( "_" );
                }

				// @todo ubtmake: If this is a compile triggered from the editor it will have passed along the game's target name, not the editor target name.
				// At this point in Unreal Build Tool, we can't possibly know what the actual editor target name is, but we can take a guess.
				// Even if we get it wrong, this won't have any side effects aside from not being able to share the exact same cached UBT Makefile
				// between hot reloads invoked from the editor and hot reloads invoked from within the IDE.
				string TargetName = Target.TargetName;
				if( !TargetName.EndsWith( "Editor", StringComparison.InvariantCultureIgnoreCase ) && Target.bIsEditorRecompile )
				{
					TargetName += "Editor";
				}

				// @todo ubtmake: Should we also have the platform Architecture in this string?
				TargetCollectionName.Append( TargetName + "-" + Target.Platform.ToString() + "-" + Target.Configuration.ToString() );
            }

            return TargetCollectionName.ToString();
        }

		/// <summary>
		/// Sets up UBT when running from UAT
		/// </summary>
		/// <param name="UProjectDir"></param>
		public static void SetupUBTFromUAT(string UProjectFile)
		{
			// Reset project file
			SetProjectFile(String.Empty);
			// when running UAT, the working directory is the root UE4 dir
			BuildConfiguration.RelativeEnginePath = "Engine";
			SetProjectFile(UProjectFile);
		}


		/// <summary>
		/// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
		/// the same for every invocation on that makefile, but we need a new suffix each time.
		/// </summary>
		protected static void PatchActionHistoryForHotReloadAssembling( List<OnlyModule> OnlyModules )
		{
			// Gather all of the response files for link actions.  We're going to need to patch 'em up after we figure out new
			// names for all of the output files and import libraries
			var ResponseFilePaths = new List<string>();

			// Keep a map of the original file names and their new file names, so we can fix up response files after
			var OriginalFileNameAndNewFileNameList_NoExtensions = new List<Tuple<string, string>>();

			// Finally, we'll keep track of any file items that we had to create counterparts for change file names, so we can fix those up too
			var AffectedOriginalFileItemAndNewFileItemMap = new Dictionary<FileItem,FileItem>();

			foreach( var Action in ActionGraph.AllActions )
			{
				if( Action.ActionType == ActionType.Link )
				{
					// Assume that the first produced item (with no extension) is our output file name
					var OriginalFileNameWithoutExtension = Utils.GetFilenameWithoutAnyExtensions( Action.ProducedItems[0].AbsolutePath );

					// Remove the numbered suffix from the end of the file
					var IndexOfLastHyphen = OriginalFileNameWithoutExtension.LastIndexOf( '-' );
					var OriginalNumberSuffix = OriginalFileNameWithoutExtension.Substring( IndexOfLastHyphen );
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
						// Remove "-####-Platform-Configuration" suffix in Debug configuration
						var IndexOfSecondLastHyphen = OriginalFileNameWithoutExtension.LastIndexOf('-', IndexOfLastHyphen - 1, IndexOfLastHyphen);
						var IndexOfThirdLastHyphen = OriginalFileNameWithoutExtension.LastIndexOf('-', IndexOfSecondLastHyphen - 1, IndexOfSecondLastHyphen);
						OriginalFileNameWithoutNumberSuffix = OriginalFileNameWithoutExtension.Substring(0, IndexOfThirdLastHyphen);
						PlatformConfigSuffix = OriginalFileNameWithoutExtension.Substring(IndexOfSecondLastHyphen);
					}

					// Figure out which suffix to use
					string UniqueSuffix = null;
					{ 
						int FirstHyphenIndex = OriginalFileNameWithoutNumberSuffix.IndexOf( '-' );
						if( FirstHyphenIndex == -1 )
						{
							throw new BuildException( "Expected produced item file name '{0}' to start with a prefix (such as 'UE4Editor-') when hot reloading", OriginalFileNameWithoutExtension );
						}
						string OutputFileNamePrefix = OriginalFileNameWithoutNumberSuffix.Substring( 0, FirstHyphenIndex + 1 );

						// Get the module name from the file name
						string ModuleName = OriginalFileNameWithoutNumberSuffix.Substring( OutputFileNamePrefix.Length );

						// Add a new random suffix
						if( OnlyModules != null )
						{ 
							foreach( var OnlyModule in OnlyModules )
							{
								if( OnlyModule.OnlyModuleName.Equals( ModuleName, StringComparison.InvariantCultureIgnoreCase ) )
								{
									// OK, we found the module!
									UniqueSuffix = "-" + OnlyModule.OnlyModuleSuffix;
									break;
								}
							}
						}
						if( UniqueSuffix == null )
						{
							UniqueSuffix = "-" + (new Random((int)(DateTime.Now.Ticks % Int32.MaxValue)).Next(10000)).ToString();
						}
					}
					var NewFileNameWithoutExtension = OriginalFileNameWithoutNumberSuffix + UniqueSuffix + PlatformConfigSuffix;

					// Find the response file in the command line.  We'll need to make a copy of it with our new file name.
					{
						string ResponseFileExtension = ".response";
						var ResponseExtensionIndex = Action.CommandArguments.IndexOf( ResponseFileExtension, StringComparison.InvariantCultureIgnoreCase );
						if( ResponseExtensionIndex != -1 )
						{ 
							var ResponseFilePathIndex = Action.CommandArguments.LastIndexOf( "@\"", ResponseExtensionIndex );
							if( ResponseFilePathIndex == -1 )
							{
								throw new BuildException( "Couldn't find response file path in action's command arguments when hot reloading" );
							}

							var OriginalResponseFilePathWithoutExtension = Action.CommandArguments.Substring( ResponseFilePathIndex + 2, ( ResponseExtensionIndex - ResponseFilePathIndex ) - 2 );
							var OriginalResponseFilePath = OriginalResponseFilePathWithoutExtension + ResponseFileExtension;

							var NewResponseFilePath = OriginalResponseFilePath.Replace( OriginalFileNameWithoutExtension, NewFileNameWithoutExtension );

							// Copy the old response file to the new path
							File.Copy( OriginalResponseFilePath, NewResponseFilePath, overwrite: true);

							// Keep track of the new response file name.  We'll have to do some edits afterwards.
							ResponseFilePaths.Add( NewResponseFilePath );
						}
					}


					// Go ahead and replace all occurrences of our file name in the command-line (ignoring extensions)
					Action.CommandArguments = Action.CommandArguments.Replace( OriginalFileNameWithoutExtension, NewFileNameWithoutExtension );

					
					// Update this action's list of produced items too
					{
						for( int ItemIndex = 0; ItemIndex < Action.ProducedItems.Count; ++ItemIndex )
						{
							var OriginalProducedItem = Action.ProducedItems[ ItemIndex ];

							var OriginalProducedItemFilePath = OriginalProducedItem.AbsolutePath;
							var NewProducedItemFilePath = OriginalProducedItemFilePath.Replace( OriginalFileNameWithoutExtension, NewFileNameWithoutExtension );
						
							if( OriginalProducedItemFilePath != NewProducedItemFilePath )
							{
								// OK, the produced item's file name changed so we'll update it to point to our new file
								var NewProducedItem = FileItem.GetItemByFullPath( NewProducedItemFilePath );
								Action.ProducedItems[ ItemIndex ] = NewProducedItem;

								// Copy the other important settings from the original file item
								NewProducedItem.bNeedsHotReloadNumbersDLLCleanUp = OriginalProducedItem.bNeedsHotReloadNumbersDLLCleanUp;
								NewProducedItem.ProducingAction = OriginalProducedItem.ProducingAction;
								NewProducedItem.bIsRemoteFile = OriginalProducedItem.bIsRemoteFile;

								// Keep track of it so we can fix up dependencies in a second pass afterwards
								AffectedOriginalFileItemAndNewFileItemMap.Add( OriginalProducedItem, NewProducedItem );
							}
						}
					}

					// The status description of the item has the file name, so we'll update it too
					Action.StatusDescription = Action.StatusDescription.Replace( OriginalFileNameWithoutExtension, NewFileNameWithoutExtension );

		
					// Keep track of the file names, so we can fix up response files afterwards.
					OriginalFileNameAndNewFileNameList_NoExtensions.Add( Tuple.Create( OriginalFileNameWithoutExtension, NewFileNameWithoutExtension ) );
				}
			}

			
			// Do another pass and update any actions that depended on the original file names that we changed
			foreach( var Action in ActionGraph.AllActions )
			{
				for( int ItemIndex = 0; ItemIndex < Action.PrerequisiteItems.Count; ++ItemIndex )
				{
					var OriginalFileItem = Action.PrerequisiteItems[ ItemIndex ];

					FileItem NewFileItem;
					if( AffectedOriginalFileItemAndNewFileItemMap.TryGetValue( OriginalFileItem, out NewFileItem ) )
					{
						// OK, looks like we need to replace this file item because we've renamed the file
						Action.PrerequisiteItems[ ItemIndex ] = NewFileItem;
					}
				}
			}


			if( OriginalFileNameAndNewFileNameList_NoExtensions.Count > 0 )
			{	
				foreach( var ResponseFilePath in ResponseFilePaths )
				{
					// Load the file up
					StringBuilder FileContents = new StringBuilder( Utils.ReadAllText( ResponseFilePath ) );

					// Replace all of the old file names with new ones
					foreach( var FileNameTuple in OriginalFileNameAndNewFileNameList_NoExtensions )
					{
						var OriginalFileNameWithoutExtension = FileNameTuple.Item1;
						var NewFileNameWithoutExtension = FileNameTuple.Item2;

						FileContents.Replace( OriginalFileNameWithoutExtension, NewFileNameWithoutExtension );
					}

					// Overwrite the original file
					var FileContentsString = FileContents.ToString();
					File.WriteAllText( ResponseFilePath, FileContentsString, System.Text.Encoding.UTF8 );
				}
			}
		}
    }
}
