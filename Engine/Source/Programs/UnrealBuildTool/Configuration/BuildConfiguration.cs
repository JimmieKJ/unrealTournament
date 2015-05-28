// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Xml;
using System.Reflection;

namespace UnrealBuildTool
{
	public class BuildConfiguration
	{
		/// <summary>
		/// Whether to unify C++ code into larger files for faster compilation.
		/// </summary>
		[XmlConfig]
		public static bool bUseUnityBuild;
        /// <summary>
        /// Whether to _force_ unify C++ code into larger files for faster compilation.
        /// </summary>
		[XmlConfig]
        public static bool bForceUnityBuild;

		/// <summary>
		/// The number of source files in a game module before unity build will be activated for that module.  This
		/// allows small game modules to have faster iterative compile times for single files, at the expense of slower full
		/// rebuild times.  This setting can be overridden by the bFasterWithoutUnity option in a module's Build.cs file.
		/// </summary>
		[XmlConfig]
		public static int MinGameModuleSourceFilesForUnityBuild;

		/// <summary>
		/// New Monolithic Graphics drivers have optional "fast calls" replacing various D3d functions
		/// </summary>
		[XmlConfig]
		public static bool bUseFastMonoCalls;

        /// <summary>
		/// An approximate number of bytes of C++ code to target for inclusion in a single unified C++ file.
		/// </summary>
		[XmlConfig]
		public static int NumIncludedBytesPerUnityCPP;

		/// <summary>
		/// Whether to stress test the C++ unity build robustness by including all C++ files files in a project from a single unified file.
		/// </summary>
		[XmlConfig]
		public static bool bStressTestUnity;

		/// <summary>
		/// Whether headers in system paths should be checked for modification when determining outdated actions.
		/// </summary>
		[XmlConfig]
		public static bool bCheckSystemHeadersForModification;

		/// <summary>
		/// Whether headers in the Development\External folder should be checked for modification when determining outdated actions.
		/// </summary>
		[XmlConfig]
		public static bool bCheckExternalHeadersForModification;

		/// <summary>
		/// Whether to ignore import library files that are out of date when building targets.  Set this to true to improve iteration time.
		/// </summary>
		[XmlConfig]
		public static bool bIgnoreOutdatedImportLibraries;

		/// <summary>
		/// Whether to globally disable debug info generation; see DebugInfoHeuristics.cs for per-config and per-platform options.
		/// </summary>
		[XmlConfig]
		public static bool bDisableDebugInfo;

		/// <summary>
		/// Whether to disable debug info on PC in development builds (for faster developer iteration, as link times are extremely fast with debug info disabled.)
		/// </summary>
		[XmlConfig]
		public static bool bOmitPCDebugInfoInDevelopment;

		/// <summary>
		/// Whether PDB files should be used for Visual C++ builds.
		/// </summary>
		[XmlConfig]
		public static bool bUsePDBFiles;

 		/// <summary>
 		/// Whether PCH files should be used.
		/// </summary>
		[XmlConfig]
		public static bool bUsePCHFiles;

		/// <summary>
		/// Whether to generate command line dependencies for compile actions when requested
		/// </summary>
		[XmlConfig]
		public static bool bUseActionHistory;

		/// <summary>
		/// The minimum number of files that must use a pre-compiled header before it will be created and used.
		/// </summary>
		[XmlConfig]
		public static int MinFilesUsingPrecompiledHeader;

		/// <summary>
		/// When enabled, a precompiled header is always generated for game modules, even if there are only a few source files
		/// in the module.  This greatly improves compile times for iterative changes on a few files in the project, at the expense of slower
		/// full rebuild times for small game projects.  This can be overridden by setting MinFilesUsingPrecompiledHeaderOverride in
		/// a module's Build.cs file
		/// </summary>
		[XmlConfig]
		public static bool bForcePrecompiledHeaderForGameModules;

		/// <summary>
		/// Whether debug info should be written to the console.
		/// </summary>
		[XmlConfig]
		public static bool bPrintDebugInfo;

		/// <summary>
		/// Prints performance diagnostics about include dependencies and other bits
		/// </summary>
		[XmlConfig]
		public static bool bPrintPerformanceInfo;

		/// <summary>
		/// Whether to log detailed action stats. This forces local execution.
		/// </summary>
		[XmlConfig]
		public static bool bLogDetailedActionStats;

		/// <summary>
		/// Whether to deploy the executable after compilation on platforms that require deployment.
		/// </summary>
		[XmlConfig]
		public static bool bDeployAfterCompile;

		/// <summary>
		/// Whether to clean Builds directory on a remote Mac before building
		/// </summary>
		[XmlConfig]
		public static bool bFlushBuildDirOnRemoteMac;

		/// <summary>
		/// Whether XGE may be used.
		/// </summary>
		[XmlConfig]
		public static bool bAllowXGE;

		/// <summary>
		/// Whether we should just export the XGE XML and pretend it succeeded
		/// </summary>
		[XmlConfig]
		public static bool bXGEExport;

		/// <summary>
		/// Whether to display the XGE build monitor.
		/// </summary>
		[XmlConfig]
		public static bool bShowXGEMonitor;

		/// <summary>
		/// When enabled, XGE will stop compiling targets after a compile error occurs.  Recommended, as it saves computing resources for others.
		/// </summary>
		[XmlConfig]
		public static bool bStopXGECompilationAfterErrors;

		/// <summary>
		/// When enabled, allows XGE to compile pre-compiled header files on remote machines.  Otherwise, PCHs are always generated locally.
		/// </summary>
		[XmlConfig]
		public static bool bAllowRemotelyCompiledPCHs;

        /// <summary>
        /// Whether SN-DBS may be used.
        /// </summary>
        [XmlConfig]
        public static bool bAllowSNDBS;

		/// <summary>
		/// Whether or not to delete outdated produced items.
		/// </summary>
		[XmlConfig]
		public static bool bShouldDeleteAllOutdatedProducedItems;

		/// <summary>
		/// Whether to use incremental linking or not.
		/// </summary>
		[XmlConfig]
		public static bool bUseIncrementalLinking;

		/// <summary>
		/// Whether to allow the use of LTCG (link time code generation) 
		/// </summary>
		[XmlConfig]
		public static bool bAllowLTCG;

		/// <summary>
		/// Whether to support edit and continue.  Only works on Microsoft compilers in 32-bit compiles.
		/// </summary>
		[XmlConfig]
		public static bool bSupportEditAndContinue;

		/// <summary>
		/// Whether to omit frame pointers or not. Disabling is useful for e.g. memory profiling on the PC
		/// </summary>
		[XmlConfig]
		public static bool bOmitFramePointers;
        
		/// <summary>
		/// Processor count multiplier for local execution. Can be below 1 to reserve CPU for other tasks.
		/// </summary>
		[XmlConfig]
		public static double ProcessorCountMultiplier;

		/// <summary>
		/// Maximum processor count for local execution. 
		/// </summary>
		[XmlConfig]
		public static int MaxProcessorCount;

		/// <summary>
		/// The intermediate folder - i.e. Intermediate/Build.
		/// </summary>
		[XmlConfig]
		public static string BaseIntermediateFolder;

		/// <summary>
		/// Relative root engine path.
		/// </summary>
		private static string _RelativeEnginePath = "../../Engine/";
		public static string RelativeEnginePath
		{
			get
			{
				return _RelativeEnginePath;
			}
			set
			{
				_RelativeEnginePath = value;
			}
		}

		/// <summary>
		/// The path to the intermediate folder, relative to Engine/Source.
		/// </summary>
		public static string BaseIntermediatePath
		{
			get
			{
				return RelativeEnginePath + BaseIntermediateFolder;
			}
		}

		/// <summary>
		/// The intermediate folder - i.e. Intermediate/Build/Platform[-Architecture] for the current platform
		/// </summary>
		private static string _PlatformIntermediateFolder = null;
		public static string PlatformIntermediateFolder
		{
			get
			{
				if (_PlatformIntermediateFolder == null)
				{
					throw new BuildException("Attempted to use PlatformIntermediateFolder before it was initialized");
				}
				return _PlatformIntermediateFolder;
			}
			set { _PlatformIntermediateFolder = value; }
		}

		/// <summary>
		/// The path to the platform intermediate folder, relative to Engine/Source.
		/// </summary>
		public static string PlatformIntermediatePath
		{
			get
			{
				return RelativeEnginePath + PlatformIntermediateFolder;
			}
		}

		/// <summary>
		/// Whether to generate a dSYM file or not.
		/// </summary>
		[XmlConfig]
		public static bool bGeneratedSYMFile;

		/// <summary>
		/// Whether to strip iOS symbols or not (implied by bGeneratedSYMFile).
		/// </summary>
		[XmlConfig]
		public static bool bStripSymbolsOnIOS;

		/// <summary>
		/// If true, then a stub IPA will be generated when compiling is done (minimal files needed for a valid IPA)
		/// </summary>
		[XmlConfig]
		public static bool bCreateStubIPA;

		/// <summary>
		/// If true, then a stub IPA will be generated when compiling is done (minimal files needed for a valid IPA)
		/// </summary>
		[XmlConfig]
		public static bool bCopyAppBundleBackToDevice;

		/// <summary>
		/// If true, then enable memory profiling in the build (defines USE_MALLOC_PROFILER=1 and forces bOmitFramePointers=false)
		/// </summary>
		[XmlConfig]
		public static bool bUseMallocProfiler;

		/// <summary>
		/// Enables code analysis mode.  Currently, this has specific requirements.  It only works on Windows
		/// platform with the MSVC compiler.  Also, it requires a version of the compiler that supports the
		/// /analyze option, such as Visual Studio 2013
		/// </summary>
		[XmlConfig]
		public static bool bEnableCodeAnalysis;

		/// <summary>
		/// Enables "Shared PCHs", an experimental feature which may significantly speed up compile times by attempting to
		/// share certain PCH files between modules that UBT detects is including those PCH's header files
		/// </summary>
		[XmlConfig]
		public static bool bUseSharedPCHs;

		/// <summary>
		/// Whether the dependency cache includes pre-resolved include locations so UBT doesn't have to re-resolve each include location just to check the timestamp.
		/// This is technically not fully correct because the dependency cache is global and each module could have a different set of include paths that could cause headers
		/// to resolve files differently. In practice this is not the case, and significantly speeds up UBT when nothing is to be done.
		/// </summary>
		[XmlConfig]
		public static bool bUseIncludeDependencyResolveCache;

		/// <summary>
		/// Used to test the dependency resolve cache. This will verify the resolve cache has no conflicts by resolving every time and checking against any previous resolve attempts.
		/// </summary>
		[XmlConfig]
		public static bool bTestIncludeDependencyResolveCache;

		/// <summary>
		/// True if we should compile Debug builds using Debug CRT libs and Debug third party dependencies.  Otherwise, we'll actually
		/// compile against the Release CRT but leave optimizations turned off, so that it is still easy to debug.  If you enable this
		/// setting, actual Debug versions of third party static libraries will be needed when compiling in Debug.
		/// </summary>
		[XmlConfig]
		public static bool bDebugBuildsActuallyUseDebugCRT;

		/// <summary>
		/// Tells the UBT to check if module currently being built is violating EULA.
		/// </summary>
		[XmlConfig]
		public static bool bCheckLicenseViolations;

		/// <summary>
		/// Tells the UBT to break build if module currently being built is violating EULA.
		/// </summary>
		[XmlConfig]
		public static bool bBreakBuildOnLicenseViolation;

		/// <summary>
		/// Enables support for very fast iterative builds by caching target data.  Turning this on causes Unreal Build Tool to emit
		/// 'UBT Makefiles' for targets when they're built the first time.  Subsequent builds will load these Makefiles and begin
		/// outdatedness checking and build invocation very quickly.  The caveat is that if source files are added or removed to
		/// the project, UBT will need to gather information about those in order for your build to complete successfully.  Currently,
		/// you must run the project file generator after adding/removing source files to tell UBT to re-gather this information.
		/// 
		/// Events that can invalidate the 'UBT Makefile':  
		///		- Adding/removing .cpp files
		///		- Adding/removing .h files with UObjects
		///		- Adding new UObject types to a file that didn't previously have any
		///		- Changing global build settings (most settings in this file qualify.)
		///		- Changed code that affects how Unreal Header Tool works
		///	
		///	You can force regeneration of the 'UBT Makefile' by passing the '-gather' argument, or simply regenerating project files
		///
		///	This also enables the fast include file dependency scanning and caching system that allows Unreal Build Tool to detect out 
		/// of date dependencies very quickly.  When enabled, a deep C++ include graph does not have to be generated, and instead
		/// we only scan and cache indirect includes for after a dependent build product was already found to be out of date.  During the
		/// next build, we'll load those cached indirect includes and check for outdatedness.
		/// </summary>
		[XmlConfig]
		public static bool bUseUBTMakefiles;
	
		/// <summary>
		/// Whether DMUCS/Distcc may be used.
		/// </summary>
		[XmlConfig]
		public static bool bAllowDistcc;

		/// <summary>
		/// When enabled allows DMUCS/Distcc to fallback to local compilation when remote compiling fails. Defaults to true as separation of pre-process and compile stages can introduce non-fatal errors.
		/// </summary>
		[XmlConfig]
		public static bool bAllowDistccLocalFallback;

		/// <summary>
		/// Path to the Distcc & DMUCS executables.
		/// </summary>
		[XmlConfig]
		public static string DistccExecutablesPath;

		/// <summary>
		/// Run UnrealCodeAnalyzer instead of regular compilation.
		/// </summary>
		[XmlConfig]
		public static bool bRunUnrealCodeAnalyzer;

		/// <summary>
		/// Percentage of cpp files in module where header must be included to get included in PCH.
		/// </summary>
		[XmlConfig]
		public static float UCAUsageThreshold;

		/// <summary>
		/// If specified, UnrealCodeAnalyzer will analyze only specific module.
		/// </summary>
		[XmlConfig]
		public static string UCAModuleToAnalyze;

		/// <summary>
		/// Sets the configuration back to defaults.
		/// </summary>
		public static void LoadDefaults()
		{
			bAllowLTCG = false;
			bAllowRemotelyCompiledPCHs = false;
			bAllowXGE = true;
            bAllowSNDBS = true;

			// Don't bother to check external (stable) headers for modification.  It slows down UBT's dependency checking.
			bCheckExternalHeadersForModification = false;
			bCheckSystemHeadersForModification = false;

			bCopyAppBundleBackToDevice = false;
			bCreateStubIPA = true;
			bDeployAfterCompile = false;
			bDisableDebugInfo = false;
			bEnableCodeAnalysis = false;
			bFlushBuildDirOnRemoteMac = false;
			bGeneratedSYMFile = false;
			bStripSymbolsOnIOS = bGeneratedSYMFile;

			// By default we don't bother relinking targets if only a dependent .lib has changed, as chances are that
			// the import library wasn't actually different unless a dependent header file of this target was also changed,
			// in which case the target would be rebuilt automatically.
			bIgnoreOutdatedImportLibraries = true;

			bLogDetailedActionStats = false;
			bOmitFramePointers = true;
			bOmitPCDebugInfoInDevelopment = false;
			bPrintDebugInfo = false;
			bPrintPerformanceInfo = false;
			bShouldDeleteAllOutdatedProducedItems = false;
			bShowXGEMonitor = false;
			bStopXGECompilationAfterErrors = true;
			bStressTestUnity = false;
			bSupportEditAndContinue = false;
			bUseActionHistory = true;

			// Incremental linking can yield faster iteration times when making small changes
			// NOTE: We currently don't use incremental linking because it tends to behave a bit buggy on some computers (PDB-related compile errors)
			bUseIncrementalLinking = false;

			bUseMallocProfiler = false;
			bUsePDBFiles = false;
			bUsePCHFiles = true;

			// Shared PCHs are necessary to avoid generating tons of private PCH files for every single little module.  While
			// these private PCHs do yield fastest incremental compiles for single files, it causes full rebuilds to
			// take an inordinate amount of time, and intermediates will use up many gigabytes of disk space.
			bUseSharedPCHs = true;

			// Using unity build to coalesce source files allows for much faster full rebuild times.  For fastest iteration times on single-file
			// changes, consider turning this off.  In some cases, unity builds can cause linker errors after iterative changes to files, because
			// the synthesized file names for unity code files may change between builds.
			bUseUnityBuild = true;

			bForceUnityBuild = false;

			// For programmers working on gameplay code, it's convenient to have fast iteration times when changing only single source
			// files.  We've set this default such that smaller gameplay modules will not use the unity build feature, allowing them
			// the fastest possible iterative compile times at the expense of slower full rebuild times.  For larger game modules, unity
			// build will automatically be activated.
			MinGameModuleSourceFilesForUnityBuild = 32;

			// This controls how big unity files are.  Use smaller values for faster iterative iteration times at the cost of slower full rebuild times.
			// This setting can greatly affect how effective parallelized compilation can be.
			NumIncludedBytesPerUnityCPP = 384 * 1024;

			// This sets the number of source files (post-unity-combine) in a module before we'll bother to generate a precompiled header
			// for that module.  If you want the fastest iterative builds for non-header changes (at the expense of slower full rebuild times),
			// set this to value to 1
			MinFilesUsingPrecompiledHeader = 6;

			// We want iterative changes to gameplay modules to be very fast!  This makes full rebuilds for small game projects a bit slower, 
			// but changes to individual code files will compile quickly.
			bForcePrecompiledHeaderForGameModules = true;

			// When using the local executor (not XGE), run a single action on each CPU core.  Note that you can set this to a larger value
			// to get slightly faster build times in many cases, but your computer's responsiveness during compiling may be much worse.
			ProcessorCountMultiplier = 1.0;

			MaxProcessorCount = int.MaxValue;

			bTestIncludeDependencyResolveCache = false;
			// if we are testing the resolve cache, we require UBT to use it.
			bUseIncludeDependencyResolveCache = true;

			//IMPORTANT THIS IS THE MAIN SWITCH FOR MONO FAST CALLS
			//  if this is set to true, then fast calls will be on by default on Dingo, and if false it will be off by default on Dingo.
			//  This can be overridden by -fastmonocalls  or -nofastmonocalls in the NMAKE params.
			bUseFastMonoCalls = false;

			// By default we use the Release C++ Runtime (CRT), even when compiling Debug builds.  This is because the Debug C++
			// Runtime isn't very useful when debugging Unreal Engine projects, and linking against the Debug CRT libraries forces
			// our third party library dependencies to also be compiled using the Debug CRT (and often perform more slowly.)  Often
			// it can be inconvenient to require a separate copy of the debug versions of third party static libraries simply
			// so that you can debug your program's code.
			bDebugBuildsActuallyUseDebugCRT = false;

			// set up some paths
			BaseIntermediateFolder = "Intermediate/Build/";

			// By default check and stop the build on EULA violation
			bCheckLicenseViolations = false;
			bBreakBuildOnLicenseViolation = true;

			// Enables support for fast include dependency scanning, as well as gathering data for 'UBT Makefiles', then quickly
			// assembling builds in subsequent runs using data in those cached makefiles
			// NOTE: This feature is new and has a number of known issues (search the code for '@todo ubtmake')
			bUseUBTMakefiles = false;// !Utils.IsRunningOnMono;	// @todo ubtmake: Needs support for Mac

			// Distcc requires some setup - so by default disable it so we don't break local or remote building
			bAllowDistcc = false;
			bAllowDistccLocalFallback = true;

			// The normal Posix place to install distcc/dmucs would be /usr/local so start there
			DistccExecutablesPath = "/usr/local/bin";

			// By default compile code instead of analyzing it.
			bRunUnrealCodeAnalyzer = false;

			// If header is included in 30% or more cpp files in module it should be included in PCH.
			UCAUsageThreshold = 0.3f;

			// The default for normal Mac users should be to use DistCode which installs as an Xcode plugin and provides dynamic host management
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				string UserDir = Environment.GetEnvironmentVariable("HOME");
				string MacDistccExecutablesPath = UserDir + "/Library/Application Support/Developer/Shared/Xcode/Plug-ins/Distcc 3.2.xcplugin/Contents/usr/bin";

				// But we should use the standard Posix directory when not installed - a build farm would use a static .dmucs hosts file not DistCode.
				if (System.IO.Directory.Exists (MacDistccExecutablesPath)) 
				{
					DistccExecutablesPath = MacDistccExecutablesPath;
				}
			}
		}

		/// <summary>
		/// Validates the configuration. E.g. some options are mutually exclusive whereof some imply others. Also
		/// some functionality is not available on all platforms.
		/// @warning: the order of validation is important
		/// </summary>
		/// <param name="Configuration">Current configuration (e.g. development, debug, ...)</param>
		/// <param name="Platform">Current platform (e.g. Win32, PS3, ...)</param>
		/// <param name="bCreateDebugInfo">True if debug info should be created</param>
		public static void ValidateConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Platform);

			// E&C support.
			if( bSupportEditAndContinue )
			{
				bUseIncrementalLinking = BuildPlatform.ShouldUseIncrementalLinking(Platform, Configuration);
			}

			// Incremental linking.
			if (bUseIncrementalLinking)
			{
				bUsePDBFiles = BuildPlatform.ShouldUsePDBFiles(Platform, Configuration, bCreateDebugInfo);
			}

			// Detailed stats
            if (bLogDetailedActionStats && bAllowXGE)
			{
                // Some build machines apparently have this turned on, so if you really want detailed stats, don't run with XGE
                bLogDetailedActionStats = false;
			}

			// PDB
			if (bUsePDBFiles)
			{
				// NOTE: Currently we allow XGE to run, even with PDBs, until we notice an issue with this
				bool bDisallowXGEWithPDBFiles = false;
				if (bDisallowXGEWithPDBFiles)
				{
					// Force local execution as we have one PDB for all files using the same PCH. This currently doesn't
					// scale well with XGE due to required networking bandwidth. Xoreax mentioned that this was going to
					// be fixed in a future version of the software.
					bAllowXGE = false;
				}
			}

			// Allow for the build platform to perform custom validation here...
			// NOTE: This CAN modify the static BuildConfiguration settings!!!!
			BuildPlatform.ValidateBuildConfiguration(Configuration, Platform, bCreateDebugInfo);

            if (!BuildPlatform.CanUseXGE())
            {
                bAllowXGE = false;
            }

			if (!BuildPlatform.CanUseDistcc()) 
			{
				bAllowDistcc = false;
			}
			
			if (!BuildPlatform.CanUseSNDBS()) 
			{
				bAllowSNDBS = false;
			}
		}
	}
}
