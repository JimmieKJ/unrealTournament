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
	}

	/// <summary>
	/// The optimization level that may be compilation targets for C++ files.
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
	/// Whether the Common Language Runtime is enabled when compiling C++ targets (enables C++/CLI)
	/// </summary>
	public enum CPPCLRMode
	{
		CLRDisabled,
		CLREnabled,
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

	public class PrivateAssemblyInfoConfiguration
	{
		/// <summary>
		/// True if the assembly should be copied to an output folder.  Often, referenced assemblies must
		/// be loaded from the directory that the main executable resides in (or a sub-folder).  Note that
		/// PDB files will be copied as well.
		/// </summary>
		public bool bShouldCopyToOutputDirectory = false;

		/// <summary>
		/// Optional sub-directory to copy the assembly to
		/// </summary>
		public string DestinationSubDirectory = "";
	}

	/// <summary>
	/// Describes a private assembly dependency
	/// </summary>
	public class PrivateAssemblyInfo
	{
		/// <summary>
		/// The file item for the reference assembly on disk
		/// </summary>
		public FileItem FileItem = null;

		/// <summary>
		/// The configuration of the private assembly info
		/// </summary>
		public PrivateAssemblyInfoConfiguration Config = new PrivateAssemblyInfoConfiguration();
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

	public abstract class NativeBuildEnvironmentConfiguration
	{
		public class TargetInfo
		{
			/// <summary>
			/// The platform to be compiled/linked for.
			/// </summary>
			public CPPTargetPlatform Platform;

			/// <summary>
			/// The architecture that is being compiled/linked (empty string by default)
			/// </summary>
			public string Architecture;

			/// <summary>
			/// The configuration to be compiled/linked for.
			/// </summary>
			public CPPTargetConfiguration Configuration;

			public TargetInfo()
			{
				Architecture = "";
			}

			public TargetInfo(TargetInfo Target)
			{
				Platform = Target.Platform;
				Architecture = Target.Architecture;
				Configuration = Target.Configuration;
			}
		}

		public TargetInfo Target;

		[Obsolete("Use Target.Platform instead")]
		public CPPTargetPlatform TargetPlatform
		{
			get
			{
				return Target.Platform;
			}
			set
			{
				Target.Platform = value;
			}
		}

		[Obsolete("Use Target.Architecture instead")]
		public string TargetArchitecture
		{
			get
			{
				return Target.Architecture;
			}
			set
			{
				Target.Architecture = value;
			}
		}

		[Obsolete("Use Target.Configuration instead")]
		public CPPTargetConfiguration TargetConfiguration
		{
			get
			{
				return Target.Configuration;
			}
			set
			{
				Target.Configuration = value;
			}
		}

		protected NativeBuildEnvironmentConfiguration()
		{
			Target = new TargetInfo();
		}

		protected NativeBuildEnvironmentConfiguration(NativeBuildEnvironmentConfiguration Configuration)
		{
			Target = new TargetInfo(Configuration.Target);
		}

	}

	/// <summary>
	/// Encapsulates the configuration of an environment that a C++ file is compiled in.
	/// </summary>
	public class CPPEnvironmentConfiguration : NativeBuildEnvironmentConfiguration
	{
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
		/// PCH header file name as it appears in an #include statement in source code (might include partial, or no relative path.)
		/// This is needed by some compilers to use PCH features.
		/// </summary>
		public string PCHHeaderNameInCode;

		/// <summary>
		/// The name of the header file which is precompiled.
		/// </summary>
		public FileReference PrecompiledHeaderIncludeFilename = null;

		/// <summary>
		/// Whether the compilation should create, use, or do nothing with the precompiled header.
		/// </summary>
		public PrecompiledHeaderAction PrecompiledHeaderAction = PrecompiledHeaderAction.None;

		/// <summary>
		/// True if the precompiled header needs to be "force included" by the compiler.  This is usually used with
		/// the "Shared PCH" feature of UnrealBuildTool.  Note that certain platform toolchains *always* force
		/// include PCH header files first.
		/// </summary>
		public bool bForceIncludePrecompiledHeader = false;

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
		/// True if the environment contains performance critical code.
		/// </summary>
		public ModuleRules.CodeOptimization OptimizeCode = ModuleRules.CodeOptimization.Default;

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
		/// Whether the CLR (Common Language Runtime) support should be enabled for C++ targets (C++/CLI).
		/// </summary>
		public CPPCLRMode CLRMode = CPPCLRMode.CLRDisabled;

		/// <summary>
		/// The include paths to look for included files in.
		/// </summary>
		public readonly CPPIncludeInfo CPPIncludeInfo = new CPPIncludeInfo();

		/// <summary>
		/// Paths where .NET framework assembly references are found, when compiling CLR applications.
		/// </summary>
		public List<string> SystemDotNetAssemblyPaths = new List<string>();

		/// <summary>
		/// Full path and file name of .NET framework assemblies we're referencing
		/// </summary>
		public List<string> FrameworkAssemblyDependencies = new List<string>();

		/// <summary>
		/// List of private CLR assemblies that, when modified, will force recompilation of any CLR source files
		/// </summary>
		public List<PrivateAssemblyInfoConfiguration> PrivateAssemblyDependencies = new List<PrivateAssemblyInfoConfiguration>();

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
		public CPPEnvironmentConfiguration(CPPEnvironmentConfiguration InCopyEnvironment) :
			base(InCopyEnvironment)
		{
			OutputDirectory = InCopyEnvironment.OutputDirectory;
			PCHOutputDirectory = InCopyEnvironment.PCHOutputDirectory;
			LocalShadowDirectory = InCopyEnvironment.LocalShadowDirectory;
			PCHHeaderNameInCode = InCopyEnvironment.PCHHeaderNameInCode;
			PrecompiledHeaderIncludeFilename = InCopyEnvironment.PrecompiledHeaderIncludeFilename;
			PrecompiledHeaderAction = InCopyEnvironment.PrecompiledHeaderAction;
			bForceIncludePrecompiledHeader = InCopyEnvironment.bForceIncludePrecompiledHeader;
			bUseRTTI = InCopyEnvironment.bUseRTTI;
			bUseAVX = InCopyEnvironment.bUseAVX;
			bFasterWithoutUnity = InCopyEnvironment.bFasterWithoutUnity;
			MinSourceFilesForUnityBuildOverride = InCopyEnvironment.MinSourceFilesForUnityBuildOverride;
			MinFilesUsingPrecompiledHeaderOverride = InCopyEnvironment.MinFilesUsingPrecompiledHeaderOverride;
			bBuildLocallyWithSNDBS = InCopyEnvironment.bBuildLocallyWithSNDBS;
			bEnableExceptions = InCopyEnvironment.bEnableExceptions;
			bEnableShadowVariableWarning = InCopyEnvironment.bEnableShadowVariableWarning;
			OptimizeCode = InCopyEnvironment.OptimizeCode;
			bCreateDebugInfo = InCopyEnvironment.bCreateDebugInfo;
			bIsBuildingLibrary = InCopyEnvironment.bIsBuildingLibrary;
			bIsBuildingDLL = InCopyEnvironment.bIsBuildingDLL;
			bUseStaticCRT = InCopyEnvironment.bUseStaticCRT;
			bEnableOSX109Support = InCopyEnvironment.bEnableOSX109Support;
			CLRMode = InCopyEnvironment.CLRMode;
			CPPIncludeInfo.IncludePaths.UnionWith(InCopyEnvironment.CPPIncludeInfo.IncludePaths);
			CPPIncludeInfo.SystemIncludePaths.UnionWith(InCopyEnvironment.CPPIncludeInfo.SystemIncludePaths);
			SystemDotNetAssemblyPaths.AddRange(InCopyEnvironment.SystemDotNetAssemblyPaths);
			FrameworkAssemblyDependencies.AddRange(InCopyEnvironment.FrameworkAssemblyDependencies);
			PrivateAssemblyDependencies.AddRange(InCopyEnvironment.PrivateAssemblyDependencies);
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
	public partial class CPPEnvironment
	{
		/// <summary>
		/// The file containing the precompiled header data.
		/// </summary>
		public FileItem PrecompiledHeaderFile = null;

		/// <summary>
		/// List of private CLR assemblies that, when modified, will force recompilation of any CLR source files
		/// </summary>
		public List<PrivateAssemblyInfo> PrivateAssemblyDependencies = new List<PrivateAssemblyInfo>();

		public CPPEnvironmentConfiguration Config = new CPPEnvironmentConfiguration();

		/// <summary>
		/// List of available shared global PCH headers
		/// </summary>
		public List<SharedPCHHeaderInfo> SharedPCHHeaderFiles = new List<SharedPCHHeaderInfo>();

		/// <summary>
		/// List of "shared" precompiled header environments, potentially shared between modules
		/// </summary>
		public readonly List<PrecompileHeaderEnvironment> SharedPCHEnvironments = new List<PrecompileHeaderEnvironment>();

		// Whether or not UHT is being built
		public bool bHackHeaderGenerator;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public CPPEnvironment()
		{ }

		/// <summary>
		/// Copy constructor.
		/// </summary>
		protected CPPEnvironment(CPPEnvironment InCopyEnvironment)
		{
			PrecompiledHeaderFile = InCopyEnvironment.PrecompiledHeaderFile;
			PrivateAssemblyDependencies.AddRange(InCopyEnvironment.PrivateAssemblyDependencies);
			SharedPCHHeaderFiles.AddRange(InCopyEnvironment.SharedPCHHeaderFiles);
			SharedPCHEnvironments.AddRange(InCopyEnvironment.SharedPCHEnvironments);
			bHackHeaderGenerator = InCopyEnvironment.bHackHeaderGenerator;

			Config = new CPPEnvironmentConfiguration(InCopyEnvironment.Config);
		}

		/// <summary>
		/// Whether to use PCH files with the current target
		/// </summary>
		/// <returns>true if PCH files should be used, false otherwise</returns>
		public bool ShouldUsePCHs()
		{
			return UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Config.Target.Platform).ShouldUsePCHFiles(Config.Target.Platform, Config.Target.Configuration);
		}

		public void AddPrivateAssembly(string FilePath)
		{
			PrivateAssemblyInfo NewPrivateAssembly = new PrivateAssemblyInfo();
			NewPrivateAssembly.FileItem = FileItem.GetItemByPath(FilePath);
			PrivateAssemblyDependencies.Add(NewPrivateAssembly);
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
