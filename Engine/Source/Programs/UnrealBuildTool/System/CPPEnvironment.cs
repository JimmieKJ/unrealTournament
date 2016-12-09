// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// The platforms that may be compilation targets for C++ files.
	/// </summary>
	public enum CPPTargetPlatform
	{
		Win32,
		Win64,
		Mac,
		XboxOne,
		PS4,
		Android,
		IOS,
		HTML5,
		Linux,
		TVOS,
		Switch,
	}

	/// <summary>
	/// Compiler configuration. This controls whether to use define debug macros and other compiler settings. Note that optimization level should be based on the bOptimizeCode variable rather than
	/// this setting, so it can be modified on a per-module basis without introducing an incompatibility between object files or PCHs.
	/// </summary>
	public enum CPPTargetConfiguration
	{
		Debug,
		Development,
		Shipping
	}

	/// <summary>
	/// The optimization level that may be compilation targets for C# files.
	/// </summary>
	public enum CSharpTargetConfiguration
	{
		Debug,
		Development,
	}

	/// <summary>
	/// The possible interactions between a precompiled header and a C++ file being compiled.
	/// </summary>
	public enum PrecompiledHeaderAction
	{
		None,
		Include,
		Create
	}

	/// <summary>
	/// Encapsulates the compilation output of compiling a set of C++ files.
	/// </summary>
	public class CPPOutput
	{
		public List<FileItem> ObjectFiles = new List<FileItem>();
		public List<FileItem> DebugDataFiles = new List<FileItem>();
		public FileItem PrecompiledHeaderFile = null;
	}

	/// <summary>
	/// Encapsulates the environment that a C# file is compiled in.
	/// </summary>
	public class CSharpEnvironment
	{
		/// <summary>
		/// The configuration to be compiled for.
		/// </summary>
		public CSharpTargetConfiguration TargetConfiguration;

		/// <summary>
		/// The target platform used to set the environment. Doesn't affect output.
		/// </summary>
		public CPPTargetPlatform EnvironmentTargetPlatform;
	}

	/// <summary>
	/// Encapsulates the configuration of an environment that a C++ file is compiled in.
	/// </summary>
	public class CPPEnvironmentConfiguration
	{
		/// <summary>
		/// The platform to be compiled/linked for.
		/// </summary>
		public CPPTargetPlatform Platform;

		/// <summary>
		/// The configuration to be compiled/linked for.
		/// </summary>
		public CPPTargetConfiguration Configuration;

		/// <summary>
		/// The architecture that is being compiled/linked (empty string by default)
		/// </summary>
		public string Architecture;

		/// <summary>
		/// The directory to put the output object/debug files in.
		/// </summary>
		public DirectoryReference OutputDirectory = null;

		/// <summary>
		/// The directory to put precompiled header files in. Experimental setting to allow using a path on a faster drive. Defaults to the standard output directory if not set.
		/// </summary>
		public DirectoryReference PCHOutputDirectory = null;

		/// <summary>
		/// The directory to shadow source files in for syncing to remote compile servers
		/// </summary>
		public DirectoryReference LocalShadowDirectory = null;

		/// <summary>
		/// The name of the header file which is precompiled.
		/// </summary>
		public FileReference PrecompiledHeaderIncludeFilename = null;

		/// <summary>
		/// Whether the compilation should create, use, or do nothing with the precompiled header.
		/// </summary>
		public PrecompiledHeaderAction PrecompiledHeaderAction = PrecompiledHeaderAction.None;

		/// <summary>
		/// Use run time type information
		/// </summary>
		public bool bUseRTTI = false;

		/// <summary>
		/// Use AVX instructions
		/// </summary>
		public bool bUseAVX = false;

		/// <summary>
		/// Enable buffer security checks.   This should usually be enabled as it prevents severe security risks.
		/// </summary>
		public bool bEnableBufferSecurityChecks = true;

		/// <summary>
		/// If true and unity builds are enabled, this module will build without unity.
		/// </summary>
		public bool bFasterWithoutUnity = false;

		/// <summary>
		/// The number of source files in this module before unity build will be activated for that module.  If set to
		/// anything besides -1, will override the default setting which is controlled by MinGameModuleSourceFilesForUnityBuild
		/// </summary>
		public int MinSourceFilesForUnityBuildOverride = 0;

		/// <summary>
		/// Overrides BuildConfiguration.MinFilesUsingPrecompiledHeader if non-zero.
		/// </summary>
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/// <summary>
		/// Module uses a #import so must be built locally when compiling with SN-DBS
		/// </summary>
		public bool bBuildLocallyWithSNDBS = false;

		/// <summary>
		/// Enable exception handling
		/// </summary>
		public bool bEnableExceptions = false;

		/// <summary>
		/// Whether to warn about the use of shadow variables
		/// </summary>
		public bool bEnableShadowVariableWarning = false;

		/// <summary>
		/// True if compiler optimizations should be enabled. This setting is distinct from the configuration (see CPPTargetConfiguration).
		/// </summary>
		public bool bOptimizeCode = false;

		/// <summary>
		/// True if debug info should be created.
		/// </summary>
		public bool bCreateDebugInfo = true;

		/// <summary>
		/// True if we're compiling .cpp files that will go into a library (.lib file)
		/// </summary>
		public bool bIsBuildingLibrary = false;

		/// <summary>
		/// True if we're compiling a DLL
		/// </summary>
		public bool bIsBuildingDLL = false;

		/// <summary>
		/// Whether we should compile using the statically-linked CRT. This is not widely supported for the whole engine, but is required for programs that need to run without dependencies.
		/// </summary>
		public bool bUseStaticCRT = false;

		/// <summary>
		/// Whether we should compile with support for OS X 10.9 Mavericks. Used for some tools that we need to be compatible with this version of OS X.
		/// </summary>
		public bool bEnableOSX109Support = false;

		/// <summary>
		/// The include paths to look for included files in.
		/// </summary>
		public readonly CPPIncludeInfo CPPIncludeInfo = new CPPIncludeInfo();

		/// <summary>
		/// List of header files to force include
		/// </summary>
		public List<FileReference> ForceIncludeFiles = new List<FileReference>();

		/// <summary>
		/// The C++ preprocessor definitions to use.
		/// </summary>
		public List<string> Definitions = new List<string>();

		/// <summary>
		/// Additional arguments to pass to the compiler.
		/// </summary>
		public string AdditionalArguments = "";

		/// <summary>
		/// A list of additional frameworks whose include paths are needed.
		/// </summary>
		public List<UEBuildFramework> AdditionalFrameworks = new List<UEBuildFramework>();
		
        /// <summary>
        /// Default constructor.
        /// </summary>
        public CPPEnvironmentConfiguration()
		{
		}

		/// <summary>
		/// Copy constructor.
		/// </summary>
		public CPPEnvironmentConfiguration(CPPEnvironmentConfiguration InCopyEnvironment)
		{
			Platform = InCopyEnvironment.Platform;
			Configuration = InCopyEnvironment.Configuration;
			Architecture = InCopyEnvironment.Architecture;
			OutputDirectory = InCopyEnvironment.OutputDirectory;
			PCHOutputDirectory = InCopyEnvironment.PCHOutputDirectory;
			LocalShadowDirectory = InCopyEnvironment.LocalShadowDirectory;
			PrecompiledHeaderIncludeFilename = InCopyEnvironment.PrecompiledHeaderIncludeFilename;
			PrecompiledHeaderAction = InCopyEnvironment.PrecompiledHeaderAction;
			bUseRTTI = InCopyEnvironment.bUseRTTI;
			bUseAVX = InCopyEnvironment.bUseAVX;
			bFasterWithoutUnity = InCopyEnvironment.bFasterWithoutUnity;
			MinSourceFilesForUnityBuildOverride = InCopyEnvironment.MinSourceFilesForUnityBuildOverride;
			MinFilesUsingPrecompiledHeaderOverride = InCopyEnvironment.MinFilesUsingPrecompiledHeaderOverride;
			bBuildLocallyWithSNDBS = InCopyEnvironment.bBuildLocallyWithSNDBS;
			bEnableExceptions = InCopyEnvironment.bEnableExceptions;
			bEnableShadowVariableWarning = InCopyEnvironment.bEnableShadowVariableWarning;
			bOptimizeCode = InCopyEnvironment.bOptimizeCode;
			bCreateDebugInfo = InCopyEnvironment.bCreateDebugInfo;
			bIsBuildingLibrary = InCopyEnvironment.bIsBuildingLibrary;
			bIsBuildingDLL = InCopyEnvironment.bIsBuildingDLL;
			bUseStaticCRT = InCopyEnvironment.bUseStaticCRT;
			bEnableOSX109Support = InCopyEnvironment.bEnableOSX109Support;
			CPPIncludeInfo.IncludePaths.UnionWith(InCopyEnvironment.CPPIncludeInfo.IncludePaths);
			CPPIncludeInfo.SystemIncludePaths.UnionWith(InCopyEnvironment.CPPIncludeInfo.SystemIncludePaths);
			ForceIncludeFiles.AddRange(InCopyEnvironment.ForceIncludeFiles);
			Definitions.AddRange(InCopyEnvironment.Definitions);
			AdditionalArguments = InCopyEnvironment.AdditionalArguments;
			AdditionalFrameworks.AddRange(InCopyEnvironment.AdditionalFrameworks);
		}
	}

	/// <summary>
	/// Info about a "Shared PCH" header, including which module it is associated with and all of that module's dependencies
	/// </summary>
	public class SharedPCHHeaderInfo
	{
		/// The actual header file that the PCH will be created from
		public FileItem PCHHeaderFile;

		/// The module this header belongs to
		public UEBuildModuleCPP Module;

		/// All dependent modules
		public Dictionary<string, UEBuildModule> Dependencies;

		/// Performance diagnostics: The number of modules using this shared PCH header
		public int NumModulesUsingThisPCH = 0;
	}


	/// <summary>
	/// Encapsulates the environment that a C++ file is compiled in.
	/// </summary>
	public class CPPEnvironment
	{
		/// <summary>
		/// The file containing the precompiled header data.
		/// </summary>
		public FileItem PrecompiledHeaderFile = null;

		/// <summary>
		/// Header file cache for this target
		/// </summary>
		public CPPHeaders Headers;

		/// <summary>
		/// Compile settings
		/// </summary>
		public CPPEnvironmentConfiguration Config = new CPPEnvironmentConfiguration();

		/// <summary>
		/// Whether or not UHT is being built
		/// </summary>
		public bool bHackHeaderGenerator;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public CPPEnvironment(CPPHeaders Headers)
		{
			this.Headers = Headers;
		}

		/// <summary>
		/// Copy constructor.
		/// </summary>
		protected CPPEnvironment(CPPEnvironment InCopyEnvironment)
		{
			PrecompiledHeaderFile = InCopyEnvironment.PrecompiledHeaderFile;
			Headers = InCopyEnvironment.Headers;
			bHackHeaderGenerator = InCopyEnvironment.bHackHeaderGenerator;

			Config = new CPPEnvironmentConfiguration(InCopyEnvironment.Config);
		}

		/// <summary>
		/// Whether to use PCH files with the current target
		/// </summary>
		/// <returns>true if PCH files should be used, false otherwise</returns>
		public bool ShouldUsePCHs()
		{
			return UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Config.Platform).ShouldUsePCHFiles(Config.Platform, Config.Configuration);
		}

		/// <summary>
		/// Performs a deep copy of this CPPEnvironment object.
		/// </summary>
		/// <returns>Copied new CPPEnvironment object.</returns>
		public virtual CPPEnvironment DeepCopy()
		{
			return new CPPEnvironment(this);
		}
	};
}
