// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace UnrealBuildTool
{
	/** The platforms that may be compilation targets for C++ files. */
	public enum CPPTargetPlatform
	{
		Win32,
		Win64,
		Mac,
		XboxOne,
		PS4,
		Android,
		WinRT,
		WinRT_ARM,
		IOS,
		HTML5,
        Linux,
	}

	/** The optimization level that may be compilation targets for C++ files. */
	public enum CPPTargetConfiguration
	{
		Debug,
		Development,
		Shipping
	}

	/** The optimization level that may be compilation targets for C# files. */
	public enum CSharpTargetConfiguration
	{
		Debug,
		Development,
	}

	/** The possible interactions between a precompiled header and a C++ file being compiled. */
	public enum PrecompiledHeaderAction
	{
		None,
		Include,
		Create
	}

	/** Whether the Common Language Runtime is enabled when compiling C++ targets (enables C++/CLI) */
	public enum CPPCLRMode
	{
		CLRDisabled,
		CLREnabled,
	}

	/** Encapsulates the compilation output of compiling a set of C++ files. */
	public class CPPOutput
	{
		public List<FileItem> ObjectFiles = new List<FileItem>();
		public List<FileItem> DebugDataFiles = new List<FileItem>();
		public FileItem PrecompiledHeaderFile = null;
	}

	public class PrivateAssemblyInfoConfiguration
	{
		/** True if the assembly should be copied to an output folder.  Often, referenced assemblies must
		    be loaded from the directory that the main executable resides in (or a sub-folder).  Note that
		    PDB files will be copied as well. */
		public bool bShouldCopyToOutputDirectory = false;

		/** Optional sub-directory to copy the assembly to */
		public string DestinationSubDirectory = "";
	}

	/** Describes a private assembly dependency */
	public class PrivateAssemblyInfo
	{
		/** The file item for the reference assembly on disk */
		public FileItem FileItem = null;

		/** The configuration of the private assembly info */
		public PrivateAssemblyInfoConfiguration Config = new PrivateAssemblyInfoConfiguration();
	}

	/** Encapsulates the environment that a C# file is compiled in. */
	public class CSharpEnvironment
	{
		/** The configuration to be compiled for. */
		public CSharpTargetConfiguration TargetConfiguration;
		/** The target platform used to set the environment. Doesn't affect output. */
		public CPPTargetPlatform EnvironmentTargetPlatform;
	}

	public abstract class NativeBuildEnvironmentConfiguration
	{
		public class TargetInfo
		{
			/** The platform to be compiled/linked for. */
			public CPPTargetPlatform Platform;

			/** The architecture that is being compiled/linked (empty string by default) */
			public string Architecture;

			/** The configuration to be compiled/linked for. */
			public CPPTargetConfiguration Configuration;

			public TargetInfo()
			{
				Architecture = "";
			}

			public TargetInfo(TargetInfo Target)
			{
				Platform      = Target.Platform;
				Architecture  = Target.Architecture;
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

	/** Encapsulates the configuration of an environment that a C++ file is compiled in. */
	public class CPPEnvironmentConfiguration : NativeBuildEnvironmentConfiguration
	{
		/** The directory to put the output object/debug files in. */
		public string OutputDirectory = null;

		/** The directory to shadow source files in for syncing to remote compile servers */
		public string LocalShadowDirectory = null;

		/** PCH header file name as it appears in an #include statement in source code (might include partial, or no relative path.)
		    This is needed by some compilers to use PCH features. */
		public string PCHHeaderNameInCode;

		/** The name of the header file which is precompiled. */
		public string PrecompiledHeaderIncludeFilename = null;

		/** Whether the compilation should create, use, or do nothing with the precompiled header. */
		public PrecompiledHeaderAction PrecompiledHeaderAction = PrecompiledHeaderAction.None;

		/** True if the precompiled header needs to be "force included" by the compiler.  This is usually used with
		    the "Shared PCH" feature of UnrealBuildTool.  Note that certain platform toolchains *always* force
		    include PCH header files first. */
		public bool bForceIncludePrecompiledHeader = false;

		/** Use run time type information */
		public bool bUseRTTI = false;

		/** Enable buffer security checks.   This should usually be enabled as it prevents severe security risks. */
		public bool bEnableBufferSecurityChecks = true;

        /** If true and unity builds are enabled, this module will build without unity. */
        public bool bFasterWithoutUnity = false;

        /** Overrides BuildConfiguration.MinFilesUsingPrecompiledHeader if non-zero. */
        public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/** Enable exception handling */
		public bool bEnableExceptions = false;

		/** Whether to warn about the use of shadow variables */
		public bool bEnableShadowVariableWarning = false;

		/** True if the environment contains performance critical code. */
		public ModuleRules.CodeOptimization OptimizeCode = ModuleRules.CodeOptimization.Default;

		/** True if debug info should be created. */
		public bool bCreateDebugInfo = true;

		/** True if we're compiling .cpp files that will go into a library (.lib file) */
		public bool bIsBuildingLibrary = false;

		/** True if we're compiling a DLL */
		public bool bIsBuildingDLL = false;

		/** Whether we should compile using the statically-linked CRT. This is not widely supported for the whole engine, but is required for programs that need to run without dependencies. */
		public bool bUseStaticCRT = false;

		/** Whether the CLR (Common Language Runtime) support should be enabled for C++ targets (C++/CLI). */
		public CPPCLRMode CLRMode = CPPCLRMode.CLRDisabled;

		/** The include paths to look for included files in. */
		public readonly CPPIncludeInfo CPPIncludeInfo = new CPPIncludeInfo();

		/** Paths where .NET framework assembly references are found, when compiling CLR applications. */
		public List<string> SystemDotNetAssemblyPaths = new List<string>();

		/** Full path and file name of .NET framework assemblies we're referencing */
		public List<string> FrameworkAssemblyDependencies = new List<string>();

 		/** List of private CLR assemblies that, when modified, will force recompilation of any CLR source files */
 		public List<PrivateAssemblyInfoConfiguration> PrivateAssemblyDependencies = new List<PrivateAssemblyInfoConfiguration>();

		/** The C++ preprocessor definitions to use. */
		public List<string> Definitions = new List<string>();

		/** Additional arguments to pass to the compiler. */
		public string AdditionalArguments = "";

		/** A list of additional frameworks whose include paths are needed. */
		public List<UEBuildFramework> AdditionalFrameworks = new List<UEBuildFramework>();

		/** Default constructor. */
		public CPPEnvironmentConfiguration()
		{
		}

		/** Copy constructor. */
		public CPPEnvironmentConfiguration(CPPEnvironmentConfiguration InCopyEnvironment):
			base(InCopyEnvironment)
		{
			OutputDirectory                        = InCopyEnvironment.OutputDirectory;
			LocalShadowDirectory                   = InCopyEnvironment.LocalShadowDirectory;
			PCHHeaderNameInCode                    = InCopyEnvironment.PCHHeaderNameInCode;
			PrecompiledHeaderIncludeFilename       = InCopyEnvironment.PrecompiledHeaderIncludeFilename;
			PrecompiledHeaderAction                = InCopyEnvironment.PrecompiledHeaderAction;
			bForceIncludePrecompiledHeader         = InCopyEnvironment.bForceIncludePrecompiledHeader;
			bUseRTTI                               = InCopyEnvironment.bUseRTTI;
			bFasterWithoutUnity                    = InCopyEnvironment.bFasterWithoutUnity;
			MinFilesUsingPrecompiledHeaderOverride = InCopyEnvironment.MinFilesUsingPrecompiledHeaderOverride;
			bEnableExceptions                      = InCopyEnvironment.bEnableExceptions;
			bEnableShadowVariableWarning          = InCopyEnvironment.bEnableShadowVariableWarning;
			OptimizeCode                           = InCopyEnvironment.OptimizeCode;
			bCreateDebugInfo                       = InCopyEnvironment.bCreateDebugInfo;
			bIsBuildingLibrary                     = InCopyEnvironment.bIsBuildingLibrary;
			bIsBuildingDLL                         = InCopyEnvironment.bIsBuildingDLL;
			bUseStaticCRT						   = InCopyEnvironment.bUseStaticCRT;
			CLRMode                                = InCopyEnvironment.CLRMode;
			CPPIncludeInfo.IncludePaths            .UnionWith(InCopyEnvironment.CPPIncludeInfo.IncludePaths);
			CPPIncludeInfo.SystemIncludePaths      .UnionWith(InCopyEnvironment.CPPIncludeInfo.SystemIncludePaths);
			SystemDotNetAssemblyPaths    .AddRange(InCopyEnvironment.SystemDotNetAssemblyPaths);
			FrameworkAssemblyDependencies.AddRange(InCopyEnvironment.FrameworkAssemblyDependencies);
 			PrivateAssemblyDependencies  .AddRange(InCopyEnvironment.PrivateAssemblyDependencies);
			Definitions                  .AddRange(InCopyEnvironment.Definitions);
			AdditionalArguments                    = InCopyEnvironment.AdditionalArguments;
			AdditionalFrameworks		 .AddRange(InCopyEnvironment.AdditionalFrameworks);
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


	/** Encapsulates the environment that a C++ file is compiled in. */
	public partial class CPPEnvironment
	{
		/** The file containing the precompiled header data. */
		public FileItem PrecompiledHeaderFile = null;

		/** List of private CLR assemblies that, when modified, will force recompilation of any CLR source files */
		public List<PrivateAssemblyInfo> PrivateAssemblyDependencies = new List<PrivateAssemblyInfo>();

		public CPPEnvironmentConfiguration Config = new CPPEnvironmentConfiguration();

		/** List of available shared global PCH headers */
		public List<SharedPCHHeaderInfo> SharedPCHHeaderFiles = new List<SharedPCHHeaderInfo>();

		/** List of "shared" precompiled header environments, potentially shared between modules */
		public readonly List<PrecompileHeaderEnvironment> SharedPCHEnvironments = new List<PrecompileHeaderEnvironment>();

		// Whether or not UHT is being built
		public bool bHackHeaderGenerator;

		/** Default constructor. */
		public CPPEnvironment()
		{}

		/** Copy constructor. */
		protected CPPEnvironment(CPPEnvironment InCopyEnvironment)
		{
			PrecompiledHeaderFile = InCopyEnvironment.PrecompiledHeaderFile;
			PrivateAssemblyDependencies.AddRange(InCopyEnvironment.PrivateAssemblyDependencies);
			SharedPCHHeaderFiles.AddRange( InCopyEnvironment.SharedPCHHeaderFiles );
			SharedPCHEnvironments.AddRange( InCopyEnvironment.SharedPCHEnvironments );
			bHackHeaderGenerator = InCopyEnvironment.bHackHeaderGenerator;

			Config = new CPPEnvironmentConfiguration(InCopyEnvironment.Config);
		}

		/**
		 * Creates actions to compile a set of C++ source files.
		 * @param CPPFiles - The C++ source files to compile.
		 * @param ModuleName - Name of the module these files are associated with
		 * @return The object files produced by the actions.
		 */
		public CPPOutput CompileFiles(UEBuildTarget Target, List<FileItem> CPPFiles, string ModuleName)
		{
			return UEToolChain.GetPlatformToolChain(Config.Target.Platform).CompileCPPFiles(Target, this, CPPFiles, ModuleName);
		}

		/**
		 * Creates actions to compile a set of Windows resource script files.
		 * @param RCFiles - The resource script files to compile.
		 * @return The compiled resource (.res) files produced by the actions.
		 */
		public CPPOutput CompileRCFiles(UEBuildTarget Target, List<FileItem> RCFiles)
		{
			return UEToolChain.GetPlatformToolChain(Config.Target.Platform).CompileRCFiles(Target, this, RCFiles);
		}

		/**
		 * Whether to use PCH files with the current target
		 * 
		 * @return	true if PCH files should be used, false otherwise
		 */
		public bool ShouldUsePCHs()
		{
			return UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Config.Target.Platform).ShouldUsePCHFiles(Config.Target.Platform, Config.Target.Configuration);
		}

		public void AddPrivateAssembly( string FilePath )
		{
			PrivateAssemblyInfo NewPrivateAssembly = new PrivateAssemblyInfo();
			NewPrivateAssembly.FileItem = FileItem.GetItemByPath( FilePath );
			PrivateAssemblyDependencies.Add( NewPrivateAssembly );
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
