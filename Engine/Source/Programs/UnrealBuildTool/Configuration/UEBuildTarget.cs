// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Runtime.Serialization;

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
		Desktop,
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
		Desktop,
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
		public bool bPrecompile;
		public bool bUsePrecompiled;
		public List<string> ForeignPlugins;
		public string ForceReceiptFileName;
	}


	/**
	 * A target that can be built
	 */
	[Serializable]
	public class UEBuildTarget : IDeserializationCallback
	{
		void IDeserializationCallback.OnDeserialization(object sender)
		{
			if (OnlyModules == null)
			{
				OnlyModules = new List<OnlyModule>();
			}

			if (ModuleCsMap == null)
			{
				ModuleCsMap = new Dictionary<string,string>();
			}
		}

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
			List<string> ForeignPlugins = new List<string>();
			string ForceReceiptFileName = null;

			// If true, the recompile was launched by the editor.
			bool bIsEditorRecompile = false;

			// Settings for creating/using static libraries for the engine
			bool bPrecompile = false;
			bool bUsePrecompiled = UnrealBuildTool.RunningRocket();

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

						case "-PLUGIN":
							{
								if (ArgumentIndex + 1 >= Arguments.Count)
								{
									throw new BuildException("Expected plugin filename after -Plugin argument, but found nothing.");
								}

								ForeignPlugins.Add(Arguments[++ArgumentIndex]);
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

						case "-KDEVELOPFILE":
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
							OnlyModules = OnlyModules,
							bPrecompile = bPrecompile,
							bUsePrecompiled = bUsePrecompiled,
							ForeignPlugins = ForeignPlugins,
							ForceReceiptFileName = ForceReceiptFileName
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
			UEBuildTarget BuildTarget = null;
			if( !ProjectFileGenerator.bGenerateProjectFiles )
			{
				// Configure the rules compiler
				string PossibleAssemblyName = Desc.TargetName;
				if (Desc.bIsEditorRecompile == true)
				{
					PossibleAssemblyName += "_EditorRecompile";
				}

				// Scan the disk to find source files for all known targets and modules, and generate "in memory" project
				// file data that will be used to determine what to build
				RulesCompiler.SetAssemblyNameAndGameFolders( PossibleAssemblyName, UEBuildTarget.DiscoverAllGameFolders(), Desc.ForeignPlugins );
			}

			// Try getting it from the RulesCompiler
			UEBuildTarget Target = RulesCompiler.CreateTarget(Desc);
			if (Target == null)
			{
				if (UEBuildConfiguration.bCleanProject)
				{
					return null;
				}
				throw new BuildException( "Couldn't find target name {0}.", Desc.TargetName );
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

						case "-KDEVELOPFILE":
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
		 * Look for all folders with a uproject file, these are valid games
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

			return AllGameFolders;
		}

		/** The target rules */
		[NonSerialized]
		public TargetRules Rules = null;

		/** Type of target */
		public TargetRules.TargetType TargetType;

		/** The name of the application the target is part of. For targets with bUseSharedBuildEnvironment = true, this is typically the name of the base application, eg. UE4Editor for any game editor. */
		public string AppName;

		/** The name of the target */
		public string TargetName;

		/** Whether the target uses the shared build environment. If false, AppName==TargetName and all binaries should be written to the project directory. */
		public bool bUseSharedBuildEnvironment;

		/** Platform as defined by the VCProject and passed via the command line. Not the same as internal config names. */
		public UnrealTargetPlatform Platform;

		/** Target as defined by the VCProject and passed via the command line. Not necessarily the same as internal name. */
		public UnrealTargetConfiguration Configuration;

		/** TargetInfo object which can be passed to RulesCompiler */
		public TargetInfo TargetInfo;

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

		/** Whether to build target modules that can be reused for future builds */
		public bool bPrecompile;

		/** Whether to use precompiled target modules */
		public bool bUsePrecompiled;

		/** The C++ environment that all the environments used to compile UE-based modules are derived from. */
		[NonSerialized]
		public CPPEnvironment GlobalCompileEnvironment = new CPPEnvironment();

		/** The link environment all binary link environments are derived from. */
		[NonSerialized]
		public LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment();

		/** All plugins which are valid for this target */
		[NonSerialized]
		public List<PluginInfo> ValidPlugins;

		/** All plugins which are built for this target */
		[NonSerialized]
		public List<PluginInfo> BuildPlugins;

		/** All plugin dependencies for this target. This differs from the list of plugins that is built for Rocket, where we build everything, but link in only the enabled plugins. */
		[NonSerialized]
		public List<PluginInfo> EnabledPlugins;

		/** Additional plugin filenames which are foreign to this target */
		[NonSerialized]
		public List<string> ForeignPlugins;

		/** All application binaries; may include binaries not built by this target. */
		[NonSerialized]
		public List<UEBuildBinary> AppBinaries = new List<UEBuildBinary>();

		/** Extra engine module names to either include in the binary (monolithic) or create side-by-side DLLs for (modular) */
		[NonSerialized]
		public List<string> ExtraModuleNames = new List<string>();

		/** Extra engine module names which are compiled but not linked into any binary in precompiled engine distributions */
		[NonSerialized]
		public List<UEBuildBinary> PrecompiledBinaries = new List<UEBuildBinary>();

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

		/** Used to map names of modules to their .Build.cs filename */
		private Dictionary<string, string> ModuleCsMap = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);

		/** The receipt for this target, which contains a record of this build. */
		private BuildReceipt Receipt;

		/** Force output of the receipt to an additional filename */
		[NonSerialized]
		private string ForceReceiptFileName;

		/** The name of the .Target.cs file, if the target was created with one */
		private readonly string TargetCsFilenameField;
		public string TargetCsFilename { get { return TargetCsFilenameField; } }

		/// <summary>
		/// A list of the module filenames which were used to build this target.
		/// </summary>
		/// <returns></returns>
		public List<string> GetAllModuleBuildCsFilenames()
		{
			return new List<string>(ModuleCsMap.Values);
		}

		/// <summary>
		/// Whether this target should be compiled in monolithic mode
		/// </summary>
		/// <returns>true if it should, false if it shouldn't</returns>
		public bool ShouldCompileMonolithic()
		{
			return bCompileMonolithic;	// @todo ubtmake: We need to make sure this function and similar things aren't called in assembler mode
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InDesc">Target descriptor</param>
		/// <param name="InRules">The target rules, as created by RulesCompiler.</param>
		/// <param name="InPossibleAppName">The AppName for shared binaries of this target type, if used (null if there is none).</param>
		/// <param name="InTargetCsFilename">The name of the target </param>
		public UEBuildTarget(TargetDescriptor InDesc, TargetRules InRules, string InPossibleAppName, string InTargetCsFilename)
		{
			AppName = InDesc.TargetName;
			TargetName = InDesc.TargetName;
			Platform = InDesc.Platform;
			Configuration = InDesc.Configuration;
			Rules = InRules;
			TargetType = Rules.Type;
			bEditorRecompile = InDesc.bIsEditorRecompile;
			bPrecompile = InDesc.bPrecompile;
			bUsePrecompiled = InDesc.bUsePrecompiled;
			ForeignPlugins = InDesc.ForeignPlugins;
			ForceReceiptFileName = InDesc.ForceReceiptFileName;

			Debug.Assert(InTargetCsFilename == null || InTargetCsFilename.EndsWith(".Target.cs", StringComparison.InvariantCultureIgnoreCase));
			TargetCsFilenameField = InTargetCsFilename;

			{
				bCompileMonolithic = Rules.ShouldCompileMonolithic(InDesc.Platform, InDesc.Configuration);

				// Platforms may *require* monolithic compilation...
				bCompileMonolithic |= UEBuildPlatform.PlatformRequiresMonolithicBuilds(InDesc.Platform, InDesc.Configuration);

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

			TargetInfo = new TargetInfo(Platform, Configuration, Rules.Type, bCompileMonolithic);

			if(InPossibleAppName != null && InRules.ShouldUseSharedBuildEnvironment(TargetInfo))
			{
				AppName = InPossibleAppName;
				bUseSharedBuildEnvironment = true;
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
			if (!bUseSharedBuildEnvironment)
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

			RemoteRoot = InDesc.RemoteRoot;

			OnlyModules = InDesc.OnlyModules;

			// Construct the output paths for this target's executable
			string OutputDirectory;
			if((bCompileMonolithic || TargetType == TargetRules.TargetType.Program) && !Rules.bOutputToEngineBinaries)
			{
				OutputDirectory = ProjectDirectory;
			}
			else
			{
				OutputDirectory = Path.GetFullPath(BuildConfiguration.RelativeEnginePath);
			}
			OutputPaths = MakeExecutablePaths(OutputDirectory, bCompileMonolithic? TargetName : AppName, Platform, Configuration, Rules.UndecoratedConfiguration, bCompileMonolithic && UnrealBuildTool.HasUProjectFile(), Rules.ExeBinariesSubFolder);

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
		/// <param name="Manifest">Manifest</param>
		protected void CleanTarget(BuildManifest Manifest)
		{
			{
				Log.TraceVerbose("Cleaning target {0} - AppName {1}", TargetName, AppName);

				var TargetFilename = RulesCompiler.GetTargetFilename(TargetName);
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

				// Delete the intermediate folder for each binary. This will catch all plugin intermediate folders, as well as any project and engine folders.
				foreach(UEBuildBinary Binary in AppBinaries)
				{
					if(Directory.Exists(Binary.Config.IntermediateDirectory))
					{
						CleanDirectory(Binary.Config.IntermediateDirectory);
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
					UEBuildModuleCPP Module = GetModuleByName(ModuleName) as UEBuildModuleCPP;
					if (Module != null && Module.GeneratedCodeDirectory != null && Directory.Exists(Module.GeneratedCodeDirectory))
					{
						if(!UnrealBuildTool.IsEngineInstalled() || !Utils.IsFileUnderDirectory(Module.GeneratedCodeDirectory, BuildConfiguration.RelativeEnginePath))
						{
							Log.TraceVerbose("\t\tDeleting " + Module.GeneratedCodeDirectory);
							CleanDirectory(Module.GeneratedCodeDirectory);
						}
					}
				}


				//
				{
					var AppEnginePath = Path.Combine(PlatformEngineBuildDataFolder, TargetName, Configuration.ToString());
					if (Directory.Exists(AppEnginePath))
					{
						CleanDirectory(AppEnginePath);
					}
				}

				// Clean the intermediate directory
				if( !String.IsNullOrEmpty( ProjectIntermediateDirectory ) && (!UnrealBuildTool.IsEngineInstalled() || !Utils.IsFileUnderDirectory(ProjectIntermediateDirectory, BuildConfiguration.RelativeEnginePath)))
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
					// @todo ubtmake: Does not yet support cleaning Makefiles that were generated for multiple targets (that code path is currently not used though.)

					var TargetDescs = new List<TargetDescriptor>();
					TargetDescs.Add( new TargetDescriptor
						{
							TargetName = GetTargetName(),
							Platform = this.Platform,
							Configuration = this.Configuration
						} );

					// Normal makefile
					{					
					var UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath( TargetDescs );
					if (File.Exists(UBTMakefilePath))
					{
						Log.TraceVerbose("\tDeleting " + UBTMakefilePath);
						CleanFile(UBTMakefilePath);
					}
				}

					// Hot reload makefile
					{					
						UEBuildConfiguration.bHotReloadFromIDE = true;
						var UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath( TargetDescs );
						if (File.Exists(UBTMakefilePath))
						{
							Log.TraceVerbose("\tDeleting " + UBTMakefilePath);
							CleanFile(UBTMakefilePath);
						}
						UEBuildConfiguration.bHotReloadFromIDE = false;
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
				ModuleRules Rules = RulesCompiler.CreateModuleRules(ModuleName, TargetInfo, out ModuleRulesFileName);

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
        public void GenerateManifest()
		{
			string ManifestPath;
			if (UnrealBuildTool.RunningRocket() && UnrealBuildTool.HasUProjectFile())
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

			// Expand all the paths in the receipt; they'll currently use variables for the engine and project directories
			BuildReceipt ReceiptWithFullPaths = new BuildReceipt(Receipt);
			ReceiptWithFullPaths.ExpandPathVariables(BuildConfiguration.RelativeEnginePath, ProjectDirectory);

			foreach(BuildProduct BuildProduct in ReceiptWithFullPaths.BuildProducts)
			{
				// If we're cleaning, don't add any precompiled binaries to the manifest. We don't want to delete them.
				if(UEBuildConfiguration.bCleanProject && bUsePrecompiled && BuildProduct.IsPrecompiled)
				{
					continue;
				}

				// Don't add static libraries into the manifest unless we're explicitly building them; we don't submit them to Perforce.
				if(!UEBuildConfiguration.bCleanProject && !bPrecompile && (BuildProduct.Type == BuildProductType.StaticLibrary || BuildProduct.Type == BuildProductType.ImportLibrary))
				{
					continue;
				}

				// Otherwise add it
				Manifest.AddBuildProduct(BuildProduct.Path);
			}

			IUEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			if(OnlyModules.Count == 0)
			{
				Manifest.AddBuildProduct(BuildReceipt.GetDefaultPath(ProjectDirectory, TargetName, Platform, Configuration, BuildPlatform.GetActiveArchitecture()));
			}

			if (UEBuildConfiguration.bCleanProject)
			{
				CleanTarget(Manifest);
			}
			if (UEBuildConfiguration.bGenerateManifest)
			{
				Utils.WriteClass<BuildManifest>(Manifest, ManifestPath, "");
			}
		}

		/** Creates the receipt for the target */
		private void PrepareReceipt(IUEToolChain ToolChain)
		{
			Receipt = new BuildReceipt();

			// Add the target properties
			Receipt.SetProperty("TargetName", TargetName);
			Receipt.SetProperty("Platform", Platform.ToString());
			Receipt.SetProperty("Configuration", Configuration.ToString());
			Receipt.SetProperty("Precompile", bPrecompile.ToString());

			// Merge all the binary receipts into this
			foreach(UEBuildBinary Binary in AppBinaries)
			{
				BuildReceipt BinaryReceipt = Binary.MakeReceipt(ToolChain);
				if(Binary.Config.Type == UEBuildBinaryType.StaticLibrary)
				{
					BinaryReceipt.RuntimeDependencies.Clear();
				}
				Receipt.Merge(BinaryReceipt);
			}

			// Convert all the paths to use variables for the engine and project root
			Receipt.InsertStandardPathVariables(BuildConfiguration.RelativeEnginePath, ProjectDirectory);
		}

		/** Writes the receipt for this target to disk */
		public void WriteReceipt()
		{
			if(Receipt != null)
			{
				IUEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
				if(OnlyModules == null || OnlyModules.Count == 0)
				{
					string ReceiptFileName = BuildReceipt.GetDefaultPath(ProjectDirectory, TargetName, Platform, Configuration, BuildPlatform.GetActiveArchitecture());
					Directory.CreateDirectory(Path.GetDirectoryName(ReceiptFileName));
					Receipt.Write(ReceiptFileName);
				}
				if(ForceReceiptFileName != null)
				{
					Directory.CreateDirectory(Path.GetDirectoryName(ForceReceiptFileName));
					Receipt.Write(ForceReceiptFileName);
				}
			}
		}

		/** Builds the target, appending list of output files and returns building result. */
		public ECompilationResult Build(IUEToolChain TargetToolChain, out List<FileItem> OutputItems, out List<UHTModuleInfo> UObjectModules, out string EULAViolationWarning)
		{
			OutputItems = new List<FileItem>();
			UObjectModules = new List<UHTModuleInfo>();

			PreBuildSetup(TargetToolChain);

			EULAViolationWarning = !ProjectFileGenerator.bGenerateProjectFiles
				? CheckForEULAViolation()
				: null;

			// If we're compiling monolithic, make sure the executable knows about all referenced modules
			if (ShouldCompileMonolithic())
			{
				var ExecutableBinary = AppBinaries[0];

				// Add all the modules that the executable depends on. Plugins will be already included in this list.
				var AllReferencedModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: true, bForceCircular: true);
				foreach (var CurModule in AllReferencedModules)
				{
					if (CurModule.Binary == null || CurModule.Binary == ExecutableBinary || CurModule.Binary.Config.Type == UEBuildBinaryType.StaticLibrary)
					{
						ExecutableBinary.AddModule(CurModule.Name);
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
			}

			// On Mac and Linux we have actions that should be executed after all the binaries are created
			if (GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Mac || GlobalLinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Linux)
			{
				TargetToolChain.SetupBundleDependencies(AppBinaries, TargetName);
			}

			// Generate the external file list 
			if(UEBuildConfiguration.bGenerateExternalFileList)
			{
				GenerateExternalFileList();
				return ECompilationResult.Succeeded;
			}

			// Create a receipt for the target
			PrepareReceipt(TargetToolChain);

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
			if( !ProjectFileGenerator.bGenerateProjectFiles )
			{
				GlobalCompileEnvironment.SharedPCHHeaderFiles = FindSharedPCHHeaders();
			}

			if( (BuildConfiguration.bXGEExport && UEBuildConfiguration.bGenerateManifest) || (!UEBuildConfiguration.bGenerateManifest && !UEBuildConfiguration.bCleanProject && !ProjectFileGenerator.bGenerateProjectFiles) )
			{
				var UObjectDiscoveryStartTime = DateTime.UtcNow;

				// Reconstruct a full list of binaries. Binaries which aren't compiled are stripped out of AppBinaries, but we still need to scan them for UHT.
				List<UEBuildBinary> AllAppBinaries = AppBinaries.Union(PrecompiledBinaries).Distinct().ToList();
	
				// Figure out which modules have UObjects that we may need to generate headers for
				foreach( var Binary in AllAppBinaries )
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
							UHTModuleInfo.GeneratedCPPFilenameBase = Path.Combine( DependencyModuleCPP.GeneratedCodeDirectory, UHTModuleInfo.ModuleName ) + ".generated";
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

				// NOTE: Even in Gather mode, we need to run UHT to make sure the files exist for the static action graph to be setup correctly.  This is because UHT generates .cpp
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

			GlobalLinkEnvironment.bShouldCompileMonolithic = ShouldCompileMonolithic();

			// Build the target's binaries.
			foreach (var Binary in AppBinaries)
			{
				OutputItems.AddRange(Binary.Build(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment));
			}

			if( BuildConfiguration.bPrintPerformanceInfo )
			{
				foreach( var SharedPCH in GlobalCompileEnvironment.SharedPCHHeaderFiles )
				{
					Log.TraceInformation( "Shared PCH '" + SharedPCH.Module.Name + "': Used " + SharedPCH.NumModulesUsingThisPCH + " times" );
				}
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
		public void PreBuildSetup(IUEToolChain TargetToolChain)
		{
			// Set up the global compile and link environment in GlobalCompileEnvironment and GlobalLinkEnvironment.
			SetupGlobalEnvironment();

			// Setup the target's modules.
			SetupModules();

			// Setup the target's binaries.
			SetupBinaries();

			// Setup the target's plugins
			SetupPlugins();

			// Add the enabled plugins to the build
			foreach (PluginInfo BuildPlugin in BuildPlugins)
			{
				AddPlugin(BuildPlugin);
			}

			// Allow the platform to setup binaries/plugins/modules
			UEBuildPlatform.GetBuildPlatform(Platform).SetupBinaries(this);

			// Describe what's being built.
			Log.TraceVerbose("Building {0} - {1} - {2} - {3}", AppName, TargetName, Platform, Configuration);

			// Put the non-executable output files (PDB, import library, etc) in the intermediate directory.
			GlobalLinkEnvironment.Config.IntermediateDirectory = Path.GetFullPath(GlobalCompileEnvironment.Config.OutputDirectory);
			GlobalLinkEnvironment.Config.OutputDirectory = GlobalLinkEnvironment.Config.IntermediateDirectory;

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalLinkEnvironment.Config.LocalShadowDirectory = GlobalLinkEnvironment.Config.OutputDirectory;

			// Add all of the extra modules, including game modules, that need to be compiled along
			// with this app.  These modules are always statically linked in monolithic targets, but not necessarily linked to anything in modular targets,
			// and may still be required at runtime in order for the application to load and function properly!
			AddExtraModules();

			// Add all the precompiled modules to the target. In contrast to "Extra Modules", these modules are not compiled into monolithic targets by default.
			AddPrecompiledModules();

			// Bind modules for all app binaries. Static libraries can be linked into other binaries, so bound modules to those first.
			foreach(UEBuildBinary Binary in AppBinaries)
			{
				if(Binary.Config.Type == UEBuildBinaryType.StaticLibrary)
				{
					Binary.BindModules();
				}
			}
			foreach (var Binary in AppBinaries)
			{
				if(Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
				{
					Binary.BindModules();
				}
			}

			// Process all referenced modules and create new binaries for DLL dependencies if needed. The AppBinaries 
			// list may have entries added to it as modules are bound, so make sure we handle those too.
			for(int Idx = 0; Idx < AppBinaries.Count; Idx++)
			{
				AppBinaries[Idx].ProcessUnboundModules();
			}

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
			if (!ProjectFileGenerator.bGenerateProjectFiles && UnrealBuildTool.RunningRocket() && AppName != "UnrealHeaderTool")
			{
				var FilteredBinaries = new List<UEBuildBinary>();

				// Have to do absolute here as this could be a project that is under the root
				string FullEnginePath = Path.GetFullPath(BuildConfiguration.RelativeEnginePath);

				// We only want to build rocket projects...
				foreach (var DLLBinary in AppBinaries)
				{
					if (!Utils.IsFileUnderDirectory(DLLBinary.Config.OutputFilePath, FullEnginePath))
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

			if (ShouldCheckOutputDistributionLevel())
			{
				// Check the distribution level of all binaries based on the dependencies they have
				foreach (var Binary in AppBinaries)
				{
					Binary.CheckOutputDistributionLevelAgainstDependencies();
				}
			}
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
				foreach (var TargetModuleName in BinaryCPP.ModuleNames)
				{
					string UnusedFilename;
					ModuleRules CheckRules = RulesCompiler.CreateModuleRules(TargetModuleName, TargetInfo, out UnusedFilename);
					if ((CheckRules != null) && (CheckRules.Type != ModuleRules.ModuleType.External))
					{
						PrivateDependencyModuleNames.Add(TargetModuleName);
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
				NewModule.bSkipDefinitionsForCompileEnvironment = false;
				var LinkerFixupsFileContents = GenerateLinkerFixupsContents(ExecutableBinary, NewModule.CreateModuleCompileEnvironment(GlobalCompileEnvironment), HeaderFilename, LinkerFixupsName, PrivateDependencyModuleNames);
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

		private  List<string> GenerateLinkerFixupsContents(UEBuildBinary ExecutableBinary, CPPEnvironment CompileEnvironment, string HeaderFilename, string LinkerFixupsName, List<string> PrivateDependencyModuleNames)
		{
			var Result = new List<string>();

			Result.Add("#include \"" + HeaderFilename + "\"");

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
				Result.Add("#ifndef " + MacroName);
				Result.Add(String.Format("\t#define {0} {1}", MacroName, MacroValue));
				Result.Add("#endif");
			}

			// Add a function that is not referenced by anything that invokes all the empty functions in the different static libraries
			Result.Add("void " + LinkerFixupsName + "()");
			Result.Add("{");

			// Fill out the body of the function with the empty function calls. This is what causes the static libraries to be considered relevant
			var DependencyModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
			foreach(string ModuleName in DependencyModules.OfType<UEBuildModuleCPP>().Where(CPPModule => CPPModule.AutoGenerateCppInfo != null).Select(CPPModule => CPPModule.Name).Distinct())
			{
				Result.Add("    extern void EmptyLinkFunctionForGeneratedCode" + ModuleName + "();");
				Result.Add("    EmptyLinkFunctionForGeneratedCode" + ModuleName + "();");
			}
			foreach (var DependencyModuleName in PrivateDependencyModuleNames)
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
				InGeneratedCodeDirectory: null,
				InIsRedistributableOverride: null,
				InIntelliSenseGatherer: null,
				InSourceFiles: SourceFiles.ToList(),
				InPublicIncludePaths: new List<string>(),
                InPublicLibraryPaths: new List<string>(),
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
				InRuntimeDependencies: new List<RuntimeDependency>(),
				InOptimizeCode: ModuleRules.CodeOptimization.Default,
				InAllowSharedPCH: false,
				InSharedPCHHeaderFile: "",
				InUseRTTI: false,
				InEnableBufferSecurityChecks: true,
				InFasterWithoutUnity: true,
				InMinFilesUsingPrecompiledHeaderOverride: 0,
				InEnableExceptions: false,
				InEnableShadowVariableWarnings: true,
				bInBuildSourceFiles: true,
				InBuildCsFilename: null);
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
							if( !String.IsNullOrEmpty( CPPModule.SharedPCHHeaderFile ) && CPPModule.Binary.Config.bAllowCompilation )
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
									CPPModule.GetAllDependencyModules(ModuleDependencies, ModuleList, bIncludeDynamicallyLoaded, bForceCircular: false, bOnlyDirectDependencies:false);

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
        public void AddPlugin(PluginInfo Plugin)
		{
			UEBuildBinaryType BinaryType = ShouldCompileMonolithic() ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
			if(Plugin.Descriptor.Modules != null)
			{
				foreach(ModuleDescriptor Module in Plugin.Descriptor.Modules)
				{
					if (Module.IsCompiledInConfiguration(Platform, TargetType, UEBuildConfiguration.bBuildDeveloperTools, UEBuildConfiguration.bBuildEditor))
					{
						// Add the corresponding binary for it
						string ModuleFileName = RulesCompiler.GetModuleFilename(Module.Name);
						bool bHasSource = (!String.IsNullOrEmpty(ModuleFileName) && Directory.EnumerateFiles(Path.GetDirectoryName(ModuleFileName), "*.cpp", SearchOption.AllDirectories).Any());
						AddBinaryForModule(Module.Name, BinaryType, bAllowCompilation: bHasSource, bIsCrossTarget: false);

						// Add it to the binary if we're compiling monolithic (and it's enabled)
						if(ShouldCompileMonolithic() && EnabledPlugins.Contains(Plugin))
						{
							AppBinaries[0].AddModule(Module.Name);
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
			foreach (var ModuleName in ExtraModuleNames)
			{
				UEBuildBinaryCPP Binary = FindOrAddBinaryForModule(ModuleName, false);
				Binary.AddModule(ModuleName);
			}
		}

		/// <summary>
		/// Adds all the precompiled modules into the target. Precompiled modules are compiled alongside the target, but not linked into it unless directly referenced.
		/// </summary>
		protected void AddPrecompiledModules()
		{
			if(bPrecompile || bUsePrecompiled)
			{
				// Find all the precompiled module names.
				List<string> PrecompiledModuleNames = new List<string>();
				Rules.GetModulesToPrecompile(TargetInfo, PrecompiledModuleNames);

				// Add all the enabled plugins to the precompiled module list. Plugins are always precompiled, even if bPrecompile is not set, so we should precompile their dependencies.
				foreach(PluginInfo Plugin in BuildPlugins)
                {
                    if (Plugin.Descriptor.Modules != null)
                    {
                        foreach (ModuleDescriptor Module in Plugin.Descriptor.Modules)
                        {
                            if (Module.IsCompiledInConfiguration(Platform, TargetType, UEBuildConfiguration.bBuildDeveloperTools, UEBuildConfiguration.bBuildEditor))
                            {
                                PrecompiledModuleNames.Add(Module.Name);
                            }
                        }
                    }
				}

				// When running in Rocket, all engine modules have to be precompiled, but are precompiled using a different target (UE4Game) with a separate list of precompiled module names.
				// Add every referenced engine module to the list instead.
				if(UnrealBuildTool.RunningRocket())
				{
					List<UEBuildModule> TargetModules = new List<UEBuildModule>();
					foreach(UEBuildBinary TargetBinary in AppBinaries)
					{
						foreach(string ModuleName in TargetBinary.Config.ModuleNames)
						{
							UEBuildModule TargetModule = FindOrCreateModuleByName(ModuleName);
							TargetModule.RecursivelyAddPrecompiledModules(TargetModules);
						}
					}
					foreach(string ExtraModuleName in ExtraModuleNames)
					{
						UEBuildModule ExtraModule = FindOrCreateModuleByName(ExtraModuleName);
						ExtraModule.RecursivelyAddPrecompiledModules(TargetModules);
					}
					foreach(UEBuildModuleCPP TargetModule in TargetModules)
					{
						if(TargetModule is UEBuildModuleCPP && !RulesCompiler.IsGameModule(TargetModule.Name))
						{
							PrecompiledModuleNames.Add(TargetModule.Name);
						}
					}
				}

				// Build the final list of precompiled modules from the list of module names
				List<UEBuildModule> PrecompiledModules = new List<UEBuildModule>();
				foreach(string PrecompiledModuleName in PrecompiledModuleNames)
				{
					UEBuildModule PrecompiledModule = FindOrCreateModuleByName(PrecompiledModuleName);
					PrecompiledModule.RecursivelyAddPrecompiledModules(PrecompiledModules);
				}

				// Find all the module names which are already bound to one binary and cannot be bound to another.
				// For monolithic builds, modules which are already bound to static libraries (eg. plugins) do not need new binaries. 
				// For modular builds, modules which are already bound to DLLs or executables (eg. launch, extra modules, plugins) do not need new binaries.
				HashSet<string> BoundModuleNames = new HashSet<string>();
				foreach(UEBuildBinary Binary in AppBinaries)
				{
					foreach(string ModuleName in Binary.Config.ModuleNames)
					{
						if(!bCompileMonolithic || Binary.Config.Type == UEBuildBinaryType.StaticLibrary)
						{
							BoundModuleNames.Add(ModuleName);
						}
					}
				}

				// Create binaries for the of names to actual modules. Make sure every referenced module is also precompiled.
				foreach(UEBuildModule PrecompiledModule in PrecompiledModules)
				{
					if(PrecompiledModule is UEBuildModuleCPP && !BoundModuleNames.Contains(PrecompiledModule.Name))
					{
						UEBuildBinaryType BinaryType = bCompileMonolithic? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
						UEBuildBinary Binary = AddBinaryForModule(PrecompiledModule.Name, BinaryType, bAllowCompilation: !bUsePrecompiled, bIsCrossTarget: false);
						PrecompiledBinaries.Add(Binary);
						BoundModuleNames.Add(PrecompiledModule.Name);
					}
				}
			}
		}

		public UEBuildBinaryCPP FindOrAddBinaryForModule(string ModuleName, bool bIsCrossTarget)
		{
			UEBuildBinaryCPP Binary;
			if (ShouldCompileMonolithic())
			{
				// When linking monolithically, any unbound modules will be linked into the main executable
				Binary = (UEBuildBinaryCPP)AppBinaries[0];
			}
			else
			{
				// Otherwise create a new module for it
				Binary = AddBinaryForModule(ModuleName, UEBuildBinaryType.DynamicLinkLibrary, bAllowCompilation: true, bIsCrossTarget: bIsCrossTarget);
			}
			return Binary;
		}

		/// <summary>
		/// Adds a binary for the given module. Does not check whether a binary already exists, or whether a binary should be created for this build configuration.
		/// </summary>
		/// <param name="ModuleName">Name of the binary</param>
		/// <param name="BinaryType">Type of binary to be created</param>
		/// <param name="bAllowCompilation">Whether this binary can be compiled. The function will check whether plugin binaries can be compiled.</param>
		/// <param name="bIsCrossTarget">True if module is for supporting a different target-platform</param>
		/// <returns>The new binary</returns>
		private UEBuildBinaryCPP AddBinaryForModule(string ModuleName, UEBuildBinaryType BinaryType, bool bAllowCompilation, bool bIsCrossTarget)
		{
			// Get the plugin info for this module
			PluginInfo Plugin = FindPluginForModule(ModuleName);

			// Get the root output directory and base name (target name/app name) for this binary
			string BaseOutputDirectory;
			if(Plugin != null)
			{
				BaseOutputDirectory = Path.GetFullPath(Plugin.Directory);
			}
			else if(RulesCompiler.IsGameModule(ModuleName) || !bUseSharedBuildEnvironment)
			{
				BaseOutputDirectory = Path.GetFullPath(ProjectDirectory);
			}
			else
			{
				BaseOutputDirectory = Path.GetFullPath(BuildConfiguration.RelativeEnginePath);
			}

			// Get the configuration that this module will be built in. Engine modules compiled in DebugGame will use Development.
			UnrealTargetConfiguration ModuleConfiguration = Configuration;
			if(Configuration == UnrealTargetConfiguration.DebugGame && !RulesCompiler.IsGameModule(ModuleName))
			{
				ModuleConfiguration = UnrealTargetConfiguration.Development;
			}

			// Get the output and intermediate directories for this module
			string OutputDirectory = Path.Combine(BaseOutputDirectory, "Binaries", Platform.ToString());
 			string IntermediateDirectory = Path.Combine(BaseOutputDirectory, BuildConfiguration.PlatformIntermediateFolder, AppName, ModuleConfiguration.ToString());

			// Append a subdirectory if the module rules specifies one
			ModuleRules ModuleRules;
			if(RulesCompiler.TryCreateModuleRules(ModuleName, TargetInfo, out ModuleRules) && !String.IsNullOrEmpty(ModuleRules.BinariesSubFolder))
			{
				OutputDirectory = Path.Combine(OutputDirectory, ModuleRules.BinariesSubFolder);
				IntermediateDirectory = Path.Combine(IntermediateDirectory, ModuleRules.BinariesSubFolder);
			}

			// Get the output filenames
			string BaseBinaryPath = Path.Combine(OutputDirectory, MakeBinaryFileName(AppName + "-" + ModuleName, Platform, ModuleConfiguration, Rules.UndecoratedConfiguration, BinaryType));
			string[] OutputFilePaths = UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BaseBinaryPath);

			// Prepare the configuration object
			UEBuildBinaryConfiguration Config = new UEBuildBinaryConfiguration(BinaryType);
			Config.OutputFilePaths = OutputFilePaths;
			Config.IntermediateDirectory = IntermediateDirectory;
			Config.bHasModuleRules = (ModuleRules != null);
			Config.bAllowExports = (BinaryType == UEBuildBinaryType.DynamicLinkLibrary);
			Config.bAllowCompilation = bAllowCompilation;
			Config.bIsCrossTarget = bIsCrossTarget;
			Config.ModuleNames.Add(ModuleName);

			// Create the new binary
			UEBuildBinaryCPP Binary = new UEBuildBinaryCPP(this, Config);
			AppBinaries.Add(Binary);
			return Binary;
		}

		/// <summary>
		/// Find the plugin which contains a given module
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <returns>Matching plugin, or null if not found</returns>
		private PluginInfo FindPluginForModule(string ModuleName)
		{
			return ValidPlugins.FirstOrDefault(ValidPlugin => ValidPlugin.Descriptor.Modules != null && ValidPlugin.Descriptor.Modules.Any(Module => Module.Name == ModuleName));
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

		/// <summary>
		/// Makes a filename (without path) for a compiled binary (e.g. "Core-Win64-Debug.lib") */
		/// </summary>
		/// <param name="BinaryName">The name of this binary</param>
		/// <param name="Platform">The platform being built for</param>
		/// <param name="Configuration">The configuration being built</param>
 		/// <param name="UndecoratedConfiguration">The target configuration which doesn't require a platform and configuration suffix. Development by default.</param>
		/// <param name="BinaryType">Type of binary</param>
		/// <returns>Name of the binary</returns>
		public static string MakeBinaryFileName(string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, UnrealTargetConfiguration UndecoratedConfiguration, UEBuildBinaryType BinaryType)
		{
			StringBuilder Result = new StringBuilder();

			if (Platform == UnrealTargetPlatform.Linux && (BinaryType == UEBuildBinaryType.DynamicLinkLibrary || BinaryType == UEBuildBinaryType.StaticLibrary))
			{
				Result.Append("lib");
			}

			Result.Append(BinaryName);

			if(Configuration != UndecoratedConfiguration)
			{
				Result.AppendFormat("-{0}-{1}", Platform.ToString(), Configuration.ToString());
			}

			IUEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			Result.Append(BuildPlatform.ApplyArchitectureName(""));

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
		public static string[] MakeExecutablePaths(string BaseDirectory, string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, UnrealTargetConfiguration UndecoratedConfiguration, bool bIncludesGameModules, string ExeSubFolder)
		{
			// Get the configuration for the executable. If we're building DebugGame, and this executable only contains engine modules, use the same name as development.
			UnrealTargetConfiguration ExeConfiguration = Configuration;
			if(Configuration == UnrealTargetConfiguration.DebugGame && !bIncludesGameModules)
			{
				ExeConfiguration = UnrealTargetConfiguration.Development;
			}

			// Build the binary path
			string BinaryPath = Path.Combine(BaseDirectory, "Binaries", Platform.ToString());
			if (!String.IsNullOrEmpty(ExeSubFolder))
			{
				BinaryPath = Path.Combine(BinaryPath, ExeSubFolder);
			}
			BinaryPath = Path.Combine(BinaryPath, MakeBinaryFileName(BinaryName, Platform, ExeConfiguration, UndecoratedConfiguration, UEBuildBinaryType.Executable));

			// Allow the platform to customize the output path (and output several executables at once if necessary)
			return UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BinaryPath);
		}

		/** Sets up the modules for the target. */
		protected virtual void SetupModules()
		{
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			List<string> PlatformExtraModules = new List<string>();
			BuildPlatform.GetExtraModules(TargetInfo, this, ref PlatformExtraModules);
			ExtraModuleNames.AddRange(PlatformExtraModules);			
		}

		/** Sets up the plugins for this target */
		protected virtual void SetupPlugins()
		{
			// Filter the plugins list by the current project
			ValidPlugins = Plugins.ReadAvailablePlugins(UnrealBuildTool.GetUProjectFile());

			// Remove any plugins for platforms we don't have
			foreach (UnrealTargetPlatform TargetPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				if (TargetPlatform != UnrealTargetPlatform.Desktop && UEBuildPlatform.GetBuildPlatform(TargetPlatform, true) == null)
				{
					string DirectoryFragment = String.Format("/{0}/", TargetPlatform.ToString());
					ValidPlugins.RemoveAll(x => x.Directory.Replace('\\', '/').Contains(DirectoryFragment));
				}
			}

			// Build a list of enabled plugins
			EnabledPlugins = new List<PluginInfo>();

			// If we're compiling against the engine, add the plugins enabled for this target
			if(UEBuildConfiguration.bCompileAgainstEngine)
			{
				ProjectDescriptor Project = UnrealBuildTool.HasUProjectFile()? ProjectDescriptor.FromFile(UnrealBuildTool.GetUProjectFile()) : null;
				foreach(PluginInfo ValidPlugin in ValidPlugins)
				{
					if(UProjectInfo.IsPluginEnabledForProject(ValidPlugin, Project, Platform))
					{
						EnabledPlugins.Add(ValidPlugin);
					}
				}
			}

			// Add the plugins explicitly required by the target rules
			foreach(string AdditionalPlugin in Rules.AdditionalPlugins)
			{
				PluginInfo Plugin = ValidPlugins.FirstOrDefault(ValidPlugin => ValidPlugin.Name == AdditionalPlugin);
				if(Plugin == null)
				{
					throw new BuildException("Plugin '{0}' is in the list of additional plugins for {1}, but was not found.", AdditionalPlugin, TargetName);
				}
				if(!EnabledPlugins.Contains(Plugin))
				{
					EnabledPlugins.Add(Plugin);
				}
			}

			// Set the list of plugins that should be built
			if (bPrecompile && TargetType != TargetRules.TargetType.Program)
			{
				BuildPlugins = new List<PluginInfo>(ValidPlugins);
			}
			else
			{
				BuildPlugins = new List<PluginInfo>(EnabledPlugins);
			}

			// Add any foreign plugins to the list
			if(ForeignPlugins != null)
			{
				foreach(string ForeignPlugin in ForeignPlugins)
				{
					PluginInfo ForeignPluginInfo = new PluginInfo(ForeignPlugin, PluginLoadedFrom.GameProject);
					ValidPlugins.Add(ForeignPluginInfo);
					BuildPlugins.Add(ForeignPluginInfo);
				}
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
					TargetInfo,
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

				if(!bUseSharedBuildEnvironment)
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

			// Installed Engine intermediates go to the project's intermediate folder. Installed Engine never writes to the engine intermediate folder. (Those files are immutable)
			// Also, when compiling in monolithic, all intermediates go to the project's folder.  This is because a project can change definitions that affects all engine translation
			// units too, so they can't be shared between different targets.  They are effectively project-specific engine intermediates.
			if( UnrealBuildTool.IsEngineInstalled() || ( UnrealBuildTool.HasUProjectFile() && ShouldCompileMonolithic() ) )
			{
				var IntermediateConfiguration = Configuration;
				if( UnrealBuildTool.RunningRocket() )
				{
					// Only Development and Shipping are supported for engine modules
					IntermediateConfiguration = Configuration == UnrealTargetConfiguration.DebugGame ? UnrealTargetConfiguration.Development : Configuration;
				}

				GlobalCompileEnvironment.Config.OutputDirectory = Path.Combine(BuildConfiguration.PlatformIntermediatePath, OutputAppName, IntermediateConfiguration.ToString());
				if (ShouldCompileMonolithic())
				{
					if(!String.IsNullOrEmpty(UnrealBuildTool.GetUProjectPath()))
					{
						GlobalCompileEnvironment.Config.OutputDirectory = Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.PlatformIntermediateFolder, OutputAppName, IntermediateConfiguration.ToString());
					}
					else if(ForeignPlugins.Count > 0)
					{
						GlobalCompileEnvironment.Config.OutputDirectory = Path.Combine(Path.GetDirectoryName(ForeignPlugins[0]), BuildConfiguration.PlatformIntermediateFolder, OutputAppName, IntermediateConfiguration.ToString());
					}
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

			// Initialize the compile and link environments for the platform, configuration, and project.
			SetUpPlatformEnvironment();
			SetUpConfigurationEnvironment();
			SetUpProjectEnvironment();

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

		void SetUpProjectEnvironment()
		{
			UEBuildPlatform.GetBuildPlatform(Platform).SetUpProjectEnvironment(Platform);
		}

		/** Registers a module with this target. */
		public void RegisterModule(UEBuildModule Module)
		{
			Debug.Assert(Module.Target == this);
			Modules.Add(Module.Name, Module);
			ModuleCsMap.Add(Module.Name, Module.BuildCsFilename);
		}

		/** Finds a module given its name.  Throws an exception if the module couldn't be found. */
		public UEBuildModule FindOrCreateModuleByName(string ModuleName)
		{
			UEBuildModule Module;
			if (!Modules.TryGetValue(ModuleName, out Module))
			{
				// Create the module!  (It will be added to our hash table in its constructor)

				// @todo projectfiles: Cross-platform modules can appear here during project generation, but they may have already
				//   been filtered out by the project generator.  This causes the projects to not be added to directories properly.
				string ModuleFileName;
				var RulesObject = RulesCompiler.CreateModuleRules(ModuleName, TargetInfo, out ModuleFileName);
				var ModuleDirectory = Path.GetDirectoryName(ModuleFileName);

				// Get the plugin for this module
				PluginInfo Plugin = FindPluginForModule(ModuleName);

				// Making an assumption here that any project file path that contains '../../'
				// is NOT from the engine and therefore must be an application-specific module.
				// Get the type of module we're creating
				var ModuleType = UEBuildModuleType.Unknown;
				var ModuleFileRelativeToEngineDirectory = Utils.MakePathRelativeTo(ModuleFileName, ProjectFileGenerator.EngineRelativePath);

				// see if it's external
				if (RulesObject.Type == ModuleRules.ModuleType.External)
				{
					ModuleType = UEBuildModuleType.ThirdParty;
				}
				else
				{
					// Check if it's a plugin
					if(Plugin != null)
					{
						ModuleDescriptor Descriptor = Plugin.Descriptor.Modules.FirstOrDefault(x => x.Name == ModuleName);
						if(Descriptor != null)
						{
							ModuleType = UEBuildModule.GetModuleTypeFromDescriptor(Descriptor);
						}
					}
					if (ModuleType == UEBuildModuleType.Unknown)
					{
						// not a plugin, see if it is a game module 
						if (ModuleFileRelativeToEngineDirectory.StartsWith("..") || Path.IsPathRooted(ModuleFileRelativeToEngineDirectory))
						{
							ModuleType = UEBuildModuleType.Game;
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

				// Get the generated code directory. Plugins always write to their own intermediate directory so they can be copied between projects, shared engine 
				// intermediates go in the engine intermediate folder, and anything else goes in the project folder.
				string GeneratedCodeDirectory = null;
				if (RulesObject.Type != ModuleRules.ModuleType.External)
				{
					if(Plugin != null)
					{
						GeneratedCodeDirectory = Plugin.Directory;
					}
					else if(bUseSharedBuildEnvironment && Utils.IsFileUnderDirectory(RulesCompiler.GetModuleFilename(ModuleName), BuildConfiguration.RelativeEnginePath))
					{
						GeneratedCodeDirectory = BuildConfiguration.RelativeEnginePath;
					}
					else
					{
						GeneratedCodeDirectory = ProjectDirectory;
					}
					GeneratedCodeDirectory = Path.GetFullPath(Path.Combine(GeneratedCodeDirectory, BuildConfiguration.PlatformIntermediateFolder, AppName, "Inc", ModuleName));
				}

				// Don't generate include paths for third party modules; they don't follow our conventions. Core is a special-case... leave it alone
				if (RulesObject.Type != ModuleRules.ModuleType.External && ModuleName != "Core")
				{
					// Add the default include paths to the module rules, if they exist.
					RulesCompiler.AddDefaultIncludePathsToModuleRules(ModuleName, ModuleFileName, ModuleFileRelativeToEngineDirectory, IsGameModule, RulesObject);

					// Add the path to the generated headers 
					if(GeneratedCodeDirectory != null)
					{
						string EngineSourceDirectory = Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source");
						string RelativeGeneratedCodeDirectory = Utils.CleanDirectorySeparators(Utils.MakePathRelativeTo(GeneratedCodeDirectory, EngineSourceDirectory), '/');
						RulesObject.PublicIncludePaths.Add(RelativeGeneratedCodeDirectory);
					}
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
					if (bDiscoverFiles && (!UnrealBuildTool.IsEngineInstalled() || !Utils.IsFileUnderDirectory(ModuleFileName, BuildConfiguration.RelativeEnginePath)))
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
				Module = InstantiateModule(RulesObject, ModuleName, ModuleType, ModuleDirectory, GeneratedCodeDirectory, IntelliSenseGatherer, FoundSourceFiles, bBuildFiles, ModuleFileName);

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
			string               GeneratedCodeDirectory,
			IntelliSenseGatherer IntelliSenseGatherer,
			List<FileItem>       ModuleSourceFiles,
			bool                 bBuildSourceFiles,
			string               InBuildCsFile)
		{
			switch (RulesObject.Type)
			{
				case ModuleRules.ModuleType.CPlusPlus:
					return new UEBuildModuleCPP(
							InTarget: this,
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InGeneratedCodeDirectory: GeneratedCodeDirectory,
							InIsRedistributableOverride: RulesObject.IsRedistributableOverride,
							InIntelliSenseGatherer: IntelliSenseGatherer,
							InSourceFiles: ModuleSourceFiles,
							InPublicSystemIncludePaths: RulesObject.PublicSystemIncludePaths,
                            InPublicLibraryPaths: RulesObject.PublicLibraryPaths,
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
							InRuntimeDependencies: RulesObject.RuntimeDependencies,
							InOptimizeCode: RulesObject.OptimizeCode,
							InAllowSharedPCH: (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs) ? false : true,
							InSharedPCHHeaderFile: RulesObject.SharedPCHHeaderFile,
							InUseRTTI: RulesObject.bUseRTTI,
							InEnableBufferSecurityChecks: RulesObject.bEnableBufferSecurityChecks,
							InFasterWithoutUnity: RulesObject.bFasterWithoutUnity,
							InMinFilesUsingPrecompiledHeaderOverride: RulesObject.MinFilesUsingPrecompiledHeaderOverride,
							InEnableExceptions: RulesObject.bEnableExceptions,
							InEnableShadowVariableWarnings: RulesObject.bEnableShadowVariableWarnings,
							bInBuildSourceFiles: bBuildSourceFiles,
							InBuildCsFilename: InBuildCsFile
						);

				case ModuleRules.ModuleType.CPlusPlusCLR:
					return new UEBuildModuleCPPCLR(
							InTarget: this,
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InGeneratedCodeDirectory: GeneratedCodeDirectory,
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
							InRuntimeDependencies: RulesObject.RuntimeDependencies,
							InOptimizeCode: RulesObject.OptimizeCode,
							InAllowSharedPCH: (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs) ? false : true,
							InSharedPCHHeaderFile: RulesObject.SharedPCHHeaderFile,
							InUseRTTI: RulesObject.bUseRTTI,
							InEnableBufferSecurityChecks: RulesObject.bEnableBufferSecurityChecks,
							InFasterWithoutUnity: RulesObject.bFasterWithoutUnity,
							InMinFilesUsingPrecompiledHeaderOverride: RulesObject.MinFilesUsingPrecompiledHeaderOverride,
							InEnableExceptions: RulesObject.bEnableExceptions,
							InEnableShadowVariableWarnings: RulesObject.bEnableShadowVariableWarnings,
							bInBuildSourceFiles : bBuildSourceFiles,
							InBuildCsFilename: InBuildCsFile
						);

				case ModuleRules.ModuleType.External:
					return new UEBuildExternalModule(
							InTarget: this,
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
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
							InPublicDelayLoadDLLs: RulesObject.PublicDelayLoadDLLs,
							InRuntimeDependencies: RulesObject.RuntimeDependencies,
							InBuildCsFilename: InBuildCsFile
						);

				default:
					throw new BuildException("Unrecognized module type specified by 'Rules' object {0}", RulesObject.ToString());
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

		/// <summary>
		/// Determines whether we should check binary output paths for this target are
		/// appropriate for the distribution level of their direct module dependencies
		/// </summary>
		private bool ShouldCheckOutputDistributionLevel()
		{
			if (Rules != null)
			{
				// We don't want to check this for uprojects (but do want to include UE4Game)
				if (!UnrealBuildTool.HasUProjectFile() && !Rules.bOutputPubliclyDistributable)
				{
					return true;
				}
			}
			return false;
		}
	}
}
