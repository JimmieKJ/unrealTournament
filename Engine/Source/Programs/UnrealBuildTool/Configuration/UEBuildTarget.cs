// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Runtime.Serialization;
using Tools.DotNETCommon.CaselessDictionary;
using System.Text.RegularExpressions;

namespace UnrealBuildTool
{
	public enum UnrealTargetPlatform
	{
		Unknown,
		Win32,
		Win64,
		Mac,
		XboxOne,
		PS4,
		IOS,
		Android,
		HTML5,
		Linux,
		AllDesktop,
		TVOS,
	}

	public enum UnrealPlatformGroup
	{
		Windows,	// this group is just to lump Win32 and Win64 into Windows directories, removing the special Windows logic in MakeListOfUnsupportedPlatforms
		Microsoft,
		Apple,
		IOS, // making IOS a group allows TVOS to compile IOS code
		Unix,
		Android,
		Sony,
		/*
		*  These two groups can be further used to conditionally compile files for a given platform. e.g
		*  Core/Private/HTML5/Simulator/<VC tool chain files>
		*  Core/Private/HTML5/Device/<emscripten toolchain files>.  
		*  Note: There's no default group - if the platform is not registered as device or simulator - both are rejected. 
		*/
		Device,
		Simulator,
		AllDesktop,
	}

	public enum UnrealTargetConfiguration
	{
		Unknown,
		Debug,
		DebugGame,
		Development,
		Shipping,
		Test,
	}

	public enum UnrealProjectType
	{
		CPlusPlus,	// C++ or C++/CLI
		CSharp,		// C#
	}

	public enum ErrorMode
	{
		Ignore,
		Suppress,
		Check,
	}

	/// <summary>
	/// Helper class for holding project name, platform and config together.
	/// </summary>
	public class UnrealBuildConfig
	{
		/// <summary>
		/// Project to build.
		/// </summary>
		public string Name;

		/// <summary>
		/// Platform to build the project for.
		/// </summary>
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Config to build the project with.
		/// </summary>
		public UnrealTargetConfiguration Config;

		/// <summary>
		/// Constructor
		/// </summary>
		public UnrealBuildConfig(string InName, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfig)
		{
			Name = InName;
			Platform = InPlatform;
			Config = InConfig;
		}

		/// <summary>
		/// Overridden ToString() to make this class esier to read when debugging.
		/// </summary>
		public override string ToString()
		{
			return String.Format("{0}, {1}, {2}", Name, Platform, Config);
		}
	};

	/// <summary>
	/// A container for a binary files (dll, exe) with its associated debug info.
	/// </summary>
	public class BuildManifest
	{
		public readonly List<string> BuildProducts = new List<string>();

		public readonly List<string> LibraryBuildProducts = new List<string>();

		public BuildManifest()
		{
		}

		public void AddBuildProduct(string FileName)
		{
			string FullFileName = Path.GetFullPath(FileName);
			if (!BuildProducts.Contains(FullFileName))
			{
				BuildProducts.Add(FullFileName);
			}
		}

		public void AddBuildProduct(string FileName, string DebugInfoExtension)
		{
			AddBuildProduct(FileName);
			if (!String.IsNullOrEmpty(DebugInfoExtension))
			{
				AddBuildProduct(Path.ChangeExtension(FileName, DebugInfoExtension));
			}
		}

		public void AddLibraryBuildProduct(string FileName)
		{
			string FullFileName = Path.GetFullPath(FileName);
			if (!LibraryBuildProducts.Contains(FullFileName))
			{
				LibraryBuildProducts.Add(FullFileName);
			}
		}
	}

	/// <summary>
	/// A list of external files required to build a given target
	/// </summary>
	public class ExternalFileList
	{
		public readonly List<string> FileNames = new List<string>();
	}

	[Serializable]
	public class FlatModuleCsDataType : ISerializable
	{
		public FlatModuleCsDataType(SerializationInfo Info, StreamingContext Context)
		{
			BuildCsFilename = Info.GetString("bf");
			ModuleSourceFolder = (DirectoryReference)Info.GetValue("mf", typeof(DirectoryReference));
			UHTHeaderNames = (List<string>)Info.GetValue("hn", typeof(List<string>));
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("bf", BuildCsFilename);
			Info.AddValue("mf", ModuleSourceFolder);
			Info.AddValue("hn", UHTHeaderNames);
		}

		public FlatModuleCsDataType(string InBuildCsFilename)
		{
			BuildCsFilename = InBuildCsFilename;
		}

		public string BuildCsFilename;
		public DirectoryReference ModuleSourceFolder;
		public List<string> UHTHeaderNames = new List<string>();
	}

	[Serializable]
	public class OnlyModule : ISerializable
	{
		public OnlyModule(SerializationInfo Info, StreamingContext Context)
		{
			OnlyModuleName = Info.GetString("mn");
			OnlyModuleSuffix = Info.GetString("ms");
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("mn", OnlyModuleName);
			Info.AddValue("ms", OnlyModuleSuffix);
		}

		public OnlyModule(string InitOnlyModuleName)
		{
			OnlyModuleName = InitOnlyModuleName;
			OnlyModuleSuffix = String.Empty;
		}

		public OnlyModule(string InitOnlyModuleName, string InitOnlyModuleSuffix)
		{
			OnlyModuleName = InitOnlyModuleName;
			OnlyModuleSuffix = InitOnlyModuleSuffix;
		}

		/// <summary>
		/// If building only a single module, this is the module name to build
		/// </summary>
		public readonly string OnlyModuleName;

		/// <summary>
		/// When building only a single module, the optional suffix for the module file name
		/// </summary>
		public readonly string OnlyModuleSuffix;
	}


	/// <summary>
	/// Describes all of the information needed to initialize a UEBuildTarget object
	/// </summary>
	public class TargetDescriptor
	{
		public FileReference ProjectFile;
		public string TargetName;
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public string Architecture;
		public List<string> AdditionalDefinitions;
		public bool bIsEditorRecompile;
		public string RemoteRoot;
		public List<OnlyModule> OnlyModules;
		public bool bPrecompile;
		public bool bUsePrecompiled;
		public List<FileReference> ForeignPlugins;
		public string ForceReceiptFileName;
		public UEBuildPlatformContext PlatformContext;
	}

    /// <summary>
    /// Holds information for targeting specific platform (platform type + cook flavor)
    /// </summary>
    public struct TargetPlatformDescriptor
    {
        public UnrealTargetPlatform Type;
        public string CookFlavor;

        public TargetPlatformDescriptor(UnrealTargetPlatform InType)
        {
            Type = InType;
            CookFlavor = "";
        }
        public TargetPlatformDescriptor(UnrealTargetPlatform InType, string InCookFlavor)
        {
            Type = InType;
            CookFlavor = InCookFlavor;
        }

        public override string ToString()
        {
            return Type.ToString();
        }
    }

	/// <summary>
	/// A target that can be built
	/// </summary>
	[Serializable]
	public class UEBuildTarget : ISerializable
	{
		public string GetAppName()
		{
			return AppName;
		}

		public string GetTargetName()
		{
			return TargetName;
		}

		public static UnrealTargetPlatform CPPTargetPlatformToUnrealTargetPlatform(CPPTargetPlatform InCPPPlatform)
		{
			switch (InCPPPlatform)
			{
				case CPPTargetPlatform.Win32:			return UnrealTargetPlatform.Win32;
				case CPPTargetPlatform.Win64:			return UnrealTargetPlatform.Win64;
				case CPPTargetPlatform.Mac:				return UnrealTargetPlatform.Mac;
				case CPPTargetPlatform.XboxOne:			return UnrealTargetPlatform.XboxOne;
				case CPPTargetPlatform.PS4:				return UnrealTargetPlatform.PS4;
				case CPPTargetPlatform.Android:			return UnrealTargetPlatform.Android;
				case CPPTargetPlatform.IOS:				return UnrealTargetPlatform.IOS;
				case CPPTargetPlatform.HTML5:			return UnrealTargetPlatform.HTML5;
                case CPPTargetPlatform.Linux:			return UnrealTargetPlatform.Linux;
				case CPPTargetPlatform.TVOS:			return UnrealTargetPlatform.TVOS;
			}
			throw new BuildException("CPPTargetPlatformToUnrealTargetPlatform: Unknown CPPTargetPlatform {0}", InCPPPlatform.ToString());
		}


		public static List<TargetDescriptor> ParseTargetCommandLine(string[] SourceArguments, ref FileReference ProjectFile)
		{
			List<TargetDescriptor> Targets = new List<TargetDescriptor>();

			string TargetName = null;
			List<string> AdditionalDefinitions = new List<string>();
			UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;
			UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;
			string Architecture = null;
			string RemoteRoot = null;
			List<OnlyModule> OnlyModules = new List<OnlyModule>();
			List<FileReference> ForeignPlugins = new List<FileReference>();
			string ForceReceiptFileName = null;

			// If true, the recompile was launched by the editor.
			bool bIsEditorRecompile = false;

			// Settings for creating/using static libraries for the engine
			bool bPrecompile = false;
			bool bUsePrecompiled = UnrealBuildTool.IsEngineInstalled();

			// Combine the two arrays of arguments
			List<string> Arguments = new List<string>(SourceArguments.Length);
			Arguments.AddRange(SourceArguments);

			List<string> PossibleTargetNames = new List<string>();
			for (int ArgumentIndex = 0; ArgumentIndex < Arguments.Count; ArgumentIndex++)
			{
				UnrealTargetPlatform ParsedPlatform = UEBuildPlatform.ConvertStringToPlatform(Arguments[ArgumentIndex]);
				if (ParsedPlatform != UnrealTargetPlatform.Unknown)
				{
					Platform = ParsedPlatform;
				}
				else
				{
					switch (Arguments[ArgumentIndex].ToUpperInvariant())
					{
						case "-MODULE":
							// Specifies a module to recompile.  Can be specified more than once on the command-line to compile multiple specific modules.
							{
								if (ArgumentIndex + 1 >= Arguments.Count)
								{
									throw new BuildException("Expected module name after -Module argument, but found nothing.");
								}
								string OnlyModuleName = Arguments[++ArgumentIndex];

								OnlyModules.Add(new OnlyModule(OnlyModuleName));
							}
							break;

						case "-MODULEWITHSUFFIX":
							{
								// Specifies a module name to compile along with a suffix to append to the DLL file name.  Can be specified more than once on the command-line to compile multiple specific modules.
								if (ArgumentIndex + 2 >= Arguments.Count)
								{
									throw new BuildException("Expected module name and module suffix -ModuleWithSuffix argument");
								}

								string OnlyModuleName = Arguments[++ArgumentIndex];
								string OnlyModuleSuffix = Arguments[++ArgumentIndex];

								OnlyModules.Add(new OnlyModule(OnlyModuleName, OnlyModuleSuffix));
							}
							break;

						case "-PLUGIN":
							{
								if (ArgumentIndex + 1 >= Arguments.Count)
								{
									throw new BuildException("Expected plugin filename after -Plugin argument, but found nothing.");
								}

								ForeignPlugins.Add(new FileReference(Arguments[++ArgumentIndex]));
							}
							break;

						case "-RECEIPT":
							{
								if (ArgumentIndex + 1 >= Arguments.Count)
								{
									throw new BuildException("Expected path to the generated receipt after -Receipt argument, but found nothing.");
								}

								ForceReceiptFileName = Arguments[++ArgumentIndex];
							}
							break;

						// Configuration names:
						case "DEBUG":
							Configuration = UnrealTargetConfiguration.Debug;
							break;
						case "DEBUGGAME":
							Configuration = UnrealTargetConfiguration.DebugGame;
							break;
						case "DEVELOPMENT":
							Configuration = UnrealTargetConfiguration.Development;
							break;
						case "SHIPPING":
							Configuration = UnrealTargetConfiguration.Shipping;
							break;
						case "TEST":
							Configuration = UnrealTargetConfiguration.Test;
							break;

						// -Define <definition> adds a definition to the global C++ compilation environment.
						case "-DEFINE":
							if (ArgumentIndex + 1 >= Arguments.Count)
							{
								throw new BuildException("Expected path after -define argument, but found nothing.");
							}
							ArgumentIndex++;
							AdditionalDefinitions.Add(Arguments[ArgumentIndex]);
							break;

						// -RemoteRoot <RemoteRoot> sets where the generated binaries are CookerSynced.
						case "-REMOTEROOT":
							if (ArgumentIndex + 1 >= Arguments.Count)
							{
								throw new BuildException("Expected path after -RemoteRoot argument, but found nothing.");
							}
							ArgumentIndex++;
							if (Arguments[ArgumentIndex].StartsWith("xe:\\") == true)
							{
								RemoteRoot = Arguments[ArgumentIndex].Substring("xe:\\".Length);
							}
							else if (Arguments[ArgumentIndex].StartsWith("devkit:\\") == true)
							{
								RemoteRoot = Arguments[ArgumentIndex].Substring("devkit:\\".Length);
							}
							break;

						// Disable editor support
						case "-NOEDITOR":
							UEBuildConfiguration.bBuildEditor = false;
							break;

						case "-NOEDITORONLYDATA":
							UEBuildConfiguration.bBuildWithEditorOnlyData = false;
							break;

						case "-DEPLOY":
							// Does nothing at the moment...
							break;

						case "-PROJECTFILES":
							{
								// Force platform to Win64 for building IntelliSense files
								Platform = UnrealTargetPlatform.Win64;

								// Force configuration to Development for IntelliSense
								Configuration = UnrealTargetConfiguration.Development;
							}
							break;

						case "-XCODEPROJECTFILE":
							{
								// @todo Mac: Don't want to force a platform/config for generated projects, in case they affect defines/includes (each project's individual configuration should be generated with correct settings)

								// Force platform to Mac for building IntelliSense files
								Platform = UnrealTargetPlatform.Mac;

								// Force configuration to Development for IntelliSense
								Configuration = UnrealTargetConfiguration.Development;
							}
							break;

						case "-MAKEFILE":
							{
								// Force platform to Linux for building IntelliSense files
								Platform = UnrealTargetPlatform.Linux;

								// Force configuration to Development for IntelliSense
								Configuration = UnrealTargetConfiguration.Development;
							}
							break;

						case "-CMAKEFILE":
							{
								Platform = BuildHostPlatform.Current.Platform;

								// Force configuration to Development for IntelliSense
								Configuration = UnrealTargetConfiguration.Development;
							}
							break;

						case "-QMAKEFILE":
							{
								// Force platform to Linux for building IntelliSense files
								Platform = UnrealTargetPlatform.Linux;

								// Force configuration to Development for IntelliSense
								Configuration = UnrealTargetConfiguration.Development;
							}
							break;

						case "-KDEVELOPFILE":
							{
								// Force platform to Linux for building IntelliSense files
								Platform = UnrealTargetPlatform.Linux;

								// Force configuration to Development for IntelliSense
								Configuration = UnrealTargetConfiguration.Development;
							}
							break;

						case "-CODELITEFILE":
							{
								Platform = BuildHostPlatform.Current.Platform;

								// Force configuration to Development for IntelliSense
								Configuration = UnrealTargetConfiguration.Development;
							}
							break;

						case "-EDITORRECOMPILE":
							{
								bIsEditorRecompile = true;
							}
							break;

						case "-PRECOMPILE":
							{
								// Make static libraries for all engine modules as intermediates for this target
								bPrecompile = true;
							}
							break;

						case "-USEPRECOMPILED":
							{
								// Use existing static libraries for all engine modules in this target
								bUsePrecompiled = true;
							}
							break;

						default:
							PossibleTargetNames.Add(Arguments[ArgumentIndex]);
							break;
					}
				}
			}

			if (Platform == UnrealTargetPlatform.Unknown)
			{
				throw new BuildException("Couldn't find platform name.");
			}
			if (Configuration == UnrealTargetConfiguration.Unknown)
			{
				throw new BuildException("Couldn't determine configuration name.");
			}

			if (PossibleTargetNames.Count > 0)
			{
				// We have possible targets!
				foreach (string PossibleTargetName in PossibleTargetNames)
				{
					// If Engine is installed, the PossibleTargetName could contain a path
					TargetName = PossibleTargetName;

					// If a project file was not specified see if we can find one
					if (ProjectFile == null && UProjectInfo.TryGetProjectForTarget(TargetName, out ProjectFile))
					{
						Log.TraceVerbose("Found project file for {0} - {1}", TargetName, ProjectFile);
					}

					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
					UEBuildPlatformContext PlatformContext = BuildPlatform.CreateContext(ProjectFile);

					if(Architecture == null)
					{
						Architecture = PlatformContext.GetActiveArchitecture();
					}

					Targets.Add(new TargetDescriptor()
						{
							ProjectFile = ProjectFile,
							TargetName = TargetName,
							Platform = Platform,
							Configuration = Configuration,
							Architecture = Architecture,
							AdditionalDefinitions = AdditionalDefinitions,
							bIsEditorRecompile = bIsEditorRecompile,
							RemoteRoot = RemoteRoot,
							OnlyModules = OnlyModules,
							bPrecompile = bPrecompile,
							bUsePrecompiled = bUsePrecompiled,
							ForeignPlugins = ForeignPlugins,
							ForceReceiptFileName = ForceReceiptFileName,
							PlatformContext = PlatformContext
						});
					break;
				}
			}
			if (Targets.Count == 0)
			{
				throw new BuildException("No target name was specified on the command-line.");
			}
			return Targets;
		}

		/// <summary>
		/// Creates a target object for the specified target name.
		/// </summary>
		/// <param name="Desc">Information about the target</param>
		/// <param name="RulesAssembly">The assembly containing the target rules</param>
		/// <returns>The build target object for the specified build rules source file</returns>
		public static UEBuildTarget CreateTarget(TargetDescriptor Desc)
		{
			DateTime CreateTargetStartTime = DateTime.UtcNow;

			RulesAssembly RulesAssembly;
			if (Desc.ProjectFile != null)
			{
				RulesAssembly = RulesCompiler.CreateProjectRulesAssembly(Desc.ProjectFile);
			}
			else
			{
				RulesAssembly = RulesCompiler.CreateEngineRulesAssembly();
			}
			if (Desc.ForeignPlugins != null)
			{
				foreach (FileReference ForeignPlugin in Desc.ForeignPlugins)
				{
					RulesAssembly = RulesCompiler.CreatePluginRulesAssembly(ForeignPlugin, RulesAssembly);
				}
			}

			FileReference TargetFileName;
			TargetRules RulesObject = RulesAssembly.CreateTargetRules(Desc.TargetName, new TargetInfo(Desc.Platform, Desc.Configuration, Desc.Architecture), Desc.bIsEditorRecompile, out TargetFileName);
			if (Desc.bIsEditorRecompile)
			{
				// Now that we found the actual Editor target, make sure we're no longer using the old TargetName (which is the Game target)
				int TargetSuffixIndex = RulesObject.TargetName.LastIndexOf("Target");
				Desc.TargetName = (TargetSuffixIndex > 0) ? RulesObject.TargetName.Substring(0, TargetSuffixIndex) : RulesObject.TargetName;
			}
			if ((ProjectFileGenerator.bGenerateProjectFiles == false) && (RulesObject.SupportsPlatform(Desc.Platform) == false))
			{
				if (UEBuildConfiguration.bCleanProject)
				{
					return null;
				}
				throw new BuildException("{0} does not support the {1} platform.", Desc.TargetName, Desc.Platform.ToString());
			}

			// Generate a build target from this rules module
			UEBuildTarget BuildTarget = null;
			switch (RulesObject.Type)
			{
				case TargetRules.TargetType.Game:
					BuildTarget = new UEBuildGame(Desc, RulesObject, RulesAssembly, TargetFileName);
					break;
				case TargetRules.TargetType.Editor:
					BuildTarget = new UEBuildEditor(Desc, RulesObject, RulesAssembly, TargetFileName);
					break;
				case TargetRules.TargetType.Client:
					BuildTarget = new UEBuildClient(Desc, RulesObject, RulesAssembly, TargetFileName);
					break;
				case TargetRules.TargetType.Server:
					BuildTarget = new UEBuildServer(Desc, RulesObject, RulesAssembly, TargetFileName);
					break;
				case TargetRules.TargetType.Program:
					BuildTarget = new UEBuildTarget(Desc, RulesObject, RulesAssembly, null, TargetFileName);
					break;
			}

			if (BuildConfiguration.bPrintPerformanceInfo)
			{
				double CreateTargetTime = (DateTime.UtcNow - CreateTargetStartTime).TotalSeconds;
				Log.TraceInformation("CreateTarget for " + Desc.TargetName + " took " + CreateTargetTime + "s");
			}

			if (BuildTarget == null)
			{
				throw new BuildException("Failed to create build target for '{0}'.", Desc.TargetName);
			}

			return BuildTarget;
		}

		/// Parses only the target platform and configuration from the specified command-line argument list
		public static void ParsePlatformAndConfiguration(string[] SourceArguments,
			out UnrealTargetPlatform Platform, out UnrealTargetConfiguration Configuration,
			bool bThrowExceptionOnFailure = true)
		{
			Platform = UnrealTargetPlatform.Unknown;
			Configuration = UnrealTargetConfiguration.Unknown;

			foreach (string CurArgument in SourceArguments)
			{
				UnrealTargetPlatform ParsedPlatform = UEBuildPlatform.ConvertStringToPlatform(CurArgument);
				if (ParsedPlatform != UnrealTargetPlatform.Unknown)
				{
					Platform = ParsedPlatform;
				}
				else
				{
					switch (CurArgument.ToUpperInvariant())
					{
						// Configuration names:
						case "DEBUG":
							Configuration = UnrealTargetConfiguration.Debug;
							break;
						case "DEBUGGAME":
							Configuration = UnrealTargetConfiguration.DebugGame;
							break;
						case "DEVELOPMENT":
							Configuration = UnrealTargetConfiguration.Development;
							break;
						case "SHIPPING":
							Configuration = UnrealTargetConfiguration.Shipping;
							break;
						case "TEST":
							Configuration = UnrealTargetConfiguration.Test;
							break;

						case "-PROJECTFILES":
							// Force platform to Win64 and configuration to Development for building IntelliSense files
							Platform = UnrealTargetPlatform.Win64;
							Configuration = UnrealTargetConfiguration.Development;
							break;

						case "-XCODEPROJECTFILE":
							// @todo Mac: Don't want to force a platform/config for generated projects, in case they affect defines/includes (each project's individual configuration should be generated with correct settings)

							// Force platform to Mac and configuration to Development for building IntelliSense files
							Platform = UnrealTargetPlatform.Mac;
							Configuration = UnrealTargetConfiguration.Development;
							break;

						case "-MAKEFILE":
							// Force platform to Linux and configuration to Development for building IntelliSense files
							Platform = UnrealTargetPlatform.Linux;
							Configuration = UnrealTargetConfiguration.Development;
							break;

						case "-CMAKEFILE":
							Platform = BuildHostPlatform.Current.Platform;
							Configuration = UnrealTargetConfiguration.Development;
							break;

						case "-QMAKEFILE":
							// Force platform to Linux and configuration to Development for building IntelliSense files
							Platform = UnrealTargetPlatform.Linux;
							Configuration = UnrealTargetConfiguration.Development;
							break;

						case "-KDEVELOPFILE":
							// Force platform to Linux and configuration to Development for building IntelliSense files
							Platform = UnrealTargetPlatform.Linux;
							Configuration = UnrealTargetConfiguration.Development;
							break;

						case "-CODELITEFILE":
							Platform = BuildHostPlatform.Current.Platform;
							// Force configuration to Development for IntelliSense
							Configuration = UnrealTargetConfiguration.Development;
							break;
					}
				}
			}

			if (bThrowExceptionOnFailure == true)
			{
				if (Platform == UnrealTargetPlatform.Unknown)
				{
					throw new BuildException("Couldn't find platform name.");
				}
				if (Configuration == UnrealTargetConfiguration.Unknown)
				{
					throw new BuildException("Couldn't determine configuration name.");
				}
			}
		}


		/// <summary>
		/// Look for all folders with a uproject file, these are valid games
		/// This is defined as a valid game
		/// </summary>
		public static List<DirectoryReference> DiscoverAllGameFolders()
		{
			List<DirectoryReference> AllGameFolders = new List<DirectoryReference>();

			// Add all the normal game folders. The UProjectInfo list is already filtered for projects specified on the command line.
			List<UProjectInfo> GameProjects = UProjectInfo.FilterGameProjects(true, null);
			foreach (UProjectInfo GameProject in GameProjects)
			{
				AllGameFolders.Add(GameProject.Folder);
			}

			return AllGameFolders;
		}

		/// <summary>
		/// The target rules
		/// </summary>
		[NonSerialized]
		public TargetRules Rules = null;

		/// <summary>
		/// The rules assembly to use when searching for modules
		/// </summary>
		[NonSerialized]
		public RulesAssembly RulesAssembly;

		/// <summary>
		/// The project file for this target
		/// </summary>
		public FileReference ProjectFile;

		/// <summary>
		/// The project descriptor for this target
		/// </summary>
		[NonSerialized]
		public ProjectDescriptor ProjectDescriptor;

		/// <summary>
		/// Type of target
		/// </summary>
		public TargetRules.TargetType TargetType;

		/// <summary>
		/// The name of the application the target is part of. For targets with bUseSharedBuildEnvironment = true, this is typically the name of the base application, eg. UE4Editor for any game editor.
		/// </summary>
		public string AppName;

		/// <summary>
		/// The name of the target
		/// </summary>
		public string TargetName;

		/// <summary>
		/// Whether the target uses the shared build environment. If false, AppName==TargetName and all binaries should be written to the project directory.
		/// </summary>
		public bool bUseSharedBuildEnvironment;

		/// <summary>
		/// Platform as defined by the VCProject and passed via the command line. Not the same as internal config names.
		/// </summary>
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Target as defined by the VCProject and passed via the command line. Not necessarily the same as internal name.
		/// </summary>
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// TargetInfo object which can be passed to RulesCompiler
		/// </summary>
		public TargetInfo TargetInfo;

		/// <summary>
		/// Root directory for the active project. Typically contains the .uproject file, or the engine root.
		/// </summary>
		public DirectoryReference ProjectDirectory;

		/// <summary>
		/// Default directory for intermediate files. Typically underneath ProjectDirectory.
		/// </summary>
		public DirectoryReference ProjectIntermediateDirectory;

		/// <summary>
		/// Directory for engine intermediates. For an agnostic editor/game executable, this will be under the engine directory. For monolithic executables this will be the same as the project intermediate directory.
		/// </summary>
		public DirectoryReference EngineIntermediateDirectory;

		/// <summary>
		/// Output paths of final executable.
		/// </summary>
		public List<FileReference> OutputPaths;

		/// <summary>
		/// Returns the OutputPath is there is only one entry in OutputPaths
		/// </summary>
		public FileReference OutputPath
		{
			get
			{
				if (OutputPaths.Count != 1)
				{
					throw new BuildException("Attempted to use UEBuildTarget.OutputPath property, but there are multiple (or no) OutputPaths. You need to handle multiple in the code that called this (size = {0})", OutputPaths.Count);
				}
				return OutputPaths[0];
			}
		}

		/// <summary>
		/// Remote path of the binary if it is to be synced with CookerSync
		/// </summary>
		public string RemoteRoot;

		/// <summary>
		/// Whether to build target modules that can be reused for future builds
		/// </summary>
		public bool bPrecompile;

		/// <summary>
		/// Whether to use precompiled target modules
		/// </summary>
		public bool bUsePrecompiled;

		/// <summary>
		/// The C++ environment that all the environments used to compile UE-based modules are derived from.
		/// </summary>
		[NonSerialized]
		public CPPEnvironment GlobalCompileEnvironment = new CPPEnvironment();

		/// <summary>
		/// The link environment all binary link environments are derived from.
		/// </summary>
		[NonSerialized]
		public LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment();

		/// <summary>
		/// All plugins which are valid for this target
		/// </summary>
		[NonSerialized]
		public List<PluginInfo> ValidPlugins;

		/// <summary>
		/// All plugins which are built for this target
		/// </summary>
		[NonSerialized]
		public List<PluginInfo> BuildPlugins;

		/// <summary>
		/// All plugin dependencies for this target. This differs from the list of plugins that is built for Launcher, where we build everything, but link in only the enabled plugins.
		/// </summary>
		[NonSerialized]
		public List<PluginInfo> EnabledPlugins;

		/// <summary>
		/// Additional plugin filenames which are foreign to this target
		/// </summary>
		[NonSerialized]
		public List<PluginInfo> UnrealHeaderToolPlugins;

		/// <summary>
		/// Additional plugin filenames to include when building UnrealHeaderTool for the current target
		/// </summary>
		public List<FileReference> ForeignPlugins = new List<FileReference>();

		/// <summary>
		/// All application binaries; may include binaries not built by this target.
		/// </summary>
		[NonSerialized]
		public List<UEBuildBinary> AppBinaries = new List<UEBuildBinary>();

		/// <summary>
		/// Extra engine module names to either include in the binary (monolithic) or create side-by-side DLLs for (modular)
		/// </summary>
		[NonSerialized]
		public List<string> ExtraModuleNames = new List<string>();

		/// <summary>
		/// Extra engine module names which are compiled but not linked into any binary in precompiled engine distributions
		/// </summary>
		[NonSerialized]
		public List<UEBuildBinary> PrecompiledBinaries = new List<UEBuildBinary>();

		/// <summary>
		/// True if re-compiling this target from the editor
		/// </summary>
		public bool bEditorRecompile;

		/// <summary>
		/// If building only a specific set of modules, these are the modules to build
		/// </summary>
		public List<OnlyModule> OnlyModules = new List<OnlyModule>();

		/// <summary>
		/// Kept to determine the correct module parsing order when filtering modules.
		/// </summary>
		[NonSerialized]
		protected List<UEBuildBinary> NonFilteredModules = new List<UEBuildBinary>();

		/// <summary>
		/// true if target should be compiled in monolithic mode, false if not
		/// </summary>
		protected bool bCompileMonolithic = false;

		/// <summary>
		/// Used to keep track of all modules by name.
		/// </summary>
		[NonSerialized]
		private Dictionary<string, UEBuildModule> Modules = new CaselessDictionary<UEBuildModule>();

		/// <summary>
		/// Used to map names of modules to their .Build.cs filename
		/// </summary>
		public CaselessDictionary<FlatModuleCsDataType> FlatModuleCsData = new CaselessDictionary<FlatModuleCsDataType>();

		/// <summary>
		/// The receipt for this target, which contains a record of this build.
		/// </summary>
		private TargetReceipt Receipt;
		public TargetReceipt BuildReceipt { get { return Receipt; } }

		/// <summary>
		/// Filename for the receipt for this target.
		/// </summary>
		private string ReceiptFileName;
		public string BuildReceiptFileName { get { return ReceiptFileName; } }

		/// <summary>
		/// Version manifests to be written to each output folder
		/// </summary>
		private KeyValuePair<FileReference, VersionManifest>[] FileReferenceToVersionManifestPairs;

		/// <summary>
		/// Force output of the receipt to an additional filename
		/// </summary>
		[NonSerialized]
		private string ForceReceiptFileName;

		/// <summary>
		/// The name of the .Target.cs file, if the target was created with one
		/// </summary>
		private readonly FileReference TargetCsFilenameField;
		public FileReference TargetCsFilename { get { return TargetCsFilenameField; } }

		/// <summary>
		/// The cached platform context for this target
		/// </summary>
		[NonSerialized]
		UEBuildPlatformContext PlatformContext;

		/// <summary>
		/// List of scripts to run before building
		/// </summary>
		FileReference[] PreBuildStepScripts;

		/// <summary>
		/// List of scripts to run after building
		/// </summary>
		FileReference[] PostBuildStepScripts;

		/// <summary>
		/// A list of the module filenames which were used to build this target.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<string> GetAllModuleBuildCsFilenames()
		{
			return FlatModuleCsData.Values.Select(Data => Data.BuildCsFilename);
		}

		/// <summary>
		/// A list of the module filenames which were used to build this target.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<string> GetAllModuleFolders()
		{
			return FlatModuleCsData.Values.SelectMany(Data => Data.UHTHeaderNames);
		}

		/// <summary>
		/// Whether this target should be compiled in monolithic mode
		/// </summary>
		/// <returns>true if it should, false if it shouldn't</returns>
		public bool ShouldCompileMonolithic()
		{
			return bCompileMonolithic;	// @todo ubtmake: We need to make sure this function and similar things aren't called in assembler mode
		}

		public UEBuildTarget(SerializationInfo Info, StreamingContext Context)
		{
			TargetType = (TargetRules.TargetType)Info.GetInt32("tt");
			ProjectFile = (FileReference)Info.GetValue("pf", typeof(FileReference));
			AppName = Info.GetString("an");
			TargetName = Info.GetString("tn");
			bUseSharedBuildEnvironment = Info.GetBoolean("sb");
			Platform = (UnrealTargetPlatform)Info.GetInt32("pl");
			Configuration = (UnrealTargetConfiguration)Info.GetInt32("co");
			TargetInfo = (TargetInfo)Info.GetValue("ti", typeof(TargetInfo));
			ProjectDirectory = (DirectoryReference)Info.GetValue("pd", typeof(DirectoryReference));
			ProjectIntermediateDirectory = (DirectoryReference)Info.GetValue("pi", typeof(DirectoryReference));
			EngineIntermediateDirectory = (DirectoryReference)Info.GetValue("ed", typeof(DirectoryReference));
			OutputPaths = (List<FileReference>)Info.GetValue("op", typeof(List<FileReference>));
			RemoteRoot = Info.GetString("rr");
			bPrecompile = Info.GetBoolean("pc");
			bUsePrecompiled = Info.GetBoolean("up");
			bEditorRecompile = Info.GetBoolean("er");
			OnlyModules = (List<OnlyModule>)Info.GetValue("om", typeof(List<OnlyModule>));
			bCompileMonolithic = Info.GetBoolean("cm");
			string[] FlatModuleCsDataKeys = (string[])Info.GetValue("fk", typeof(string[]));
			FlatModuleCsDataType[] FlatModuleCsDataValues = (FlatModuleCsDataType[])Info.GetValue("fv", typeof(FlatModuleCsDataType[]));
			for (int Index = 0; Index != FlatModuleCsDataKeys.Length; ++Index)
			{
				FlatModuleCsData.Add(FlatModuleCsDataKeys[Index], FlatModuleCsDataValues[Index]);
			}
			Receipt = (TargetReceipt)Info.GetValue("re", typeof(TargetReceipt));
			ReceiptFileName = Info.GetString("rf");
			FileReferenceToVersionManifestPairs = (KeyValuePair<FileReference, VersionManifest>[])Info.GetValue("vm", typeof(KeyValuePair<FileReference, VersionManifest>[]));
			TargetCsFilenameField = (FileReference)Info.GetValue("tc", typeof(FileReference));
			PlatformContext = UEBuildPlatform.GetBuildPlatform(Platform).CreateContext(ProjectFile);
			PreBuildStepScripts = (FileReference[])Info.GetValue("pr", typeof(FileReference[]));
			PostBuildStepScripts = (FileReference[])Info.GetValue("po", typeof(FileReference[]));
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("tt", (int)TargetType);
			Info.AddValue("pf", ProjectFile);
			Info.AddValue("an", AppName);
			Info.AddValue("tn", TargetName);
			Info.AddValue("sb", bUseSharedBuildEnvironment);
			Info.AddValue("pl", (int)Platform);
			Info.AddValue("co", (int)Configuration);
			Info.AddValue("ti", TargetInfo);
			Info.AddValue("pd", ProjectDirectory);
			Info.AddValue("pi", ProjectIntermediateDirectory);
			Info.AddValue("ed", EngineIntermediateDirectory);
			Info.AddValue("op", OutputPaths);
			Info.AddValue("rr", RemoteRoot);
			Info.AddValue("pc", bPrecompile);
			Info.AddValue("up", bUsePrecompiled);
			Info.AddValue("er", bEditorRecompile);
			Info.AddValue("om", OnlyModules);
			Info.AddValue("cm", bCompileMonolithic);
			Info.AddValue("fk", FlatModuleCsData.Keys.ToArray());
			Info.AddValue("fv", FlatModuleCsData.Values.ToArray());
			Info.AddValue("re", Receipt);
			Info.AddValue("rf", ReceiptFileName);
			Info.AddValue("vm", FileReferenceToVersionManifestPairs);
			Info.AddValue("tc", TargetCsFilenameField);
			Info.AddValue("pr", PreBuildStepScripts);
			Info.AddValue("po", PostBuildStepScripts);
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InDesc">Target descriptor</param>
		/// <param name="InRules">The target rules, as created by RulesCompiler.</param>
		/// <param name="InPossibleAppName">The AppName for shared binaries of this target type, if used (null if there is none).</param>
		/// <param name="InTargetCsFilename">The name of the target </param>
		public UEBuildTarget(TargetDescriptor InDesc, TargetRules InRules, RulesAssembly InRulesAssembly, string InPossibleAppName, FileReference InTargetCsFilename)
		{
			ProjectFile = InDesc.ProjectFile;
			AppName = InDesc.TargetName;
			TargetName = InDesc.TargetName;
			Platform = InDesc.Platform;
			Configuration = InDesc.Configuration;
			Rules = InRules;
			RulesAssembly = InRulesAssembly;
			TargetType = Rules.Type;
			bEditorRecompile = InDesc.bIsEditorRecompile;
			bPrecompile = InDesc.bPrecompile;
			bUsePrecompiled = InDesc.bUsePrecompiled;
			ForeignPlugins = InDesc.ForeignPlugins;
			ForceReceiptFileName = InDesc.ForceReceiptFileName;
			PlatformContext = InDesc.PlatformContext;

			Debug.Assert(InTargetCsFilename == null || InTargetCsFilename.HasExtension(".Target.cs"));
			TargetCsFilenameField = InTargetCsFilename;

			{
				bCompileMonolithic = Rules.ShouldCompileMonolithic(InDesc.Platform, InDesc.Configuration);

				// Platforms may *require* monolithic compilation...
				bCompileMonolithic |= UEBuildPlatform.PlatformRequiresMonolithicBuilds(InDesc.Platform, InDesc.Configuration);

				// Force monolithic or modular mode if we were asked to
				if (UnrealBuildTool.CommandLineContains("-Monolithic") ||
					UnrealBuildTool.CommandLineContains("MONOLITHIC_BUILD=1"))
				{
					bCompileMonolithic = true;
				}
				else if (UnrealBuildTool.CommandLineContains("-Modular"))
				{
					bCompileMonolithic = false;
				}
			}

			TargetInfo = new TargetInfo(Platform, Configuration, PlatformContext.GetActiveArchitecture(), Rules.Type, bCompileMonolithic);

			if (InPossibleAppName != null && InRules.ShouldUseSharedBuildEnvironment(TargetInfo))
			{
				AppName = InPossibleAppName;
				bUseSharedBuildEnvironment = true;
			}

			// Figure out what the project directory is. If we have a uproject file, use that. Otherwise use the engine directory.
			if (ProjectFile != null)
			{
				ProjectDirectory = ProjectFile.Directory;
			}
			else
			{
				ProjectDirectory = UnrealBuildTool.EngineDirectory;
			}

			// Build the project intermediate directory
			ProjectIntermediateDirectory = DirectoryReference.Combine(ProjectDirectory, BuildConfiguration.PlatformIntermediateFolder, GetTargetName(), Configuration.ToString());

			// Build the engine intermediate directory. If we're building agnostic engine binaries, we can use the engine intermediates folder. Otherwise we need to use the project intermediates directory.
			if (!bUseSharedBuildEnvironment)
			{
				EngineIntermediateDirectory = ProjectIntermediateDirectory;
			}
			else if (Configuration == UnrealTargetConfiguration.DebugGame)
			{
				EngineIntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, BuildConfiguration.PlatformIntermediateFolder, AppName, UnrealTargetConfiguration.Development.ToString());
			}
			else
			{
				EngineIntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, BuildConfiguration.PlatformIntermediateFolder, AppName, Configuration.ToString());
			}

			// Get the receipt path for this target
			ReceiptFileName = TargetReceipt.GetDefaultPath(ProjectDirectory.FullName, TargetName, Platform, Configuration, PlatformContext.GetActiveArchitecture());

			// Read the project descriptor
			if (ProjectFile != null)
			{
				ProjectDescriptor = ProjectDescriptor.FromFile(ProjectFile.FullName);
			}

			RemoteRoot = InDesc.RemoteRoot;

			OnlyModules = InDesc.OnlyModules;

			// Construct the output paths for this target's executable
			DirectoryReference OutputDirectory;
			if ((bCompileMonolithic || TargetType == TargetRules.TargetType.Program) && !Rules.bOutputToEngineBinaries)
			{
				OutputDirectory = ProjectDirectory;
			}
			else
			{
				OutputDirectory = UnrealBuildTool.EngineDirectory;
			}
			OutputPaths = MakeExecutablePaths(OutputDirectory, bCompileMonolithic ? TargetName : AppName, Platform, Configuration, TargetInfo.Architecture, Rules.UndecoratedConfiguration, bCompileMonolithic && ProjectFile != null, Rules.ExeBinariesSubFolder, ProjectFile);

			// handle some special case defines (so build system can pass -DEFINE as normal instead of needing
			// to know about special parameters)
			foreach (string Define in InDesc.AdditionalDefinitions)
			{
				switch (Define)
				{
					case "WITH_EDITOR=0":
						UEBuildConfiguration.bBuildEditor = false;
						break;

					case "WITH_EDITORONLY_DATA=0":
						UEBuildConfiguration.bBuildWithEditorOnlyData = false;
						break;

					// Memory profiler doesn't work if frame pointers are omitted
					case "USE_MALLOC_PROFILER=1":
						BuildConfiguration.bOmitFramePointers = false;
						break;

					case "WITH_LEAN_AND_MEAN_UE=1":
						UEBuildConfiguration.bCompileLeanAndMeanUE = true;
						break;
				}
			}

			// Add the definitions specified on the command-line.
			GlobalCompileEnvironment.Config.Definitions.AddRange(InDesc.AdditionalDefinitions);
		}

		/// <summary>
		/// Attempts to delete a file. Will retry a few times before failing.
		/// </summary>
		/// <param name="Filename"></param>
		public static void CleanFile(string Filename)
		{
			const int RetryDelayStep = 200;
			int RetryDelay = 1000;
			int RetryCount = 10;
			bool bResult = false;
			do
			{
				try
				{
					File.Delete(Filename);
					bResult = true;
				}
				catch (Exception Ex)
				{
					// This happens mostly because some other stale process is still locking this file
					Log.TraceVerbose(Ex.Message);
					if (--RetryCount < 0)
					{
						throw Ex;
					}
					System.Threading.Thread.Sleep(RetryDelay);
					// Try with a slightly longer delay next time
					RetryDelay += RetryDelayStep;
				}
			}
			while (!bResult);
		}

		/// <summary>
		/// Attempts to delete a directory. Will retry a few times before failing.
		/// </summary>
		/// <param name="DirectoryPath"></param>
		void CleanDirectory(string DirectoryPath)
		{
			const int RetryDelayStep = 200;
			int RetryDelay = 1000;
			int RetryCount = 10;
			bool bResult = false;
			do
			{
				try
				{
					Directory.Delete(DirectoryPath, true);
					bResult = true;
				}
				catch (DirectoryNotFoundException)
				{
					// this is ok, someone else may have killed it for us.
					bResult = true;
				}
				catch (Exception Ex)
				{
					// This happens mostly because some other stale process is still locking this file
					Log.TraceVerbose(Ex.Message);
					if (--RetryCount < 0)
					{
						throw Ex;
					}
					System.Threading.Thread.Sleep(RetryDelay);
					// Try with a slightly longer delay next time
					RetryDelay += RetryDelayStep;
				}
			}
			while (!bResult);
		}

		/// <summary>
		/// Cleans UnrealHeaderTool
		/// </summary>
		private void CleanUnrealHeaderTool()
		{
			if (!UnrealBuildTool.IsEngineInstalled())
			{
				StringBuilder UBTArguments = new StringBuilder();

				UBTArguments.Append("UnrealHeaderTool");
				// Which desktop platform do we need to clean UHT for?
				UBTArguments.Append(" " + BuildHostPlatform.Current.Platform.ToString());
				UBTArguments.Append(" " + UnrealTargetConfiguration.Development.ToString());
				// NOTE: We disable mutex when launching UBT from within UBT to clean UHT
				UBTArguments.Append(" -NoMutex -Clean");

				if(UnrealBuildTool.CommandLineContains("-ignorejunk"))
				{
					UBTArguments.Append(" -ignorejunk");
				}

				ExternalExecution.RunExternalExecutable(UnrealBuildTool.GetUBTPath(), UBTArguments.ToString());
			}
		}

		/// <summary>
		/// Cleans all target intermediate files. May also clean UHT if the target uses UObjects.
		/// </summary>
		/// <param name="Manifest">Manifest</param>
		protected void CleanTarget(BuildManifest Manifest)
		{
			// Expand all the paths in the receipt; they'll currently use variables for the engine and project directories
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			// Expand all the paths in the receipt; they'll currently use variables for the engine and project directories
			TargetReceipt ReceiptWithFullPaths;
			if (!TargetReceipt.TryRead(ReceiptFileName, out ReceiptWithFullPaths))
			{
				ReceiptWithFullPaths = new TargetReceipt(Receipt);
			}
			ReceiptWithFullPaths.ExpandPathVariables(UnrealBuildTool.EngineDirectory, ProjectDirectory);

			{
				Log.TraceVerbose("Cleaning target {0} - AppName {1}", TargetName, AppName);

				FileReference TargetFilename = RulesAssembly.GetTargetFileName(TargetName);
				Log.TraceVerbose("\tTargetFilename {0}", TargetFilename);

				// Collect all files to delete.
				string[] AdditionalFileExtensions = new string[] { ".lib", ".exp", ".dll.response" };
				List<string> AllFilesToDelete = new List<string>();
				foreach (BuildProduct BuildProduct in ReceiptWithFullPaths.BuildProducts)
				{
					// If we're cleaning, don't add any precompiled binaries to the manifest. We don't want to delete them.
					if (UEBuildConfiguration.bCleanProject && bUsePrecompiled && BuildProduct.IsPrecompiled)
					{
						continue;
					}

					// if we're cleaning and in an installed binary and the build product path matches the engine install location, we don't want to delete them
					if (UEBuildConfiguration.bCleanProject && UnrealBuildTool.IsEngineInstalled() && BuildProduct.Path.Contains(Path.GetFullPath(BuildConfiguration.RelativeEnginePath)))
					{
						continue;
					}

					// Don't add static libraries into the manifest unless we're explicitly building them; we don't submit them to Perforce.
					if (!UEBuildConfiguration.bCleanProject && !bPrecompile && (BuildProduct.Type == BuildProductType.StaticLibrary || BuildProduct.Type == BuildProductType.ImportLibrary))
					{
						continue;
					}

					AllFilesToDelete.Add(BuildProduct.Path);
					string FileExt = Path.GetExtension(BuildProduct.Path);
					if (FileExt == ".dll" || FileExt == ".exe")
					{
						string ManifestFileWithoutExtension = Utils.GetPathWithoutExtension(BuildProduct.Path);
						foreach (string AdditionalExt in AdditionalFileExtensions)
						{
							string AdditionalFileToDelete = ManifestFileWithoutExtension + AdditionalExt;
							AllFilesToDelete.Add(AdditionalFileToDelete);
						}
					}
				}
				if (OnlyModules.Count == 0)
				{
					AllFilesToDelete.Add(ReceiptFileName);
				}

				//@todo. This does not clean up files that are no longer built by the target...				
				// Delete all output files listed in the manifest as well as any additional files.
				foreach (string FileToDelete in AllFilesToDelete)
				{
					if (File.Exists(FileToDelete))
					{
						Log.TraceVerbose("\t\tDeleting " + FileToDelete);
						CleanFile(FileToDelete);
					}
				}

				// Delete the intermediate folder for each binary. This will catch all plugin intermediate folders, as well as any project and engine folders.
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					if (Binary.Config.IntermediateDirectory.Exists())
					{
						if (!UnrealBuildTool.IsEngineInstalled() || !Binary.Config.IntermediateDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
						{
							CleanDirectory(Binary.Config.IntermediateDirectory.FullName);
						}
					}
				}

				// Generate a list of all the modules of each AppBinaries entry
				List<string> ModuleList = new List<string>();
				bool bTargetUsesUObjectModule = false;
				foreach (UEBuildBinary AppBin in AppBinaries)
				{
					UEBuildBinaryCPP AppBinCPP = AppBin as UEBuildBinaryCPP;
					if (AppBinCPP != null)
					{
						// Collect all modules used by this binary.
						Log.TraceVerbose("\tProcessing AppBinary " + AppBin.Config.OutputFilePaths[0]);
						foreach (string ModuleName in AppBinCPP.Modules.Select(x => x.Name))
						{
							if (ModuleList.Contains(ModuleName) == false)
							{
								Log.TraceVerbose("\t\tModule: " + ModuleName);
								ModuleList.Add(ModuleName);
								if (ModuleName == "CoreUObject")
								{
									bTargetUsesUObjectModule = true;
								}
							}
						}
					}
					else
					{
						Log.TraceVerbose("\t********* Skipping " + AppBin.Config.OutputFilePaths[0]);
					}
				}

				string BaseEngineBuildDataFolder = Path.GetFullPath(BuildConfiguration.BaseIntermediatePath).Replace("\\", "/");
				string PlatformEngineBuildDataFolder = BuildConfiguration.BaseIntermediatePath;

				// Delete generated header files
				foreach (string ModuleName in ModuleList)
				{
					UEBuildModuleCPP Module = GetModuleByName(ModuleName) as UEBuildModuleCPP;
					if (Module != null && Module.GeneratedCodeDirectory != null && Module.GeneratedCodeDirectory.Exists())
					{
						if (!UnrealBuildTool.IsEngineInstalled() || !Module.GeneratedCodeDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
						{
							Log.TraceVerbose("\t\tDeleting " + Module.GeneratedCodeDirectory);
							CleanDirectory(Module.GeneratedCodeDirectory.FullName);
						}
					}
				}


				//
				{
					string AppEnginePath = Path.Combine(PlatformEngineBuildDataFolder, TargetName, Configuration.ToString());
					if (Directory.Exists(AppEnginePath))
					{
						CleanDirectory(AppEnginePath);
					}
				}

				// Clean the intermediate directory
				if (ProjectIntermediateDirectory != null && (!UnrealBuildTool.IsEngineInstalled() || !ProjectIntermediateDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory)))
				{
					if (ProjectIntermediateDirectory.Exists())
					{
						CleanDirectory(ProjectIntermediateDirectory.FullName);
					}
				}
				if (!UnrealBuildTool.IsEngineInstalled())
				{
					// This is always under Engine installation folder
					if (GlobalLinkEnvironment.Config.IntermediateDirectory.Exists())
					{
						Log.TraceVerbose("\tDeleting " + GlobalLinkEnvironment.Config.IntermediateDirectory);
						CleanDirectory(GlobalLinkEnvironment.Config.IntermediateDirectory.FullName);
					}
				}
				else if (ShouldCompileMonolithic())
				{
					// Only in monolithic, otherwise it's pointing to Engine installation folder
					if (GlobalLinkEnvironment.Config.OutputDirectory.Exists())
					{
						Log.TraceVerbose("\tDeleting " + GlobalLinkEnvironment.Config.OutputDirectory);
						CleanDirectory(GlobalCompileEnvironment.Config.OutputDirectory.FullName);
					}
				}

				// Delete the dependency caches
				{
					FileReference FlatCPPIncludeDependencyCacheFilename = FlatCPPIncludeDependencyCache.GetDependencyCachePathForTarget(this);
					if (FlatCPPIncludeDependencyCacheFilename.Exists())
					{
						Log.TraceVerbose("\tDeleting " + FlatCPPIncludeDependencyCacheFilename);
						CleanFile(FlatCPPIncludeDependencyCacheFilename.FullName);
					}
					FileReference DependencyCacheFilename = DependencyCache.GetDependencyCachePathForTarget(this);
					if (DependencyCacheFilename.Exists())
					{
						Log.TraceVerbose("\tDeleting " + DependencyCacheFilename);
						CleanFile(DependencyCacheFilename.FullName);
					}
				}

				// Delete the UBT makefile
				{
					// @todo ubtmake: Does not yet support cleaning Makefiles that were generated for multiple targets (that code path is currently not used though.)

					List<TargetDescriptor> TargetDescs = new List<TargetDescriptor>();
					TargetDescs.Add(new TargetDescriptor
						{
							TargetName = GetTargetName(),
							Platform = this.Platform,
							Configuration = this.Configuration
						});

					// Normal makefile
					{
						FileReference UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath(TargetDescs);
						if (UBTMakefilePath.Exists())
						{
							Log.TraceVerbose("\tDeleting " + UBTMakefilePath);
							CleanFile(UBTMakefilePath.FullName);
						}
					}

					// Hot reload makefile
					{
						UEBuildConfiguration.bHotReloadFromIDE = true;
						FileReference UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath(TargetDescs);
						if (UBTMakefilePath.Exists())
						{
							Log.TraceVerbose("\tDeleting " + UBTMakefilePath);
							CleanFile(UBTMakefilePath.FullName);
						}
						UEBuildConfiguration.bHotReloadFromIDE = false;
					}
				}

				// Delete the action history
				{
					FileReference ActionHistoryFilename = ActionHistory.GeneratePathForTarget(this);
					if (ActionHistoryFilename.Exists())
					{
						Log.TraceVerbose("\tDeleting " + ActionHistoryFilename);
						CleanFile(ActionHistoryFilename.FullName);
					}
				}

				// Finally clean UnrealHeaderTool if this target uses CoreUObject modules and we're not cleaning UHT already
				// and we want UHT to be cleaned.
				if (!UEBuildConfiguration.bDoNotBuildUHT && bTargetUsesUObjectModule && GetTargetName() != "UnrealHeaderTool")
				{
					CleanUnrealHeaderTool();
				}
			}

		}

		class BuldProductComparer : IEqualityComparer<BuildProduct>
		{
			public bool Equals(BuildProduct x, BuildProduct y)
			{
				if (Object.ReferenceEquals(x, y))
					return true;

				if (Object.ReferenceEquals(x, null) || Object.ReferenceEquals(y, null))
					return false;

				return x.Path == y.Path && x.Type == y.Type;
			}

			public int GetHashCode(BuildProduct product)
			{
				if (Object.ReferenceEquals(product, null))
					return 0;

				int hashProductPath = product.Path == null ? 0 : product.Path.GetHashCode();
				int hashProductType = product.Type.GetHashCode();
				return hashProductPath ^ hashProductType;
			}
		}
		/// <summary>
		/// Cleans all removed module intermediate files
		/// </summary>
		public void CleanStaleModules()
		{
			// Expand all the paths in the receipt; they'll currently use variables for the engine and project directories
			TargetReceipt ReceiptWithFullPaths = new TargetReceipt(Receipt);
			ReceiptWithFullPaths.ExpandPathVariables(UnrealBuildTool.EngineDirectory, ProjectDirectory);
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			// Expand all the paths in the receipt; they'll currently use variables for the engine and project directories
			TargetReceipt BuiltReceiptWithFullPaths;
			if (!TargetReceipt.TryRead(ReceiptFileName, out BuiltReceiptWithFullPaths))
			{
				return;
			}
			BuiltReceiptWithFullPaths.ExpandPathVariables(UnrealBuildTool.EngineDirectory, ProjectDirectory);

			{
				// Collect all files to build
				string[] AdditionalFileExtensions = new string[] { ".lib", ".exp", ".dll.response" };
				List<string> AllFilesToDelete = new List<string>();
				foreach (BuildProduct BuildProduct in BuiltReceiptWithFullPaths.BuildProducts)
				{
					// don't add any precompiled binaries to the manifest. We don't want to delete them.
					if (bUsePrecompiled && BuildProduct.IsPrecompiled)
					{
						continue;
					}

					// don't add any installed binary and the build product path matches the engine install location, we don't want to delete them
					if (UnrealBuildTool.IsEngineInstalled() && BuildProduct.Path.Contains(Path.GetFullPath(BuildConfiguration.RelativeEnginePath)))
					{
						continue;
					}

					// Don't add static libraries into the manifest unless we're explicitly building them; we don't submit them to Perforce.
					if (!UEBuildConfiguration.bCleanProject && !bPrecompile && (BuildProduct.Type == BuildProductType.StaticLibrary || BuildProduct.Type == BuildProductType.ImportLibrary))
					{
						continue;
					}

					if (!ReceiptWithFullPaths.BuildProducts.Contains(BuildProduct, new UEBuildTarget.BuldProductComparer()))
					{
						AllFilesToDelete.Add(BuildProduct.Path);
						string FileExt = Path.GetExtension(BuildProduct.Path);
						if (FileExt == ".dll" || FileExt == ".exe")
						{
							string ManifestFileWithoutExtension = Utils.GetPathWithoutExtension(BuildProduct.Path);
							foreach (string AdditionalExt in AdditionalFileExtensions)
							{
								string AdditionalFileToDelete = ManifestFileWithoutExtension + AdditionalExt;
								AllFilesToDelete.Add(AdditionalFileToDelete);
							}
						}
					}
				}

				//@todo. This does not clean up files that are no longer built by the target...				
				// Delete all output files listed in the manifest as well as any additional files.
				foreach (string FileToDelete in AllFilesToDelete)
				{
					if (File.Exists(FileToDelete))
					{
						Log.TraceVerbose("\t\tDeleting " + FileToDelete);
						CleanFile(FileToDelete);
					}
				}

				// delete intermediate directory of the extra files
			}

			// The engine updates the PATH environment variable to supply all valid locations for DLLs, but the Windows loader reads imported DLLs from the first location it finds them. 
			// If modules are moved from one place to another, we have to be sure to clean up the old versions so that they're not loaded accidentally causing unintuitive import errors.
			HashSet<FileReference> OutputFiles = new HashSet<FileReference>();
			Dictionary<string, FileReference> OutputFileNames = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);
			foreach(UEBuildBinary Binary in AppBinaries)
			{
				foreach(FileReference OutputFile in Binary.Config.OutputFilePaths)
				{
					OutputFiles.Add(OutputFile);
					OutputFileNames[OutputFile.GetFileName()] = OutputFile;
				}
			}

			// Search all the output directories for files with a name matching one of our output files
			foreach(DirectoryReference OutputDirectory in OutputFiles.Select(x => x.Directory).Distinct())
			{
				foreach(FileReference ExistingFile in OutputDirectory.EnumerateFileReferences())
				{
					FileReference OutputFile;
					if(OutputFileNames.TryGetValue(ExistingFile.GetFileName(), out OutputFile) && !OutputFiles.Contains(ExistingFile))
					{
						Log.TraceInformation("Deleting '{0}' to avoid ambiguity with '{1}'", ExistingFile, OutputFile);
						CleanFile(ExistingFile.FullName);
					}
				}
			}
		}

		/// <summary>
		/// Generates a list of external files which are required to build this target
		/// </summary>
		public void GenerateExternalFileList()
		{
			string FileListPath = "../Intermediate/Build/ExternalFiles.xml";

			// Create a set of filenames
			HashSet<FileReference> Files = new HashSet<FileReference>();
			GetExternalFileList(Files);

			// Normalize all the filenames
			HashSet<string> NormalizedFileNames = new HashSet<string>(StringComparer.CurrentCultureIgnoreCase);
			foreach (string FileName in Files.Select(x => x.FullName))
			{
				string NormalizedFileName = FileName.Replace('\\', '/');
				NormalizedFileNames.Add(NormalizedFileName);
			}

			// Add the existing filenames
			if (UEBuildConfiguration.bMergeExternalFileList)
			{
				foreach (string FileName in Utils.ReadClass<ExternalFileList>(FileListPath).FileNames)
				{
					NormalizedFileNames.Add(FileName);
				}
			}

			// Write the output list
			ExternalFileList FileList = new ExternalFileList();
			FileList.FileNames.AddRange(NormalizedFileNames);
			FileList.FileNames.Sort();
			Utils.WriteClass<ExternalFileList>(FileList, FileListPath, "");
		}

		/// <summary>
		/// Create a list of all the externally referenced files
		/// </summary>
		/// <param name="Files">Set of referenced files</param>
		void GetExternalFileList(HashSet<FileReference> Files)
		{
			// Find all the modules we depend on
			HashSet<UEBuildModule> Modules = new HashSet<UEBuildModule>();
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				foreach (UEBuildModule Module in Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false))
				{
					Modules.Add(Module);
				}
			}

			// Get the platform we're building for
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			foreach (UEBuildModule Module in Modules)
			{
				// Create the module rules
				FileReference ModuleRulesFileName;
				ModuleRules Rules = CreateModuleRulesAndSetDefaults(Module.Name, out ModuleRulesFileName);

				// Add Additional Bundle Resources for all modules
				foreach (UEBuildBundleResource Resource in Rules.AdditionalBundleResources)
				{
					if (Directory.Exists(Resource.ResourcePath))
					{
						Files.UnionWith(new DirectoryReference(Resource.ResourcePath).EnumerateFileReferences("*", SearchOption.AllDirectories));
					}
					else
					{
						Files.Add(new FileReference(Resource.ResourcePath));
					}
				}

				// Add any zip files from Additional Frameworks
				foreach (UEBuildFramework Framework in Rules.PublicAdditionalFrameworks)
				{
					if (!String.IsNullOrEmpty(Framework.FrameworkZipPath))
					{
						Files.Add(FileReference.Combine(Module.ModuleDirectory, Framework.FrameworkZipPath));
					}
				}

				// Add all the include paths etc. for external modules
				UEBuildExternalModule ExternalModule = Module as UEBuildExternalModule;
				if (ExternalModule != null)
				{
					// Add the rules file itself
					Files.Add(ModuleRulesFileName);

					// Get a list of all the library paths
					List<string> LibraryPaths = new List<string>();
					LibraryPaths.Add(Directory.GetCurrentDirectory());
					LibraryPaths.AddRange(Rules.PublicLibraryPaths.Where(x => !x.StartsWith("$(")).Select(x => Path.GetFullPath(x.Replace('/', '\\'))));

					// Get all the extensions to look for
					List<string> LibraryExtensions = new List<string>();
					LibraryExtensions.Add(BuildPlatform.GetBinaryExtension(UEBuildBinaryType.StaticLibrary));
					LibraryExtensions.Add(BuildPlatform.GetBinaryExtension(UEBuildBinaryType.DynamicLinkLibrary));

					// Add all the libraries
					foreach (string LibraryExtension in LibraryExtensions)
					{
						foreach (string LibraryName in Rules.PublicAdditionalLibraries)
						{
							foreach (string LibraryPath in LibraryPaths)
							{
								string LibraryFileName = Path.Combine(LibraryPath, LibraryName);
								if (File.Exists(LibraryFileName))
								{
									Files.Add(new FileReference(LibraryFileName));
								}

								string UnixLibraryFileName = Path.Combine(LibraryPath, "lib" + LibraryName + LibraryExtension);
								if (File.Exists(UnixLibraryFileName))
								{
									Files.Add(new FileReference(UnixLibraryFileName));
								}
							}
						}
					}

					// Add all the additional shadow files
					foreach (string AdditionalShadowFile in Rules.PublicAdditionalShadowFiles)
					{
						string ShadowFileName = Path.GetFullPath(AdditionalShadowFile);
						if (File.Exists(ShadowFileName))
						{
							Files.Add(new FileReference(ShadowFileName));
						}
					}

					// Find all the include paths
					List<string> AllIncludePaths = new List<string>();
					AllIncludePaths.AddRange(Rules.PublicIncludePaths);
					AllIncludePaths.AddRange(Rules.PublicSystemIncludePaths);

					// Add all the include paths
					foreach (string IncludePath in AllIncludePaths.Where(x => !x.StartsWith("$(")))
					{
						if (Directory.Exists(IncludePath))
						{
							foreach (string IncludeFileName in Directory.EnumerateFiles(IncludePath, "*", SearchOption.AllDirectories))
							{
								string Extension = Path.GetExtension(IncludeFileName).ToLower();
								if (Extension == ".h" || Extension == ".inl")
								{
									Files.Add(new FileReference(IncludeFileName));
								}
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Lists the folders involved in the build of this target. Outputs a Json file
		/// that lists pertitent info about each folder involved in the build.
		/// </summary>
		public void ListBuildFolders()
		{
			// local function that takes a RuntimeDependency path and resolves it (replacing Env vars that we support)
			Func<string, DirectoryReference> ResolveRuntimeDependencyFolder = (string DependencyPath) =>
			{
				return new DirectoryReference(Path.GetDirectoryName(
					// Regex to replace the env vars we support $(EngineDir|ProjectDir), ignoring case
					Regex.Replace(DependencyPath, @"\$\((?<Type>Engine|Project)Dir\)", M => 
						M.Groups["Type"].Value.Equals("Engine", StringComparison.InvariantCultureIgnoreCase)
							? UnrealBuildTool.EngineDirectory.FullName 
							: ProjectDirectory.FullName,
					RegexOptions.IgnoreCase)));
			};

			var Start = DateTime.UtcNow;
			// Create a set of directories used for each binary
			var DirectoriesToScan = AppBinaries
				// get a flattened list of modules in all the binaries
				.SelectMany(Binary => Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: true, bForceCircular: false))
				// remove duplicate modules
				.Distinct()
				// get all directories that the module uses (source folder and any runtime dependencies)
				.SelectMany(Module => Module
					// resolve any runtime dependency folders and add them.
					.RuntimeDependencies.Select(Dependency => ResolveRuntimeDependencyFolder(Dependency.Path))
					// Add on the module source directory
					.Concat(new[] { Module.ModuleDirectory }))
				// remove any duplicate folders since some modules may be from the same plugin
				.Distinct()
				// Project to a list as we need to do an O(n^2) operation below.
				.ToList();

			DirectoriesToScan.Where(RemovalCandidate =>
				// O(n^2) search to remove subfolders of any we are already searching.
				// look for directories that aren't subdirectories of any other directory in the list.
				!DirectoriesToScan.Any(DirectoryToScan =>
					// != check because this inner loop will eventually check against itself
					RemovalCandidate != DirectoryToScan &&
					RemovalCandidate.IsUnderDirectory(DirectoryToScan)))
				// grab the full name
				.Select(Dir=>Dir.FullName)
				// sort the final output
				.OrderBy(Dir=> Dir)
				// log the folders
				.ToList().ForEach(Dir => Log.TraceInformation("BuildFolder:{0}", Dir));

			var Finish = DateTime.UtcNow;
			Log.TraceInformation("Took {0} sec to list build folders.", (Finish - Start).TotalSeconds);
		}

		/// <summary>
		/// Generates a public manifest file for writing out
		/// </summary>
		public void GenerateManifest()
		{
			FileReference ManifestPath;
			if (UnrealBuildTool.IsEngineInstalled() && ProjectFile != null)
			{
				ManifestPath = FileReference.Combine(ProjectFile.Directory, BuildConfiguration.BaseIntermediateFolder, "Manifest.xml");
			}
			else
			{
				ManifestPath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Manifest.xml");
			}

			BuildManifest Manifest = new BuildManifest();
			if (UEBuildConfiguration.bMergeManifests)
			{
				// Load in existing manifest (if any)
				Manifest = Utils.ReadClass<BuildManifest>(ManifestPath.FullName);
			}

			if(!BuildConfiguration.bEnableCodeAnalysis)
			{
				// Expand all the paths in the receipt; they'll currently use variables for the engine and project directories
				TargetReceipt ReceiptWithFullPaths = new TargetReceipt(Receipt);
				ReceiptWithFullPaths.ExpandPathVariables(UnrealBuildTool.EngineDirectory, ProjectDirectory);

				foreach (BuildProduct BuildProduct in ReceiptWithFullPaths.BuildProducts)
				{
					// If we're cleaning, don't add any precompiled binaries to the manifest. We don't want to delete them.
					if (UEBuildConfiguration.bCleanProject && bUsePrecompiled && BuildProduct.IsPrecompiled)
					{
						continue;
					}

					// Don't add static libraries into the manifest unless we're explicitly building them; we don't submit them to Perforce.
					if (!UEBuildConfiguration.bCleanProject && !bPrecompile && (BuildProduct.Type == BuildProductType.StaticLibrary || BuildProduct.Type == BuildProductType.ImportLibrary))
					{
						Manifest.LibraryBuildProducts.Add(BuildProduct.Path);
					}
					else
					{
						Manifest.AddBuildProduct(BuildProduct.Path);
					}
				}

				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
				if (OnlyModules.Count == 0)
				{
					Manifest.AddBuildProduct(ReceiptFileName);
				}
			}

			if (UEBuildConfiguration.bCleanProject)
			{
				CleanTarget(Manifest);
			}
			if (UEBuildConfiguration.bGenerateManifest)
			{
				Utils.WriteClass<BuildManifest>(Manifest, ManifestPath.FullName, "");
			}
		}

		/// <summary>
		/// Prepare all the receipts this target (all the .target and .modules files). See the VersionManifest class for an explanation of what these files are.
		/// </summary>
		public void PrepareReceipts(UEToolChain ToolChain)
		{
			// Read the version file
			BuildVersion Version;
			if (!BuildVersion.TryRead(Path.Combine(BuildConfiguration.RelativeEnginePath, "Build", "Build.version"), out Version))
			{
				Version = new BuildVersion();
			}

			// Create a unique identifier for this build, which can be used to identify modules when the changelist is constant. It's fine to share this between runs with the same makefile; 
			// the output won't change. By default we leave it blank when compiling a subset of modules (for hot reload, etc...), otherwise it won't match anything else. When writing to a directory
			// that already contains a manifest, we'll reuse the build id that's already in there (see below).
			string BuildId = (OnlyModules.Count == 0 && !bEditorRecompile) ? Guid.NewGuid().ToString() : "";

			// Find all the build products and modules from this binary
			Receipt = new TargetReceipt(TargetName, Platform, Configuration, BuildId, Version);
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				// Get all the build products for this binary
				Dictionary<FileReference, BuildProductType> BuildProducts = new Dictionary<FileReference, BuildProductType>();
				Binary.GetBuildProducts(ToolChain, BuildProducts);

				// Add them to the receipt
				foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProducts)
				{
					string NormalizedPath = TargetReceipt.InsertPathVariables(BuildProductPair.Key, UnrealBuildTool.EngineDirectory, ProjectDirectory);
					BuildProduct BuildProduct = Receipt.AddBuildProduct(NormalizedPath, BuildProductPair.Value);
					BuildProduct.IsPrecompiled = !Binary.Config.bAllowCompilation;
				}
			}

			// Add the project file
			if(ProjectFile != null)
			{
				string NormalizedPath = TargetReceipt.InsertPathVariables(ProjectFile, UnrealBuildTool.EngineDirectory, ProjectDirectory);
				Receipt.RuntimeDependencies.Add(NormalizedPath, StagedFileType.UFS);
			}

			// Add the descriptors for all enabled plugins
			foreach(PluginInfo EnabledPlugin in EnabledPlugins)
			{
				string SourcePath = TargetReceipt.InsertPathVariables(EnabledPlugin.File, UnrealBuildTool.EngineDirectory, ProjectDirectory);
				Receipt.RuntimeDependencies.Add(SourcePath, StagedFileType.UFS);
			}

			// Add slate runtime dependencies
            if (Rules.bUsesSlate)
            {
				Receipt.RuntimeDependencies.Add("$(EngineDir)/Content/Slate/...", StagedFileType.UFS);
				if(ProjectFile != null)
				{
					Receipt.RuntimeDependencies.Add("$(ProjectDir)/Content/Slate/...", StagedFileType.UFS);
				}
				if (Rules.bUsesSlateEditorStyle)
				{
					Receipt.RuntimeDependencies.Add("$(EngineDir)/Content/Editor/Slate/...", StagedFileType.UFS);
				}
			}

			// Find all the modules which are part of this target
			HashSet<UEBuildModule> UniqueLinkedModules = new HashSet<UEBuildModule>();
			foreach (UEBuildBinaryCPP Binary in AppBinaries.OfType<UEBuildBinaryCPP>())
			{
				if (!PrecompiledBinaries.Contains(Binary))
				{
					foreach (UEBuildModule Module in Binary.Modules)
					{
						if (UniqueLinkedModules.Add(Module))
						{
							foreach (RuntimeDependency RuntimeDependency in Module.RuntimeDependencies)
							{
								string SourcePath = TargetReceipt.InsertPathVariables(RuntimeDependency.Path, UnrealBuildTool.EngineDirectory, ProjectDirectory);
								Receipt.RuntimeDependencies.Add(SourcePath, RuntimeDependency.Type);
							}
							Receipt.AdditionalProperties.AddRange(Module.Rules.AdditionalPropertiesForReceipt);
						}
					}
				}
			}

			// Add any dependencies of precompiled modules into the receipt
			if(bPrecompile)
			{
				// Add the runtime dependencies of precompiled modules that are not directly part of this target
				foreach (UEBuildBinaryCPP Binary in AppBinaries.OfType<UEBuildBinaryCPP>())
				{
					if(PrecompiledBinaries.Contains(Binary))
					{
						foreach (UEBuildModule Module in Binary.Modules)
						{
							if (UniqueLinkedModules.Add(Module))
							{
								foreach (RuntimeDependency RuntimeDependency in Module.RuntimeDependencies)
								{
									// Ignore project-relative dependencies when we're compiling targets without projects - we won't be able to resolve them.
									if(ProjectFile != null || RuntimeDependency.Path.IndexOf("$(ProjectDir)", StringComparison.InvariantCultureIgnoreCase) == -1)
									{
										string SourcePath = TargetReceipt.InsertPathVariables(RuntimeDependency.Path, UnrealBuildTool.EngineDirectory, ProjectDirectory);
										Receipt.PrecompiledRuntimeDependencies.Add(SourcePath);
									}
								}
							}
						}
					}
				}

				// Add all the files which are required to use the precompiled modules
				HashSet<FileReference> ExternalFiles = new HashSet<FileReference>();
				GetExternalFileList(ExternalFiles);

				// Convert them into relative to the target receipt
				foreach(FileReference ExternalFile in ExternalFiles)
				{
					if(ExternalFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) || ExternalFile.IsUnderDirectory(ProjectDirectory))
					{
						string VariablePath = TargetReceipt.InsertPathVariables(ExternalFile, UnrealBuildTool.EngineDirectory, ProjectDirectory);
						Receipt.PrecompiledBuildDependencies.Add(VariablePath);
					}
				}
			}

			// Prepare all the version manifests
			Dictionary<FileReference, VersionManifest> FileNameToVersionManifest = new Dictionary<FileReference, VersionManifest>();
			if (!bCompileMonolithic)
			{
				// Create the receipts for each folder
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					if(Binary.Config.Type == UEBuildBinaryType.DynamicLinkLibrary)
					{
						DirectoryReference DirectoryName = Binary.Config.OutputFilePath.Directory;
						bool bIsGameDirectory = !DirectoryName.IsUnderDirectory(UnrealBuildTool.EngineDirectory);
						FileReference ManifestFileName = FileReference.Combine(DirectoryName, VersionManifest.GetStandardFileName(AppName, Platform, Configuration, PlatformContext.GetActiveArchitecture(), bIsGameDirectory));

						VersionManifest Manifest;
						if (!FileNameToVersionManifest.TryGetValue(ManifestFileName, out Manifest))
						{
							Manifest = new VersionManifest(Version.Changelist, BuildId);

							VersionManifest ExistingManifest;
							if (VersionManifest.TryRead(ManifestFileName.FullName, out ExistingManifest) && Version.Changelist == ExistingManifest.Changelist)
							{
								if (OnlyModules.Count > 0)
								{
									// We're just building an existing module; reuse the existing manifest AND build id.
									Manifest = ExistingManifest;
								}
								else if (Version.Changelist != 0)
								{
									// We're rebuilding at the same changelist. Keep all the existing binaries.
									Manifest.ModuleNameToFileName.Union(ExistingManifest.ModuleNameToFileName);
								}
							}

							FileNameToVersionManifest.Add(ManifestFileName, Manifest);
						}

						foreach (string ModuleName in Binary.Config.ModuleNames)
						{
							Manifest.ModuleNameToFileName[ModuleName] = Binary.Config.OutputFilePath.GetFileName();
						}
					}
				}
			}
			FileReferenceToVersionManifestPairs = FileNameToVersionManifest.ToArray();
		}

		/// <summary>
		/// We generate new version manifests with a unique build id for every build by default, which prevents the engine opportunistically trying to load stale modules
		/// discovered through directory searches. If we're not updating any files in the engine folder, we can safely recycle the existing build id without requiring a 
		/// new UBT invocation to update it when switching between projects.
		/// </summary>
		public void RecycleVersionManifests(bool bModifyingEngineFiles)
		{
			if (FileReferenceToVersionManifestPairs != null)
			{
				if (bModifyingEngineFiles)
				{
					// Delete all the existing manifests, so we don't try to recycle partial builds in future (the current build may fail after modifying engine files, 
					// causing bModifyingEngineFiles to be incorrect on the next invocation).
					foreach (KeyValuePair<FileReference, VersionManifest> FileNameToVersionManifest in FileReferenceToVersionManifestPairs)
					{
						FileNameToVersionManifest.Key.Delete();
					}
				}
				else if(OutputPaths.Count == 1 && OutputPaths[0].IsUnderDirectory(UnrealBuildTool.EngineDirectory))
				{
					// Get the path to the manifest for the base executable. AppBinaries may have precompiled binaries removed, so we use the target's output path for the executable instead.
					FileReference ManifestFileName = FileReference.Combine(OutputPaths[0].Directory, VersionManifest.GetStandardFileName(AppName, Platform, Configuration, PlatformContext.GetActiveArchitecture(), false));

					// Try to read it and update all the other manifests with the same build id
					VersionManifest BaseManifest;
					if (VersionManifest.TryRead(ManifestFileName.FullName, out BaseManifest))
					{
						foreach (KeyValuePair<FileReference, VersionManifest> FileNameToVersionManifest in FileReferenceToVersionManifestPairs)
						{
							FileNameToVersionManifest.Value.BuildId = BaseManifest.BuildId;
						}
					}
				}
			}
		}

		/// <summary>
		/// Writes out the version manifest
		/// </summary>
		public void WriteReceipts()
		{
			if (Receipt != null)
			{
				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
				if (OnlyModules == null || OnlyModules.Count == 0)
				{
					Directory.CreateDirectory(Path.GetDirectoryName(ReceiptFileName));
					Receipt.Write(ReceiptFileName);
				}
				if (ForceReceiptFileName != null)
				{
					Directory.CreateDirectory(Path.GetDirectoryName(ForceReceiptFileName));
					Receipt.Write(ForceReceiptFileName);
				}
			}
			if (FileReferenceToVersionManifestPairs != null)
			{
				foreach (KeyValuePair<FileReference, VersionManifest> FileNameToVersionManifest in FileReferenceToVersionManifestPairs)
				{
					FileNameToVersionManifest.Value.Write(FileNameToVersionManifest.Key.FullName);
				}
			}
		}

		/// <summary>
		/// Gathers dependency modules for given binaries list.
		/// </summary>
		/// <param name="Binaries">Binaries list.</param>
		/// <returns>Dependency modules set.</returns>
		static HashSet<UEBuildModuleCPP> GatherDependencyModules(List<UEBuildBinary> Binaries)
		{
			HashSet<UEBuildModuleCPP> Output = new HashSet<UEBuildModuleCPP>();

			foreach (UEBuildBinary Binary in Binaries)
			{
				List<UEBuildModule> DependencyModules = Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
				foreach (UEBuildModuleCPP Module in DependencyModules.OfType<UEBuildModuleCPP>())
				{
					if (Module.Binary != null)
					{
						Output.Add(Module);
					}
				}
			}

			return Output;
		}

		/// <summary>
		/// Builds the target, appending list of output files and returns building result.
		/// </summary>
		public ECompilationResult Build(UEToolChain TargetToolChain, out List<FileItem> OutputItems, out List<UHTModuleInfo> UObjectModules)
		{
			OutputItems = new List<FileItem>();
			UObjectModules = new List<UHTModuleInfo>();

			PreBuildSetup(TargetToolChain);

			if(!ProjectFileGenerator.bGenerateProjectFiles)
			{
				CheckForEULAViolation();
			}

			// Execute the pre-build steps
			if(!ExecuteCustomPreBuildSteps())
			{
				return ECompilationResult.OtherCompilationError;
			}

			// If we're compiling monolithic, make sure the executable knows about all referenced modules
			if (ShouldCompileMonolithic())
			{
				UEBuildBinary ExecutableBinary = AppBinaries[0];

				// Add all the modules that the executable depends on. Plugins will be already included in this list.
				List<UEBuildModule> AllReferencedModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: true, bForceCircular: true);
				foreach (UEBuildModule CurModule in AllReferencedModules)
				{
					if (CurModule.Binary == null || CurModule.Binary == ExecutableBinary || CurModule.Binary.Config.Type == UEBuildBinaryType.StaticLibrary)
					{
						ExecutableBinary.AddModule(CurModule);
					}
				}

				bool IsCurrentPlatform;
				if (Utils.IsRunningOnMono)
				{
					IsCurrentPlatform = Platform == UnrealTargetPlatform.Mac;
				}
				else
				{
					IsCurrentPlatform = Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Win32;
				}

				if ((TargetRules.IsAGame(TargetType) || (TargetType == TargetRules.TargetType.Server))
					&& IsCurrentPlatform)
				{
					// The hardcoded engine directory needs to be a relative path to match the normal EngineDir format. Not doing so breaks the network file system (TTP#315861).
					string OutputFilePath = ExecutableBinary.Config.OutputFilePath.FullName;
					if (Platform == UnrealTargetPlatform.Mac && OutputFilePath.Contains(".app/Contents/MacOS"))
					{
						OutputFilePath = OutputFilePath.Substring(0, OutputFilePath.LastIndexOf(".app/Contents/MacOS") + 4);
					}
					string EnginePath = Utils.CleanDirectorySeparators(Utils.MakePathRelativeTo(ProjectFileGenerator.EngineRelativePath, Path.GetDirectoryName(OutputFilePath)), '/');
					if (EnginePath.EndsWith("/") == false)
					{
						EnginePath += "/";
					}
					GlobalCompileEnvironment.Config.Definitions.Add("UE_ENGINE_DIRECTORY=" + EnginePath);
				}

				// Set the define for the project name. This allows the executable to locate the correct project file to use, which may not be the same as the game name or target.
				if (ProjectFile != null)
				{
					string ProjectName = ProjectFile.GetFileNameWithoutExtension();
					GlobalCompileEnvironment.Config.Definitions.Add(String.Format("UE_PROJECT_NAME={0}", ProjectName));
				}
			}

			// On Mac and Linux we have actions that should be executed after all the binaries are created
			if (GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Mac || GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Linux)
			{
				TargetToolChain.SetupBundleDependencies(AppBinaries, TargetName);
			}

			// Generate the external file list 
			if (UEBuildConfiguration.bGenerateExternalFileList)
			{
				GenerateExternalFileList();
				return ECompilationResult.Succeeded;
			}

			// Generate the TPS list
			if(UEBuildConfiguration.bListBuildFolders)
			{
				ListBuildFolders();
				return ECompilationResult.Succeeded;
			}

			// Create a receipt for the target
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				PrepareReceipts(TargetToolChain);
			}

			// If we're only generating the manifest, return now
			if (UEBuildConfiguration.bGenerateManifest || UEBuildConfiguration.bCleanProject)
			{
				GenerateManifest();
				if (!BuildConfiguration.bXGEExport)
				{
					return ECompilationResult.Succeeded;
				}
			}

			// We don't need to worry about shared PCH headers when only generating project files.  This is
			// just an optimization
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				GlobalCompileEnvironment.SharedPCHHeaderFiles = FindSharedPCHHeaders();
			}

			if ((BuildConfiguration.bXGEExport && UEBuildConfiguration.bGenerateManifest) || (!UEBuildConfiguration.bGenerateManifest && !UEBuildConfiguration.bCleanProject && !ProjectFileGenerator.bGenerateProjectFiles))
			{
				// Reconstruct a full list of binaries. Binaries which aren't compiled are stripped out of AppBinaries, but we still need to scan them for UHT.
				List<UEBuildBinary> AllAppBinaries = AppBinaries.Union(PrecompiledBinaries).ToList();

				HashSet<UEBuildModuleCPP> ModulesToGenerateHeadersFor = GatherDependencyModules(AllAppBinaries);

				if (OnlyModules.Count > 0)
				{
					HashSet<UEBuildModuleCPP> CorrectlyOrderedModules = GatherDependencyModules(NonFilteredModules);

					CorrectlyOrderedModules.RemoveWhere((Module) => !ModulesToGenerateHeadersFor.Contains(Module));
					ModulesToGenerateHeadersFor = CorrectlyOrderedModules;
				}

				ExternalExecution.SetupUObjectModules(ModulesToGenerateHeadersFor, this, GlobalCompileEnvironment, UObjectModules, FlatModuleCsData, Rules.GetGeneratedCodeVersion());

				// NOTE: Even in Gather mode, we need to run UHT to make sure the files exist for the static action graph to be setup correctly.  This is because UHT generates .cpp
				// files that are injected as top level prerequisites.  If UHT only emitted included header files, we wouldn't need to run it during the Gather phase at all.
				if (UObjectModules.Count > 0)
				{
					// Execute the header tool
					FileReference ModuleInfoFileName = FileReference.Combine(ProjectIntermediateDirectory, GetTargetName() + ".uhtmanifest");
					ECompilationResult UHTResult = ECompilationResult.OtherCompilationError;
					if (!ExternalExecution.ExecuteHeaderToolIfNecessary(TargetToolChain, this, GlobalCompileEnvironment, UObjectModules, ModuleInfoFileName, ref UHTResult))
					{
						Log.TraceInformation("UnrealHeaderTool failed for target '" + GetTargetName() + "' (platform: " + Platform.ToString() + ", module info: " + ModuleInfoFileName + ").");
						return UHTResult;
					}
				}
			}

			if (ShouldCompileMonolithic() && !ProjectFileGenerator.bGenerateProjectFiles && Rules != null && TargetType != TargetRules.TargetType.Program)
			{
				// All non-program monolithic binaries implicitly depend on all static plugin libraries so they are always linked appropriately
				// In order to do this, we create a new module here with a cpp file we emit that invokes an empty function in each library.
				// If we do not do this, there will be no static initialization for libs if no symbols are referenced in them.
				CreateLinkerFixupsCPPFile();
			}

			GlobalLinkEnvironment.bShouldCompileMonolithic = ShouldCompileMonolithic();

			// Build the target's binaries.
			foreach (UEBuildBinary Binary in AppBinaries)
			{
                OutputItems.AddRange(Binary.Build(this, TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment));  
			}

			if (BuildConfiguration.bPrintPerformanceInfo)
			{
				foreach (SharedPCHHeaderInfo SharedPCH in GlobalCompileEnvironment.SharedPCHHeaderFiles)
				{
					Log.TraceInformation("Shared PCH '" + SharedPCH.Module.Name + "': Used " + SharedPCH.NumModulesUsingThisPCH + " times");
				}
			}

			return ECompilationResult.Succeeded;
		}

		/// <summary>
		/// Check for EULA violation dependency issues.
		/// </summary>
		private void CheckForEULAViolation()
		{
			if (TargetType != TargetRules.TargetType.Editor && TargetType != TargetRules.TargetType.Program && Configuration == UnrealTargetConfiguration.Shipping &&
				BuildConfiguration.bCheckLicenseViolations)
			{
				bool bLicenseViolation = false;
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					IEnumerable<UEBuildModule> NonRedistModules = Binary.GetAllDependencyModules(true, false).Where((DependencyModule) =>
							!IsRedistributable(DependencyModule) && DependencyModule.Name != Binary.Target.AppName
						);

					if (NonRedistModules.Count() != 0)
					{
						string Message = string.Format("Non-editor build cannot depend on non-redistributable modules. {0} depends on '{1}'.", Binary.ToString(), string.Join("', '", NonRedistModules));
						if(BuildConfiguration.bBreakBuildOnLicenseViolation)
						{
							Log.TraceError("ERROR: {0}", Message);
						}
						else
						{
							Log.TraceWarning("WARNING: {0}", Message);
						}
						bLicenseViolation = true;
					}
				}
				if (BuildConfiguration.bBreakBuildOnLicenseViolation && bLicenseViolation)
				{
					throw new BuildException("Non-editor build cannot depend on non-redistributable modules.");
				}
			}
		}

		/// <summary>
		/// Tells if this module can be redistributed.
		/// </summary>
		public static bool IsRedistributable(UEBuildModule Module)
		{
			if(Module.Rules != null && Module.Rules.IsRedistributableOverride.HasValue)
			{
				return Module.Rules.IsRedistributableOverride.Value;
			}

			if(Module.RulesFile != null)
			{
				return !Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceDeveloperDirectory) && !Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceEditorDirectory);
			}

			return true;
		}

		/// <summary>
		/// Setup target before build. This method finds dependencies, sets up global environment etc.
		/// </summary>
		public void PreBuildSetup(UEToolChain TargetToolChain)
		{
			// Set up the global compile and link environment in GlobalCompileEnvironment and GlobalLinkEnvironment.
			SetupGlobalEnvironment(TargetToolChain);

			// Setup the target's modules.
			SetupModules();

			// Setup the target's binaries.
			SetupBinaries();

			// Setup the target's plugins
			SetupPlugins();

			// Setup the custom build steps for this target
			SetupCustomBuildSteps();

			// Create all the modules for each binary
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				foreach (string ModuleName in Binary.Config.ModuleNames)
				{
					UEBuildModule Module = FindOrCreateModuleByName(ModuleName);
					Module.Binary = Binary;
					Binary.AddModule(Module);
				}
			}

			// Add the enabled plugins to the build
			foreach (PluginInfo BuildPlugin in BuildPlugins)
			{
				AddPlugin(BuildPlugin);
			}

			// Describe what's being built.
			Log.TraceVerbose("Building {0} - {1} - {2} - {3}", AppName, TargetName, Platform, Configuration);

			// Put the non-executable output files (PDB, import library, etc) in the intermediate directory.
			GlobalLinkEnvironment.Config.IntermediateDirectory = GlobalCompileEnvironment.Config.OutputDirectory;
			GlobalLinkEnvironment.Config.OutputDirectory = GlobalLinkEnvironment.Config.IntermediateDirectory;

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalLinkEnvironment.Config.LocalShadowDirectory = GlobalLinkEnvironment.Config.OutputDirectory;

			// Add all of the extra modules, including game modules, that need to be compiled along
			// with this app.  These modules are always statically linked in monolithic targets, but not necessarily linked to anything in modular targets,
			// and may still be required at runtime in order for the application to load and function properly!
			AddExtraModules();

			// Create all the modules referenced by the existing binaries
			foreach(UEBuildBinary Binary in AppBinaries)
			{
				Binary.CreateAllDependentModules(this);
			}

			// Bind every referenced C++ module to a binary
			for (int Idx = 0; Idx < AppBinaries.Count; Idx++)
			{
				List<UEBuildModule> DependencyModules = AppBinaries[Idx].GetAllDependencyModules(true, true);
				foreach (UEBuildModuleCPP DependencyModule in DependencyModules.OfType<UEBuildModuleCPP>())
				{
					if(DependencyModule.Binary == null)
					{
						AddModuleToBinary(DependencyModule, false);
					}
				}
			}

			// Add all the precompiled modules to the target. In contrast to "Extra Modules", these modules are not compiled into monolithic targets by default.
			AddPrecompiledModules();

			// Add the external and non-C++ referenced modules to the binaries that reference them.
			foreach (UEBuildModuleCPP Module in Modules.Values.OfType<UEBuildModuleCPP>())
			{
				if(Module.Binary != null)
				{
					foreach (UEBuildModule ReferencedModule in Module.GetUnboundReferences())
					{
						Module.Binary.AddModule(ReferencedModule);
					}
				}
			}

			// Markup all the binaries containing modules with circular references
			if (!bCompileMonolithic)
			{
				foreach (UEBuildModule Module in Modules.Values.Where(x => x.Binary != null))
				{
					foreach (string CircularlyReferencedModuleName in Module.Rules.CircularlyReferencedDependentModules)
					{
						UEBuildModule CircularlyReferencedModule;
						if (Modules.TryGetValue(CircularlyReferencedModuleName, out CircularlyReferencedModule) && CircularlyReferencedModule.Binary != null)
						{
							CircularlyReferencedModule.Binary.SetCreateImportLibrarySeparately(true);
						}
					}
				}
			}

			// On Mac AppBinaries paths for non-console targets need to be adjusted to be inside the app bundle
			if (GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Mac && !GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				TargetToolChain.FixBundleBinariesPaths(this, AppBinaries);
			}

			// If we're building a single module, then find the binary for that module and add it to our target
			if (OnlyModules.Count > 0)
			{
				NonFilteredModules = AppBinaries;
				AppBinaries = GetFilteredOnlyModules(AppBinaries, OnlyModules);
				if (AppBinaries.Count == 0)
				{
					throw new BuildException("One or more of the modules specified using the '-module' argument could not be found.");
				}
			}
			else if (UEBuildConfiguration.bHotReloadFromIDE)
			{
				AppBinaries = GetFilteredGameModules(AppBinaries);
				if (AppBinaries.Count == 0)
				{
					throw new BuildException("One or more of the modules specified using the '-module' argument could not be found.");
				}
			}

			// Filter out binaries that were already built and are just used for linking. We will not build these binaries but we need them for link information
			{
				List<UEBuildBinary> FilteredBinaries = new List<UEBuildBinary>();

				foreach (UEBuildBinary DLLBinary in AppBinaries)
				{
					if (DLLBinary.Config.bAllowCompilation)
					{
						FilteredBinaries.Add(DLLBinary);
					}
				}

				// Copy the result into the final list
				AppBinaries = FilteredBinaries;

				if (AppBinaries.Count == 0)
				{
					throw new BuildException("No modules found to build. All requested binaries were already built.");
				}
			}

			// If we are only interested in platform specific binaries, filter everything else out now
			if (UnrealBuildTool.GetOnlyPlatformSpecificFor() != UnrealTargetPlatform.Unknown)
			{
				List<UEBuildBinary> FilteredBinaries = new List<UEBuildBinary>();

				List<OnlyModule> OtherThingsWeNeedToBuild = new List<OnlyModule>();

				foreach (UEBuildBinary DLLBinary in AppBinaries)
				{
					if (DLLBinary.Config.bIsCrossTarget)
					{
						FilteredBinaries.Add(DLLBinary);
						bool bIncludeDynamicallyLoaded = false;
						List<UEBuildModule> AllReferencedModules = DLLBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded, bForceCircular: true);
						foreach (UEBuildModule Other in AllReferencedModules)
						{
							OtherThingsWeNeedToBuild.Add(new OnlyModule(Other.Name));
						}
					}
				}
				foreach (UEBuildBinary DLLBinary in AppBinaries)
				{
					if (!FilteredBinaries.Contains(DLLBinary) && DLLBinary.FindOnlyModule(OtherThingsWeNeedToBuild) != null)
					{
						if (!DLLBinary.Config.OutputFilePath.Exists())
						{
							throw new BuildException("Module {0} is potentially needed for the {1} platform to work, but it isn't actually compiled. This either needs to be marked as platform specific or made a dynamic dependency of something compiled with the base editor.", DLLBinary.Config.OutputFilePath, UnrealBuildTool.GetOnlyPlatformSpecificFor().ToString());
						}
					}
				}

				// Copy the result into the final list
				AppBinaries = FilteredBinaries;
			}

			//@todo.Rocket: Will users be able to rebuild UnrealHeaderTool? NO
			if (!ProjectFileGenerator.bGenerateProjectFiles && UnrealBuildTool.IsEngineInstalled() && AppName != "UnrealHeaderTool")
			{
				List<UEBuildBinary> FilteredBinaries = new List<UEBuildBinary>();

				// We only want to build projects outside of the Engine Directory...
				foreach (UEBuildBinary DLLBinary in AppBinaries)
				{
					if (!DLLBinary.Config.OutputFilePath.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
					{
						FilteredBinaries.Add(DLLBinary);
					}
					else if (!PrecompiledBinaries.Contains(DLLBinary))
					{
						PrecompiledBinaries.Add(DLLBinary);
					}
				}

				// Copy the result into the final list
				AppBinaries = FilteredBinaries;

				if (AppBinaries.Count == 0)
				{
					throw new BuildException("No modules found to build. All requested binaries were already part of the installed engine data.");
				}
			}

			// If we're just compiling a single file, filter the list of binaries to only include the file we're interested in.
			if (!String.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile))
			{
				FileItem SingleFileItem = FileItem.GetItemByPath(BuildConfiguration.SingleFileToCompile);

				HashSet<UEBuildModuleCPP> Dependencies = GatherDependencyModules(AppBinaries);

				// We only want to build the binaries for this single file
				List<UEBuildBinary> FilteredBinaries = new List<UEBuildBinary>();
				foreach (UEBuildModuleCPP Dependency in Dependencies)
				{
					bool bFileExistsInDependency = Dependency.SourceFilesFound.CPPFiles.Exists(x => x.AbsolutePath == SingleFileItem.AbsolutePath);
					if (bFileExistsInDependency)
					{
						FilteredBinaries.Add(Dependency.Binary);

						UEBuildModuleCPP.SourceFilesClass EmptySourceFileList = new UEBuildModuleCPP.SourceFilesClass();
						Dependency.SourceFilesToBuild.CopyFrom(EmptySourceFileList);
						Dependency.SourceFilesToBuild.CPPFiles.Add(SingleFileItem);
					}
				}
				AppBinaries = FilteredBinaries;

				// Check we have at least one match
				if(AppBinaries.Count == 0)
				{
					throw new BuildException("Couldn't find any module containing {0} in {1}.", SingleFileItem.Reference, TargetName);
				}
			}

			if (ShouldCheckOutputDistributionLevel() && !ProjectFileGenerator.bGenerateProjectFiles)
			{
				// Check the distribution level of all binaries based on the dependencies they have
				Dictionary<UEBuildModule, UEBuildModuleDistribution> ModuleDistributionCache = new Dictionary<UEBuildModule, UEBuildModuleDistribution>();
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					Binary.CheckOutputDistributionLevelAgainstDependencies(ModuleDistributionCache);
				}
			}
		}

		/// <summary>
		/// Writes scripts for all the custom build steps
		/// </summary>
		private void SetupCustomBuildSteps()
		{
			// Make sure the intermediate directory exists
			DirectoryReference ScriptDirectory = ProjectIntermediateDirectory;
			if(!ScriptDirectory.Exists())
			{
				ScriptDirectory.CreateDirectory();
			}

			// Find all the pre-build steps
			List<Tuple<CustomBuildSteps, PluginInfo>> PreBuildSteps = new List<Tuple<CustomBuildSteps,PluginInfo>>();
			if(ProjectDescriptor != null && ProjectDescriptor.PreBuildSteps != null)
			{
				PreBuildSteps.Add(Tuple.Create(ProjectDescriptor.PreBuildSteps, (PluginInfo)null));
			}
			foreach(PluginInfo EnabledPlugin in EnabledPlugins.Where(x => x.Descriptor.PreBuildSteps != null))
			{
				PreBuildSteps.Add(Tuple.Create(EnabledPlugin.Descriptor.PreBuildSteps, EnabledPlugin));
			}
			PreBuildStepScripts = WriteCustomBuildStepScripts(BuildHostPlatform.Current.Platform, ScriptDirectory, "PreBuild", PreBuildSteps);

			// Find all the post-build steps
			List<Tuple<CustomBuildSteps, PluginInfo>> PostBuildSteps = new List<Tuple<CustomBuildSteps,PluginInfo>>();
			if(ProjectDescriptor != null && ProjectDescriptor.PostBuildSteps != null)
			{
				PostBuildSteps.Add(Tuple.Create(ProjectDescriptor.PostBuildSteps, (PluginInfo)null));
			}
			foreach(PluginInfo EnabledPlugin in EnabledPlugins.Where(x => x.Descriptor.PostBuildSteps != null))
			{
				PostBuildSteps.Add(Tuple.Create(EnabledPlugin.Descriptor.PostBuildSteps, EnabledPlugin));
			}
			PostBuildStepScripts = WriteCustomBuildStepScripts(BuildHostPlatform.Current.Platform, ScriptDirectory, "PostBuild", PostBuildSteps);
		}

		/// <summary>
		/// Write scripts containing the custom build steps for the given host platform
		/// </summary>
		/// <param name="HostPlatform">The current host platform</param>
		/// <param name="Directory">The output directory for the scripts</param>
		/// <param name="FilePrefix">Bare prefix for all the created script files</param>
		/// <param name="BuildStepsAndPluginInfo">List of custom build steps, and their matching PluginInfo (if appropriate)</param>
		/// <returns>List of created script files</returns>
		private FileReference[] WriteCustomBuildStepScripts(UnrealTargetPlatform HostPlatform, DirectoryReference Directory, string FilePrefix, List<Tuple<CustomBuildSteps, PluginInfo>> BuildStepsAndPluginInfo)
		{
			List<FileReference> ScriptFiles = new List<FileReference>();
			foreach(Tuple<CustomBuildSteps, PluginInfo> Pair in BuildStepsAndPluginInfo)
			{
				CustomBuildSteps BuildSteps = Pair.Item1;
				if(BuildSteps.HasHostPlatform(HostPlatform))
				{
					// Find all the standard variables
					Dictionary<string, string> Variables = new Dictionary<string,string>();
					Variables.Add("EngineDir", UnrealBuildTool.EngineDirectory.FullName);
					Variables.Add("ProjectDir", ProjectDirectory.FullName);
					Variables.Add("TargetName", TargetName);
					Variables.Add("TargetPlatform", Platform.ToString());
					Variables.Add("TargetConfiguration", Configuration.ToString());
					Variables.Add("TargetType", TargetType.ToString());
					if(ProjectFile != null)
					{
						Variables.Add("ProjectFile", ProjectFile.FullName);
					}
					if(Pair.Item2 != null)
					{
						Variables.Add("PluginDir", Pair.Item2.Directory.FullName);
					}

					// Get the commands to execute
					string[] Commands;
					if(BuildSteps.TryGetCommands(HostPlatform, Variables, out Commands))
					{
						// Get the output path to the script
						string ScriptExtension = (HostPlatform == UnrealTargetPlatform.Win64)? ".bat" : ".sh";
						FileReference ScriptFile = FileReference.Combine(Directory, String.Format("{0}-{1}{2}", FilePrefix, ScriptFiles.Count + 1, ScriptExtension));

						// Write it to disk
						List<string> AllCommands = new List<string>(Commands);
						if(HostPlatform == UnrealTargetPlatform.Win64)
						{
							AllCommands.Insert(0, "@echo off");
						}
						File.WriteAllLines(ScriptFile.FullName, AllCommands);

						// Add the output file to the list of generated scripts
						ScriptFiles.Add(ScriptFile);
					}
				}
			}
			return ScriptFiles.ToArray();
		}

		/// <summary>
		/// Executes the custom pre-build steps
		/// </summary>
		public bool ExecuteCustomPreBuildSteps()
		{
			return ExecuteCustomBuildSteps(PreBuildStepScripts);
		}

		/// <summary>
		/// Executes the custom post-build steps
		/// </summary>
		public bool ExecuteCustomPostBuildSteps()
		{
			return ExecuteCustomBuildSteps(PostBuildStepScripts);
		}

		/// <summary>
		/// Executes a list of custom build step scripts
		/// </summary>
		/// <param name="ScriptFiles">List of script files to execute</param>
		/// <returns>True if the steps succeeded, false otherwise</returns>
		private bool ExecuteCustomBuildSteps(FileReference[] ScriptFiles)
		{
			UnrealTargetPlatform HostPlatform = BuildHostPlatform.Current.Platform;
			foreach(FileReference ScriptFile in ScriptFiles)
			{
				ProcessStartInfo StartInfo = new ProcessStartInfo();
				if(HostPlatform == UnrealTargetPlatform.Win64)
				{
					StartInfo.FileName = "cmd.exe";
					StartInfo.Arguments = String.Format("/C \"{0}\"", ScriptFile.FullName);
				}
				else
				{
					StartInfo.FileName = "/bin/sh";
					StartInfo.Arguments = String.Format("\"{0}\"", ScriptFile.FullName);
				}

				int ReturnCode = Utils.RunLocalProcessAndLogOutput(StartInfo);
				if(ReturnCode != 0)
				{
					Log.TraceError("Custom build step terminated with exit code {0}", ReturnCode);
					return false;
				}
			}
			return true;
		}

		private static FileReference AddModuleFilenameSuffix(string ModuleName, FileReference FilePath, string Suffix)
		{
			int MatchPos = FilePath.FullName.LastIndexOf(ModuleName, StringComparison.InvariantCultureIgnoreCase);
			if (MatchPos < 0)
			{
				throw new BuildException("Failed to find module name \"{0}\" specified on the command line inside of the output filename \"{1}\" to add appendage.", ModuleName, FilePath);
			}
			string Appendage = "-" + Suffix;
			return new FileReference(FilePath.FullName.Insert(MatchPos + ModuleName.Length, Appendage));
		}

		private static List<UEBuildBinary> GetFilteredOnlyModules(List<UEBuildBinary> Binaries, List<OnlyModule> OnlyModules)
		{
			List<UEBuildBinary> Result = new List<UEBuildBinary>();

			foreach (UEBuildBinary Binary in Binaries)
			{
				// If we're doing an OnlyModule compile, we never want the executable that static libraries are linked into for monolithic builds
				if(Binary.Config.Type != UEBuildBinaryType.Executable)
				{
					OnlyModule FoundOnlyModule = Binary.FindOnlyModule(OnlyModules);
					if (FoundOnlyModule != null)
					{
						Result.Add(Binary);

						if (!String.IsNullOrEmpty(FoundOnlyModule.OnlyModuleSuffix))
						{
							Binary.Config.OriginalOutputFilePaths = Binary.Config.OutputFilePaths;
							Binary.Config.OutputFilePaths = Binary.Config.OutputFilePaths.Select(Path => AddModuleFilenameSuffix(FoundOnlyModule.OnlyModuleName, Path, FoundOnlyModule.OnlyModuleSuffix)).ToList();
						}
					}
				}
			}

			return Result;
		}

		private static List<UEBuildBinary> GetFilteredGameModules(List<UEBuildBinary> Binaries)
		{
			List<UEBuildBinary> Result = new List<UEBuildBinary>();

			foreach (UEBuildBinary DLLBinary in Binaries)
			{
				List<UEBuildModule> GameModules = DLLBinary.FindGameModules();
				if (GameModules != null && GameModules.Count > 0)
				{
					Result.Add(DLLBinary);

					string UniqueSuffix = (new Random((int)(DateTime.Now.Ticks % Int32.MaxValue)).Next(10000)).ToString();

					DLLBinary.Config.OriginalOutputFilePaths = DLLBinary.Config.OutputFilePaths;
					DLLBinary.Config.OutputFilePaths = DLLBinary.Config.OutputFilePaths.Select(Path => AddModuleFilenameSuffix(GameModules[0].Name, Path, UniqueSuffix)).ToList();
				}
			}

			return Result;
		}

		/// <summary>
		/// All non-program monolithic binaries implicitly depend on all static plugin libraries so they are always linked appropriately
		/// In order to do this, we create a new module here with a cpp file we emit that invokes an empty function in each library.
		/// If we do not do this, there will be no static initialization for libs if no symbols are referenced in them.
		/// </summary>
		private void CreateLinkerFixupsCPPFile()
		{
			UEBuildBinary ExecutableBinary = AppBinaries[0];

			List<string> PrivateDependencyModuleNames = new List<string>();

			UEBuildBinaryCPP BinaryCPP = ExecutableBinary as UEBuildBinaryCPP;
			if (BinaryCPP != null)
			{
				foreach (UEBuildModule TargetModule in BinaryCPP.Modules)
				{
					ModuleRules CheckRules = TargetModule.Rules;
					if (CheckRules.Type != ModuleRules.ModuleType.External)
					{
						PrivateDependencyModuleNames.Add(TargetModule.Name);
					}
				}
			}

			// We ALWAYS have to write this file as the IMPLEMENT_PRIMARY_GAME_MODULE function depends on the UELinkerFixups function existing.
			{
				string LinkerFixupsName = "UELinkerFixups";

				// Include an empty header so UEBuildModule.Compile does not complain about a lack of PCH
				string HeaderFilename = LinkerFixupsName + "Name.h";
				FileReference LinkerFixupHeaderFilenameWithPath = FileReference.Combine(GlobalCompileEnvironment.Config.OutputDirectory, HeaderFilename);

				// Create the cpp filename
				FileReference LinkerFixupCPPFilename = FileReference.Combine(GlobalCompileEnvironment.Config.OutputDirectory, LinkerFixupsName + ".cpp");
				if (!LinkerFixupCPPFilename.Exists())
				{
					// Create a dummy file in case it doesn't exist yet so that the module does not complain it's not there
					ResponseFile.Create(LinkerFixupHeaderFilenameWithPath, new List<string>());
					ResponseFile.Create(LinkerFixupCPPFilename, new List<string>(new string[] { String.Format("#include \"{0}\"", LinkerFixupHeaderFilenameWithPath) }));
				}

				// Create the source file list (just the one cpp file)
				List<FileItem> SourceFiles = new List<FileItem>();
				FileItem LinkerFixupCPPFileItem = FileItem.GetItemByFileReference(LinkerFixupCPPFilename);
				SourceFiles.Add(LinkerFixupCPPFileItem);

				// Create the CPP module
				DirectoryReference FakeModuleDirectory = LinkerFixupCPPFilename.Directory;
				UEBuildModuleCPP NewModule = CreateArtificialModule(LinkerFixupsName, FakeModuleDirectory, SourceFiles, PrivateDependencyModuleNames);

				// Now bind this new module to the executable binary so it will link the plugin libs correctly
				NewModule.bSkipDefinitionsForCompileEnvironment = true;
				NewModule.Rules.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
				NewModule.RecursivelyCreateModules(this);
				BindArtificialModuleToBinary(NewModule, ExecutableBinary);

				// Create the cpp file
				NewModule.bSkipDefinitionsForCompileEnvironment = false;
				List<string> LinkerFixupsFileContents = GenerateLinkerFixupsContents(ExecutableBinary, NewModule.CreateModuleCompileEnvironment(this, GlobalCompileEnvironment), HeaderFilename, LinkerFixupsName, PrivateDependencyModuleNames);
				NewModule.bSkipDefinitionsForCompileEnvironment = true;

				// Determine if the file changed. Write it if it either doesn't exist or the contents are different.
				bool bShouldWriteFile = true;
				if (LinkerFixupCPPFilename.Exists())
				{
					string[] ExistingFixupText = File.ReadAllLines(LinkerFixupCPPFilename.FullName);
					string JoinedNewContents = string.Join("", LinkerFixupsFileContents.ToArray());
					string JoinedOldContents = string.Join("", ExistingFixupText);
					bShouldWriteFile = (JoinedNewContents != JoinedOldContents);
				}

				// If we determined that we should write the file, write it now.
				if (bShouldWriteFile)
				{
					ResponseFile.Create(LinkerFixupHeaderFilenameWithPath, new List<string>());
					ResponseFile.Create(LinkerFixupCPPFilename, LinkerFixupsFileContents);

					// Update the cached file states so that the linker fixups definitely get rebuilt
					FileItem.GetItemByFileReference(LinkerFixupHeaderFilenameWithPath).ResetFileInfo();
					LinkerFixupCPPFileItem.ResetFileInfo();
				}
			}
		}

		private List<string> GenerateLinkerFixupsContents(UEBuildBinary ExecutableBinary, CPPEnvironment CompileEnvironment, string HeaderFilename, string LinkerFixupsName, List<string> PrivateDependencyModuleNames)
		{
			List<string> Result = new List<string>();

			Result.Add("#include \"" + HeaderFilename + "\"");

			// To reduce the size of the command line for the compiler, we're going to put all definitions inside of the cpp file.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				string MacroName;
				string MacroValue = String.Empty;
				int EqualsIndex = Definition.IndexOf('=');
				if (EqualsIndex >= 0)
				{
					MacroName = Definition.Substring(0, EqualsIndex);
					MacroValue = Definition.Substring(EqualsIndex + 1);
				}
				else
				{
					MacroName = Definition;
				}
				Result.Add("#ifndef " + MacroName);
				Result.Add(String.Format("\t#define {0} {1}", MacroName, MacroValue));
				Result.Add("#endif");
			}

			// Add a function that is not referenced by anything that invokes all the empty functions in the different static libraries
			Result.Add("void " + LinkerFixupsName + "()");
			Result.Add("{");

			// Fill out the body of the function with the empty function calls. This is what causes the static libraries to be considered relevant
			List<UEBuildModule> DependencyModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
            HashSet<string> AlreadyAddedEmptyLinkFunctions = new HashSet<string>();
            foreach (UEBuildModuleCPP BuildModuleCPP in DependencyModules.OfType<UEBuildModuleCPP>().Where(CPPModule => CPPModule.AutoGenerateCppInfo != null))
			{
                int NumGeneratedCppFilesWithTheFunction = BuildModuleCPP.FindNumberOfGeneratedCppFiles();
                if(NumGeneratedCppFilesWithTheFunction == 0)
                {
                    Result.Add("    //" + BuildModuleCPP.Name + " has no generated files, path: " + BuildModuleCPP.GeneratedCodeDirectory.ToString());
                }
                for (int FileIdx = 1; FileIdx <= NumGeneratedCppFilesWithTheFunction; ++FileIdx)
                {
                    string FunctionName = "EmptyLinkFunctionForGeneratedCode" + FileIdx + BuildModuleCPP.Name;
                    if (AlreadyAddedEmptyLinkFunctions.Add(FunctionName))
                    {
                        Result.Add("    extern void " + FunctionName + "();");
                        Result.Add("    " + FunctionName + "();");
                    }
                }
			}
			foreach (string DependencyModuleName in PrivateDependencyModuleNames)
			{
				Result.Add("    extern void EmptyLinkFunctionForStaticInitialization" + DependencyModuleName + "();");
				Result.Add("    EmptyLinkFunctionForStaticInitialization" + DependencyModuleName + "();");
			}

			// End the function body that was started above
			Result.Add("}");

			return Result;
		}

		/// <summary>
		/// Binds artificial module to given binary.
		/// </summary>
		/// <param name="Module">Module to bind.</param>
		/// <param name="Binary">Binary to bind.</param>
		private void BindArtificialModuleToBinary(UEBuildModuleCPP Module, UEBuildBinary Binary)
		{
			Module.Binary = Binary;

			// Process dependencies for this new module
			Module.CachePCHUsageForModuleSourceFiles(this, Module.CreateModuleCompileEnvironment(this, GlobalCompileEnvironment));

			// Add module to binary
			Binary.AddModule(Module);
		}

		/// <summary>
		/// Creates artificial module.
		/// </summary>
		/// <param name="Name">Name of the module.</param>
		/// <param name="Directory">Directory of the module.</param>
		/// <param name="SourceFiles">Source files.</param>
		/// <param name="PrivateDependencyModuleNames">Private dependency list.</param>
		/// <returns>Created module.</returns>
		private UEBuildModuleCPP CreateArtificialModule(string Name, DirectoryReference Directory, IEnumerable<FileItem> SourceFiles, IEnumerable<string> PrivateDependencyModuleNames)
		{
			ModuleRules Rules = new ModuleRules();
			Rules.PrivateDependencyModuleNames.AddRange(PrivateDependencyModuleNames);

			return new UEBuildModuleCPP(
				InName: Name,
				InType: UHTModuleType.GameRuntime,
				InModuleDirectory: Directory,
				InGeneratedCodeDirectory: null,
				InIntelliSenseGatherer: null,
				InSourceFiles: SourceFiles.ToList(),
				InRules: Rules,
				bInBuildSourceFiles: true,
				InRulesFile: null);
		}


		/// <summary>
		/// For any dependent module that has "SharedPCHHeaderFile" set in its module rules, we gather these headers
		/// and sort them in order from least-dependent to most-dependent such that larger, more complex PCH headers
		/// appear last in the list
		/// </summary>
		/// <returns>List of shared PCH headers to use</returns>
		private List<SharedPCHHeaderInfo> FindSharedPCHHeaders()
		{
			// List of modules, with all of the dependencies of that module
			List<SharedPCHHeaderInfo> SharedPCHHeaderFiles = new List<SharedPCHHeaderInfo>();

			// Build up our list of modules with "shared PCH headers".  The list will be in dependency order, with modules
			// that depend on previous modules appearing later in the list
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				UEBuildBinaryCPP CPPBinary = Binary as UEBuildBinaryCPP;
				if (CPPBinary != null)
				{
					foreach (UEBuildModule Module in CPPBinary.Modules)
					{
						UEBuildModuleCPP CPPModule = Module as UEBuildModuleCPP;
						if (CPPModule != null)
						{
							if (!String.IsNullOrEmpty(CPPModule.Rules.SharedPCHHeaderFile) && CPPModule.Binary.Config.bAllowCompilation)
							{
								// @todo SharedPCH: Ideally we could figure the PCH header name automatically, and simply use a boolean in the module
								//     definition to opt into exposing a shared PCH.  Unfortunately we don't determine which private PCH header "goes with"
								//     a module until a bit later in the process.  It shouldn't be hard to change that though.
								string SharedPCHHeaderFilePath = ProjectFileGenerator.RootRelativePath + "/Engine/Source/" + CPPModule.Rules.SharedPCHHeaderFile;
								FileItem SharedPCHHeaderFileItem = FileItem.GetExistingItemByPath(SharedPCHHeaderFilePath);
								if (SharedPCHHeaderFileItem != null)
								{
									List<UEBuildModule> ModuleDependencies = new List<UEBuildModule>();
									bool bIncludeDynamicallyLoaded = false;
									CPPModule.GetAllDependencyModules(ModuleDependencies, new HashSet<UEBuildModule>(), bIncludeDynamicallyLoaded, bForceCircular: false, bOnlyDirectDependencies: false);

									// Figure out where to insert the shared PCH into our list, based off the module dependency ordering
									int InsertAtIndex = SharedPCHHeaderFiles.Count;
									for (int ExistingModuleIndex = SharedPCHHeaderFiles.Count - 1; ExistingModuleIndex >= 0; --ExistingModuleIndex)
									{
										UEBuildModule ExistingModule = SharedPCHHeaderFiles[ExistingModuleIndex].Module;
										Dictionary<string, UEBuildModule> ExistingModuleDependencies = SharedPCHHeaderFiles[ExistingModuleIndex].Dependencies;

										// If the module to add to the list is dependent on any modules already in our header list, that module
										// must be inserted after any of those dependencies in the list
										foreach (KeyValuePair<string, UEBuildModule> ExistingModuleDependency in ExistingModuleDependencies)
										{
											if (ExistingModuleDependency.Value == CPPModule)
											{
												// Make sure we're not a circular dependency of this module.  Circular dependencies always
												// point "upstream".  That is, modules like Engine point to UnrealEd in their
												// CircularlyReferencedDependentModules array, but the natural dependency order is
												// that UnrealEd depends on Engine.  We use this to avoid having modules such as UnrealEd
												// appear before Engine in our shared PCH list.
												// @todo SharedPCH: This is not very easy for people to discover.  Luckily we won't have many shared PCHs in total.
												if (!ExistingModule.HasCircularDependencyOn(CPPModule.Name))
												{
													// We are at least dependent on this module.  We'll keep searching the list to find
													// further-descendant modules we might be dependent on
													InsertAtIndex = ExistingModuleIndex;
													break;
												}
											}
										}
									}

									SharedPCHHeaderInfo NewSharedPCHHeaderInfo = new SharedPCHHeaderInfo();
									NewSharedPCHHeaderInfo.PCHHeaderFile = SharedPCHHeaderFileItem;
									NewSharedPCHHeaderInfo.Module = CPPModule;
									NewSharedPCHHeaderInfo.Dependencies = ModuleDependencies.ToDictionary(M => M.Name);
									SharedPCHHeaderFiles.Insert(InsertAtIndex, NewSharedPCHHeaderInfo);
								}
								else
								{
									throw new BuildException("Could not locate the specified SharedPCH header file '{0}' for module '{1}'.", SharedPCHHeaderFilePath, CPPModule.Name);
								}
							}
						}
					}
				}
			}

			if (SharedPCHHeaderFiles.Count > 0)
			{
				Log.TraceVerbose("Detected {0} shared PCH headers (listed in dependency order):", SharedPCHHeaderFiles.Count);
				foreach (SharedPCHHeaderInfo CurSharedPCH in SharedPCHHeaderFiles)
				{
					Log.TraceVerbose("	" + CurSharedPCH.PCHHeaderFile.AbsolutePath + "  (module: " + CurSharedPCH.Module.Name + ")");
				}
			}
			else
			{
				Log.TraceVerbose("Did not detect any shared PCH headers");
			}
			return SharedPCHHeaderFiles;
		}

		/// <summary>
		/// Include the given plugin in the target. It may be included as a separate binary, or compiled into a monolithic executable.
		/// </summary>
		public void AddPlugin(PluginInfo Plugin)
		{
			UEBuildBinaryType BinaryType = ShouldCompileMonolithic() ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
			if (Plugin.Descriptor.Modules != null)
			{
				foreach (ModuleDescriptor Module in Plugin.Descriptor.Modules)
				{
					if (Module.IsCompiledInConfiguration(Platform, TargetType, UEBuildConfiguration.bBuildDeveloperTools, UEBuildConfiguration.bBuildEditor))
					{
						UEBuildModule ModuleInstance = FindOrCreateModuleByName(Module.Name);

						// Add the corresponding binary for it
						bool bHasSource = RulesAssembly.DoesModuleHaveSource(Module.Name);
						ModuleInstance.Binary = CreateBinaryForModule(ModuleInstance, BinaryType, bAllowCompilation: bHasSource, bIsCrossTarget: false);

						// If it's not enabled, add it to the precompiled binaries list
						if (!EnabledPlugins.Contains(Plugin))
						{
							PrecompiledBinaries.Add(ModuleInstance.Binary);
						}

						// Add it to the binary if we're compiling monolithic (and it's enabled)
						if (ShouldCompileMonolithic() && EnabledPlugins.Contains(Plugin))
						{
							AppBinaries[0].AddModule(ModuleInstance);
						}
					}
				}
			}
		}

		/// When building a target, this is called to add any additional modules that should be compiled along
		/// with the main target.  If you override this in a derived class, remember to call the base implementation!
		protected virtual void AddExtraModules()
		{
			// Add extra modules that will either link into the main binary (monolithic), or be linked into separate DLL files (modular)
			foreach (string ModuleName in ExtraModuleNames)
			{
				UEBuildModule Module = FindOrCreateModuleByName(ModuleName);
				AddModuleToBinary(Module, false);
			}
		}

		/// <summary>
		/// Adds all the precompiled modules into the target. Precompiled modules are compiled alongside the target, but not linked into it unless directly referenced.
		/// </summary>
		protected void AddPrecompiledModules()
		{
			if (bPrecompile || bUsePrecompiled)
			{
				// Find all the modules that are part of the target
				List<UEBuildModule> PrecompiledModules = new List<UEBuildModule>();
				foreach (UEBuildModuleCPP Module in Modules.Values.OfType<UEBuildModuleCPP>())
				{
					if(Module.Binary != null && Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && !PrecompiledModules.Contains(Module))
					{
						PrecompiledModules.Add(Module);
					}
				}

				// If we're precompiling a base engine target, create binaries for all the engine modules that are compatible with it.
				if (bPrecompile && ProjectFile == null && TargetType != TargetRules.TargetType.Program)
				{
					// Find all the known module names in this assembly
					List<string> ModuleNames = new List<string>();
					RulesAssembly.GetAllModuleNames(ModuleNames);

					// Find all the platform folders to exclude from the list of precompiled modules
					List<string> ExcludeFolders = new List<string>();
					foreach (UnrealTargetPlatform TargetPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
					{
						if (TargetPlatform != Platform)
						{
							string DirectoryFragment = Path.DirectorySeparatorChar + TargetPlatform.ToString() + Path.DirectorySeparatorChar;
							ExcludeFolders.Add(DirectoryFragment);
						}
					}

					// Also exclude all the platform groups that this platform is not a part of
					List<UnrealPlatformGroup> IncludePlatformGroups = new List<UnrealPlatformGroup>(UEBuildPlatform.GetPlatformGroups(Platform));
					foreach (UnrealPlatformGroup PlatformGroup in Enum.GetValues(typeof(UnrealPlatformGroup)))
					{
						if (!IncludePlatformGroups.Contains(PlatformGroup))
						{
							string DirectoryFragment = Path.DirectorySeparatorChar + PlatformGroup.ToString() + Path.DirectorySeparatorChar;
							ExcludeFolders.Add(DirectoryFragment);
						}
					}

					// Find all the directories containing engine modules that may be compatible with this target
					List<DirectoryReference> Directories = new List<DirectoryReference>();
					if (TargetType == TargetRules.TargetType.Editor)
					{
						Directories.Add(UnrealBuildTool.EngineSourceEditorDirectory);
					}
					Directories.Add(UnrealBuildTool.EngineSourceRuntimeDirectory);

					// Also allow anything in the developer directory in non-shipping configurations (though we blacklist by default unless the PrecompileForTargets
					// setting indicates that it's actually useful at runtime).
					bool bAllowDeveloperModules = false;
					if(Configuration != UnrealTargetConfiguration.Shipping)
					{
						Directories.Add(UnrealBuildTool.EngineSourceDeveloperDirectory);
						bAllowDeveloperModules = true;
					}

					// Find all the modules that are not part of the standard set
					HashSet<string> FilteredModuleNames = new HashSet<string>();
					foreach (string ModuleName in ModuleNames)
					{
						FileReference ModuleFileName = RulesAssembly.GetModuleFileName(ModuleName);
						if (Directories.Any(x => ModuleFileName.IsUnderDirectory(x)))
						{
							string RelativeFileName = ModuleFileName.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory);
							if (ExcludeFolders.All(x => RelativeFileName.IndexOf(x, StringComparison.InvariantCultureIgnoreCase) == -1) && !PrecompiledModules.Any(x => x.Name == ModuleName))
							{
								FilteredModuleNames.Add(ModuleName);
							}
						}
					}

					// Add all the plugins which aren't already being built
					foreach (PluginInfo Plugin in ValidPlugins.Except(BuildPlugins))
					{
						if (Plugin.LoadedFrom == PluginLoadedFrom.Engine && Plugin.Descriptor.Modules != null)
						{
							foreach (ModuleDescriptor ModuleDescriptor in Plugin.Descriptor.Modules)
							{
								if (ModuleDescriptor.IsCompiledInConfiguration(Platform, TargetType, bAllowDeveloperModules && UEBuildConfiguration.bBuildDeveloperTools, UEBuildConfiguration.bBuildEditor))
								{
									string RelativeFileName = RulesAssembly.GetModuleFileName(ModuleDescriptor.Name).MakeRelativeTo(UnrealBuildTool.EngineDirectory);
									if (!ExcludeFolders.Any(x => RelativeFileName.Contains(x)) && !PrecompiledModules.Any(x => x.Name == ModuleDescriptor.Name))
									{
										FilteredModuleNames.Add(ModuleDescriptor.Name);
									}
								}
							}
						}
					}

					// Create rules for each remaining module, and check that it's set to be precompiled
					foreach(string FilteredModuleName in FilteredModuleNames)
					{
						FileReference ModuleFileName = null;

						// Try to create the rules object, but catch any exceptions if it fails. Some modules (eg. SQLite) may determine that they are unavailable in the constructor.
						ModuleRules Rules;
						try
						{
							Rules = RulesAssembly.CreateModuleRules(FilteredModuleName, TargetInfo, out ModuleFileName);
						}
						catch (BuildException)
						{
							Rules = null;
						}

						// Figure out if it can be precompiled
						bool bCanPrecompile = false;
						if (Rules != null && Rules.Type == ModuleRules.ModuleType.CPlusPlus)
						{
							switch (Rules.PrecompileForTargets)
							{
								case ModuleRules.PrecompileTargetsType.None:
									bCanPrecompile = false;
									break;
								case ModuleRules.PrecompileTargetsType.Default:
									bCanPrecompile = !ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineSourceDeveloperDirectory) || TargetType == TargetRules.TargetType.Editor;
									break;
								case ModuleRules.PrecompileTargetsType.Game:
									bCanPrecompile = (TargetType == TargetRules.TargetType.Client || TargetType == TargetRules.TargetType.Server || TargetType == TargetRules.TargetType.Game);
									break;
								case ModuleRules.PrecompileTargetsType.Editor:
									bCanPrecompile = (TargetType == TargetRules.TargetType.Editor);
									break;
								case ModuleRules.PrecompileTargetsType.Any:
									bCanPrecompile = true;
									break;
							}
						}

						// Create the module
						if (bCanPrecompile)
						{
							UEBuildModule Module = FindOrCreateModuleByName(FilteredModuleName);
							Module.RecursivelyCreateModules(this);
							PrecompiledModules.Add(Module);
						}
					}
				}

				// In monolithic, make sure every module is compiled into a static library first. Even if it's linked into an existing executable, we can add it into a static library and link it into
				// the executable afterwards. In modular builds, just compile every module into a DLL.
				UEBuildBinaryType PrecompiledBinaryType = bCompileMonolithic ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
				foreach (UEBuildModule PrecompiledModule in PrecompiledModules)
				{
					if (PrecompiledModule.Binary == null || (bCompileMonolithic && PrecompiledModule.Binary.Config.Type != UEBuildBinaryType.StaticLibrary))
					{
						PrecompiledModule.Binary = CreateBinaryForModule(PrecompiledModule, PrecompiledBinaryType, bAllowCompilation: !bUsePrecompiled, bIsCrossTarget: false);
						PrecompiledBinaries.Add(PrecompiledModule.Binary);
					}
					else if (bUsePrecompiled && !ProjectFileGenerator.bGenerateProjectFiles)
					{
						// Even if there's already a binary for this module, we never want to compile it.
						PrecompiledModule.Binary.Config.bAllowCompilation = false;
					}
				}
			}
		}

		public void AddModuleToBinary(UEBuildModule Module, bool bIsCrossTarget)
		{
			if (ShouldCompileMonolithic())
			{
				// When linking monolithically, any unbound modules will be linked into the main executable
				Module.Binary = (UEBuildBinaryCPP)AppBinaries[0];
				Module.Binary.AddModule(Module);
			}
			else
			{
				// Otherwise create a new module for it
				Module.Binary = CreateBinaryForModule(Module, UEBuildBinaryType.DynamicLinkLibrary, bAllowCompilation: true, bIsCrossTarget: bIsCrossTarget);
			}

            if (Module.Binary == null)
            {
                throw new BuildException("Failed to set up binary for module {0}", Module.Name);
            }
		}

		/// <summary>
		/// Adds a binary for the given module. Does not check whether a binary already exists, or whether a binary should be created for this build configuration.
		/// </summary>
		/// <param name="Module">The module to create a binary for</param>
		/// <param name="BinaryType">Type of binary to be created</param>
		/// <param name="bAllowCompilation">Whether this binary can be compiled. The function will check whether plugin binaries can be compiled.</param>
		/// <returns>The new binary</returns>
		private UEBuildBinaryCPP CreateBinaryForModule(UEBuildModule Module, UEBuildBinaryType BinaryType, bool bAllowCompilation, bool bIsCrossTarget)
		{
			// Get the plugin info for this module
			PluginInfo Plugin = null;
			if (Module.RulesFile != null)
			{
				RulesAssembly.TryGetPluginForModule(Module.RulesFile, out Plugin);
			}

			// Get the root output directory and base name (target name/app name) for this binary
			DirectoryReference BaseOutputDirectory;
			if (Plugin != null)
			{
				BaseOutputDirectory = Plugin.Directory;
			}
			else if (RulesAssembly.IsGameModule(Module.Name) || !bUseSharedBuildEnvironment)
			{
				BaseOutputDirectory = ProjectDirectory;
			}
			else
			{
				BaseOutputDirectory = UnrealBuildTool.EngineDirectory;
			}

			// Get the configuration that this module will be built in. Engine modules compiled in DebugGame will use Development.
			UnrealTargetConfiguration ModuleConfiguration = Configuration;
			if (Configuration == UnrealTargetConfiguration.DebugGame && !RulesAssembly.IsGameModule(Module.Name))
			{
				ModuleConfiguration = UnrealTargetConfiguration.Development;
			}

			// Get the output and intermediate directories for this module
			DirectoryReference OutputDirectory = DirectoryReference.Combine(BaseOutputDirectory, "Binaries", Platform.ToString());
			DirectoryReference IntermediateDirectory = DirectoryReference.Combine(BaseOutputDirectory, BuildConfiguration.PlatformIntermediateFolder, AppName, ModuleConfiguration.ToString());

			// Append a subdirectory if the module rules specifies one
			if (Module.Rules != null && !String.IsNullOrEmpty(Module.Rules.BinariesSubFolder))
			{
				OutputDirectory = DirectoryReference.Combine(OutputDirectory, Module.Rules.BinariesSubFolder);
				IntermediateDirectory = DirectoryReference.Combine(IntermediateDirectory, Module.Rules.BinariesSubFolder);
			}

			// Get the output filenames
			FileReference BaseBinaryPath = FileReference.Combine(OutputDirectory, MakeBinaryFileName(AppName + "-" + Module.Name, Platform, ModuleConfiguration, PlatformContext.GetActiveArchitecture(), Rules.UndecoratedConfiguration, BinaryType));
			List<FileReference> OutputFilePaths = UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BaseBinaryPath, ProjectFile);

			// Prepare the configuration object
			UEBuildBinaryConfiguration Config = new UEBuildBinaryConfiguration(
				InType: BinaryType,
				InOutputFilePaths: OutputFilePaths,
				InIntermediateDirectory: IntermediateDirectory,
				bInHasModuleRules: Module.Rules != null,
				bInAllowExports: BinaryType == UEBuildBinaryType.DynamicLinkLibrary,
				bInAllowCompilation: bAllowCompilation,
				bInIsCrossTarget: bIsCrossTarget,
				InModuleNames: new string[] { Module.Name }
			);

			// Create the new binary
			UEBuildBinaryCPP Binary = new UEBuildBinaryCPP(this, Config);
			Binary.AddModule(Module);
			AppBinaries.Add(Binary);
			return Binary;
		}

		/// <returns>true if debug information is created, false otherwise.</returns>
		public bool IsCreatingDebugInfo()
		{
			return GlobalCompileEnvironment.Config.bCreateDebugInfo;
		}

		/// <returns>The overall output directory of actions for this target.</returns>
		public DirectoryReference GetOutputDirectory()
		{
			return GlobalCompileEnvironment.Config.OutputDirectory;
		}

		/// <returns>true if we are building for dedicated server, false otherwise.</returns>
		[Obsolete("IsBuildingDedicatedServer() has been deprecated and will be removed in future release. Update your code to use TargetInfo.Type instead or your code will not compile.")]
		public bool IsBuildingDedicatedServer()
		{
			return false;
		}

		/// <summary>
		/// Makes a filename (without path) for a compiled binary (e.g. "Core-Win64-Debug.lib") */
		/// </summary>
		/// <param name="BinaryName">The name of this binary</param>
		/// <param name="Platform">The platform being built for</param>
		/// <param name="Configuration">The configuration being built</param>
		/// <param name="UndecoratedConfiguration">The target configuration which doesn't require a platform and configuration suffix. Development by default.</param>
		/// <param name="BinaryType">Type of binary</param>
		/// <returns>Name of the binary</returns>
		public static string MakeBinaryFileName(string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, UnrealTargetConfiguration UndecoratedConfiguration, UEBuildBinaryType BinaryType)
		{
			StringBuilder Result = new StringBuilder();

			if (Platform == UnrealTargetPlatform.Linux && (BinaryType == UEBuildBinaryType.DynamicLinkLibrary || BinaryType == UEBuildBinaryType.StaticLibrary))
			{
				Result.Append("lib");
			}

			Result.Append(BinaryName);

			if (Configuration != UndecoratedConfiguration)
			{
				Result.AppendFormat("-{0}-{1}", Platform.ToString(), Configuration.ToString());
			}

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			if(BuildPlatform.RequiresArchitectureSuffix())
			{
				Result.Append(Architecture);
			}

			if (BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				Result.AppendFormat("-{0}.analysis", BuildConfiguration.UCAModuleToAnalyze);
			}
			else
			{
				Result.Append(BuildPlatform.GetBinaryExtension(BinaryType));
			}

			return Result.ToString();
		}

		/// <summary>
		/// Determine the output path for a target's executable
		/// </summary>
		/// <param name="BaseDirectory">The base directory for the executable; typically either the engine directory or project directory.</param>
		/// <param name="BinaryName">Name of the binary</param>
		/// <param name="Platform">Target platform to build for</param>
		/// <param name="Configuration">Target configuration being built</param>
		/// <param name="UndecoratedConfiguration">The configuration which doesn't have a "-{Platform}-{Configuration}" suffix added to the binary</param>
		/// <param name="bIncludesGameModules">Whether this executable contains game modules</param>
		/// <param name="ExeSubFolder">Subfolder for executables. May be null.</param>
		/// <returns>List of executable paths for this target</returns>
		public static List<FileReference> MakeExecutablePaths(DirectoryReference BaseDirectory, string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, UnrealTargetConfiguration UndecoratedConfiguration, bool bIncludesGameModules, string ExeSubFolder, FileReference ProjectFile)
		{
			// Get the configuration for the executable. If we're building DebugGame, and this executable only contains engine modules, use the same name as development.
			UnrealTargetConfiguration ExeConfiguration = Configuration;
			if (Configuration == UnrealTargetConfiguration.DebugGame && !bIncludesGameModules)
			{
				ExeConfiguration = UnrealTargetConfiguration.Development;
			}

			// Build the binary path
			DirectoryReference BinaryDirectory = DirectoryReference.Combine(BaseDirectory, "Binaries", Platform.ToString());
			if (!String.IsNullOrEmpty(ExeSubFolder))
			{
				BinaryDirectory = DirectoryReference.Combine(BinaryDirectory, ExeSubFolder);
			}
			FileReference BinaryFile = FileReference.Combine(BinaryDirectory, MakeBinaryFileName(BinaryName, Platform, ExeConfiguration, Architecture, UndecoratedConfiguration, UEBuildBinaryType.Executable));

			// Allow the platform to customize the output path (and output several executables at once if necessary)
			return UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BinaryFile, ProjectFile);
		}

		/// <summary>
		/// Sets up the modules for the target.
		/// </summary>
		protected virtual void SetupModules()
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			List<string> PlatformExtraModules = new List<string>();
			PlatformContext.AddExtraModules(TargetInfo, PlatformExtraModules);
			ExtraModuleNames.AddRange(PlatformExtraModules);
		}

		/// <summary>
		/// Sets up the plugins for this target
		/// </summary>
		protected virtual void SetupPlugins()
		{
			// Filter the plugins list by the current project
			ValidPlugins = new List<PluginInfo>(RulesAssembly.EnumeratePlugins());

			// Remove any plugins for platforms we don't have
			List<string> ExcludeFolders = new List<string>();
			foreach (UnrealTargetPlatform TargetPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				if (UEBuildPlatform.GetBuildPlatform(TargetPlatform, true) == null)
				{
					string DirectoryFragment = Path.DirectorySeparatorChar + TargetPlatform.ToString() + Path.DirectorySeparatorChar;
					ExcludeFolders.Add(DirectoryFragment);
				}
			}
			ValidPlugins.RemoveAll(x => x.Descriptor.bRequiresBuildPlatform && ShouldExcludePlugin(x, ExcludeFolders));

			// Build a list of enabled plugins
			EnabledPlugins = new List<PluginInfo>();
			UnrealHeaderToolPlugins = new List<PluginInfo>();

			// If we're compiling against the engine, add the plugins enabled for this target
			if (UEBuildConfiguration.bCompileAgainstEngine)
			{
				ProjectDescriptor Project = (UEBuildConfiguration.bCompileAgainstEngine && ProjectFile != null) ? ProjectDescriptor.FromFile(ProjectFile.FullName) : null;
				foreach (PluginInfo ValidPlugin in ValidPlugins)
				{
					if(UProjectInfo.IsPluginEnabledForProject(ValidPlugin, Project, Platform, TargetType))
					{
						if (ValidPlugin.Descriptor.bCanBeUsedWithUnrealHeaderTool)
						{
							UnrealHeaderToolPlugins.Add(ValidPlugin);							
						}
						EnabledPlugins.Add(ValidPlugin);						
					}
				}
			}

			// Add the plugins explicitly required by the target rules
			foreach (string AdditionalPlugin in Rules.AdditionalPlugins)
			{
				PluginInfo Plugin = ValidPlugins.FirstOrDefault(ValidPlugin => ValidPlugin.Name == AdditionalPlugin);
				if (Plugin == null)
				{
					throw new BuildException("Plugin '{0}' is in the list of additional plugins for {1}, but was not found.", AdditionalPlugin, TargetName);
				}
				if (!EnabledPlugins.Contains(Plugin))
				{
					EnabledPlugins.Add(Plugin);
				}
			}

			// Remove any enabled plugins that are unused on the current platform. This prevents having to stage the .uplugin files, but requires that the project descriptor
			// doesn't have a platform-neutral reference to it.
			EnabledPlugins.RemoveAll(Plugin => !UProjectInfo.IsPluginDescriptorRequiredForProject(Plugin, ProjectDescriptor, Platform, TargetType, UEBuildConfiguration.bBuildDeveloperTools, UEBuildConfiguration.bBuildEditor));

			// Set the list of plugins that should be built
			if (Rules.bBuildAllPlugins)
			{
				BuildPlugins = new List<PluginInfo>(ValidPlugins);
			}
			else
			{
				BuildPlugins = new List<PluginInfo>(EnabledPlugins);
			}

			// Add any foreign plugins to the list
			if (ForeignPlugins != null)
			{
				foreach (FileReference ForeignPlugin in ForeignPlugins)
				{
					PluginInfo ForeignPluginInfo = ValidPlugins.FirstOrDefault(x => x.File == ForeignPlugin);
					if (!BuildPlugins.Contains(ForeignPluginInfo))
					{
						BuildPlugins.Add(ForeignPluginInfo);
					}
				}
			}
		}

		/// <summary>
		/// Checks whether a plugin path contains a platform directory fragment
		/// </summary>
		private bool ShouldExcludePlugin(PluginInfo Plugin, List<string> ExcludeFragments)
		{
			if (Plugin.LoadedFrom == PluginLoadedFrom.Engine)
			{
				string RelativePathFromRoot = Plugin.File.MakeRelativeTo(UnrealBuildTool.EngineDirectory);
				return ExcludeFragments.Any(x => RelativePathFromRoot.Contains(x));
			}
			else if(ProjectFile != null)
			{
				string RelativePathFromRoot = Plugin.File.MakeRelativeTo(ProjectFile.Directory);
				return ExcludeFragments.Any(x => RelativePathFromRoot.Contains(x));
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Sets up the binaries for the target.
		/// </summary>
		protected virtual void SetupBinaries()
		{
			if (Rules != null)
			{
				List<UEBuildBinaryConfiguration> RulesBuildBinaryConfigurations = new List<UEBuildBinaryConfiguration>();
				List<string> RulesExtraModuleNames = new List<string>();
				Rules.SetupBinaries(
					TargetInfo,
					ref RulesBuildBinaryConfigurations,
					ref RulesExtraModuleNames
					);

				foreach (UEBuildBinaryConfiguration BinaryConfig in RulesBuildBinaryConfigurations)
				{
					BinaryConfig.OutputFilePaths = OutputPaths.ToList();
					BinaryConfig.IntermediateDirectory = ProjectIntermediateDirectory;

					if (BinaryConfig.ModuleNames.Count > 0)
					{
						AppBinaries.Add(new UEBuildBinaryCPP(this, BinaryConfig));
					}
					else
					{
						AppBinaries.Add(new UEBuildBinaryCSDLL(this, BinaryConfig));
					}
				}

				ExtraModuleNames.AddRange(RulesExtraModuleNames);
			}
		}

		public virtual void SetupDefaultGlobalEnvironment(
			TargetInfo Target,
			ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
			ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
			)
		{
			// Default does nothing
		}

		/// <summary>
		/// Sets up the global compile and link environment for the target.
		/// </summary>
		public virtual void SetupGlobalEnvironment(UEToolChain ToolChain)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			ToolChain.SetUpGlobalEnvironment();

			// Allow each target type (Game/Editor/Server) to set a default global environment state
			{
				LinkEnvironmentConfiguration RulesLinkEnvConfig = GlobalLinkEnvironment.Config;
				CPPEnvironmentConfiguration RulesCPPEnvConfig = GlobalCompileEnvironment.Config;
				SetupDefaultGlobalEnvironment(
						TargetInfo,
						ref RulesLinkEnvConfig,
						ref RulesCPPEnvConfig
						);
				// Copy them back...
				GlobalLinkEnvironment.Config = RulesLinkEnvConfig;
				GlobalCompileEnvironment.Config = RulesCPPEnvConfig;
			}

			// If there is a Rules object present, let it set things up before validation.
			// This mirrors the behavior of the UEBuild<TARGET> setup, where the 
			// SetupGlobalEnvironment call would first set the target-specific values, 
			// and then call Base.SetupGlobalEnvironment
			if (Rules != null)
			{
				LinkEnvironmentConfiguration RulesLinkEnvConfig = GlobalLinkEnvironment.Config;
				CPPEnvironmentConfiguration RulesCPPEnvConfig = GlobalCompileEnvironment.Config;

                if (!bUseSharedBuildEnvironment)
				{
					Rules.SetupGlobalEnvironment(
						TargetInfo,
						ref RulesLinkEnvConfig,
						ref RulesCPPEnvConfig
						);

					// Copy them back...
					GlobalLinkEnvironment.Config = RulesLinkEnvConfig;
					GlobalCompileEnvironment.Config = RulesCPPEnvConfig;
				}
				GlobalCompileEnvironment.Config.Definitions.Add(String.Format("IS_PROGRAM={0}", TargetType == TargetRules.TargetType.Program ? "1" : "0"));
			}

			// See if the target rules defined any signing keys and add the appropriate defines
			if (!string.IsNullOrEmpty(Rules.PakSigningKeysFile))
			{
				string FullFilename = Path.Combine(ProjectDirectory.FullName, Rules.PakSigningKeysFile);

				Log.TraceVerbose("Adding signing keys to executable from '{0}'", FullFilename);

				if (File.Exists(FullFilename))
				{
					string[] Lines = File.ReadAllLines(FullFilename);
					List<string> Keys = new List<string>();
					foreach (string Line in Lines)
					{
						if (!string.IsNullOrEmpty(Line))
						{
							if (Line.StartsWith("0x"))
							{
								Keys.Add(Line.Trim());
							}
						}
					}

					if (Keys.Count == 3)
					{
						GlobalCompileEnvironment.Config.Definitions.Add("DECRYPTION_KEY_MODULUS=\"" + Keys[1] + "\"");
						GlobalCompileEnvironment.Config.Definitions.Add("DECRYPTION_KEY_EXPONENT=\"" + Keys[2] + "\"");
					}
					else
					{
						Log.TraceWarning("Contents of signing key file are invalid so will be ignored");
					}
				}
				else
				{
					Log.TraceVerbose("Signing key file is missing! Executable will not include signing keys");
				}
			}

			// Validate UE configuration - needs to happen before setting any environment mojo and after argument parsing.
			PlatformContext.ValidateUEBuildConfiguration();
			UEBuildConfiguration.ValidateConfiguration();

			// Add the 'Engine/Source' path as a global include path for all modules
			string EngineSourceDirectory = Path.GetFullPath(Path.Combine("..", "..", "Engine", "Source"));
			if (!Directory.Exists(EngineSourceDirectory))
			{
				throw new BuildException("Couldn't find Engine/Source directory using relative path");
			}
			GlobalCompileEnvironment.Config.CPPIncludeInfo.IncludePaths.Add(EngineSourceDirectory);

			//@todo.PLATFORM: Do any platform specific tool chain initialization here if required

			if (BuildConfiguration.bUseMallocProfiler)
			{
				BuildConfiguration.bOmitFramePointers = false;
				GlobalCompileEnvironment.Config.Definitions.Add("USE_MALLOC_PROFILER=1");
			}

			string OutputAppName = GetAppName();

			UnrealTargetConfiguration EngineTargetConfiguration = Configuration == UnrealTargetConfiguration.DebugGame ? UnrealTargetConfiguration.Development : Configuration;
			GlobalCompileEnvironment.Config.OutputDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, BuildConfiguration.PlatformIntermediateFolder, OutputAppName, EngineTargetConfiguration.ToString());

			// Installed Engine intermediates go to the project's intermediate folder. Installed Engine never writes to the engine intermediate folder. (Those files are immutable)
			// Also, when compiling in monolithic, all intermediates go to the project's folder.  This is because a project can change definitions that affects all engine translation
			// units too, so they can't be shared between different targets.  They are effectively project-specific engine intermediates.
			if (UnrealBuildTool.IsEngineInstalled() || (ProjectFile != null && ShouldCompileMonolithic()))
			{
				if (ShouldCompileMonolithic())
				{
					if (ProjectFile != null)
					{
						GlobalCompileEnvironment.Config.OutputDirectory = DirectoryReference.Combine(ProjectFile.Directory, BuildConfiguration.PlatformIntermediateFolder, OutputAppName, Configuration.ToString());
					}
					else if (ForeignPlugins.Count > 0)
					{
						GlobalCompileEnvironment.Config.OutputDirectory = DirectoryReference.Combine(ForeignPlugins[0].Directory, BuildConfiguration.PlatformIntermediateFolder, OutputAppName, Configuration.ToString());
					}
				}
			}

			if(BuildConfiguration.PCHOutputDirectory != null)
			{
				GlobalCompileEnvironment.Config.PCHOutputDirectory = DirectoryReference.Combine(new DirectoryReference(BuildConfiguration.PCHOutputDirectory), BuildConfiguration.PlatformIntermediateFolder, OutputAppName, Configuration.ToString());
			}

			if (UEBuildConfiguration.bForceCompileDevelopmentAutomationTests)
            {
                GlobalCompileEnvironment.Config.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=1");
            }
            else
            {
                switch(Configuration)
                {
                    case UnrealTargetConfiguration.Test:
                    case UnrealTargetConfiguration.Shipping:
                        GlobalCompileEnvironment.Config.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=0");
                        break;
                    default:
                        GlobalCompileEnvironment.Config.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=1");
                        break;
                }
            }

            if (UEBuildConfiguration.bForceCompilePerformanceAutomationTests)
            {
                GlobalCompileEnvironment.Config.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=1");
            }
            else
            {
                switch (Configuration)
                {
                    case UnrealTargetConfiguration.Shipping:
                        GlobalCompileEnvironment.Config.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=0");
                        break;
                    default:
                        GlobalCompileEnvironment.Config.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=1");
                        break;
                }
            }

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalCompileEnvironment.Config.LocalShadowDirectory = GlobalCompileEnvironment.Config.OutputDirectory;

			GlobalCompileEnvironment.Config.Definitions.Add("UNICODE");
			GlobalCompileEnvironment.Config.Definitions.Add("_UNICODE");
			GlobalCompileEnvironment.Config.Definitions.Add("__UNREAL__");

			GlobalCompileEnvironment.Config.Definitions.Add(String.Format("IS_MONOLITHIC={0}", ShouldCompileMonolithic() ? "1" : "0"));

			if (UEBuildConfiguration.bCompileAgainstEngine)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_ENGINE=1");
				GlobalCompileEnvironment.Config.Definitions.Add(
					String.Format("WITH_UNREAL_DEVELOPER_TOOLS={0}", UEBuildConfiguration.bBuildDeveloperTools ? "1" : "0"));
				// Automatically include CoreUObject
				UEBuildConfiguration.bCompileAgainstCoreUObject = true;
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_ENGINE=0");
				// Can't have developer tools w/out engine
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_UNREAL_DEVELOPER_TOOLS=0");
			}

			if (UEBuildConfiguration.bCompileAgainstCoreUObject)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_COREUOBJECT=1");
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_COREUOBJECT=0");
			}

			if (UEBuildConfiguration.bCompileWithStatsWithoutEngine)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("USE_STATS_WITHOUT_ENGINE=1");
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("USE_STATS_WITHOUT_ENGINE=0");
			}

			if (UEBuildConfiguration.bCompileWithPluginSupport)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_PLUGIN_SUPPORT=1");
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_PLUGIN_SUPPORT=0");
			}

			if (UEBuildConfiguration.bUseLoggingInShipping)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("USE_LOGGING_IN_SHIPPING=1");
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("USE_LOGGING_IN_SHIPPING=0");
			}

			if (UEBuildConfiguration.bUseChecksInShipping)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("USE_CHECKS_IN_SHIPPING=1");
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("USE_CHECKS_IN_SHIPPING=0");
			}

			// Propagate whether we want a lean and mean build to the C++ code.
			if (UEBuildConfiguration.bCompileLeanAndMeanUE)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_MINIMAL=1");
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_MINIMAL=0");
			}

			// Disable editor when its not needed
			if (BuildPlatform.ShouldNotBuildEditor(Platform, Configuration) == true)
			{
				UEBuildConfiguration.bBuildEditor = false;
			}

			// Disable the DDC and a few other things related to preparing assets
			if (BuildPlatform.BuildRequiresCookedData(Platform, Configuration) == true)
			{
				UEBuildConfiguration.bBuildRequiresCookedData = true;
			}

			// bBuildEditor has now been set appropriately for all platforms, so this is here to make sure the #define 
			if (UEBuildConfiguration.bBuildEditor)
			{
				// Must have editor only data if building the editor.
				UEBuildConfiguration.bBuildWithEditorOnlyData = true;
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_EDITOR=1");
			}
			else if (!GlobalCompileEnvironment.Config.Definitions.Contains("WITH_EDITOR=0"))
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_EDITOR=0");
			}

			if (UEBuildConfiguration.bBuildWithEditorOnlyData == false)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_EDITORONLY_DATA=0");
			}

			// Check if server-only code should be compiled out.
			if (UEBuildConfiguration.bWithServerCode == true)
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_SERVER_CODE=1");
			}
			else
			{
				GlobalCompileEnvironment.Config.Definitions.Add("WITH_SERVER_CODE=0");
			}

			// tell the compiled code the name of the UBT platform (this affects folder on disk, etc that the game may need to know)
			GlobalCompileEnvironment.Config.Definitions.Add("UBT_COMPILED_PLATFORM=" + Platform.ToString());
			GlobalCompileEnvironment.Config.Definitions.Add("UBT_COMPILED_TARGET=" + TargetType.ToString());

			// Initialize the compile and link environments for the platform, configuration, and project.
			SetUpPlatformEnvironment();
			SetUpConfigurationEnvironment();
			SetUpProjectEnvironment();

			// Validates current settings and updates if required.
			BuildConfiguration.ValidateConfiguration(
				GlobalCompileEnvironment.Config.Target.Configuration,
				GlobalCompileEnvironment.Config.Target.Platform,
				GlobalCompileEnvironment.Config.bCreateDebugInfo,
				PlatformContext);
		}

		void SetUpPlatformEnvironment()
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			CPPTargetPlatform MainCompilePlatform = BuildPlatform.DefaultCppPlatform;

			GlobalLinkEnvironment.Config.Target.Platform = MainCompilePlatform;
			GlobalCompileEnvironment.Config.Target.Platform = MainCompilePlatform;

			string ActiveArchitecture = PlatformContext.GetActiveArchitecture();
			GlobalCompileEnvironment.Config.Target.Architecture = ActiveArchitecture;
			GlobalLinkEnvironment.Config.Target.Architecture = ActiveArchitecture;

			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				Rules.ConfigureToolchain(TargetInfo);
			}

			// Set up the platform-specific environment.
			PlatformContext.SetUpEnvironment(this);
		}

		void SetUpConfigurationEnvironment()
		{
			PlatformContext.SetUpConfigurationEnvironment(TargetInfo, GlobalCompileEnvironment, GlobalLinkEnvironment);

			// Check to see if we're compiling a library or not
			bool bIsBuildingDLL = OutputPaths[0].HasExtension(".dll");
			GlobalCompileEnvironment.Config.bIsBuildingDLL = bIsBuildingDLL;
			GlobalLinkEnvironment.Config.bIsBuildingDLL = bIsBuildingDLL;
			bool bIsBuildingLibrary = OutputPaths[0].HasExtension(".lib");
			GlobalCompileEnvironment.Config.bIsBuildingLibrary = bIsBuildingLibrary;
			GlobalLinkEnvironment.Config.bIsBuildingLibrary = bIsBuildingLibrary;
		}

		void SetUpProjectEnvironment()
		{
			PlatformContext.SetUpProjectEnvironment();
		}

		/// <summary>
		/// Create a rules object for the given module, and set any default values for this target
		/// </summary>
		private ModuleRules CreateModuleRulesAndSetDefaults(string ModuleName, out FileReference ModuleFileName)
		{
			// Create the rules from the assembly
			ModuleRules RulesObject = RulesAssembly.CreateModuleRules(ModuleName, TargetInfo, out ModuleFileName);

			// Reads additional dependencies array for project module from project file and fills PrivateDependencyModuleNames. 
			if (ProjectDescriptor != null && ProjectDescriptor.Modules != null)
			{
				ModuleDescriptor Module = ProjectDescriptor.Modules.FirstOrDefault(x => x.Name.Equals(ModuleName, StringComparison.InvariantCultureIgnoreCase));
				if (Module != null && Module.AdditionalDependencies != null)
				{
					RulesObject.PrivateDependencyModuleNames.AddRange(Module.AdditionalDependencies);
				}
			}

			// Validate rules object
			if (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus)
			{
				if (RulesObject.PrivateAssemblyReferences.Count > 0)
				{
					throw new BuildException("Module rules for '{0}' may not specify PrivateAssemblyReferences unless it is a CPlusPlusCLR module type.", ModuleName);
				}

				List<string> InvalidDependencies = RulesObject.DynamicallyLoadedModuleNames.Intersect(RulesObject.PublicDependencyModuleNames.Concat(RulesObject.PrivateDependencyModuleNames)).ToList();
				if (InvalidDependencies.Count != 0)
				{
					throw new BuildException("Module rules for '{0}' should not be dependent on modules which are also dynamically loaded: {1}", ModuleName, String.Join(", ", InvalidDependencies));
				}

				// Choose code optimization options based on module type (game/engine) if default optimization method is selected.
				bool bIsEngineModule = ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory);
				if (RulesObject.OptimizeCode == ModuleRules.CodeOptimization.Default)
				{
					// Engine/Source and Engine/Plugins are considered 'Engine' code...
					if (bIsEngineModule)
					{
						// Engine module - always optimize (except Debug).
						RulesObject.OptimizeCode = ModuleRules.CodeOptimization.Always;
					}
					else
					{
						// Game module - do not optimize in Debug and DebugGame builds.
						RulesObject.OptimizeCode = ModuleRules.CodeOptimization.InNonDebugBuilds;
					}
				}

				// Disable shared PCHs for game modules by default (but not game plugins, since they won't depend on the game's PCH!)
				if (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.Default)
				{
					if (ProjectFile == null || !ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(ProjectFile.Directory, "Source")))
					{
						// Engine module or plugin module -- allow shared PCHs
						RulesObject.PCHUsage = ModuleRules.PCHUsageMode.UseSharedPCHs;
					}
					else
					{
						// Game module.  Do not enable shared PCHs by default, because games usually have a large precompiled header of their own and compile times would suffer.
						RulesObject.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
					}
				}
			}
			return RulesObject;
		}

		/// <summary>
		/// Finds a module given its name.  Throws an exception if the module couldn't be found.
		/// </summary>
		public UEBuildModule FindOrCreateModuleByName(string ModuleName)
		{
			UEBuildModule Module;
			if (!Modules.TryGetValue(ModuleName, out Module))
			{
				// Create the module!  (It will be added to our hash table in its constructor)

				// @todo projectfiles: Cross-platform modules can appear here during project generation, but they may have already
				//   been filtered out by the project generator.  This causes the projects to not be added to directories properly.
				FileReference ModuleFileName;
				ModuleRules RulesObject = CreateModuleRulesAndSetDefaults(ModuleName, out ModuleFileName);
				DirectoryReference ModuleDirectory = ModuleFileName.Directory;

				// Get the type of module we're creating
				UHTModuleType? ModuleType = null;

				// Get the plugin for this module
				PluginInfo Plugin;
				RulesAssembly.TryGetPluginForModule(ModuleFileName, out Plugin);

				// Get the module descriptor for this module if it's a plugin
				ModuleDescriptor PluginModuleDesc = null;
				if (Plugin != null)
				{
					PluginModuleDesc = Plugin.Descriptor.Modules.FirstOrDefault(x => x.Name == ModuleName);
					if (PluginModuleDesc != null && PluginModuleDesc.Type == ModuleHostType.Program)
					{
						ModuleType = UHTModuleType.Program;
					}
				}

				if (ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
				{
					if (RulesObject.Type == ModuleRules.ModuleType.External)
					{
						ModuleType = UHTModuleType.EngineThirdParty;
					}
					else
					{
						if (!ModuleType.HasValue && PluginModuleDesc != null)
						{
							ModuleType = ExternalExecution.GetEngineModuleTypeFromDescriptor(PluginModuleDesc);
						}

						if (!ModuleType.HasValue)
						{
							ModuleType = ExternalExecution.GetEngineModuleTypeBasedOnLocation(ModuleFileName);
						}
					}
				}
				else
				{
					if (RulesObject.Type == ModuleRules.ModuleType.External)
					{
						ModuleType = UHTModuleType.GameThirdParty;
					}
					else
					{
						if (!ModuleType.HasValue && PluginModuleDesc != null)
						{
							ModuleType = ExternalExecution.GetGameModuleTypeFromDescriptor(PluginModuleDesc);
						}

						if (!ModuleType.HasValue)
						{
							if (ProjectDescriptor != null && ProjectDescriptor.Modules != null)
							{
								ModuleDescriptor ProjectModule = ProjectDescriptor.Modules.FirstOrDefault(x => x.Name == ModuleName);
								if (ProjectModule != null)
								{
									ModuleType = UHTModuleTypeExtensions.GameModuleTypeFromHostType(ProjectModule.Type);
								}
								else
								{
									// No descriptor file or module was not on the list
									ModuleType = UHTModuleType.GameRuntime;
								}
							}
						}
					}
				}

				if (!ModuleType.HasValue)
				{
					throw new BuildException("Unable to determine module type for {0}", ModuleFileName);
				}

				// Get the base directory for paths referenced by the module. If the module's under the UProject source directory use that, otherwise leave it relative to the Engine source directory.
				if (ProjectFile != null)
				{
					DirectoryReference ProjectSourceDirectoryName = DirectoryReference.Combine(ProjectFile.Directory, "Source");
					if (ModuleFileName.IsUnderDirectory(ProjectSourceDirectoryName))
					{
						RulesObject.PublicIncludePaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicIncludePaths);
						RulesObject.PrivateIncludePaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PrivateIncludePaths);
						RulesObject.PublicLibraryPaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicLibraryPaths);
						RulesObject.PublicAdditionalShadowFiles = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicAdditionalShadowFiles);
					}
				}

				// Get the generated code directory. Plugins always write to their own intermediate directory so they can be copied between projects, shared engine 
				// intermediates go in the engine intermediate folder, and anything else goes in the project folder.
				DirectoryReference GeneratedCodeDirectory = null;
				if (RulesObject.Type != ModuleRules.ModuleType.External)
				{
					if (Plugin != null)
					{
						GeneratedCodeDirectory = Plugin.Directory;
					}
					else if (bUseSharedBuildEnvironment && ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
					{
						GeneratedCodeDirectory = UnrealBuildTool.EngineDirectory;
					}
					else
					{
						GeneratedCodeDirectory = ProjectDirectory;
					}
					GeneratedCodeDirectory = DirectoryReference.Combine(GeneratedCodeDirectory, BuildConfiguration.PlatformIntermediateFolder, AppName, "Inc", ModuleName);
				}

				// Don't generate include paths for third party modules; they don't follow our conventions. Core is a special-case... leave it alone
				if (RulesObject.Type != ModuleRules.ModuleType.External && ModuleName != "Core")
				{
					// Add the default include paths to the module rules, if they exist. Would be nice not to include game plugins here, but it would be a regression to change now.
					bool bIsGameModuleOrProgram = ModuleFileName.IsUnderDirectory(TargetCsFilename.Directory) || (Plugin != null && Plugin.LoadedFrom == PluginLoadedFrom.GameProject);
					AddDefaultIncludePathsToModuleRules(ModuleFileName, bIsGameModuleOrProgram, Plugin, RulesObject);

					// Add the path to the generated headers 
					if (GeneratedCodeDirectory != null)
					{
						string RelativeGeneratedCodeDirectory = Utils.CleanDirectorySeparators(GeneratedCodeDirectory.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory), '/');
						RulesObject.PublicIncludePaths.Add(RelativeGeneratedCodeDirectory);
					}
				}

				// Figure out whether we need to build this module
				// We don't care about actual source files when generating projects, as these are discovered separately
				bool bDiscoverFiles = !ProjectFileGenerator.bGenerateProjectFiles;
				bool bBuildFiles = bDiscoverFiles && (OnlyModules.Count == 0 || OnlyModules.Any(x => string.Equals(x.OnlyModuleName, ModuleName, StringComparison.InvariantCultureIgnoreCase)));

				IntelliSenseGatherer IntelliSenseGatherer = null;
				List<FileItem> FoundSourceFiles = new List<FileItem>();
				if (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus || RulesObject.Type == ModuleRules.ModuleType.CPlusPlusCLR)
				{
					ProjectFile ProjectFileForIDE = null;
					if (ProjectFileGenerator.bGenerateProjectFiles && ProjectFileGenerator.ModuleToProjectFileMap.TryGetValue(ModuleName, out ProjectFileForIDE))
					{
						IntelliSenseGatherer = ProjectFileForIDE;
					}

					// So all we care about are the game module and/or plugins.
					if (bDiscoverFiles && (!UnrealBuildTool.IsEngineInstalled() || !ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory)))
					{
						List<FileReference> SourceFilePaths = new List<FileReference>();

						if (ProjectFileForIDE != null)
						{
							foreach (ProjectFile.SourceFile SourceFile in ProjectFileForIDE.SourceFiles)
							{
								SourceFilePaths.Add(SourceFile.Reference);
							}
						}
						else
						{
							// Don't have a project file for this module with the source file names cached already, so find the source files ourselves
							SourceFilePaths = SourceFileSearch.FindModuleSourceFiles(ModuleRulesFile: ModuleFileName);
						}
						FoundSourceFiles = GetCPlusPlusFilesToBuild(SourceFilePaths, ModuleDirectory, Platform);
					}
				}

				// Allow the current platform to modify the module rules
				PlatformContext.ModifyModuleRulesForActivePlatform(ModuleName, RulesObject, TargetInfo);

				// Allow all build platforms to 'adjust' the module setting. 
				// This will allow undisclosed platforms to make changes without 
				// exposing information about the platform in publicly accessible 
				// locations.
				UnrealTargetPlatform Only = UnrealBuildTool.GetOnlyPlatformSpecificFor();
				if (Only == UnrealTargetPlatform.Unknown && UnrealBuildTool.SkipNonHostPlatforms())
				{
					Only = Platform;
				}
				UEBuildPlatform.PlatformModifyHostModuleRules(ModuleName, RulesObject, TargetInfo, Only);

				// Now, go ahead and create the module builder instance
				Module = InstantiateModule(RulesObject, ModuleName, ModuleType.Value, ModuleDirectory, GeneratedCodeDirectory, IntelliSenseGatherer, FoundSourceFiles, bBuildFiles, ModuleFileName);
				Modules.Add(Module.Name, Module);
				FlatModuleCsData.Add(Module.Name, new FlatModuleCsDataType((Module.RulesFile == null) ? null : Module.RulesFile.FullName));
			}
			return Module;
		}

		protected virtual UEBuildModule InstantiateModule(
			ModuleRules RulesObject,
			string ModuleName,
			UHTModuleType ModuleType,
			DirectoryReference ModuleDirectory,
			DirectoryReference GeneratedCodeDirectory,
			IntelliSenseGatherer IntelliSenseGatherer,
			List<FileItem> ModuleSourceFiles,
			bool bBuildSourceFiles,
			FileReference InRulesFile)
		{
			switch (RulesObject.Type)
			{
				case ModuleRules.ModuleType.CPlusPlus:
					return new UEBuildModuleCPP(
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InGeneratedCodeDirectory: GeneratedCodeDirectory,
							InIntelliSenseGatherer: IntelliSenseGatherer,
							InSourceFiles: ModuleSourceFiles,
							InRules: RulesObject,
							bInBuildSourceFiles: bBuildSourceFiles,
							InRulesFile: InRulesFile
						);

				case ModuleRules.ModuleType.CPlusPlusCLR:
					return new UEBuildModuleCPPCLR(
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InGeneratedCodeDirectory: GeneratedCodeDirectory,
							InIntelliSenseGatherer: IntelliSenseGatherer,
							InSourceFiles: ModuleSourceFiles,
							InRules: RulesObject,
							bInBuildSourceFiles: bBuildSourceFiles,
							InRulesFile: InRulesFile
						);

				case ModuleRules.ModuleType.External:
					return new UEBuildExternalModule(
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InRules: RulesObject,
							InRulesFile: InRulesFile
						);

				default:
					throw new BuildException("Unrecognized module type specified by 'Rules' object {0}", RulesObject.ToString());
			}
		}

		/// <summary>
		/// Add the standard default include paths to the given modulerules object
		/// </summary>
		/// <param name="ModuleFile">The filename to the module rules file (Build.cs)</param>
		/// <param name="IsGameModule">true if it is a game module, false if not</param>
		/// <param name="RulesObject">The module rules object itself</param>
		public void AddDefaultIncludePathsToModuleRules(FileReference ModuleFile, bool IsGameModule, PluginInfo Plugin, ModuleRules RulesObject)
		{
			// Get the base source directory for this module. This may be the project source directory, engine source directory, or plugin source directory.
			if (!ModuleFile.IsUnderDirectory(UnrealBuildTool.EngineSourceDirectory))
			{
				// Add the module source directory 
				DirectoryReference BaseSourceDirectory;
				if (Plugin != null)
				{
					BaseSourceDirectory = DirectoryReference.Combine(Plugin.Directory, "Source");
				}
				else
				{
					BaseSourceDirectory = DirectoryReference.Combine(ProjectFile.Directory, "Source");
				}

				// If it's a game module (plugin or otherwise), add the root source directory to the include paths.
				if (IsGameModule)
				{
					RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(BaseSourceDirectory));
				}

				// Resolve private include paths against the project source root
				for (int Idx = 0; Idx < RulesObject.PrivateIncludePaths.Count; Idx++)
				{
					string PrivateIncludePath = RulesObject.PrivateIncludePaths[Idx];
					if (!Path.IsPathRooted(PrivateIncludePath))
					{
						PrivateIncludePath = DirectoryReference.Combine(BaseSourceDirectory, PrivateIncludePath).FullName;
					}
					RulesObject.PrivateIncludePaths[Idx] = PrivateIncludePath;
				}
			}

			// Add the 'classes' directory, if it exists
			DirectoryReference ClassesDirectory = DirectoryReference.Combine(ModuleFile.Directory, "Classes");
			if (DirectoryLookupCache.DirectoryExists(ClassesDirectory))
			{
				RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(ClassesDirectory));
			}

			// Add all the public directories
			DirectoryReference PublicDirectory = DirectoryReference.Combine(ModuleFile.Directory, "Public");
			if (DirectoryLookupCache.DirectoryExists(PublicDirectory))
			{
				RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(PublicDirectory));

				foreach (DirectoryReference PublicSubDirectory in DirectoryLookupCache.EnumerateDirectoriesRecursively(PublicDirectory))
				{
					RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(PublicSubDirectory));
				}
			}
		}

		/// <summary>
		/// Normalize an include path to be relative to the engine source directory
		/// </summary>
		public static string NormalizeIncludePath(DirectoryReference Directory)
		{
			return Utils.CleanDirectorySeparators(Directory.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory), '/');
		}

		/// <summary>
		/// Finds a module given its name.  Throws an exception if the module couldn't be found.
		/// </summary>
		public UEBuildModule GetModuleByName(string Name)
		{
			UEBuildModule Result;
			if (Modules.TryGetValue(Name, out Result))
			{
				return Result;
			}
			else
			{
				throw new BuildException("Couldn't find referenced module '{0}'.", Name);
			}
		}


		/// <summary>
		/// Combines a list of paths with a base path.
		/// </summary>
		/// <param name="BasePath">Base path to combine with. May be null or empty.</param>
		/// <param name="PathList">List of input paths to combine with. May be null.</param>
		/// <returns>List of paths relative The build module object for the specified build rules source file</returns>
		private static List<string> CombinePathList(DirectoryReference BasePath, List<string> PathList)
		{
			List<string> NewPathList = new List<string>();
			foreach (string Path in PathList)
			{
				NewPathList.Add(System.IO.Path.Combine(BasePath.FullName, Path));
			}
			return NewPathList;
		}


		/// <summary>
		/// Given a list of source files for a module, filters them into a list of files that should actually be included in a build
		/// </summary>
		/// <param name="SourceFiles">Original list of files, which may contain non-source</param>
		/// <param name="SourceFilesBaseDirectory">Directory that the source files are in</param>
		/// <param name="TargetPlatform">The platform we're going to compile for</param>
		/// <returns>The list of source files to actually compile</returns>
		static List<FileItem> GetCPlusPlusFilesToBuild(List<FileReference> SourceFiles, DirectoryReference SourceFilesBaseDirectory, UnrealTargetPlatform TargetPlatform)
		{
			// Make a list of all platform name strings that we're *not* currently compiling, to speed
			// up file path comparisons later on
			List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();
			SupportedPlatforms.Add(TargetPlatform);
			List<string> OtherPlatformNameStrings = Utils.MakeListOfUnsupportedPlatforms(SupportedPlatforms);


			// @todo projectfiles: Consider saving out cached list of source files for modules so we don't need to harvest these each time

			List<FileItem> FilteredFileItems = new List<FileItem>();
			FilteredFileItems.Capacity = SourceFiles.Count;

			// @todo projectfiles: hard-coded source file set.  Should be made extensible by platform tool chains.
			string[] CompilableSourceFileTypes = new string[]
				{
					".cpp",
					".c",
					".cc",
					".mm",
					".m",
					".rc",
					".manifest"
				};

			// When generating project files, we have no file to extract source from, so we'll locate the code files manually
			foreach (FileReference SourceFilePath in SourceFiles)
			{
				// We're only able to compile certain types of files
				bool IsCompilableSourceFile = false;
				foreach (string CurExtension in CompilableSourceFileTypes)
				{
					if (SourceFilePath.HasExtension(CurExtension))
					{
						IsCompilableSourceFile = true;
						break;
					}
				}

				if (IsCompilableSourceFile)
				{
					if (SourceFilePath.IsUnderDirectory(SourceFilesBaseDirectory))
					{
						// Store the path as relative to the project file
						string RelativeFilePath = SourceFilePath.MakeRelativeTo(SourceFilesBaseDirectory);

						// All compiled files should always be in a sub-directory under the project file directory.  We enforce this here.
						if (Path.IsPathRooted(RelativeFilePath) || RelativeFilePath.StartsWith(".."))
						{
							throw new BuildException("Error: Found source file {0} in project whose path was not relative to the base directory of the source files", RelativeFilePath);
						}

						// Check for source files that don't belong to the platform we're currently compiling.  We'll filter
						// those source files out
						bool IncludeThisFile = true;
						foreach (string CurPlatformName in OtherPlatformNameStrings)
						{
							if (RelativeFilePath.IndexOf(Path.DirectorySeparatorChar + CurPlatformName + Path.DirectorySeparatorChar, StringComparison.InvariantCultureIgnoreCase) != -1
								|| RelativeFilePath.StartsWith(CurPlatformName + Path.DirectorySeparatorChar))
							{
								IncludeThisFile = false;
								break;
							}
						}

						if (IncludeThisFile)
						{
							FilteredFileItems.Add(FileItem.GetItemByFileReference(SourceFilePath));
						}
					}
				}
			}

			// @todo projectfiles: Consider enabling this error but changing it to a warning instead.  It can fire for
			//    projects that are being digested for IntelliSense (because the module was set as a cross-
			//	  platform dependency), but all of their source files were filtered out due to platform masking
			//    in the project generator
			bool AllowEmptyProjects = true;
			if (!AllowEmptyProjects)
			{
				if (FilteredFileItems.Count == 0)
				{
					throw new BuildException("Could not find any valid source files for base directory {0}.  Project has {1} files in it", SourceFilesBaseDirectory, SourceFiles.Count);
				}
			}

			return FilteredFileItems;
		}

		/// <summary>
		/// Determines whether we should check binary output paths for this target are
		/// appropriate for the distribution level of their direct module dependencies
		/// </summary>
		private bool ShouldCheckOutputDistributionLevel()
		{
			if (Rules != null)
			{
				// We don't want to check this for uprojects (but do want to include UE4Game)
				if (ProjectFile == null && !Rules.bOutputPubliclyDistributable)
				{
					return true;
				}
			}
			return false;
		}
	}
}
