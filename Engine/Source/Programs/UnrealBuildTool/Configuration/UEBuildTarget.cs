// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;

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
		WinRT,
		WinRT_ARM,
		HTML5,
        Linux,
	}

	public enum UnrealPlatformGroup
	{
		Windows,	// this group is just to lump Win32 and Win64 into Windows directories, removing the special Windows logic in MakeListOfUnsupportedPlatforms
		Microsoft,
		Apple,
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

	/** Helper class for holding project name, platform and config together. */
	public class UnrealBuildConfig
	{
		/** Project to build. */
		public string Name;
		/** Platform to build the project for. */ 
		public UnrealTargetPlatform Platform;
		/** Config to build the project with. */
		public UnrealTargetConfiguration Config;

		/** Constructor */
		public UnrealBuildConfig(string InName, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfig)
		{
			Name = InName;
			Platform = InPlatform;
			Config = InConfig;
		}

		/** Overriden ToString() to make this class esier to read when debugging. */
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
			if(!String.IsNullOrEmpty(DebugInfoExtension))
			{
				AddBuildProduct(Path.ChangeExtension(FileName, DebugInfoExtension));
			}
		}
	}

	/// <summary>
	/// A list of extenal files required to build a given target
	/// </summary>
	public class ExternalFileList
	{
		public readonly List<string> FileNames = new List<string>();
	}

	public class OnlyModule
	{
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

		/** If building only a single module, this is the module name to build */
		public readonly string OnlyModuleName;

		/** When building only a single module, the optional suffix for the module file name */
		public readonly string OnlyModuleSuffix;
	}


	/// <summary>
	/// Describes all of the information needed to initialize a UEBuildTarget object
	/// </summary>
	public class TargetDescriptor
	{
		public string TargetName;
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public List<string> AdditionalDefinitions;
		public bool bIsEditorRecompile;
		public string RemoteRoot;
		public List<OnlyModule> OnlyModules;
	}


	/**
	 * A target that can be built
	 */
	[Serializable]
	public class UEBuildTarget
	{
		public string GetAppName()
		{
			return AppName;
		}

		public string GetTargetName()
		{
			return !String.IsNullOrEmpty(GameName) ? GameName : AppName;
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
				case CPPTargetPlatform.WinRT: 			return UnrealTargetPlatform.WinRT;
				case CPPTargetPlatform.WinRT_ARM: 		return UnrealTargetPlatform.WinRT_ARM;
				case CPPTargetPlatform.IOS:				return UnrealTargetPlatform.IOS;
				case CPPTargetPlatform.HTML5:			return UnrealTargetPlatform.HTML5;
                case CPPTargetPlatform.Linux:           return UnrealTargetPlatform.Linux;
			}
			throw new BuildException("CPPTargetPlatformToUnrealTargetPlatform: Unknown CPPTargetPlatform {0}", InCPPPlatform.ToString());
		}


		public static List<TargetDescriptor> ParseTargetCommandLine(string[] SourceArguments )
		{
			var Targets = new List<TargetDescriptor>();

			string TargetName = null;
			var AdditionalDefinitions = new List<string>();
			var Platform = UnrealTargetPlatform.Unknown;
			var Configuration = UnrealTargetConfiguration.Unknown;
			string RemoteRoot = null;
			var OnlyModules = new List<OnlyModule>();

			// If true, the recompile was launched by the editor.
			bool bIsEditorRecompile = false;


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
						// @todo ubtmake: How does this work with UBTMake?  Do we even need to support it anymore?
						case "-MODULE":
							// Specifies a module to recompile.  Can be specified more than once on the command-line to compile multiple specific modules.
							{
								if (ArgumentIndex + 1 >= Arguments.Count)
								{
									throw new BuildException("Expected module name after -Module argument, but found nothing.");
								}
								var OnlyModuleName = Arguments[++ArgumentIndex];

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

								var OnlyModuleName = Arguments[++ArgumentIndex];
								var OnlyModuleSuffix = Arguments[++ArgumentIndex];

								OnlyModules.Add(new OnlyModule(OnlyModuleName, OnlyModuleSuffix));
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
							    // Force platform to Linux for building IntelliSense files
							    Platform = UnrealTargetPlatform.Linux;

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

                        case "-EDITORRECOMPILE":
							{
								bIsEditorRecompile = true;
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
					// If running Rocket, the PossibleTargetName could contain a path
					TargetName = PossibleTargetName;

					// If a project file was not specified see if we can find one
					string CheckProjectFile = UProjectInfo.GetProjectForTarget(TargetName);
					if (string.IsNullOrEmpty(CheckProjectFile) == false)
					{
						Log.TraceVerbose("Found project file for {0} - {1}", TargetName, CheckProjectFile);
						if (UnrealBuildTool.HasUProjectFile() == false)
						{
							string NewProjectFilename = CheckProjectFile;
							if (Path.IsPathRooted(NewProjectFilename) == false)
							{
								NewProjectFilename = Path.GetFullPath(NewProjectFilename);
							}

 							NewProjectFilename = NewProjectFilename.Replace("\\", "/");
							UnrealBuildTool.SetProjectFile(NewProjectFilename);
						}
					}

					if (UnrealBuildTool.HasUProjectFile())
					{
						if( TargetName.Contains( "/" ) || TargetName.Contains( "\\" ) )
						{
							// Parse off the path
							TargetName = Path.GetFileNameWithoutExtension( TargetName );
						}
					}

					Targets.Add( new TargetDescriptor()
						{
							TargetName = TargetName,
							Platform = Platform,
							Configuration = Configuration,
							AdditionalDefinitions = AdditionalDefinitions,
							bIsEditorRecompile = bIsEditorRecompile,
							RemoteRoot = RemoteRoot,
							OnlyModules = OnlyModules
						} );
					break;
				}
			}
			if( Targets.Count == 0 )
			{
				throw new BuildException("No target name was specified on the command-line.");
			}
			return Targets;
		}

		public static UEBuildTarget CreateTarget( TargetDescriptor Desc )
		{
			string TargetName = Desc.TargetName;
			List<string> AdditionalDefinitions = Desc.AdditionalDefinitions;
			UnrealTargetPlatform Platform = Desc.Platform;
			UnrealTargetConfiguration Configuration = Desc.Configuration;
			string RemoteRoot = Desc.RemoteRoot;
			List<OnlyModule> OnlyModules = Desc.OnlyModules;
			bool bIsEditorRecompile = Desc.bIsEditorRecompile;

			UEBuildTarget BuildTarget = null;
			if( !ProjectFileGenerator.bGenerateProjectFiles )
			{
				// Configure the rules compiler
				string PossibleAssemblyName = TargetName;
				if (bIsEditorRecompile == true)
				{
					PossibleAssemblyName += "_EditorRecompile";
				}

				// Scan the disk to find source files for all known targets and modules, and generate "in memory" project
				// file data that will be used to determine what to build
				RulesCompiler.SetAssemblyNameAndGameFolders( PossibleAssemblyName, UEBuildTarget.DiscoverAllGameFolders() );
			}

			// Try getting it from the RulesCompiler
			UEBuildTarget Target = RulesCompiler.CreateTarget(
				TargetName:TargetName, 
				Target:new TargetInfo(Platform, Configuration),
				InAdditionalDefinitions:AdditionalDefinitions, 
				InRemoteRoot:RemoteRoot, 
				InOnlyModules:OnlyModules, 
				bInEditorRecompile:bIsEditorRecompile);
			if (Target == null)
			{
				if (UEBuildConfiguration.bCleanProject)
				{
					return null;
				}
				throw new BuildException( "Couldn't find target name {0}.", TargetName );
			}
			else
			{
				BuildTarget = Target;
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

			foreach (var CurArgument in SourceArguments)
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
						    // Force platform to Linux and configuration to Development for building IntelliSense files
						    Platform = UnrealTargetPlatform.Linux;
						    Configuration = UnrealTargetConfiguration.Development;
						    break;

						case "-QMAKEFILE":
						    // Force platform to Linux and configuration to Development for building IntelliSense files
						    Platform = UnrealTargetPlatform.Linux;
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


		/** 
		 * Look for all folders ending in "Game" in the branch root that have a "Source" subfolder
		 * This is defined as a valid game
		 */
		public static List<string> DiscoverAllGameFolders()
		{
			List<string> AllGameFolders = new List<string>();

			// Add all the normal game folders. The UProjectInfo list is already filtered for projects specified on the command line.
			List<UProjectInfo> GameProjects = UProjectInfo.FilterGameProjects(true, null);
			foreach (UProjectInfo GameProject in GameProjects)
			{
				AllGameFolders.Add(GameProject.Folder);
			}

			// @todo: Perversely, if we're not running rocket we need to add the Rocket root folder so that the Rocket build automation script can be found...
			if (!UnrealBuildTool.RunningRocket())
			{
				string RocketFolder = "../../Rocket";
				if (Directory.Exists(RocketFolder))
				{
					AllGameFolders.Add(RocketFolder);
				}
			}

			return AllGameFolders;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="InGameName"></param>
		/// <param name="InRulesObject"></param>
		/// <param name="InPlatform"></param>
		/// <param name="InConfiguration"></param>
		/// <returns></returns>
		public static string GetBinaryBaseName(string InGameName, TargetRules InRulesObject, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InNameAppend)
		{
			//@todo. Allow failure to get build platform here?
			bool bPlatformRequiresMonolithic = false;
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(InPlatform, true);
			if (BuildPlatform != null)
			{
				bPlatformRequiresMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(InPlatform);
			}

			if (InRulesObject.ShouldCompileMonolithic(InPlatform, InConfiguration) || bPlatformRequiresMonolithic)
			{
				return InGameName;
			}
			else
			{
				return "UE4" + InNameAppend;
			}
		}

		/** The target rules */
		[NonSerialized]
		public TargetRules Rules = null;

		/** Type of target, or null if undetermined (such as in the case of a synthetic target with no TargetRules) */
		public TargetRules.TargetType TargetType
		{
			get
			{
				return TargetTypeOrNull.Value;
			}
		}
		private readonly TargetRules.TargetType? TargetTypeOrNull;

		/** The name of the application the target is part of. */
		public string AppName;

		/** The name of the game the target is part of - can be empty */
		public string GameName;

		/** Platform as defined by the VCProject and passed via the command line. Not the same as internal config names. */
		public UnrealTargetPlatform Platform;

		/** Target as defined by the VCProject and passed via the command line. Not necessarily the same as internal name. */
		public UnrealTargetConfiguration Configuration;

		/** Root directory for the active project. Typically contains the .uproject file, or the engine root. */
		public string ProjectDirectory;

		/** Default directory for intermediate files. Typically underneath ProjectDirectory. */
		public string ProjectIntermediateDirectory;

		/** Directory for engine intermediates. For an agnostic editor/game executable, this will be under the engine directory. For monolithic executables this will be the same as the project intermediate directory. */
		public string EngineIntermediateDirectory;

		/** Output paths of final executable. */
		public string[] OutputPaths;

		/** Returns the OutputPath is there is only one entry in OutputPaths */
		public string OutputPath
		{
			get
			{
				if (OutputPaths.Length != 1)
				{
					throw new BuildException("Attempted to use UEBuildTarget.OutputPath property, but there are multiple (or no) OutputPaths. You need to handle multiple in the code that called this (size = {0})", OutputPaths.Length);
				}
				return OutputPaths[0];
			}
		}

		/** Remote path of the binary if it is to be synced with CookerSync */
		public string RemoteRoot;

		/** The C++ environment that all the environments used to compile UE-based modules are derived from. */
		[NonSerialized]
		public CPPEnvironment GlobalCompileEnvironment = new CPPEnvironment();

		/** The link environment all binary link environments are derived from. */
		[NonSerialized]
		public LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment();

		/** All plugins which are built for this target */
		[NonSerialized]
		public List<PluginInfo> BuildPlugins = new List<PluginInfo>();

		/** All plugin dependencies for this target. This differs from the list of plugins that is built for Rocket, where we build everything, but link in only the enabled plugins. */
		[NonSerialized]
		public List<PluginInfo> DependentPlugins = new List<PluginInfo>();

		/** All application binaries; may include binaries not built by this target. */
		[NonSerialized]
		public List<UEBuildBinary> AppBinaries = new List<UEBuildBinary>();

		/** Extra engine module names to either include in the binary (monolithic) or create side-by-side DLLs for (modular) */
		[NonSerialized]
		public List<string> ExtraModuleNames = new List<string>();

		/** True if re-compiling this target from the editor */
		public bool bEditorRecompile;

		/** If building only a specific set of modules, these are the modules to build */
		[NonSerialized]
		protected List<OnlyModule> OnlyModules = new List<OnlyModule>();

		/** true if target should be compiled in monolithic mode, false if not */
		protected bool bCompileMonolithic = false;

		/** Used to keep track of all modules by name. */
		[NonSerialized]
		private Dictionary<string, UEBuildModule> Modules = new Dictionary<string, UEBuildModule>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Whether this target should be compiled in monolithic mode
		/// </summary>
		/// <returns>true if it should, false if it shouldn't</returns>
		public bool ShouldCompileMonolithic()
		{
			return bCompileMonolithic;	// @todo ubtmake: We need to make sure this function and similar things aren't called in assembler mode
		}

		/** 
		 * @param InAppName - The name of the application being built, which is used to scope all intermediate and output file names.
		 * @param InGameName - The name of the game being build - can be empty
		 * @param InPlatform - The platform the target is being built for.
		 * @param InConfiguration - The configuration the target is being built for.
		 * @param InAdditionalDefinitions - Additional definitions provided on the UBT command-line for the target.
		 * @param InRemoteRoot - The remote path that the build output is synced to.
		 */
		public UEBuildTarget(
			string InAppName,
			string InGameName,
			UnrealTargetPlatform InPlatform,
			UnrealTargetConfiguration InConfiguration,
			TargetRules InRulesObject,
			List<string> InAdditionalDefinitions,
			string InRemoteRoot,
			List<OnlyModule> InOnlyModules,
			bool bInEditorRecompile)
		{
			AppName = InAppName;
			GameName = InGameName;
			Platform = InPlatform;
			Configuration = InConfiguration;
			Rules = InRulesObject;
			bEditorRecompile = bInEditorRecompile;

			{
				bCompileMonolithic = (Rules != null) ? Rules.ShouldCompileMonolithic(InPlatform, InConfiguration) : false;

				// Platforms may *require* monolithic compilation...
				bCompileMonolithic |= UEBuildPlatform.PlatformRequiresMonolithicBuilds(InPlatform, InConfiguration);

				// Force monolithic or modular mode if we were asked to
				if( UnrealBuildTool.CommandLineContains("-Monolithic") ||
					UnrealBuildTool.CommandLineContains("MONOLITHIC_BUILD=1") )
				{
					bCompileMonolithic = true;
				}
				else if( UnrealBuildTool.CommandLineContains( "-Modular" ) )
				{
					bCompileMonolithic = false;
				}
			}

			// Figure out what the project directory is. If we have a uproject file, use that. Otherwise use the engine directory.
			if (UnrealBuildTool.HasUProjectFile())
			{
				ProjectDirectory = Path.GetFullPath(UnrealBuildTool.GetUProjectPath());
			}
			else
			{
				ProjectDirectory = Path.GetFullPath(BuildConfiguration.RelativeEnginePath);
			}

			// Build the project intermediate directory
			ProjectIntermediateDirectory = Path.GetFullPath(Path.Combine(ProjectDirectory, BuildConfiguration.PlatformIntermediateFolder, GetTargetName(), Configuration.ToString()));

			// Build the engine intermediate directory. If we're building agnostic engine binaries, we can use the engine intermediates folder. Otherwise we need to use the project intermediates directory.
			if (ShouldCompileMonolithic())
			{
				EngineIntermediateDirectory = ProjectIntermediateDirectory;
			}
			else if(Configuration == UnrealTargetConfiguration.DebugGame)
			{
				EngineIntermediateDirectory = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, BuildConfiguration.PlatformIntermediateFolder, AppName, UnrealTargetConfiguration.Development.ToString()));
			}
			else
			{
				EngineIntermediateDirectory = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, BuildConfiguration.PlatformIntermediateFolder, AppName, Configuration.ToString()));
			}

			RemoteRoot = InRemoteRoot;

			OnlyModules = InOnlyModules;

			TargetTypeOrNull = (Rules != null) ? Rules.Type : (TargetRules.TargetType?)null;

			// Construct the output path based on configuration, platform, game if not specified.
            OutputPaths = MakeBinaryPaths("", AppName, UEBuildBinaryType.Executable, TargetType, null, AppName, Configuration == UnrealTargetConfiguration.Shipping ? Rules.ForceNameAsForDevelopment() : false, Rules.ExeBinariesSubFolder);
			for (int Index = 0; Index < OutputPaths.Length; Index++)
			{
				OutputPaths[Index] = Path.GetFullPath(OutputPaths[Index]);
			}

			if (bCompileMonolithic && TargetRules.IsGameType(InRulesObject.Type))
			{
				// For Rocket, UE4Game.exe and UE4Editor.exe still go into Engine/Binaries/<PLATFORM>
				if (!InRulesObject.bOutputToEngineBinaries)
				{
					// We are compiling a monolithic game...
					// We want the output to go into the <GAME>\Binaries folder
					if (UnrealBuildTool.HasUProjectFile() == false)
					{
						for (int Index = 0; Index < OutputPaths.Length; Index++)
						{
							OutputPaths[Index] = OutputPaths[Index].Replace(Path.Combine("Engine", "Binaries"), Path.Combine(InGameName, "Binaries"));
						}
					}
					else
					{
						string EnginePath = Path.GetFullPath(Path.Combine(ProjectFileGenerator.EngineRelativePath, "Binaries"));
						string UProjectPath = UnrealBuildTool.GetUProjectPath();
						if (Path.IsPathRooted(UProjectPath) == false)
						{
							string FilePath = UProjectInfo.GetProjectForTarget(InGameName);
							string FullPath = Path.GetFullPath(FilePath);
							UProjectPath = Path.GetDirectoryName(FullPath);
						}
						string ProjectPath = Path.GetFullPath(Path.Combine(UProjectPath, "Binaries"));
						for (int Index = 0; Index < OutputPaths.Length; Index++)
						{
							OutputPaths[Index] = OutputPaths[Index].Replace(EnginePath, ProjectPath);
						}
					}
				}
			}

			// handle some special case defines (so build system can pass -DEFINE as normal instead of needing
			// to know about special parameters)
			foreach (string Define in InAdditionalDefinitions)
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
			GlobalCompileEnvironment.Config.Definitions.AddRange(InAdditionalDefinitions);
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
			if (!UnrealBuildTool.RunningRocket())
			{
				var UBTArguments = new StringBuilder();

				UBTArguments.Append("UnrealHeaderTool");
				// Which desktop platform do we need to clean UHT for?
                UBTArguments.Append(" " + BuildHostPlatform.Current.Platform.ToString());
				UBTArguments.Append(" " + UnrealTargetConfiguration.Development.ToString());
				// NOTE: We disable mutex when launching UBT from within UBT to clean UHT
				UBTArguments.Append(" -NoMutex -Clean");

				ExternalExecution.RunExternalExecutable(UnrealBuildTool.GetUBTPath(), UBTArguments.ToString());
			}
		}

		/// <summary>
		/// Cleans all target intermediate files. May also clean UHT if the target uses UObjects.
		/// </summary>
		/// <param name="Binaries">Target binaries</param>
		/// <param name="Platform">Tareet platform</param>
		/// <param name="Manifest">Manifest</param>
		protected void CleanTarget(List<UEBuildBinary> Binaries, CPPTargetPlatform Platform, BuildManifest Manifest)
		{
			{
				var LocalTargetName = (TargetType == TargetRules.TargetType.Program) ? AppName : GameName;
				Log.TraceVerbose("Cleaning target {0} - AppName {1}", LocalTargetName, AppName);

				var TargetFilename = RulesCompiler.GetTargetFilename(GameName);
				Log.TraceVerbose("\tTargetFilename {0}", TargetFilename);

				// Collect all files to delete.
				var AdditionalFileExtensions = new string[] { ".lib", ".exp", ".dll.response" };
				var AllFilesToDelete = new List<string>(Manifest.BuildProducts);
				foreach (var FileManifestItem in Manifest.BuildProducts)
				{
					var FileExt = Path.GetExtension(FileManifestItem);
					if (FileExt == ".dll" || FileExt == ".exe")
					{
						var ManifestFileWithoutExtension = Utils.GetPathWithoutExtension(FileManifestItem);
						foreach (var AdditionalExt in AdditionalFileExtensions)
						{
							var AdditionalFileToDelete = ManifestFileWithoutExtension + AdditionalExt;
							AllFilesToDelete.Add(AdditionalFileToDelete);
						}
					}
				}

				//@todo. This does not clean up files that are no longer built by the target...				
				// Delete all output files listed in the manifest as well as any additional files.
				foreach (var FileToDelete in AllFilesToDelete)
				{
					if (File.Exists(FileToDelete))
					{
						Log.TraceVerbose("\t\tDeleting " + FileToDelete);
						CleanFile(FileToDelete);
					}
				}

				// Generate a list of all the modules of each AppBinaries entry
				var ModuleList = new List<string>();
				var bTargetUsesUObjectModule = false;
				foreach (var AppBin in AppBinaries)
				{
					UEBuildBinaryCPP AppBinCPP = AppBin as UEBuildBinaryCPP;
					if (AppBinCPP != null)
					{
						// Collect all modules used by this binary.
						Log.TraceVerbose("\tProcessing AppBinary " + AppBin.Config.OutputFilePaths[0]);
						foreach (string ModuleName in AppBinCPP.ModuleNames)
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

				var BaseEngineBuildDataFolder = Path.GetFullPath(BuildConfiguration.BaseIntermediatePath).Replace("\\", "/");
				var PlatformEngineBuildDataFolder = BuildConfiguration.BaseIntermediatePath;

				// Delete generated header files
				foreach (var ModuleName in ModuleList)
				{
					var Module = GetModuleByName(ModuleName);
					var ModuleIncludeDir = UEBuildModuleCPP.GetGeneratedCodeDirectoryForModule(this, Module.ModuleDirectory, ModuleName).Replace("\\", "/");
					if (!UnrealBuildTool.RunningRocket() || !ModuleIncludeDir.StartsWith(BaseEngineBuildDataFolder, StringComparison.InvariantCultureIgnoreCase))
					{
						if (Directory.Exists(ModuleIncludeDir))
						{
							Log.TraceVerbose("\t\tDeleting " + ModuleIncludeDir);
							CleanDirectory(ModuleIncludeDir);
						}
					}
				}


				//
				{
					var AppEnginePath = Path.Combine(PlatformEngineBuildDataFolder, LocalTargetName, Configuration.ToString());
					if (Directory.Exists(AppEnginePath))
					{
						CleanDirectory(AppEnginePath);
					}
				}

				// Clean the intermediate directory
				if( !String.IsNullOrEmpty( ProjectIntermediateDirectory ) )
				{
					if (Directory.Exists(ProjectIntermediateDirectory))
					{
						CleanDirectory(ProjectIntermediateDirectory);
					}
				}
				if (!UnrealBuildTool.RunningRocket())
				{
					// This is always under Rocket installation folder
					if (Directory.Exists(GlobalLinkEnvironment.Config.IntermediateDirectory))
					{
						Log.TraceVerbose("\tDeleting " + GlobalLinkEnvironment.Config.IntermediateDirectory);
						CleanDirectory(GlobalLinkEnvironment.Config.IntermediateDirectory);
					}
				}
				else if (ShouldCompileMonolithic())
				{
					// Only in monolithic, otherwise it's pointing to Rocket installation folder
					if (Directory.Exists(GlobalLinkEnvironment.Config.OutputDirectory))
					{
						Log.TraceVerbose("\tDeleting " + GlobalLinkEnvironment.Config.OutputDirectory);
						CleanDirectory(GlobalCompileEnvironment.Config.OutputDirectory);
					}
				}

				// Delete the dependency caches
				{
					var FlatCPPIncludeDependencyCacheFilename = FlatCPPIncludeDependencyCache.GetDependencyCachePathForTarget(this);
					if (File.Exists(FlatCPPIncludeDependencyCacheFilename))
					{
						Log.TraceVerbose("\tDeleting " + FlatCPPIncludeDependencyCacheFilename);
						CleanFile(FlatCPPIncludeDependencyCacheFilename);
					}
					var DependencyCacheFilename = DependencyCache.GetDependencyCachePathForTarget(this);
					if (File.Exists(DependencyCacheFilename))
					{
						Log.TraceVerbose("\tDeleting " + DependencyCacheFilename);
						CleanFile(DependencyCacheFilename);
					}
				}

				// Delete the UBT makefile
				{
					// Figure out what to call our action graph based on everything we're building
					var TargetDescs = new List<TargetDescriptor>();

					// @todo ubtmake: Only supports cleaning one target at a time :(
					TargetDescs.Add( new TargetDescriptor
						{
							TargetName = GetTargetName(),
							Platform = this.Platform,
							Configuration = this.Configuration
						} );

						var UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath( TargetDescs );
						if (File.Exists(UBTMakefilePath))
						{
							Log.TraceVerbose("\tDeleting " + UBTMakefilePath);
							CleanFile(UBTMakefilePath);
						}
					}

				// Delete the action history
				{
					var ActionHistoryFilename = ActionHistory.GeneratePathForTarget(this);
					if (File.Exists(ActionHistoryFilename))
					{
						Log.TraceVerbose("\tDeleting " + ActionHistoryFilename);
						CleanFile(ActionHistoryFilename);
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

		/** Generates a list of external files which are required to build this target */
		public void GenerateExternalFileList()
		{
			string FileListPath = "../Intermediate/Build/ExternalFiles.xml";

			// Find all the external modules
			HashSet<string> ModuleNames = new HashSet<string>();
			foreach(UEBuildBinary Binary in AppBinaries)
			{
				foreach(UEBuildModule Module in Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false))
				{
					UEBuildExternalModule ExternalModule = Module as UEBuildExternalModule;
					if(ExternalModule != null)
					{
						ModuleNames.Add(ExternalModule.Name);
					}
				}
			}

			// Create a set of filenames
			HashSet<string> FileNames = new HashSet<string>(StringComparer.CurrentCultureIgnoreCase);

			// Get the platform we're building for
			IUEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			// Add all their include paths
			foreach(string ModuleName in ModuleNames)
			{
				// Create the module rules
				string ModuleRulesFileName;
				ModuleRules Rules = RulesCompiler.CreateModuleRules(ModuleName, GetTargetInfo(), out ModuleRulesFileName);

				// Add the rules file itself
				FileNames.Add(ModuleRulesFileName);

				// Get a list of all the library paths
				List<string> LibraryPaths = new List<string>();
				LibraryPaths.Add(Directory.GetCurrentDirectory());
				LibraryPaths.AddRange(Rules.PublicLibraryPaths.Where(x => !x.StartsWith("$(")).Select(x => Path.GetFullPath(x.Replace('/', '\\'))));

				// Add all the libraries
				string LibraryExtension = BuildPlatform.GetBinaryExtension(UEBuildBinaryType.StaticLibrary);
				foreach(string LibraryName in Rules.PublicAdditionalLibraries)
				{
					foreach(string LibraryPath in LibraryPaths)
					{
						string LibraryFileName = Path.Combine(LibraryPath, LibraryName);
						if(File.Exists(LibraryFileName))
						{
							FileNames.Add(LibraryFileName);
						}

						string UnixLibraryFileName = Path.Combine(LibraryPath, "lib" + LibraryName + LibraryExtension);
						if(File.Exists(UnixLibraryFileName))
						{
							FileNames.Add(UnixLibraryFileName);
						}
					}
				}

				// Add all the additional shadow files
				foreach(string AdditionalShadowFile in Rules.PublicAdditionalShadowFiles)
				{
					string ShadowFileName = Path.GetFullPath(AdditionalShadowFile);
					if(File.Exists(ShadowFileName))
					{
						FileNames.Add(ShadowFileName);
					}
				}

				// Find all the include paths
				List<string> AllIncludePaths = new List<string>();
				AllIncludePaths.AddRange(Rules.PublicIncludePaths);
				AllIncludePaths.AddRange(Rules.PublicSystemIncludePaths);

				// Add all the include paths
				foreach(string IncludePath in AllIncludePaths.Where(x => !x.StartsWith("$(")))
				{
					if(Directory.Exists(IncludePath))
					{
						foreach(string IncludeFileName in Directory.EnumerateFiles(IncludePath, "*", SearchOption.AllDirectories))
						{
							string Extension = Path.GetExtension(IncludeFileName).ToLower();
							if(Extension == ".h" || Extension == ".inl")
							{
								FileNames.Add(IncludeFileName);
							}
						}
					}
				}
			}

			// Normalize all the filenames
			HashSet<string> NormalizedFileNames = new HashSet<string>(StringComparer.CurrentCultureIgnoreCase);
			foreach(string FileName in FileNames)
			{
				string NormalizedFileName = Path.GetFullPath(FileName).Replace('\\', '/');
				NormalizedFileNames.Add(NormalizedFileName);
			}

			// Add the existing filenames
			if(UEBuildConfiguration.bMergeExternalFileList)
			{
				foreach(string FileName in Utils.ReadClass<ExternalFileList>(FileListPath).FileNames)
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

		/** Generates a public manifest file for writing out */
        public void GenerateManifest(IUEToolChain ToolChain, List<UEBuildBinary> Binaries, CPPTargetPlatform Platform, List<string> SpecialRocketLibFilesThatAreBuildProducts)
		{
			string ManifestPath;
			if (UnrealBuildTool.RunningRocket())
			{
				ManifestPath = Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.BaseIntermediateFolder, "Manifest.xml");
			}
			else
			{
				ManifestPath = "../Intermediate/Build/Manifest.xml";
			}

			BuildManifest Manifest = new BuildManifest();
			if (UEBuildConfiguration.bMergeManifests)
			{
				// Load in existing manifest (if any)
				Manifest = Utils.ReadClass<BuildManifest>(ManifestPath);
			}

			UnrealTargetPlatform TargetPlatform = CPPTargetPlatformToUnrealTargetPlatform( Platform );
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform( TargetPlatform );


			// Iterate over all the binaries, and add the relevant info to the manifest
			foreach( UEBuildBinary Binary in Binaries )
			{
				// Get the platform specific extension for debug info files

				// Don't add static library files to the manifest as we do not check them into perforce.
				// However, add them to the manifest when cleaning the project as we do want to delete 
				// them in that case.
				if (UEBuildConfiguration.bCleanProject == false)
				{
                    if (Binary.Config.Type == UEBuildBinaryType.StaticLibrary)
					{
						continue;
					}
				}
                string DebugInfoExtension = BuildPlatform.GetDebugInfoExtension(Binary.Config.Type);

				// Create and add the binary and associated debug info
				foreach (string OutputFilePath in Binary.Config.OutputFilePaths)
				{
					Manifest.AddBuildProduct(OutputFilePath, DebugInfoExtension);
				}

				// Add all the stripped debug symbols
				if(UnrealBuildTool.BuildingRocket() && (Platform == CPPTargetPlatform.Win32 || Platform == CPPTargetPlatform.Win64))
				{
					foreach(string OutputFilePath in Binary.Config.OutputFilePaths)
					{
						string StrippedPath = Path.Combine(Path.GetDirectoryName(OutputFilePath), Path.GetFileNameWithoutExtension(OutputFilePath) + "-Stripped" + DebugInfoExtension);
						Manifest.AddBuildProduct(StrippedPath);
					}
				}

				if (Binary.Config.Type == UEBuildBinaryType.Executable &&
					  GlobalLinkEnvironment.Config.CanProduceAdditionalConsoleApp &&
					  UEBuildConfiguration.bBuildEditor)
				{
					foreach (string OutputFilePath in Binary.Config.OutputFilePaths)
					{
						Manifest.AddBuildProduct(UEBuildBinary.GetAdditionalConsoleAppPath(OutputFilePath), DebugInfoExtension);
					}
				}

                ToolChain.AddFilesToManifest(Manifest, Binary);
			}
			{
				string DebugInfoExtension = BuildPlatform.GetDebugInfoExtension(UEBuildBinaryType.StaticLibrary);
				foreach (var RedistLib in SpecialRocketLibFilesThatAreBuildProducts)
				{
					Manifest.AddBuildProduct(RedistLib, DebugInfoExtension);
				}
			}


			if (UEBuildConfiguration.bCleanProject)
			{
				CleanTarget(Binaries, Platform, Manifest);
			}
			if (UEBuildConfiguration.bGenerateManifest)
			{
				Utils.WriteClass<BuildManifest>(Manifest, ManifestPath, "");
			}
		}

		/** Builds the target, appending list of output files and returns building result. */
		public ECompilationResult Build(IUEToolChain TargetToolChain, out List<FileItem> OutputItems, out List<UHTModuleInfo> UObjectModules, out string EULAViolationWarning)
		{
			OutputItems = new List<FileItem>();
			UObjectModules = new List<UHTModuleInfo>();

			var SpecialRocketLibFilesThatAreBuildProducts = PreBuildSetup(TargetToolChain);

			EULAViolationWarning = !ProjectFileGenerator.bGenerateProjectFiles
				? CheckForEULAViolation()
				: null;

			// If we're compiling monolithic, make sure the executable knows about all referenced modules
			if (ShouldCompileMonolithic())
			{
				var ExecutableBinary = AppBinaries[0];

				// Search through every binary for dependencies. When building plugin binaries that reference optional engine modules, 
				// we still need to link them into the executable.
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					var AllReferencedModules = Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: true, bForceCircular: true);
					foreach (var CurModule in AllReferencedModules)
					{
						if (CurModule.Binary == null || CurModule.Binary == ExecutableBinary)
						{
							ExecutableBinary.AddModule(CurModule.Name);
						}
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

				if ( (TargetRules.IsAGame(TargetType) || (TargetType == TargetRules.TargetType.Server)) 
					&& IsCurrentPlatform)
				{
					// The hardcoded engine directory needs to be a relative path to match the normal EngineDir format. Not doing so breaks the network file system (TTP#315861).
					string OutputFilePath = ExecutableBinary.Config.OutputFilePath;
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
				if(UnrealBuildTool.HasUProjectFile())
				{
					string ProjectName = Path.GetFileNameWithoutExtension(UnrealBuildTool.GetUProjectFile());
					GlobalCompileEnvironment.Config.Definitions.Add(String.Format("UE_PROJECT_NAME={0}", ProjectName));
				}

				// Generate static libraries for monolithic games in Rocket
				if ((UnrealBuildTool.BuildingRocket() || UnrealBuildTool.RunningRocket()) && TargetRules.IsAGame(TargetType))
				{
					List<UEBuildModule> Modules = ExecutableBinary.GetAllDependencyModules(true, false);
					foreach (UEBuildModuleCPP Module in Modules.OfType<UEBuildModuleCPP>())
					{
						if(Utils.IsFileUnderDirectory(Module.ModuleDirectory, BuildConfiguration.RelativeEnginePath) && Module.Binary == ExecutableBinary)
						{
							UnrealTargetConfiguration LibraryConfiguration = (Configuration == UnrealTargetConfiguration.DebugGame)? UnrealTargetConfiguration.Development : Configuration;
							Module.RedistStaticLibraryPaths = MakeBinaryPaths("", "UE4Game-Redist-" + Module.Name, Platform, LibraryConfiguration, UEBuildBinaryType.StaticLibrary, TargetType, null, AppName);
							Module.bBuildingRedistStaticLibrary = UnrealBuildTool.BuildingRocket();
                            if (Module.bBuildingRedistStaticLibrary)
                            {
                                SpecialRocketLibFilesThatAreBuildProducts.AddRange(Module.RedistStaticLibraryPaths);
                            }
						}
					}
				}
			}

			// On Mac and Linux we have actions that should be executed after all the binaries are created
			if (GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Mac || GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Linux)
			{
				TargetToolChain.SetupBundleDependencies(AppBinaries, GameName);
			}

			// Generate the external file list 
			if(UEBuildConfiguration.bGenerateExternalFileList)
			{
				GenerateExternalFileList();
				return ECompilationResult.Succeeded;
			}

			// If we're only generating the manifest, return now
			if (UEBuildConfiguration.bGenerateManifest || UEBuildConfiguration.bCleanProject)
			{
                GenerateManifest(TargetToolChain, AppBinaries, GlobalLinkEnvironment.Config.Target.Platform, SpecialRocketLibFilesThatAreBuildProducts);
                if (!BuildConfiguration.bXGEExport)
                {
                    return ECompilationResult.Succeeded;
                }
			}

			// We don't need to worry about shared PCH headers when only generating project files.  This is
			// just an optimization
			if( !ProjectFileGenerator.bGenerateProjectFiles )
			{
				GlobalCompileEnvironment.SharedPCHHeaderFiles = FindSharedPCHHeaders();
			}

			if( (BuildConfiguration.bXGEExport && UEBuildConfiguration.bGenerateManifest) || (!UEBuildConfiguration.bGenerateManifest && !UEBuildConfiguration.bCleanProject && !ProjectFileGenerator.bGenerateProjectFiles) )
			{
				var UObjectDiscoveryStartTime = DateTime.UtcNow;

				// Figure out which modules have UObjects that we may need to generate headers for
				foreach( var Binary in AppBinaries )
				{
					var LocalUObjectModules = UObjectModules;	// For lambda access
					var DependencyModules = Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
					foreach( var DependencyModuleCPP in DependencyModules.OfType<UEBuildModuleCPP>().Where( CPPModule => !LocalUObjectModules.Any( Module => Module.ModuleName == CPPModule.Name ) ) )
					{
						if( !DependencyModuleCPP.bIncludedInTarget )
						{
							throw new BuildException( "Expecting module {0} to have bIncludeInTarget set", DependencyModuleCPP.Name );
						}

						var UHTModuleInfo = DependencyModuleCPP.GetUHTModuleInfo();
						if( UHTModuleInfo.PublicUObjectClassesHeaders.Count > 0 || UHTModuleInfo.PrivateUObjectHeaders.Count > 0 || UHTModuleInfo.PublicUObjectHeaders.Count > 0 )
						{
							// If we've got this far and there are no source files then it's likely we're running Rocket and ignoring
							// engine files, so we don't need a .generated.cpp either
							UEBuildModuleCPP.AutoGenerateCppInfoClass.BuildInfoClass BuildInfo = null;
							UHTModuleInfo.GeneratedCPPFilenameBase = Path.Combine( UEBuildModuleCPP.GetGeneratedCodeDirectoryForModule(this, UHTModuleInfo.ModuleDirectory, UHTModuleInfo.ModuleName), UHTModuleInfo.ModuleName ) + ".generated";
							if (DependencyModuleCPP.SourceFilesToBuild.Count != 0)
							{
								BuildInfo = new UEBuildModuleCPP.AutoGenerateCppInfoClass.BuildInfoClass( UHTModuleInfo.GeneratedCPPFilenameBase + "*.cpp" );
							}

							DependencyModuleCPP.AutoGenerateCppInfo = new UEBuildModuleCPP.AutoGenerateCppInfoClass(BuildInfo);

							// If we're running in "gather" mode only, we'll go ahead and cache PCH information for each module right now, so that we don't
							// have to do it in the assembling phase.  It's OK for gathering to take a bit longer, even if UObject headers are not out of
							// date in order to save a lot of time in the assembling runs.
							UHTModuleInfo.PCH = "";
							if( UnrealBuildTool.IsGatheringBuild && !UnrealBuildTool.IsAssemblingBuild )
							{
								// We need to figure out which PCH header this module is including, so that UHT can inject an include statement for it into any .cpp files it is synthesizing
								var ModuleCompileEnvironment = DependencyModuleCPP.CreateModuleCompileEnvironment(GlobalCompileEnvironment);
								DependencyModuleCPP.CachePCHUsageForModuleSourceFiles(ModuleCompileEnvironment);
								if (DependencyModuleCPP.ProcessedDependencies.UniquePCHHeaderFile != null)
								{
									UHTModuleInfo.PCH = DependencyModuleCPP.ProcessedDependencies.UniquePCHHeaderFile.AbsolutePath;
								}
							}

							UObjectModules.Add( UHTModuleInfo );
							Log.TraceVerbose( "Detected UObject module: " + UHTModuleInfo.ModuleName );
						}
					}
				}
				
				if( BuildConfiguration.bPrintPerformanceInfo )
				{
					double UObjectDiscoveryTime = ( DateTime.UtcNow - UObjectDiscoveryStartTime ).TotalSeconds;
					Trace.TraceInformation( "UObject discovery time: " + UObjectDiscoveryTime + "s" );
				}

				// @todo ubtmake: Even in Gather mode, we need to run UHT to make sure the files exist for the static action graph to be setup correctly.  This is because UHT generates .cpp
				// files that are injected as top level prerequisites.  If UHT only emitted included header files, we wouldn't need to run it during the Gather phase at all.
				if( UObjectModules.Count > 0 )
				{
					// Execute the header tool
					string ModuleInfoFileName = Path.GetFullPath( Path.Combine( ProjectIntermediateDirectory, "UnrealHeaderTool.manifest" ) );
					ECompilationResult UHTResult = ECompilationResult.OtherCompilationError;
					if (!ExternalExecution.ExecuteHeaderToolIfNecessary(this, GlobalCompileEnvironment, UObjectModules, ModuleInfoFileName, ref UHTResult))
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

			// Build the target's binaries.
			foreach (var Binary in AppBinaries)
			{
				OutputItems.AddRange(Binary.Build(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment));
			}

			return ECompilationResult.Succeeded;
		}

		/// <summary>
		/// Check for EULA violation dependency issues.
		/// </summary>
		/// <returns>EULAViolationWarning if error hasn't been thrown earlier. This behavior is controlled using BuildConfiguration.bBreakBuildOnLicenseViolation.</returns>
		private string CheckForEULAViolation()
		{
			string EULAViolationWarning = null;

			if (TargetType != TargetRules.TargetType.Editor && TargetType != TargetRules.TargetType.Program &&
				BuildConfiguration.bCheckLicenseViolations)
			{
				var RedistributionErrorMessageBuilder = new StringBuilder();
				foreach (var Binary in AppBinaries)
				{
					var NonRedistModules = Binary.GetAllDependencyModules(true, false).Where((DependencyModule) =>
							!DependencyModule.IsRedistributable() && DependencyModule.Name != Binary.Target.AppName
						);

					if (NonRedistModules.Count() != 0)
					{
						RedistributionErrorMessageBuilder.Append(string.Format(
							"{2}Binary {0} depends on: {1}.", Binary.ToString(), string.Join(", ", NonRedistModules),
							RedistributionErrorMessageBuilder.Length > 0 ? "\n" : ""
						));
					}
				}

				if (RedistributionErrorMessageBuilder.Length > 0)
				{
					EULAViolationWarning = string.Format("Non-editor build cannot depend on non-redistributable modules. Details:\n{0}", RedistributionErrorMessageBuilder.ToString());

					if (BuildConfiguration.bBreakBuildOnLicenseViolation)
					{
						throw new BuildException(EULAViolationWarning);
					}
				}
			}

			return EULAViolationWarning;
		}

		/// <summary>
		/// Setup target before build. This method finds dependencies, sets up global environment etc.
		/// </summary>
		/// <returns>Special Rocket lib files that are build products.</returns>
		public List<string> PreBuildSetup(IUEToolChain TargetToolChain)
		{
			// Set up the global compile and link environment in GlobalCompileEnvironment and GlobalLinkEnvironment.
			SetupGlobalEnvironment();

			// Setup the target's modules.
			SetupModules();

			// Setup the target's binaries.
			SetupBinaries();

			// Setup the target's plugins
			SetupPlugins();

			var SpecialRocketLibFilesThatAreBuildProducts = new List<string>();

			// Add the enabled plugins to the build
			foreach (PluginInfo BuildPlugin in BuildPlugins)
			{
				SpecialRocketLibFilesThatAreBuildProducts.AddRange(AddPlugin(BuildPlugin));
			}

			// Allow the platform to setup binaries/plugins/modules
			UEBuildPlatform.GetBuildPlatform(Platform).SetupBinaries(this);

			// Describe what's being built.
			Log.TraceVerbose("Building {0} - {1} - {2} - {3}", AppName, GameName, Platform, Configuration);

			// Put the non-executable output files (PDB, import library, etc) in the intermediate directory.
			GlobalLinkEnvironment.Config.IntermediateDirectory = Path.GetFullPath(GlobalCompileEnvironment.Config.OutputDirectory);
			GlobalLinkEnvironment.Config.OutputDirectory = GlobalLinkEnvironment.Config.IntermediateDirectory;

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalLinkEnvironment.Config.LocalShadowDirectory = GlobalLinkEnvironment.Config.OutputDirectory;

			// Add all of the extra modules, including game modules, that need to be compiled along
			// with this app.  These modules aren't necessarily statically linked against, but may
			// still be required at runtime in order for the application to load and function properly!
			AddExtraModules();

			// Bind modules for all app binaries.
			foreach (var Binary in AppBinaries)
			{
				Binary.BindModules();
			}

			// Process all referenced modules and create new binaries for DLL dependencies if needed
			var NewBinaries = new List<UEBuildBinary>();
			foreach (var Binary in AppBinaries)
			{
				// Add binaries for all of our dependent modules
				var FoundBinaries = Binary.ProcessUnboundModules(AppBinaries[0]);
				if (FoundBinaries != null)
				{
					NewBinaries.AddRange(FoundBinaries);
				}
			}
			AppBinaries.AddRange(NewBinaries);

			// On Mac AppBinaries paths for non-console targets need to be adjusted to be inside the app bundle
			if (GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Mac && !GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				TargetToolChain.FixBundleBinariesPaths(this, AppBinaries);
			}

			// If we're building a single module, then find the binary for that module and add it to our target
			if (OnlyModules.Count > 0)
			{
				AppBinaries = FilterOnlyModules();
			}
			else if (UEBuildConfiguration.bHotReloadFromIDE)
			{
				AppBinaries = FilterGameModules();
			}

			// Filter out binaries that were already built and are just used for linking. We will not build these binaries but we need them for link information
			{
				var FilteredBinaries = new List<UEBuildBinary>();

				foreach (var DLLBinary in AppBinaries)
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
				var FilteredBinaries = new List<UEBuildBinary>();

				var OtherThingsWeNeedToBuild = new List<OnlyModule>();

				foreach (var DLLBinary in AppBinaries)
				{
					if (DLLBinary.Config.bIsCrossTarget)
					{
						FilteredBinaries.Add(DLLBinary);
						bool bIncludeDynamicallyLoaded = false;
						var AllReferencedModules = DLLBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded, bForceCircular: true);
						foreach (var Other in AllReferencedModules)
						{
							OtherThingsWeNeedToBuild.Add(new OnlyModule(Other.Name));
						}
					}
				}
				foreach (var DLLBinary in AppBinaries)
				{
					if (!FilteredBinaries.Contains(DLLBinary) && DLLBinary.FindOnlyModule(OtherThingsWeNeedToBuild) != null)
					{
						if (!File.Exists(DLLBinary.Config.OutputFilePath))
						{
							throw new BuildException("Module {0} is potentially needed for the {1} platform to work, but it isn't actually compiled. This either needs to be marked as platform specific or made a dynamic dependency of something compiled with the base editor.", DLLBinary.Config.OutputFilePath, UnrealBuildTool.GetOnlyPlatformSpecificFor().ToString());
						}
					}
				}

				// Copy the result into the final list
				AppBinaries = FilteredBinaries;
			}

			//@todo.Rocket: Will users be able to rebuild UnrealHeaderTool? NO
			if (UnrealBuildTool.RunningRocket() && AppName != "UnrealHeaderTool")
			{
				var FilteredBinaries = new List<UEBuildBinary>();

				// Have to do absolute here as this could be a project that is under the root
				string FullUProjectPath = Path.GetFullPath(UnrealBuildTool.GetUProjectPath());

				// We only want to build rocket projects...
				foreach (var DLLBinary in AppBinaries)
				{
					if (Utils.IsFileUnderDirectory(DLLBinary.Config.OutputFilePath, FullUProjectPath))
					{
						FilteredBinaries.Add(DLLBinary);
					}
				}

				// Copy the result into the final list
				AppBinaries = FilteredBinaries;

				if (AppBinaries.Count == 0)
				{
					throw new BuildException("Rocket: No modules found to build?");
				}
			}

			return SpecialRocketLibFilesThatAreBuildProducts;
		}

		private static string AddModuleFilenameSuffix(string ModuleName, string FilePath, string Suffix)
		{			
			var MatchPos = FilePath.LastIndexOf(ModuleName, StringComparison.InvariantCultureIgnoreCase);
			if (MatchPos < 0)
			{
				throw new BuildException("Failed to find module name \"{0}\" specified on the command line inside of the output filename \"{1}\" to add appendage.", ModuleName, FilePath);
			}
			var Appendage = "-" + Suffix;
			return FilePath.Insert(MatchPos + ModuleName.Length, Appendage);
		}

		private List<UEBuildBinary> FilterOnlyModules()
		{
			var FilteredBinaries = new List<UEBuildBinary>();

			var AnyBinariesAdded = false;
			foreach (var DLLBinary in AppBinaries)
			{
				var FoundOnlyModule = DLLBinary.FindOnlyModule(OnlyModules);
				if (FoundOnlyModule != null)
				{
					FilteredBinaries.Add(DLLBinary);
					AnyBinariesAdded = true;

					if (!String.IsNullOrEmpty(FoundOnlyModule.OnlyModuleSuffix))
					{
						DLLBinary.Config.OriginalOutputFilePaths = DLLBinary.Config.OutputFilePaths != null ? (string[])DLLBinary.Config.OutputFilePaths.Clone() : null;
						for (int Index = 0; Index < DLLBinary.Config.OutputFilePaths.Length; Index++ )
						{
							DLLBinary.Config.OutputFilePaths[Index] = AddModuleFilenameSuffix(FoundOnlyModule.OnlyModuleName, DLLBinary.Config.OutputFilePaths[Index], FoundOnlyModule.OnlyModuleSuffix);
						}
					}
				}
			}

			if (!AnyBinariesAdded)
		{
				throw new BuildException("One or more of the modules specified using the '-module' argument could not be found.");
			}

			// Copy the result into the final list
			return FilteredBinaries;			
		}

		private List<UEBuildBinary> FilterGameModules()
		{
			var FilteredBinaries = new List<UEBuildBinary>();

			var AnyBinariesAdded = false;
			foreach (var DLLBinary in AppBinaries)
			{
				var GameModules = DLLBinary.FindGameModules();
				if (GameModules != null && GameModules.Count > 0)
				{
					var UniqueSuffix = (new Random((int)(DateTime.Now.Ticks % Int32.MaxValue)).Next(10000)).ToString();
					DLLBinary.Config.OriginalOutputFilePaths = DLLBinary.Config.OutputFilePaths != null ? (string[])DLLBinary.Config.OutputFilePaths.Clone() : null;
					for (int Index = 0; Index < DLLBinary.Config.OutputFilePaths.Length; Index++)
					{
						DLLBinary.Config.OutputFilePaths[Index] = AddModuleFilenameSuffix(GameModules[0].Name, DLLBinary.Config.OutputFilePaths[Index], UniqueSuffix);
					}

					FilteredBinaries.Add(DLLBinary);
					AnyBinariesAdded = true;
				}
			}

			if (!AnyBinariesAdded)
			{
				throw new BuildException("One or more of the modules specified using the '-module' argument could not be found.");
			}

			// Copy the result into the final list
			return FilteredBinaries;
		}

		/// <summary>
		/// All non-program monolithic binaries implicitly depend on all static plugin libraries so they are always linked appropriately
		/// In order to do this, we create a new module here with a cpp file we emit that invokes an empty function in each library.
		/// If we do not do this, there will be no static initialization for libs if no symbols are referenced in them.
		/// </summary>
		private void CreateLinkerFixupsCPPFile()
		{
			var ExecutableBinary = AppBinaries[0];

			List<string> PrivateDependencyModuleNames = new List<string>();

			UEBuildBinaryCPP BinaryCPP = ExecutableBinary as UEBuildBinaryCPP;
			if (BinaryCPP != null)
			{
				TargetInfo CurrentTarget = new TargetInfo(Platform, Configuration, (Rules != null) ? TargetType : (TargetRules.TargetType?)null);
				foreach (var TargetModuleName in BinaryCPP.ModuleNames)
				{
					string UnusedFilename;
					ModuleRules CheckRules = RulesCompiler.CreateModuleRules(TargetModuleName, CurrentTarget, out UnusedFilename);
					if ((CheckRules != null) && (CheckRules.Type != ModuleRules.ModuleType.External))
					{
						PrivateDependencyModuleNames.Add(TargetModuleName);
					}
				}
			}
			foreach (PluginInfo Plugin in DependentPlugins)
			{
				foreach (PluginInfo.PluginModuleInfo Module in Plugin.Modules)
				{
					if(ShouldIncludePluginModule(Plugin, Module))
					{
						PrivateDependencyModuleNames.Add(Module.Name);
					}
				}
			}

			// We ALWAYS have to write this file as the IMPLEMENT_PRIMARY_GAME_MODULE function depends on the UELinkerFixups function existing.
			{				
				string LinkerFixupsName = "UELinkerFixups";

				// Include an empty header so UEBuildModule.Compile does not complain about a lack of PCH
				string HeaderFilename                    = LinkerFixupsName + "Name.h";
				string LinkerFixupHeaderFilenameWithPath = Path.Combine(GlobalCompileEnvironment.Config.OutputDirectory, HeaderFilename);

				// Create the cpp filename
				string LinkerFixupCPPFilename = Path.Combine(GlobalCompileEnvironment.Config.OutputDirectory, LinkerFixupsName + ".cpp");
				if (!File.Exists(LinkerFixupCPPFilename))
				{
					// Create a dummy file in case it doesn't exist yet so that the module does not complain it's not there
					ResponseFile.Create(LinkerFixupHeaderFilenameWithPath, new List<string>());
					ResponseFile.Create(LinkerFixupCPPFilename, new List<string>(new string[] { String.Format("#include \"{0}\"", LinkerFixupHeaderFilenameWithPath) }));
				}

				// Create the source file list (just the one cpp file)
				List<FileItem> SourceFiles = new List<FileItem>();
				SourceFiles.Add(FileItem.GetItemByPath(LinkerFixupCPPFilename));

				// Create the CPP module
				var FakeModuleDirectory = Path.GetDirectoryName( LinkerFixupCPPFilename );
				var NewModule = CreateArtificialModule(LinkerFixupsName, FakeModuleDirectory, SourceFiles, PrivateDependencyModuleNames);

				// Now bind this new module to the executable binary so it will link the plugin libs correctly
				NewModule.bSkipDefinitionsForCompileEnvironment = true;
				BindArtificialModuleToBinary(NewModule, ExecutableBinary);

				// Create the cpp file
				List<string> LinkerFixupsFileContents = new List<string>();
				NewModule.bSkipDefinitionsForCompileEnvironment = false;
				GenerateLinkerFixupsContents(ExecutableBinary, LinkerFixupsFileContents, NewModule.CreateModuleCompileEnvironment(GlobalCompileEnvironment), HeaderFilename, LinkerFixupsName, PrivateDependencyModuleNames);
				NewModule.bSkipDefinitionsForCompileEnvironment = true;
				
				// Determine if the file changed. Write it if it either doesn't exist or the contents are different.
				bool bShouldWriteFile = true;
				if (File.Exists(LinkerFixupCPPFilename))
				{
					string[] ExistingFixupText = File.ReadAllLines(LinkerFixupCPPFilename);
					string JoinedNewContents = string.Join("", LinkerFixupsFileContents.ToArray());
					string JoinedOldContents = string.Join("", ExistingFixupText);
					bShouldWriteFile = (JoinedNewContents != JoinedOldContents);
				}

				// If we determined that we should write the file, write it now.
				if (bShouldWriteFile)
				{
					ResponseFile.Create(LinkerFixupHeaderFilenameWithPath, new List<string>());
					ResponseFile.Create(LinkerFixupCPPFilename, LinkerFixupsFileContents);
				}
			}
		}

		private void GenerateLinkerFixupsContents(UEBuildBinary ExecutableBinary, List<string> LinkerFixupsFileContents, CPPEnvironment CompileEnvironment, string HeaderFilename, string LinkerFixupsName, List<string> PrivateDependencyModuleNames)
		{
			LinkerFixupsFileContents.Add("#include \"" + HeaderFilename + "\"");

			// To reduce the size of the command line for the compiler, we're going to put all definitions inside of the cpp file.
			foreach (var Definition in CompileEnvironment.Config.Definitions)
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
				LinkerFixupsFileContents.Add("#ifndef " + MacroName);
				LinkerFixupsFileContents.Add(String.Format("\t#define {0} {1}", MacroName, MacroValue));
				LinkerFixupsFileContents.Add("#endif");
			}

			// Add a function that is not referenced by anything that invokes all the empty functions in the different static libraries
			LinkerFixupsFileContents.Add("void " + LinkerFixupsName + "()");
			LinkerFixupsFileContents.Add("{");

			// Fill out the body of the function with the empty function calls. This is what causes the static libraries to be considered relevant
			var DependencyModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
			foreach(string ModuleName in DependencyModules.OfType<UEBuildModuleCPP>().Where(CPPModule => CPPModule.AutoGenerateCppInfo != null).Select(CPPModule => CPPModule.Name).Distinct())
			{
				LinkerFixupsFileContents.Add("    extern void EmptyLinkFunctionForGeneratedCode" + ModuleName + "();");
				LinkerFixupsFileContents.Add("    EmptyLinkFunctionForGeneratedCode" + ModuleName + "();");
			}
			foreach (var DependencyModuleName in PrivateDependencyModuleNames)
			{
				LinkerFixupsFileContents.Add("    extern void EmptyLinkFunctionForStaticInitialization" + DependencyModuleName + "();");
				LinkerFixupsFileContents.Add("    EmptyLinkFunctionForStaticInitialization" + DependencyModuleName + "();");
			}

			// End the function body that was started above
			LinkerFixupsFileContents.Add("}");
		}

		/// <summary>
		/// Binds artificial module to given binary.
		/// </summary>
		/// <param name="Module">Module to bind.</param>
		/// <param name="Binary">Binary to bind.</param>
		private void BindArtificialModuleToBinary(UEBuildModuleCPP Module, UEBuildBinary Binary)
		{
			Module.Binary = Binary;
			Module.bIncludedInTarget = true;

			// Process dependencies for this new module
			Module.CachePCHUsageForModuleSourceFiles(Module.CreateModuleCompileEnvironment(GlobalCompileEnvironment));

			// Add module to binary
			Binary.AddModule(Module.Name);
		}

		/// <summary>
		/// Creates artificial module.
		/// </summary>
		/// <param name="Name">Name of the module.</param>
		/// <param name="Directory">Directory of the module.</param>
		/// <param name="SourceFiles">Source files.</param>
		/// <param name="PrivateDependencyModuleNames">Private dependency list.</param>
		/// <returns>Created module.</returns>
		private UEBuildModuleCPP CreateArtificialModule(string Name, string Directory, IEnumerable<FileItem> SourceFiles, IEnumerable<string> PrivateDependencyModuleNames)
		{
			return new UEBuildModuleCPP(
				InTarget: this,
				InName: Name,
				InType: UEBuildModuleType.Game,
				InModuleDirectory: Directory,
				InOutputDirectory: GlobalCompileEnvironment.Config.OutputDirectory,
				InIsRedistributableOverride: null,
				InIntelliSenseGatherer: null,
				InSourceFiles: SourceFiles.ToList(),
				InPublicIncludePaths: new List<string>(),
				InPublicSystemIncludePaths: new List<string>(),
				InDefinitions: new List<string>(),
				InPublicIncludePathModuleNames: new List<string>(),
				InPublicDependencyModuleNames: new List<string>(),
				InPublicDelayLoadDLLs: new List<string>(),
				InPublicAdditionalLibraries: new List<string>(),
				InPublicFrameworks: new List<string>(),
				InPublicWeakFrameworks: new List<string>(),
				InPublicAdditionalShadowFiles: new List<string>(),
				InPublicAdditionalBundleResources: new List<UEBuildBundleResource>(),
				InPublicAdditionalFrameworks: new List<UEBuildFramework>(),
				InPrivateIncludePaths: new List<string>(),
				InPrivateIncludePathModuleNames: new List<string>(),
				InPrivateDependencyModuleNames: PrivateDependencyModuleNames.ToList(),
				InCircularlyReferencedDependentModules: new List<string>(),
				InDynamicallyLoadedModuleNames: new List<string>(),
				InPlatformSpecificDynamicallyLoadedModuleNames: new List<string>(),
				InOptimizeCode: ModuleRules.CodeOptimization.Default,
				InAllowSharedPCH: false,
				InSharedPCHHeaderFile: "",
				InUseRTTI: false,
				InEnableBufferSecurityChecks: true,
				InFasterWithoutUnity: true,
				InMinFilesUsingPrecompiledHeaderOverride: 0,
				InEnableExceptions: false,
				bInBuildSourceFiles: true);
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
			var SharedPCHHeaderFiles = new List<SharedPCHHeaderInfo>();

			// Build up our list of modules with "shared PCH headers".  The list will be in dependency order, with modules
			// that depend on previous modules appearing later in the list
			foreach( var Binary in AppBinaries )
			{
				var CPPBinary = Binary as UEBuildBinaryCPP;
				if( CPPBinary != null )
				{
					foreach( var ModuleName in CPPBinary.ModuleNames )
					{
						var CPPModule = GetModuleByName( ModuleName ) as UEBuildModuleCPP;
						if( CPPModule != null )
						{
							if( !String.IsNullOrEmpty( CPPModule.SharedPCHHeaderFile ) )
							{
								// @todo SharedPCH: Ideally we could figure the PCH header name automatically, and simply use a boolean in the module
								//     definition to opt into exposing a shared PCH.  Unfortunately we don't determine which private PCH header "goes with"
								//     a module until a bit later in the process.  It shouldn't be hard to change that though.
								var SharedPCHHeaderFilePath = ProjectFileGenerator.RootRelativePath + "/Engine/Source/" + CPPModule.SharedPCHHeaderFile;
								var SharedPCHHeaderFileItem = FileItem.GetExistingItemByPath( SharedPCHHeaderFilePath );
								if( SharedPCHHeaderFileItem != null )
								{
									var ModuleDependencies = new Dictionary<string, UEBuildModule>();
									var ModuleList = new List<UEBuildModule>();
									bool bIncludeDynamicallyLoaded = false;
									CPPModule.GetAllDependencyModules(ref ModuleDependencies, ref ModuleList, bIncludeDynamicallyLoaded, bForceCircular: false, bOnlyDirectDependencies:false);

									// Figure out where to insert the shared PCH into our list, based off the module dependency ordering
									int InsertAtIndex = SharedPCHHeaderFiles.Count;
									for( var ExistingModuleIndex = SharedPCHHeaderFiles.Count - 1; ExistingModuleIndex >= 0; --ExistingModuleIndex )
									{
										var ExistingModule = SharedPCHHeaderFiles[ ExistingModuleIndex ].Module;
										var ExistingModuleDependencies = SharedPCHHeaderFiles[ ExistingModuleIndex ].Dependencies;

										// If the module to add to the list is dependent on any modules already in our header list, that module
										// must be inserted after any of those dependencies in the list
										foreach( var ExistingModuleDependency in ExistingModuleDependencies )
										{
											if( ExistingModuleDependency.Value == CPPModule )
											{
												// Make sure we're not a circular dependency of this module.  Circular dependencies always
												// point "upstream".  That is, modules like Engine point to UnrealEd in their
												// CircularlyReferencedDependentModules array, but the natural dependency order is
												// that UnrealEd depends on Engine.  We use this to avoid having modules such as UnrealEd
												// appear before Engine in our shared PCH list.
												// @todo SharedPCH: This is not very easy for people to discover.  Luckily we won't have many shared PCHs in total.
												if( !ExistingModule.CircularlyReferencedDependentModules.Contains( CPPModule.Name ) )
												{
													// We are at least dependent on this module.  We'll keep searching the list to find
													// further-descendant modules we might be dependent on
													InsertAtIndex = ExistingModuleIndex;
													break;
												}
											}
										}
									}

									var NewSharedPCHHeaderInfo = new SharedPCHHeaderInfo();
									NewSharedPCHHeaderInfo.PCHHeaderFile = SharedPCHHeaderFileItem;
									NewSharedPCHHeaderInfo.Module = CPPModule;
									NewSharedPCHHeaderInfo.Dependencies = ModuleDependencies;
									SharedPCHHeaderFiles.Insert( InsertAtIndex, NewSharedPCHHeaderInfo );
								}
								else
								{
									throw new BuildException( "Could not locate the specified SharedPCH header file '{0}' for module '{1}'.", SharedPCHHeaderFilePath, CPPModule.Name );
								}
							}
						}
					}
				}
			}

			if( SharedPCHHeaderFiles.Count > 0 )
			{
				Log.TraceVerbose("Detected {0} shared PCH headers (listed in dependency order):", SharedPCHHeaderFiles.Count);
				foreach( var CurSharedPCH in SharedPCHHeaderFiles )
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
        public List<string> AddPlugin(PluginInfo Plugin)
		{
            var SpecialRocketLibFilesThatAreBuildProducts = new List<string>();
			foreach(PluginInfo.PluginModuleInfo Module in Plugin.Modules)
			{
				if (ShouldIncludePluginModule(Plugin, Module))
				{
                    SpecialRocketLibFilesThatAreBuildProducts.AddRange(AddPluginModule(Plugin, Module));
				}
			}
            return SpecialRocketLibFilesThatAreBuildProducts;
		}

		/// <summary>
		/// Include the given plugin module in the target. Will be built in the appropriate subfolder under the plugin directory.
		/// </summary>
        public List<string> AddPluginModule(PluginInfo Plugin, PluginInfo.PluginModuleInfo Module)
		{
            var SpecialRocketLibFilesThatAreBuildProducts = new List<string>();

			bool bCompileMonolithic = ShouldCompileMonolithic();

			// Get the binary type to build
			UEBuildBinaryType BinaryType = bCompileMonolithic ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;

			// Get the output path. Don't prefix the app name for Rocket
			string[] OutputFilePaths;
			if ((UnrealBuildTool.BuildingRocket() || UnrealBuildTool.RunningRocket()) && bCompileMonolithic)
			{
				OutputFilePaths = MakeBinaryPaths(Module.Name, Module.Name, BinaryType, TargetType, Plugin, AppName);
                if (UnrealBuildTool.BuildingRocket())
                {
                    SpecialRocketLibFilesThatAreBuildProducts.AddRange(OutputFilePaths);
                }
			}
			else
			{
				OutputFilePaths = MakeBinaryPaths(Module.Name, GetAppName() + "-" + Module.Name, BinaryType, TargetType, Plugin, AppName);
			}

			// Try to determine if we have the rules file
			var ModuleFilename = RulesCompiler.GetModuleFilename(Module.Name);
			var bHasModuleRules = String.IsNullOrEmpty(ModuleFilename) == false;

			// Figure out whether we should build it from source
			var ModuleSourceFolder = bHasModuleRules ? Path.GetDirectoryName(RulesCompiler.GetModuleFilename(Module.Name)) : ModuleFilename;
			bool bShouldBeBuiltFromSource = bHasModuleRules && Directory.GetFiles(ModuleSourceFolder, "*.cpp", SearchOption.AllDirectories).Length > 0;

			string PluginIntermediateBuildPath;
			{ 
				if (Plugin.LoadedFrom == PluginInfo.LoadedFromType.Engine)
				{
					// Plugin folder is in the engine directory
					var PluginConfiguration = Configuration == UnrealTargetConfiguration.DebugGame ? UnrealTargetConfiguration.Development : Configuration;
					PluginIntermediateBuildPath = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, BuildConfiguration.PlatformIntermediateFolder, AppName, PluginConfiguration.ToString()));
				}
				else
				{
					// Plugin folder is in the project directory
					PluginIntermediateBuildPath = Path.GetFullPath(Path.Combine(ProjectDirectory, BuildConfiguration.PlatformIntermediateFolder, GetTargetName(), Configuration.ToString()));
				}
				PluginIntermediateBuildPath = Path.Combine(PluginIntermediateBuildPath, "Plugins", ShouldCompileMonolithic() ? "Static" : "Dynamic");
			}

			// Create the binary
			UEBuildBinaryConfiguration Config = new UEBuildBinaryConfiguration( InType:                  BinaryType,
																				InOutputFilePaths:       OutputFilePaths,
																				InIntermediateDirectory: PluginIntermediateBuildPath,
																				bInAllowExports:         true,
																				bInAllowCompilation:     bShouldBeBuiltFromSource,
																				bInHasModuleRules:       bHasModuleRules,
																				InModuleNames:           new List<string> { Module.Name } );
			AppBinaries.Add(new UEBuildBinaryCPP(this, Config));
            return SpecialRocketLibFilesThatAreBuildProducts;
		}

		/// When building a target, this is called to add any additional modules that should be compiled along
		/// with the main target.  If you override this in a derived class, remember to call the base implementation!
		protected virtual void AddExtraModules()
		{
			// Add extra modules that will either link into the main binary (monolithic), or be linked into separate DLL files (modular)
			foreach (var ModuleName in ExtraModuleNames)
			{
				AddExtraModule(ModuleName);
			}
		}

		/// <summary>
		/// Adds extra module to the target.
		/// </summary>
		/// <param name="ModuleName">Name of the module.</param>
		protected void AddExtraModule(string ModuleName)
		{
			if (ShouldCompileMonolithic())
			{
				// Add this module to the executable's list of included modules
				var ExecutableBinary = AppBinaries[0];
				ExecutableBinary.AddModule(ModuleName);
			}
			else
			{
				// Create a DLL binary for this module
				string[] OutputFilePaths = MakeBinaryPaths(ModuleName, GetAppName() + "-" + ModuleName, UEBuildBinaryType.DynamicLinkLibrary, TargetType, null, AppName);
				UEBuildBinaryConfiguration Config = new UEBuildBinaryConfiguration(InType: UEBuildBinaryType.DynamicLinkLibrary,
																					InOutputFilePaths: OutputFilePaths,
																					InIntermediateDirectory: RulesCompiler.IsGameModule(ModuleName) ? ProjectIntermediateDirectory : EngineIntermediateDirectory,
																					bInAllowExports: true,
																					InModuleNames: new List<string> { ModuleName });

				// Tell the target about this new binary
				AppBinaries.Add(new UEBuildBinaryCPP(this, Config));
			}
		}

		/**
		 * @return true if debug information is created, false otherwise.
		 */
		public bool IsCreatingDebugInfo()
		{
			return GlobalCompileEnvironment.Config.bCreateDebugInfo;
		}

		/**
		 * @return The overall output directory of actions for this target.
		 */
		public string GetOutputDirectory()
		{
			return GlobalCompileEnvironment.Config.OutputDirectory;
		}

		/** 
		 * @return true if we are building for dedicated server, false otherwise.
		 */
		[Obsolete("IsBuildingDedicatedServer() has been deprecated and will be removed in future release. Update your code to use TargetInfo.Type instead or your code will not compile.")]
		public bool IsBuildingDedicatedServer()
		{
			return false;
		}

		/** Given a UBT-built binary name (e.g. "Core"), returns a relative path to the binary for the current build configuration (e.g. "../Binaries/Win64/Core-Win64-Debug.lib") */
		public static string[] MakeBinaryPaths(string ModuleName, string BinaryName, UnrealTargetPlatform Platform, 
			UnrealTargetConfiguration Configuration, UEBuildBinaryType BinaryType, TargetRules.TargetType? TargetType, PluginInfo PluginInfo, string AppName, bool bForceNameAsForDevelopment = false, string ExeBinariesSubFolder = null)
		{
			// Determine the binary extension for the platform and binary type.
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			string BinaryExtension;

			if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				BinaryExtension = BuildPlatform.GetBinaryExtension(BinaryType);
			}
			else
			{
				BinaryExtension = @"-" + BuildConfiguration.UCAModuleToAnalyze + @".analysis";
			}

			UnrealTargetConfiguration LocalConfig = Configuration;
			if(Configuration == UnrealTargetConfiguration.DebugGame && !String.IsNullOrEmpty(ModuleName) && !RulesCompiler.IsGameModule(ModuleName))
			{
				LocalConfig = UnrealTargetConfiguration.Development;
			}
			
			string ModuleBinariesSubDir = "";
            if (BinaryType == UEBuildBinaryType.DynamicLinkLibrary && (string.IsNullOrEmpty(ModuleName) == false))
			{
				// Allow for modules to specify sub-folders in the Binaries folder
				var RulesFilename = RulesCompiler.GetModuleFilename(ModuleName);
				// Plugins can be binary-only and can have no rules object
				if (PluginInfo == null || !String.IsNullOrEmpty(RulesFilename))
				{
					ModuleRules ModuleRulesObj = RulesCompiler.CreateModuleRules(ModuleName, new TargetInfo(Platform, Configuration, TargetType), out RulesFilename);
					if (ModuleRulesObj != null)
					{
						ModuleBinariesSubDir = ModuleRulesObj.BinariesSubFolder;
					}
				}
			}
            else if ( BinaryType == UEBuildBinaryType.Executable && string.IsNullOrEmpty(ExeBinariesSubFolder) == false )
            {
                ModuleBinariesSubDir = ExeBinariesSubFolder;
            }

			//@todo.Rocket: This just happens to work since exp and lib files go into intermediate...

			// Build base directory string ("../Binaries/<Platform>/")
			string BinariesDirName;
			if(TargetType.HasValue && TargetType.Value == TargetRules.TargetType.Program && UnrealBuildTool.HasUProjectFile() && !ProjectFileGenerator.bGenerateProjectFiles)
			{
				BinariesDirName = Path.Combine( UnrealBuildTool.GetUProjectPath(), "Binaries" );
			}
			else if( PluginInfo != null )
			{
				BinariesDirName = Path.Combine( PluginInfo.Directory, "Binaries" );
			}
			else
			{
				BinariesDirName = Path.Combine( "..", "Binaries" );
			}
			var BaseDirectory = Path.Combine( BinariesDirName, Platform.ToString());
			if (ModuleBinariesSubDir.Length > 0)
			{
				BaseDirectory = Path.Combine(BaseDirectory, ModuleBinariesSubDir);
			}

			string BinarySuffix = "";
			if ((PluginInfo != null) && (BinaryType != UEBuildBinaryType.DynamicLinkLibrary))
			{
				BinarySuffix = "-Static";
			}

			// append the architecture to the end of the binary name
            BinarySuffix = BuildPlatform.ApplyArchitectureName(BinarySuffix);

			string OutBinaryPath = "";
			// Append binary file name
			string Prefix = "";
			if (Platform == UnrealTargetPlatform.Linux && (BinaryType == UEBuildBinaryType.DynamicLinkLibrary || BinaryType == UEBuildBinaryType.StaticLibrary))
			{
				Prefix = "lib";
			}

			if (LocalConfig == UnrealTargetConfiguration.Development || bForceNameAsForDevelopment)
			{
				OutBinaryPath = Path.Combine(BaseDirectory, String.Format("{3}{0}{1}{2}", BinaryName, BinarySuffix, BinaryExtension, Prefix));
			}
			else
			{
				OutBinaryPath = Path.Combine(BaseDirectory, String.Format("{5}{0}-{1}-{2}{3}{4}", 
					BinaryName, Platform.ToString(), LocalConfig.ToString(), BinarySuffix, BinaryExtension, Prefix));
			}

			return BuildPlatform.FinalizeBinaryPaths(OutBinaryPath);
		}

		/** Given a UBT-built binary name (e.g. "Core"), returns a relative path to the binary for the current build configuration (e.g. "../../Binaries/Win64/Core-Win64-Debug.lib") */
		public string[] MakeBinaryPaths(string ModuleName, string BinaryName, UEBuildBinaryType BinaryType, TargetRules.TargetType? TargetType, PluginInfo PluginInfo, string AppName, bool bForceNameAsForDevelopment = false, string ExeBinariesSubFolder = null)
		{
			if (String.IsNullOrEmpty(ModuleName) && Configuration == UnrealTargetConfiguration.DebugGame && !bCompileMonolithic)
			{
				return MakeBinaryPaths(ModuleName, BinaryName, Platform, UnrealTargetConfiguration.Development, BinaryType, TargetType, PluginInfo, AppName, bForceNameAsForDevelopment);
			}
			else
			{
				return MakeBinaryPaths(ModuleName, BinaryName, Platform, Configuration, BinaryType, TargetType, PluginInfo, AppName, bForceNameAsForDevelopment, ExeBinariesSubFolder);
			}
		}

		/// <summary>
		/// Determines whether the given plugin module is part of the current build.
		/// </summary>
		private bool ShouldIncludePluginModule(PluginInfo Plugin, PluginInfo.PluginModuleInfo Module)
		{
			// Check it can be built for this platform...
			if (Module.Platforms.Contains(Platform))
			{
				// ...and that it's appropriate for the current build environment.
				switch (Module.Type)
				{
					case PluginInfo.PluginModuleType.Runtime:
					case PluginInfo.PluginModuleType.RuntimeNoCommandlet:
						return true;

					case PluginInfo.PluginModuleType.Developer:
						return UEBuildConfiguration.bBuildDeveloperTools;

					case PluginInfo.PluginModuleType.Editor:
					case PluginInfo.PluginModuleType.EditorNoCommandlet:
						return UEBuildConfiguration.bBuildEditor;

					case PluginInfo.PluginModuleType.Program:
						return TargetType == TargetRules.TargetType.Program;
				}
			}
			return false;
		}

		/** Sets up the modules for the target. */
		protected virtual void SetupModules()
		{
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			List<string> PlatformExtraModules = new List<string>();
			BuildPlatform.GetExtraModules(new TargetInfo(Platform, Configuration, TargetType), this, ref PlatformExtraModules);
			ExtraModuleNames.AddRange(PlatformExtraModules);			
		}

		/** Sets up the plugins for this target */
		protected virtual void SetupPlugins()
		{
			// Filter the plugins list by the current project
			List<PluginInfo> ValidPlugins = new List<PluginInfo>();
			foreach(PluginInfo Plugin in Plugins.AllPlugins)
			{
				if(Plugin.LoadedFrom != PluginInfo.LoadedFromType.GameProject || Plugin.Directory.StartsWith(ProjectDirectory, StringComparison.InvariantCultureIgnoreCase))
				{
					ValidPlugins.Add(Plugin);
				}
			}

			// For Rocket builds, remove plugin that's designed for a NDA console
			if(UnrealBuildTool.BuildingRocket())
			{
				ValidPlugins.RemoveAll(x => x.Directory.IndexOf("\\PS4\\", StringComparison.InvariantCultureIgnoreCase) != -1 || x.Directory.IndexOf("\\XboxOne\\", StringComparison.InvariantCultureIgnoreCase) != -1);
			}

			// Build a list of enabled plugins
			List<string> EnabledPluginNames = new List<string>(Rules.AdditionalPlugins);

			// Add the list of plugins enabled by default
			if (UEBuildConfiguration.bCompileAgainstEngine)
			{
				EnabledPluginNames.AddRange(ValidPlugins.Where(x => x.bEnabledByDefault).Select(x => x.Name));
			}

			// Update the plugin list for game targets
			if(TargetType != TargetRules.TargetType.Program && UnrealBuildTool.HasUProjectFile())
			{
				// Enable all the game specific plugins by default
				EnabledPluginNames.AddRange(ValidPlugins.Where(x => x.LoadedFrom == PluginInfo.LoadedFromType.GameProject).Select(x => x.Name));

				// Use the project settings to update the plugin list for this target
				EnabledPluginNames = UProjectInfo.GetEnabledPlugins(UnrealBuildTool.GetUProjectFile(), EnabledPluginNames, Platform);
			}

			// Set the list of plugins we're dependent on
			PluginInfo[] EnabledPlugins = ValidPlugins.Where(x => EnabledPluginNames.Contains(x.Name)).Distinct().ToArray();
			DependentPlugins.AddRange(EnabledPlugins);

			// Set the list of plugins that should be built
			if (UnrealBuildTool.BuildingRocket() && TargetType != TargetRules.TargetType.Program)
			{
				BuildPlugins.AddRange(ValidPlugins);
			}
			else if (ShouldCompileMonolithic() || TargetType == TargetRules.TargetType.Program)
			{
				BuildPlugins.AddRange(EnabledPlugins);
			}
			else
			{
				BuildPlugins.AddRange(ValidPlugins);
			}
		}

		/** Sets up the binaries for the target. */
		protected virtual void SetupBinaries()
		{
			if (Rules != null)
			{
				List<UEBuildBinaryConfiguration> RulesBuildBinaryConfigurations = new List<UEBuildBinaryConfiguration>();
				List<string> RulesExtraModuleNames = new List<string>();
				Rules.SetupBinaries(
					new TargetInfo(Platform, Configuration, TargetType),
					ref RulesBuildBinaryConfigurations,
					ref RulesExtraModuleNames
					);

				foreach (UEBuildBinaryConfiguration BinaryConfig in RulesBuildBinaryConfigurations)
				{
					BinaryConfig.OutputFilePaths = OutputPaths != null ? (string[])OutputPaths.Clone() : null;
					BinaryConfig.IntermediateDirectory = ProjectIntermediateDirectory;

					if (BinaryConfig.ModuleNames.Count > 0)
					{
						AppBinaries.Add(new UEBuildBinaryCPP( this, BinaryConfig ));
					}
					else
					{
						AppBinaries.Add(new UEBuildBinaryCSDLL( this, BinaryConfig ));
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

		/** Sets up the global compile and link environment for the target. */
		public virtual void SetupGlobalEnvironment()
		{
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			var ToolChain = UEToolChain.GetPlatformToolChain(BuildPlatform.GetCPPTargetPlatform(Platform));
			ToolChain.SetUpGlobalEnvironment();

			// Allow each target type (Game/Editor/Server) to set a default global environment state
			{
				LinkEnvironmentConfiguration RulesLinkEnvConfig = GlobalLinkEnvironment.Config;
				CPPEnvironmentConfiguration RulesCPPEnvConfig = GlobalCompileEnvironment.Config;
				SetupDefaultGlobalEnvironment(
						new TargetInfo(Platform, Configuration, TargetType),
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

				// Don't let games override the global environment...
				// This will potentially cause problems due to them generating a game-agnostic exe.
				// RocketEditor is a special case.
				// 
				bool bAllowGameOverride = !TargetRules.IsGameType(TargetType);
				if (bAllowGameOverride ||
					(Rules.ToString() == "UE4EditorTarget" && UnrealBuildTool.BuildingRocket()) ||	// @todo Rocket: Hard coded target name hack
					Rules.ShouldCompileMonolithic(Platform, Configuration))
				{
					Rules.SetupGlobalEnvironment(
						new TargetInfo(Platform, Configuration, TargetType),
						ref RulesLinkEnvConfig,
						ref RulesCPPEnvConfig
						);

					// Copy them back...
					GlobalLinkEnvironment.Config = RulesLinkEnvConfig;
					GlobalCompileEnvironment.Config = RulesCPPEnvConfig;
				}
				GlobalCompileEnvironment.Config.Definitions.Add(String.Format("IS_PROGRAM={0}", TargetType == TargetRules.TargetType.Program ? "1" : "0"));
			}

			// Validate UE configuration - needs to happen before setting any environment mojo and after argument parsing.
			BuildPlatform.ValidateUEBuildConfiguration();
			UEBuildConfiguration.ValidateConfiguration();

			// Add the 'Engine/Source' path as a global include path for all modules
			var EngineSourceDirectory = Path.GetFullPath( Path.Combine( "..", "..", "Engine", "Source" ) );
			if( !Directory.Exists( EngineSourceDirectory ) )
			{
				throw new BuildException( "Couldn't find Engine/Source directory using relative path" );
			}
			GlobalCompileEnvironment.Config.CPPIncludeInfo.IncludePaths.Add( EngineSourceDirectory );

			//@todo.PLATFORM: Do any platform specific tool chain initialization here if required

			if (BuildConfiguration.bUseMallocProfiler)
			{
				BuildConfiguration.bOmitFramePointers = false;
				GlobalCompileEnvironment.Config.Definitions.Add("USE_MALLOC_PROFILER=1");
			}

			string OutputAppName = GetAppName();

            // Mark it as a Rocket build
            if (UnrealBuildTool.BuildingRocket() || UnrealBuildTool.RunningRocket())
            {
                GlobalCompileEnvironment.Config.Definitions.Add("UE_ROCKET=1");
            }
            else
            {
                GlobalCompileEnvironment.Config.Definitions.Add("UE_ROCKET=0");
            }

			// Rocket intermediates go to the project's intermediate folder.  Rocket never writes to the engine intermediate folder. (Those files are immutable)
			// Also, when compiling in monolithic, all intermediates go to the project's folder.  This is because a project can change definitions that affects all engine translation
			// units too, so they can't be shared between different targets.  They are effectively project-specific engine intermediates.
			if( UnrealBuildTool.RunningRocket() || ( UnrealBuildTool.HasUProjectFile() && ShouldCompileMonolithic() ) )
			{
				//@todo SAS: Add a true Debug mode!
				var IntermediateConfiguration = Configuration;
				if( UnrealBuildTool.RunningRocket() )
				{
				// Only Development and Shipping are supported for engine modules
					if( IntermediateConfiguration != UnrealTargetConfiguration.Development && IntermediateConfiguration != UnrealTargetConfiguration.Shipping )
				{
					IntermediateConfiguration = UnrealTargetConfiguration.Development;
				}
				}

				GlobalCompileEnvironment.Config.OutputDirectory = Path.Combine(BuildConfiguration.PlatformIntermediatePath, OutputAppName, IntermediateConfiguration.ToString());
				if (ShouldCompileMonolithic())
				{
					GlobalCompileEnvironment.Config.OutputDirectory = Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.PlatformIntermediateFolder, OutputAppName, IntermediateConfiguration.ToString());
				}
			}
			else
			{
				var GlobalEnvironmentConfiguration = Configuration == UnrealTargetConfiguration.DebugGame ? UnrealTargetConfiguration.Development : Configuration;
				GlobalCompileEnvironment.Config.OutputDirectory = Path.Combine(BuildConfiguration.PlatformIntermediatePath, OutputAppName, GlobalEnvironmentConfiguration.ToString());
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
					String.Format("WITH_UNREAL_DEVELOPER_TOOLS={0}",UEBuildConfiguration.bBuildDeveloperTools ? "1" : "0"));
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

			// Initialize the compile and link environments for the platform and configuration.
			SetUpPlatformEnvironment();
			SetUpConfigurationEnvironment();

			// Validates current settings and updates if required.
			BuildConfiguration.ValidateConfiguration(
				GlobalCompileEnvironment.Config.Target.Configuration,
				GlobalCompileEnvironment.Config.Target.Platform,
				GlobalCompileEnvironment.Config.bCreateDebugInfo);
		}

		void SetUpPlatformEnvironment()
		{
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			CPPTargetPlatform MainCompilePlatform = BuildPlatform.GetCPPTargetPlatform(Platform);

			GlobalLinkEnvironment.Config.Target.Platform = MainCompilePlatform;
			GlobalCompileEnvironment.Config.Target.Platform = MainCompilePlatform;

			string ActiveArchitecture = BuildPlatform.GetActiveArchitecture();
			GlobalCompileEnvironment.Config.Target.Architecture = ActiveArchitecture;
			GlobalLinkEnvironment.Config.Target.Architecture = ActiveArchitecture;


			// Set up the platform-specific environment.
			BuildPlatform.SetUpEnvironment(this);
		}

		void SetUpConfigurationEnvironment()
		{
			UEBuildPlatform.GetBuildPlatform(Platform).SetUpConfigurationEnvironment(this);

			var UpperExtension = Path.GetExtension (OutputPaths[0]).ToUpperInvariant (); 
			// Check to see if we're compiling a library or not
			bool bIsBuildingDLL = UpperExtension == ".DLL";
			GlobalCompileEnvironment.Config.bIsBuildingDLL = bIsBuildingDLL;
			GlobalLinkEnvironment.Config.bIsBuildingDLL = bIsBuildingDLL;
			bool bIsBuildingLibrary = UpperExtension == ".LIB";
			GlobalCompileEnvironment.Config.bIsBuildingLibrary = bIsBuildingLibrary;
			GlobalLinkEnvironment.Config.bIsBuildingLibrary = bIsBuildingLibrary;
		}

		/** Constructs a TargetInfo object for this target. */
		public TargetInfo GetTargetInfo()
		{
			if(Rules == null)
			{
				return new TargetInfo(Platform, Configuration);
			}
			else
			{
				return new TargetInfo(Platform, Configuration, TargetType);
			}
		}

		/** Registers a module with this target. */
		public void RegisterModule(UEBuildModule Module)
		{
			Debug.Assert(Module.Target == this);
			Modules.Add(Module.Name, Module);
		}

		/** Finds a module given its name.  Throws an exception if the module couldn't be found. */
		public UEBuildModule FindOrCreateModuleByName(string ModuleName)
		{
			UEBuildModule Module;
			if (!Modules.TryGetValue(ModuleName, out Module))
			{
				TargetInfo TargetInfo = GetTargetInfo();

				// Create the module!  (It will be added to our hash table in its constructor)

				// @todo projectfiles: Cross-platform modules can appear here during project generation, but they may have already
				//   been filtered out by the project generator.  This causes the projects to not be added to directories properly.
				string ModuleFileName;
				var RulesObject = RulesCompiler.CreateModuleRules(ModuleName, GetTargetInfo(), out ModuleFileName);
				var ModuleDirectory = Path.GetDirectoryName(ModuleFileName);

				// Making an assumption here that any project file path that contains '../../'
				// is NOT from the engine and therefore must be an application-specific module.
				// Get the type of module we're creating
				var ModuleType = UEBuildModuleType.Unknown;
				var ApplicationOutputPath = "";
				var ModuleFileRelativeToEngineDirectory = Utils.MakePathRelativeTo(ModuleFileName, ProjectFileGenerator.EngineRelativePath);

				// see if it's external
				if (RulesObject.Type == ModuleRules.ModuleType.External)
				{
					ModuleType = UEBuildModuleType.ThirdParty;
				}
				else
				{
					// Check if it's a plugin
					ModuleType = UEBuildModule.GetPluginModuleType(ModuleName);
					if (ModuleType == UEBuildModuleType.Unknown)
					{
						// not a plugin, see if it is a game module 
						if (ModuleFileRelativeToEngineDirectory.StartsWith("..") || Path.IsPathRooted(ModuleFileRelativeToEngineDirectory))
						{
							ApplicationOutputPath = ModuleFileName;
							int SourceIndex = ApplicationOutputPath.LastIndexOf(Path.DirectorySeparatorChar + "Source" + Path.DirectorySeparatorChar);
							if (SourceIndex != -1)
							{
								ApplicationOutputPath = ApplicationOutputPath.Substring(0, SourceIndex + 1);
								ModuleType = UEBuildModuleType.Game;
							}
							else
							{
								throw new BuildException("Unable to parse application directory for module '{0}' ({1}). Is it in '../../<APP>/Source'?",
									ModuleName, ModuleFileName);
							}
						}
						else
						{
							ModuleType = UEBuildModule.GetEngineModuleTypeBasedOnLocation(ModuleName, ModuleType, ModuleFileRelativeToEngineDirectory);
							if (ModuleType == UEBuildModuleType.Unknown)
							{
								throw new BuildException("Unable to determine module type for {0}", ModuleFileName);
							}
						}
					}
				}

				var IsGameModule = !ModuleType.IsEngineModule();

				// Get the base directory for paths referenced by the module. If the module's under the UProject source directory use that, otherwise leave it relative to the Engine source directory.
				string ProjectSourcePath = UnrealBuildTool.GetUProjectSourcePath();
				if (ProjectSourcePath != null)
				{
					string FullProjectSourcePath = Path.GetFullPath(ProjectSourcePath);
					if (Utils.IsFileUnderDirectory(ModuleFileName, FullProjectSourcePath))
					{
						RulesObject.PublicIncludePaths = CombinePathList(ProjectSourcePath, RulesObject.PublicIncludePaths);
						RulesObject.PrivateIncludePaths = CombinePathList(ProjectSourcePath, RulesObject.PrivateIncludePaths);
						RulesObject.PublicLibraryPaths = CombinePathList(ProjectSourcePath, RulesObject.PublicLibraryPaths);
						RulesObject.PublicAdditionalShadowFiles = CombinePathList(ProjectSourcePath, RulesObject.PublicAdditionalShadowFiles);
					}
				}

				// Don't generate include paths for third party modules.  They don't follow our conventions.
				if (RulesObject.Type != ModuleRules.ModuleType.External)
				{
					// Add the default include paths to the module rules, if they exist.
					RulesCompiler.AddDefaultIncludePathsToModuleRules(this, ModuleName, ModuleFileName, ModuleFileRelativeToEngineDirectory, IsGameModule: IsGameModule, RulesObject: ref RulesObject);
				}

				// Figure out whether we need to build this module
				// We don't care about actual source files when generating projects, as these are discovered separately
				bool bDiscoverFiles = !ProjectFileGenerator.bGenerateProjectFiles;
				bool bBuildFiles    = bDiscoverFiles && (OnlyModules.Count == 0 || OnlyModules.Any(x => x.OnlyModuleName == ModuleName));

				IntelliSenseGatherer IntelliSenseGatherer = null;
				List<FileItem> FoundSourceFiles = new List<FileItem>();
				if (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus || RulesObject.Type == ModuleRules.ModuleType.CPlusPlusCLR)
				{
					ProjectFile ProjectFile = null;
					if (ProjectFileGenerator.bGenerateProjectFiles && ProjectFileGenerator.ModuleToProjectFileMap.TryGetValue(ModuleName, out ProjectFile))
					{
						IntelliSenseGatherer = ProjectFile;
					}

					// So all we care about are the game module and/or plugins.
					//@todo Rocket: This assumes plugins that have source will be under the game folder...
					if (bDiscoverFiles && (!UnrealBuildTool.RunningRocket() || Utils.IsFileUnderDirectory(ModuleFileName, UnrealBuildTool.GetUProjectPath())))
					{
						var SourceFilePaths = new List<string>();

						if (ProjectFile != null)
						{
							foreach (var SourceFile in ProjectFile.SourceFiles)
							{
								SourceFilePaths.Add(SourceFile.FilePath);
							}
						}
						else
						{
							// Don't have a project file for this module with the source file names cached already, so find the source files ourselves
							SourceFilePaths = SourceFileSearch.FindModuleSourceFiles(
								ModuleRulesFile: ModuleFileName,
								ExcludeNoRedistFiles: false);
						}
						FoundSourceFiles = GetCPlusPlusFilesToBuild(SourceFilePaths, ModuleDirectory, Platform);
					}
				}

				// Now, go ahead and create the module builder instance
				Module = InstantiateModule(RulesObject, ModuleName, ModuleType, ModuleDirectory, ApplicationOutputPath, IntelliSenseGatherer, FoundSourceFiles, bBuildFiles);
				if(Module == null)
				{
					throw new BuildException("Unrecognized module type specified by 'Rules' object {0}", RulesObject.ToString());
				}

				UnrealTargetPlatform Only = UnrealBuildTool.GetOnlyPlatformSpecificFor();

				if (Only == UnrealTargetPlatform.Unknown && UnrealBuildTool.SkipNonHostPlatforms())
				{
					Only = Platform;
				}
				// Allow all build platforms to 'adjust' the module setting. 
				// This will allow undisclosed platforms to make changes without 
				// exposing information about the platform in publicly accessible 
				// locations.
				UEBuildPlatform.PlatformModifyNewlyLoadedModule(Module, TargetInfo, Only);
			}
			return Module;
		}

		protected virtual UEBuildModule InstantiateModule(
			ModuleRules          RulesObject,
			string               ModuleName,
			UEBuildModuleType    ModuleType,
			string               ModuleDirectory,
			string               ApplicationOutputPath,
			IntelliSenseGatherer IntelliSenseGatherer,
			List<FileItem>       ModuleSourceFiles,
			bool                 bBuildSourceFiles)
		{
			switch (RulesObject.Type)
			{
				case ModuleRules.ModuleType.CPlusPlus:
					return new UEBuildModuleCPP(
							InTarget: this,
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InOutputDirectory: ApplicationOutputPath,
							InIsRedistributableOverride: RulesObject.IsRedistributableOverride,
							InIntelliSenseGatherer: IntelliSenseGatherer,
							InSourceFiles: ModuleSourceFiles,
							InPublicSystemIncludePaths: RulesObject.PublicSystemIncludePaths,
							InPublicIncludePaths: RulesObject.PublicIncludePaths,
							InDefinitions: RulesObject.Definitions,
							InPublicIncludePathModuleNames: RulesObject.PublicIncludePathModuleNames,
							InPublicDependencyModuleNames: RulesObject.PublicDependencyModuleNames,
							InPublicDelayLoadDLLs: RulesObject.PublicDelayLoadDLLs,
							InPublicAdditionalLibraries: RulesObject.PublicAdditionalLibraries,
							InPublicFrameworks: RulesObject.PublicFrameworks,
							InPublicWeakFrameworks: RulesObject.PublicWeakFrameworks,
							InPublicAdditionalFrameworks: RulesObject.PublicAdditionalFrameworks,
							InPublicAdditionalShadowFiles: RulesObject.PublicAdditionalShadowFiles,
							InPublicAdditionalBundleResources: RulesObject.AdditionalBundleResources,
							InPrivateIncludePaths: RulesObject.PrivateIncludePaths,
							InPrivateIncludePathModuleNames: RulesObject.PrivateIncludePathModuleNames,
							InPrivateDependencyModuleNames: RulesObject.PrivateDependencyModuleNames,
							InCircularlyReferencedDependentModules: RulesObject.CircularlyReferencedDependentModules,
							InDynamicallyLoadedModuleNames: RulesObject.DynamicallyLoadedModuleNames,
							InPlatformSpecificDynamicallyLoadedModuleNames: RulesObject.PlatformSpecificDynamicallyLoadedModuleNames,
							InOptimizeCode: RulesObject.OptimizeCode,
							InAllowSharedPCH: (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs) ? false : true,
							InSharedPCHHeaderFile: RulesObject.SharedPCHHeaderFile,
							InUseRTTI: RulesObject.bUseRTTI,
							InEnableBufferSecurityChecks: RulesObject.bEnableBufferSecurityChecks,
							InFasterWithoutUnity: RulesObject.bFasterWithoutUnity,
							InMinFilesUsingPrecompiledHeaderOverride: RulesObject.MinFilesUsingPrecompiledHeaderOverride,
							InEnableExceptions: RulesObject.bEnableExceptions,
							bInBuildSourceFiles: bBuildSourceFiles
						);

				case ModuleRules.ModuleType.CPlusPlusCLR:
					return new UEBuildModuleCPPCLR(
							InTarget: this,
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InOutputDirectory: ApplicationOutputPath,
							InIsRedistributableOverride: RulesObject.IsRedistributableOverride,
							InIntelliSenseGatherer: IntelliSenseGatherer,
							InSourceFiles: ModuleSourceFiles,
							InDefinitions: RulesObject.Definitions,
							InPublicSystemIncludePaths: RulesObject.PublicSystemIncludePaths,
							InPublicIncludePaths: RulesObject.PublicIncludePaths,
							InPublicIncludePathModuleNames: RulesObject.PublicIncludePathModuleNames,
							InPublicDependencyModuleNames: RulesObject.PublicDependencyModuleNames,
							InPublicDelayLoadDLLs: RulesObject.PublicDelayLoadDLLs,
							InPublicAdditionalLibraries: RulesObject.PublicAdditionalLibraries,
							InPublicFrameworks: RulesObject.PublicFrameworks,
							InPublicWeakFrameworks: RulesObject.PublicWeakFrameworks,
							InPublicAdditionalFrameworks: RulesObject.PublicAdditionalFrameworks,
							InPublicAdditionalShadowFiles: RulesObject.PublicAdditionalShadowFiles,
							InPublicAdditionalBundleResources: RulesObject.AdditionalBundleResources,
							InPrivateIncludePaths: RulesObject.PrivateIncludePaths,
							InPrivateIncludePathModuleNames: RulesObject.PrivateIncludePathModuleNames,
							InPrivateDependencyModuleNames: RulesObject.PrivateDependencyModuleNames,
							InPrivateAssemblyReferences: RulesObject.PrivateAssemblyReferences,
							InCircularlyReferencedDependentModules: RulesObject.CircularlyReferencedDependentModules,
							InDynamicallyLoadedModuleNames: RulesObject.DynamicallyLoadedModuleNames,
							InPlatformSpecificDynamicallyLoadedModuleNames: RulesObject.PlatformSpecificDynamicallyLoadedModuleNames,
							InOptimizeCode: RulesObject.OptimizeCode,
							InAllowSharedPCH: (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs) ? false : true,
							InSharedPCHHeaderFile: RulesObject.SharedPCHHeaderFile,
							InUseRTTI: RulesObject.bUseRTTI,
							InEnableBufferSecurityChecks: RulesObject.bEnableBufferSecurityChecks,
							InFasterWithoutUnity: RulesObject.bFasterWithoutUnity,
							InMinFilesUsingPrecompiledHeaderOverride: RulesObject.MinFilesUsingPrecompiledHeaderOverride,
							InEnableExceptions: RulesObject.bEnableExceptions,
							bInBuildSourceFiles : bBuildSourceFiles
						);

				case ModuleRules.ModuleType.External:
					return new UEBuildExternalModule(
							InTarget: this,
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InOutputDirectory: ApplicationOutputPath,
							InIsRedistributableOverride: RulesObject.IsRedistributableOverride,
							InPublicDefinitions: RulesObject.Definitions,
							InPublicSystemIncludePaths: RulesObject.PublicSystemIncludePaths,
							InPublicIncludePaths: RulesObject.PublicIncludePaths,
							InPublicLibraryPaths: RulesObject.PublicLibraryPaths,
							InPublicAdditionalLibraries: RulesObject.PublicAdditionalLibraries,
							InPublicFrameworks: RulesObject.PublicFrameworks,
							InPublicWeakFrameworks: RulesObject.PublicWeakFrameworks,
							InPublicAdditionalFrameworks: RulesObject.PublicAdditionalFrameworks,
							InPublicAdditionalShadowFiles: RulesObject.PublicAdditionalShadowFiles,
							InPublicAdditionalBundleResources: RulesObject.AdditionalBundleResources,
							InPublicDependencyModuleNames: RulesObject.PublicDependencyModuleNames,
							InPublicDelayLoadDLLs: RulesObject.PublicDelayLoadDLLs
						);

				default:
					return null;
			}
		}

		/** Finds a module given its name.  Throws an exception if the module couldn't be found. */
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
		private static List<string> CombinePathList(string BasePath, List<string> PathList)
		{
			List<string> NewPathList = PathList;
			if (PathList != null && !String.IsNullOrEmpty(BasePath))
			{
				NewPathList = new List<string>();
				foreach (string Path in PathList)
				{
					NewPathList.Add(System.IO.Path.Combine(BasePath, Path));
				}
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
		static List<FileItem> GetCPlusPlusFilesToBuild(List<string> SourceFiles, string SourceFilesBaseDirectory, UnrealTargetPlatform TargetPlatform)
		{
			// Make a list of all platform name strings that we're *not* currently compiling, to speed
			// up file path comparisons later on
			var SupportedPlatforms = new List<UnrealTargetPlatform>();
			SupportedPlatforms.Add(TargetPlatform);
			var OtherPlatformNameStrings = Utils.MakeListOfUnsupportedPlatforms(SupportedPlatforms);


			// @todo projectfiles: Consider saving out cached list of source files for modules so we don't need to harvest these each time

			var FilteredFileItems = new List<FileItem>();
			FilteredFileItems.Capacity = SourceFiles.Count;

			// @todo projectfiles: hard-coded source file set.  Should be made extensible by platform tool chains.
			var CompilableSourceFileTypes = new string[]
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
			foreach (var SourceFilePath in SourceFiles)
			{
				// We're only able to compile certain types of files
				bool IsCompilableSourceFile = false;
				foreach (var CurExtension in CompilableSourceFileTypes)
				{
					if (SourceFilePath.EndsWith(CurExtension, StringComparison.InvariantCultureIgnoreCase))
					{
						IsCompilableSourceFile = true;
						break;
					}
				}

				if (IsCompilableSourceFile)
				{
					if (SourceFilePath.StartsWith(SourceFilesBaseDirectory + Path.DirectorySeparatorChar))
					{
						// Store the path as relative to the project file
						var RelativeFilePath = Utils.MakePathRelativeTo(SourceFilePath, SourceFilesBaseDirectory);

						// All compiled files should always be in a sub-directory under the project file directory.  We enforce this here.
						if (Path.IsPathRooted(RelativeFilePath) || RelativeFilePath.StartsWith(".."))
						{
							throw new BuildException("Error: Found source file {0} in project whose path was not relative to the base directory of the source files", RelativeFilePath);
						}

						// Check for source files that don't belong to the platform we're currently compiling.  We'll filter
						// those source files out
						bool IncludeThisFile = true;
						foreach (var CurPlatformName in OtherPlatformNameStrings)
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
							FilteredFileItems.Add(FileItem.GetItemByFullPath(SourceFilePath));
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
	}
}
