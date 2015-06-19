// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using UnrealBuildTool;
using System.Diagnostics;

namespace AutomationTool
{
	public struct SingleTargetProperties
	{
		public string TargetName;
		public TargetRules Rules;
	}

	/// <summary>
	/// Autodetected project properties.
	/// </summary>
	public class ProjectProperties
	{
		/// <summary>
		/// Full Project path. Must be a .uproject file
		/// </summary>
		public string RawProjectPath;

		/// <summary>
		/// True if the uproject contains source code.
		/// </summary>
		public bool bIsCodeBasedProject;

		/// <summary>
		/// Whether the project uses Steam (todo: substitute with more generic functionality)
		/// </summary>
		public bool bUsesSteam;

		/// <summary>
		/// Whether the project uses CEF3
		/// </summary>
		public bool bUsesCEF3;

		/// <summary>
		/// Whether the project uses visual Slate UI (as opposed to the low level windowing/messaging which is always used)
		/// </summary>
		public bool bUsesSlate = true;

		/// <summary>
		/// Hack for legacy game styling isses.  No new project should ever set this to true
		/// Whether the project uses the Slate editor style in game.  
		/// </summary>
		public bool bUsesSlateEditorStyle = false;

        /// <summary>
        // By default we use the Release C++ Runtime (CRT), even when compiling Debug builds.  This is because the Debug C++
        // Runtime isn't very useful when debugging Unreal Engine projects, and linking against the Debug CRT libraries forces
        // our third party library dependencies to also be compiled using the Debug CRT (and often perform more slowly.)  Often
        // it can be inconvenient to require a separate copy of the debug versions of third party static libraries simply
        // so that you can debug your program's code.
        /// </summary>
        public bool bDebugBuildsActuallyUseDebugCRT = false;

		/// <summary>
		/// List of all targets detected for this project.
		/// </summary>
		public Dictionary<TargetRules.TargetType, SingleTargetProperties> Targets = new Dictionary<TargetRules.TargetType, SingleTargetProperties>();

		/// <summary>
		/// List of all Engine ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigCacheIni> EngineConfigs = new Dictionary<UnrealTargetPlatform,ConfigCacheIni>();

		/// <summary>
		/// List of all Game ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigCacheIni> GameConfigs = new Dictionary<UnrealTargetPlatform, ConfigCacheIni>();

		/// <summary>
		/// List of all programs detected for this project.
		/// </summary>
		public List<SingleTargetProperties> Programs = new List<SingleTargetProperties>();

		/// <summary>
		/// Specifies if the target files were generated
		/// </summary>
		public bool bWasGenerated = false;

		internal ProjectProperties()
		{
		}
	}

	/// <summary>
	/// Project related utility functions.
	/// </summary>
	public class ProjectUtils
	{
		private static CaselessDictionary<ProjectProperties> PropertiesCache = new CaselessDictionary<ProjectProperties>();

		/// <summary>
		/// Gets a short project name (QAGame, Elemental, etc)
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="bIsUProjectFile">True if a uproject.</param>
		/// <returns>Short project name</returns>
		public static string GetShortProjectName(string RawProjectPath)
		{
			return CommandUtils.GetFilenameWithoutAnyExtensions(RawProjectPath);
		}

		/// <summary>
		/// Gets project properties.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>Properties of the project.</returns>
		public static ProjectProperties GetProjectProperties(string RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms = null)
		{
			string ProjectKey = "UE4";
			if (!String.IsNullOrEmpty(RawProjectPath))
			{
				ProjectKey = CommandUtils.ConvertSeparators(PathSeparator.Slash, Path.GetFullPath(RawProjectPath));
			}
			ProjectProperties Properties;
			if (PropertiesCache.TryGetValue(ProjectKey, out Properties) == false)
			{
				Properties = DetectProjectProperties(RawProjectPath, ClientTargetPlatforms);
				PropertiesCache.Add(ProjectKey, Properties);
			}
			return Properties;
		}

		/// <summary>
		/// Checks if the project is a UProject file with source code.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>True if the project is a UProject file with source code.</returns>
		public static bool IsCodeBasedUProjectFile(string RawProjectPath)
		{
			return GetProjectProperties(RawProjectPath, null).bIsCodeBasedProject;
		}

		/// <summary>
		/// Returns a path to the client binaries folder.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="Platform">Platform type.</param>
		/// <returns>Path to the binaries folder.</returns>
		public static string GetProjectClientBinariesFolder(string ProjectClientBinariesPath, UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown)
		{
			if (Platform != UnrealTargetPlatform.Unknown)
			{
				ProjectClientBinariesPath = CommandUtils.CombinePaths(ProjectClientBinariesPath, Platform.ToString());
			}
			return ProjectClientBinariesPath;
		}

		private static bool RequiresTempTarget(string RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms)
		{
			// check to see if we already have a Target.cs file
			if (File.Exists (Path.Combine (Path.GetDirectoryName (RawProjectPath), "Source", Path.GetFileNameWithoutExtension (RawProjectPath) + ".Target.cs")))
			{
				return false;
			}
			else 
			{
				// wasn't one in the main Source directory, let's check all sub-directories
				//@todo: may want to read each target.cs to see if it has a target corresponding to the project name as a final check
				FileInfo[] Files = (new DirectoryInfo (Path.GetDirectoryName (RawProjectPath)).GetFiles ("*.Target.cs", SearchOption.AllDirectories));
				if (Files.Length > 0)
				{
					return false;
				}
			}

			// no Target file, now check to see if build settings have changed
			List<UnrealTargetPlatform> TargetPlatforms = ClientTargetPlatforms;
			if (ClientTargetPlatforms == null || ClientTargetPlatforms.Count < 1)
			{
				// No client target platforms, add all in
				TargetPlatforms = new List<UnrealTargetPlatform>();
				foreach (UnrealTargetPlatform TargetPlatformType in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (TargetPlatformType != UnrealTargetPlatform.Unknown)
					{
						TargetPlatforms.Add(TargetPlatformType);
					}
				}
			}

			// Change the working directory to be the Engine/Source folder. We are running from Engine/Binaries/DotNET
			string oldCWD = Directory.GetCurrentDirectory();
			if (BuildConfiguration.RelativeEnginePath == "../../Engine/")
			{
				string EngineSourceDirectory = Path.Combine(UnrealBuildTool.Utils.GetExecutingAssemblyDirectory(), "..", "..", "..", "Engine", "Source");
				if (!Directory.Exists(EngineSourceDirectory)) // only set the directory if it exists, this should only happen if we are launching the editor from an artist sync
				{
					EngineSourceDirectory = Path.Combine(UnrealBuildTool.Utils.GetExecutingAssemblyDirectory(), "..", "..", "..", "Engine", "Binaries");
				}
				Directory.SetCurrentDirectory(EngineSourceDirectory);
			}

			// Read the project descriptor, and find all the plugins available to this project
			ProjectDescriptor Project = ProjectDescriptor.FromFile(RawProjectPath);
			List<PluginInfo> AvailablePlugins = Plugins.ReadAvailablePlugins(RawProjectPath);

			// check the target platforms for any differences in build settings or additional plugins
			bool RetVal = false;
			foreach (UnrealTargetPlatform TargetPlatformType in TargetPlatforms)
			{
				IUEBuildPlatform BuildPlat = UEBuildPlatform.GetBuildPlatform(TargetPlatformType, true);
				if (!GlobalCommandLine.Rocket && BuildPlat != null && !(BuildPlat as UEBuildPlatform).HasDefaultBuildConfig(TargetPlatformType, Path.GetDirectoryName(RawProjectPath)))
				{
					RetVal = true;
					break;
				}

				// find if there are any plugins enabled or disabled which differ from the default
				foreach(PluginInfo Plugin in AvailablePlugins)
				{
					bool bPluginEnabledForProject = UProjectInfo.IsPluginEnabledForProject(Plugin, Project, TargetPlatformType);
					if (bPluginEnabledForProject != Plugin.Descriptor.bEnabledByDefault || (bPluginEnabledForProject && Plugin.Descriptor.bInstalled))
					{
						if(Plugin.Descriptor.Modules.Any(Module => Module.IsCompiledInConfiguration(TargetPlatformType, TargetRules.TargetType.Game, bBuildDeveloperTools: false, bBuildEditor: false)))
						{
							RetVal = true;
							break;
						}
					}
				}
			}

			// Change back to the original directory
			Directory.SetCurrentDirectory(oldCWD);
			return RetVal;
		}

		private static void GenerateTempTarget(string RawProjectPath)
		{
			// read in the template target cs file
			var TempCSFile = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Build", "Target.cs.template");
			string TargetCSFile = File.ReadAllText(TempCSFile);

			// replace {GAME_NAME} with the game name
			TargetCSFile = TargetCSFile.Replace("{GAME_NAME}", Path.GetFileNameWithoutExtension(RawProjectPath));

			// write out the file in a new Source directory
			string FileName = CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath), "Intermediate", "Source", Path.GetFileNameWithoutExtension(RawProjectPath) + ".Target.cs");
			if (!Directory.Exists(Path.GetDirectoryName(FileName)))
			{
				Directory.CreateDirectory(Path.GetDirectoryName(FileName));
			}

			File.WriteAllText(FileName, TargetCSFile);
		}

		/// <summary>
		/// Attempts to autodetect project properties.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>Project properties.</returns>
		private static ProjectProperties DetectProjectProperties(string RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms)
		{
			var Properties = new ProjectProperties();
			Properties.RawProjectPath = RawProjectPath;

			// detect if the project is content only, but has non-default build settings
			List<string> ExtraSearchPaths = null;
			if (!string.IsNullOrEmpty(RawProjectPath))
			{
				if (RequiresTempTarget(RawProjectPath, ClientTargetPlatforms))
				{
					GenerateTempTarget(RawProjectPath);
					Properties.bWasGenerated = true;
					ExtraSearchPaths = new List<string>();
					ExtraSearchPaths.Add(CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath), "Intermediate", "Source"));
				}
				else if (File.Exists(Path.Combine(Path.GetDirectoryName(RawProjectPath), "Intermediate", "Source", Path.GetFileNameWithoutExtension(RawProjectPath) + ".Target.cs")))
				{
					File.Delete(Path.Combine(Path.GetDirectoryName(RawProjectPath), "Intermediate", "Source", Path.GetFileNameWithoutExtension(RawProjectPath) + ".Target.cs"));
				}
			}

			if (CommandUtils.CmdEnv.HasCapabilityToCompile)
			{
				DetectTargetsForProject(Properties, ExtraSearchPaths);
				Properties.bIsCodeBasedProject = !CommandUtils.IsNullOrEmpty(Properties.Targets) || !CommandUtils.IsNullOrEmpty(Properties.Programs);
			}
			else
			{
				// should never ask for engine targets if we can't compile
				if (String.IsNullOrEmpty(RawProjectPath))
				{
					throw new AutomationException("Cannot dtermine engine targets if we can't compile.");
				}

				Properties.bIsCodeBasedProject = Properties.bWasGenerated;
				// if there's a Source directory with source code in it, then mark us as having source code
				string SourceDir = CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath), "Source");
				if (Directory.Exists(SourceDir))
				{
					string[] CppFiles = Directory.GetFiles(SourceDir, "*.cpp", SearchOption.AllDirectories);
					string[] HFiles = Directory.GetFiles(SourceDir, "*.h", SearchOption.AllDirectories);
					Properties.bIsCodeBasedProject |= (CppFiles.Length > 0 || HFiles.Length > 0);
				}
			}

			// check to see if the uproject loads modules, only if we haven't already determined it is a code based project
			if (!Properties.bIsCodeBasedProject)
			{
				string uprojectStr = File.ReadAllText(RawProjectPath);
				Properties.bIsCodeBasedProject = uprojectStr.Contains("\"Modules\"");
			}

			// Get all ini files
			if (!String.IsNullOrWhiteSpace(RawProjectPath))
			{
				CommandUtils.Log("Loading ini files for {0}", RawProjectPath);

				var EngineDirectory = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine");
				foreach (UnrealTargetPlatform TargetPlatformType in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (TargetPlatformType != UnrealTargetPlatform.Unknown)
					{
						var Config = new ConfigCacheIni(TargetPlatformType, "Engine", Path.GetDirectoryName(RawProjectPath), EngineDirectory);
						Properties.EngineConfigs.Add(TargetPlatformType, Config);
					}
				}

				foreach (UnrealTargetPlatform TargetPlatformType in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (TargetPlatformType != UnrealTargetPlatform.Unknown)
					{
						var Config = new ConfigCacheIni(TargetPlatformType, "Game", Path.GetDirectoryName(RawProjectPath));
						Properties.GameConfigs.Add(TargetPlatformType, Config);
					}
				}
			}

			return Properties;
		}


		/// <summary>
		/// Gets the project's root binaries folder.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="TargetType">Target type.</param>
		/// <param name="bIsUProjectFile">True if uproject file.</param>
		/// <returns>Binaries path.</returns>
		public static string GetClientProjectBinariesRootPath(string RawProjectPath, TargetRules.TargetType TargetType, bool bIsCodeBasedProject)
		{
			var BinPath = String.Empty;
			switch (TargetType)
			{
				case TargetRules.TargetType.Program:
					BinPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries");
					break;
				case TargetRules.TargetType.Client:
				case TargetRules.TargetType.Game:
					if (!bIsCodeBasedProject)
					{
						BinPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries");
					}
					else
					{
						BinPath = CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(RawProjectPath), "Binaries");
					}
					break;
			}
			return BinPath;
		}

		/// <summary>
		/// Gets the location where all rules assemblies should go
		/// </summary>
		private static string GetRulesAssemblyFolder()
		{
			string RulesFolder;
			if (GlobalCommandLine.Installed.IsSet)
			{
				RulesFolder = CommandUtils.CombinePaths(Path.GetTempPath(), "UAT", CommandUtils.EscapePath(CommandUtils.CmdEnv.LocalRoot), "Rules"); 
			}
			else
			{
				RulesFolder = CommandUtils.CombinePaths(CommandUtils.CmdEnv.EngineSavedFolder, "Rules");
			}
			return RulesFolder;
		}

		/// <summary>
		/// Finds all targets for the project.
		/// </summary>
		/// <param name="Properties">Project properties.</param>
		/// <param name="ExtraSearchPaths">Additional search paths.</param>
		private static void DetectTargetsForProject(ProjectProperties Properties, List<string> ExtraSearchPaths = null)
		{
			Properties.Targets = new Dictionary<TargetRules.TargetType, SingleTargetProperties>();
			string TargetsDllFilename;
			string FullProjectPath = null;

			var GameFolders = new List<string>();
			var RulesFolder = GetRulesAssemblyFolder();
			if (!String.IsNullOrEmpty(Properties.RawProjectPath))
			{
				CommandUtils.Log("Looking for targets for project {0}", Properties.RawProjectPath);

				TargetsDllFilename = CommandUtils.CombinePaths(RulesFolder, String.Format("UATRules{0}.dll", Properties.RawProjectPath.GetHashCode()));

				FullProjectPath = CommandUtils.GetDirectoryName(Properties.RawProjectPath);
				GameFolders.Add(FullProjectPath);
				CommandUtils.Log("Searching for target rule files in {0}", FullProjectPath);
			}
			else
			{
				TargetsDllFilename = CommandUtils.CombinePaths(RulesFolder, String.Format("UATRules{0}.dll", "_BaseEngine_"));
			}
			RulesCompiler.SetAssemblyNameAndGameFolders(TargetsDllFilename, GameFolders);

			// the UBT code assumes a certain CWD, but artists don't have this CWD.
			var SourceDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source");
			bool DirPushed = false;
			if (CommandUtils.DirectoryExists_NoExceptions(SourceDir))
			{
				CommandUtils.PushDir(SourceDir);
				DirPushed = true;
			}
			var TargetScripts = RulesCompiler.FindAllRulesSourceFiles(RulesCompiler.RulesFileType.Target, ExtraSearchPaths);
			if (DirPushed)
			{
				CommandUtils.PopDir();
			}

			if (!CommandUtils.IsNullOrEmpty(TargetScripts))
			{
				// We only care about project target script so filter out any scripts not in the project folder, or take them all if we are just doing engine stuff
				var ProjectTargetScripts = new List<string>();
				foreach (var Filename in TargetScripts)
				{
					var FullScriptPath = CommandUtils.CombinePaths(Path.GetFullPath(Filename));
					if (FullProjectPath == null || FullScriptPath.StartsWith(FullProjectPath, StringComparison.InvariantCultureIgnoreCase))
					{
						ProjectTargetScripts.Add(FullScriptPath);
					}
				}
				TargetScripts = ProjectTargetScripts;
			}

			if (!CommandUtils.IsNullOrEmpty(TargetScripts))
			{
				CommandUtils.LogVerbose("Found {0} target rule files:", TargetScripts.Count);
				foreach (var Filename in TargetScripts)
				{
					CommandUtils.LogVerbose("  {0}", Filename);
				}

				// Check if the scripts require compilation
				bool DoNotCompile = false;
				if (!CommandUtils.IsBuildMachine && !CheckIfScriptAssemblyIsOutOfDate(TargetsDllFilename, TargetScripts))
				{
					Log.TraceInformation("Targets DLL {0} is up to date.", TargetsDllFilename);
					DoNotCompile = true;
				}
				if (!DoNotCompile && CommandUtils.FileExists_NoExceptions(TargetsDllFilename))
				{
					if (!CommandUtils.DeleteFile_NoExceptions(TargetsDllFilename, true))
					{
						DoNotCompile = true;
						CommandUtils.Log("Could not delete {0} assuming it is up to date and reusable for a recursive UAT call.", TargetsDllFilename);
					}
				}

				CompileAndLoadTargetsAssembly(Properties, TargetsDllFilename, DoNotCompile, TargetScripts);
			}
		}

		/// <summary>
		/// Optionally compiles and loads target rules assembly.
		/// </summary>
		/// <param name="Properties"></param>
		/// <param name="TargetsDllFilename"></param>
		/// <param name="DoNotCompile"></param>
		/// <param name="TargetScripts"></param>
		private static void CompileAndLoadTargetsAssembly(ProjectProperties Properties, string TargetsDllFilename, bool DoNotCompile, List<string> TargetScripts)
		{
			CommandUtils.Log("Compiling targets DLL: {0}", TargetsDllFilename);

			var ReferencedAssemblies = new List<string>() 
					{ 
						"System.dll", 
						"System.Core.dll", 
						"System.Xml.dll", 
						typeof(UnrealBuildTool.UnrealBuildTool).Assembly.Location
					};
			var TargetsDLL = DynamicCompilation.CompileAndLoadAssembly(TargetsDllFilename, TargetScripts, ReferencedAssemblies, null, DoNotCompile);
			var DummyTargetInfo = new TargetInfo(BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development);
			var AllCompiledTypes = TargetsDLL.GetTypes();
			foreach (Type TargetType in AllCompiledTypes)
			{
				// Find TargetRules but skip all "UE4Editor", "UE4Game" targets.
				if (typeof(TargetRules).IsAssignableFrom(TargetType))
				{
					// Create an instance of this type
					CommandUtils.LogVerbose("Creating target rules object: {0}", TargetType.Name);
					var Rules = Activator.CreateInstance(TargetType, DummyTargetInfo) as TargetRules;
					CommandUtils.LogVerbose("Adding target: {0} ({1})", TargetType.Name, Rules.Type);

					SingleTargetProperties TargetData;
					TargetData.TargetName = GetTargetName(TargetType);
                    Rules.TargetName = TargetData.TargetName;
					TargetData.Rules = Rules;
					if (Rules.Type == TargetRules.TargetType.Program)
					{
						Properties.Programs.Add(TargetData);
					}
					else
					{
						Properties.Targets.Add(Rules.Type, TargetData);
					}

					Properties.bUsesSteam |= Rules.bUsesSteam;
					Properties.bUsesCEF3 |= Rules.bUsesCEF3;
					Properties.bUsesSlate |= Rules.bUsesSlate;
                    Properties.bDebugBuildsActuallyUseDebugCRT |= Rules.bDebugBuildsActuallyUseDebugCRT;
					Properties.bUsesSlateEditorStyle |= Rules.bUsesSlateEditorStyle;
				}
			}
		}

		/// <summary>
		/// Checks if any of the script files in newer than the generated assembly.
		/// </summary>
		/// <param name="TargetsDllFilename"></param>
		/// <param name="TargetScripts"></param>
		/// <returns>True if the generated assembly is out of date.</returns>
		private static bool CheckIfScriptAssemblyIsOutOfDate(string TargetsDllFilename, List<string> TargetScripts)
		{
			var bOutOfDate = false;
			var AssemblyInfo = new FileInfo(TargetsDllFilename);
			if (AssemblyInfo.Exists)
			{
				foreach (var ScriptFilename in TargetScripts)
				{
					var ScriptInfo = new FileInfo(ScriptFilename);
					if (ScriptInfo.Exists && ScriptInfo.LastWriteTimeUtc > AssemblyInfo.LastWriteTimeUtc)
					{
						bOutOfDate = true;
						break;
					}
				}
			}
			else
			{
				bOutOfDate = true;
			}
			return bOutOfDate;
		}

		/// <summary>
		/// Converts class type name (usually ends with Target) to a target name (without the postfix).
		/// </summary>
		/// <param name="TargetRulesType">Tagert class.</param>
		/// <returns>Target name</returns>
		private static string GetTargetName(Type TargetRulesType)
		{
			const string TargetPostfix = "Target";
			var Name = TargetRulesType.Name;
			if (Name.EndsWith(TargetPostfix, StringComparison.InvariantCultureIgnoreCase))
			{
				Name = Name.Substring(0, Name.Length - TargetPostfix.Length);
			}
			return Name;
		}

		/// <summary>
		/// Performs initial cleanup of target rules folder
		/// </summary>
		public static void CleanupFolders()
		{
			CommandUtils.Log("Cleaning up project rules folder");
			var RulesFolder = GetRulesAssemblyFolder();
			if (CommandUtils.DirectoryExists(RulesFolder))
			{
				CommandUtils.DeleteDirectoryContents(RulesFolder);
			}
		}
	}

    public class BranchInfo
    {

        public static TargetRules.TargetType[] MonolithicKinds = new TargetRules.TargetType[]
        {
            TargetRules.TargetType.Game,
            TargetRules.TargetType.Client,
            TargetRules.TargetType.Server,
        };

		[DebuggerDisplay("{GameName}")]
        public class BranchUProject
        {
            public string GameName;
            public string FilePath;
            public ProjectProperties Properties;

            public BranchUProject(UnrealBuildTool.UProjectInfo InfoEntry)
            {
                GameName = InfoEntry.GameName;

                //not sure what the heck this path is relative to
                FilePath = Path.GetFullPath(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries", InfoEntry.FilePath));

                if (!CommandUtils.FileExists_NoExceptions(FilePath))
                {
                    throw new AutomationException("Could not resolve relative path corrctly {0} -> {1} which doesn't exist.", InfoEntry.FilePath, FilePath);
                }

                Properties = ProjectUtils.GetProjectProperties(Path.GetFullPath(FilePath));



            }
            public BranchUProject()
            {
                GameName = "UE4";
                Properties = ProjectUtils.GetProjectProperties("");
                if (!Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
                {
                    throw new AutomationException("Base UE4 project did not contain an editor target.");
                }
            }
            public TargetRules.GUBPProjectOptions Options(UnrealTargetPlatform HostPlatform)
            {
                var Options = new TargetRules.GUBPProjectOptions();
                if (Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
                {
                    var Target = Properties.Targets[TargetRules.TargetType.Editor];
                    Options = Target.Rules.GUBP_IncludeProjectInPromotedBuild_EditorTypeOnly(HostPlatform);
                }
				else if(Properties.Targets.ContainsKey(TargetRules.TargetType.Game))
				{
					var Target = Properties.Targets[TargetRules.TargetType.Game];
					Options = Target.Rules.GUBP_IncludeProjectInPromotedBuild_EditorTypeOnly(HostPlatform);
				}
                return Options;
            }
            public void Dump(List<UnrealTargetPlatform> InHostPlatforms)
            {
                CommandUtils.Log("    ShortName:    " + GameName);
                CommandUtils.Log("      FilePath          : " + FilePath);
                CommandUtils.Log("      bIsCodeBasedProject  : " + (Properties.bIsCodeBasedProject ? "YES" : "NO"));
                CommandUtils.Log("      bUsesSteam  : " + (Properties.bUsesSteam ? "YES" : "NO"));
                CommandUtils.Log("      bUsesCEF3   : " + (Properties.bUsesCEF3 ? "YES" : "NO"));
                CommandUtils.Log("      bUsesSlate  : " + (Properties.bUsesSlate ? "YES" : "NO"));
                foreach (var HostPlatform in InHostPlatforms)
                {
                    CommandUtils.Log("      For Host : " + HostPlatform.ToString());
                    if (Properties.bIsCodeBasedProject)
                    {
                        CommandUtils.Log("          Promoted  : " + (Options(HostPlatform).bIsPromotable ? "YES" : "NO"));
                        CommandUtils.Log("          SeparatePromotable  : " + (Options(HostPlatform).bSeparateGamePromotion ? "YES" : "NO"));
                        CommandUtils.Log("          Test With Shared  : " + (Options(HostPlatform).bTestWithShared ? "YES" : "NO"));
                    }
                    CommandUtils.Log("          Targets {0}:", Properties.Targets.Count);
                    foreach (var ThisTarget in Properties.Targets)
                    {
                        CommandUtils.Log("            TargetName          : " + ThisTarget.Value.TargetName);
                        CommandUtils.Log("              Type          : " + ThisTarget.Key.ToString());
                        CommandUtils.Log("              bUsesSteam  : " + (ThisTarget.Value.Rules.bUsesSteam ? "YES" : "NO"));
                        CommandUtils.Log("              bUsesCEF3   : " + (ThisTarget.Value.Rules.bUsesCEF3 ? "YES" : "NO"));
                        CommandUtils.Log("              bUsesSlate  : " + (ThisTarget.Value.Rules.bUsesSlate ? "YES" : "NO"));
                        if (Array.IndexOf(MonolithicKinds, ThisTarget.Key) >= 0)
                        {
                            var Platforms = ThisTarget.Value.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
                            var AdditionalPlatforms = ThisTarget.Value.Rules.GUBP_GetBuildOnlyPlatforms_MonolithicOnly(HostPlatform);
                            var AllPlatforms = Platforms.Union(AdditionalPlatforms);
                            foreach (var Plat in AllPlatforms)
                            {
                                var Configs = ThisTarget.Value.Rules.GUBP_GetConfigs_MonolithicOnly(HostPlatform, Plat);
                                foreach (var Config in Configs)
                                {
                                    CommandUtils.Log("                Config  : " + Plat.ToString() + " " + Config.ToString());
                                }
                            }

                        }
                    }
                    CommandUtils.Log("      Programs {0}:", Properties.Programs.Count);
                    foreach (var ThisTarget in Properties.Programs)
                    {
                        bool bInternalToolOnly;
                        bool SeparateNode;
						bool CrossCompile;
                        bool Tool = ThisTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalToolOnly, out SeparateNode, out CrossCompile);

                        CommandUtils.Log("            TargetName                    : " + ThisTarget.TargetName);
                        CommandUtils.Log("              Build With Editor           : " + (ThisTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() ? "YES" : "NO"));
                        CommandUtils.Log("              Build With Tools            : " + (Tool && !bInternalToolOnly ? "YES" : "NO"));
                        CommandUtils.Log("              Build With Internal Tools   : " + (Tool && bInternalToolOnly ? "YES" : "NO"));
                        CommandUtils.Log("              Separate Node               : " + (Tool && SeparateNode ? "YES" : "NO"));
						CommandUtils.Log("              Cross Compile               : " + (Tool && CrossCompile ? "YES" : "NO"));
                    }
                }
            }
        };
        public BranchUProject BaseEngineProject = null;
        public List<BranchUProject> CodeProjects = new List<BranchUProject>();
        public List<BranchUProject> NonCodeProjects = new List<BranchUProject>();

		public IEnumerable<BranchUProject> AllProjects
		{
			get { return CodeProjects.Union(NonCodeProjects); }
		}

        public BranchInfo(List<UnrealTargetPlatform> InHostPlatforms)
        {
            BaseEngineProject = new BranchUProject();

            var AllProjects = UnrealBuildTool.UProjectInfo.FilterGameProjects(false, null);

            foreach (var InfoEntry in AllProjects)
            {
                var UProject = new BranchUProject(InfoEntry);
                if (UProject.Properties.bIsCodeBasedProject)
                {
                    CodeProjects.Add(UProject);
                }
                else
                {
                    NonCodeProjects.Add(UProject);
                    // the base project uses BlankProject if it really needs a .uproject file
                    if (String.IsNullOrEmpty(BaseEngineProject.FilePath) && UProject.GameName == "BlankProject")
                    {
                        BaseEngineProject.FilePath = UProject.FilePath;
                    }
                }
            }
            if (String.IsNullOrEmpty(BaseEngineProject.FilePath))
            {
                throw new AutomationException("All branches must have the blank project /Samples/Sandbox/BlankProject");
            }

            CommandUtils.Log("  Base Engine:");
            BaseEngineProject.Dump(InHostPlatforms);

            CommandUtils.Log("  {0} Code projects:", CodeProjects.Count);
            foreach (var Proj in CodeProjects)
            {
                Proj.Dump(InHostPlatforms);
            }
            CommandUtils.Log("  {0} Non-Code projects:", CodeProjects.Count);
            foreach (var Proj in NonCodeProjects)
            {
                Proj.Dump(InHostPlatforms);
            }
        }

        public BranchUProject FindGame(string GameName)
        {
            foreach (var Proj in CodeProjects)
            {
                if (Proj.GameName.Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
                {
                    return Proj;
                }
            }
            foreach (var Proj in NonCodeProjects)
            {
                if (Proj.GameName.Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
                {
                    return Proj;
                }
            }
            return null;
        }
		public BranchUProject FindGameChecked(string GameName)
		{
			BranchUProject Project = FindGame(GameName);
			if(Project == null)
			{
				throw new AutomationException("Cannot find project '{0}' in branch", GameName);
			}
			return Project;
		}
        public SingleTargetProperties FindProgram(string ProgramName)
        {
            foreach (var Proj in BaseEngineProject.Properties.Programs)
            {
                if (Proj.TargetName.Equals(ProgramName, StringComparison.InvariantCultureIgnoreCase))
                {
                    return Proj;
                }
            }
            SingleTargetProperties Result;
            Result.TargetName = ProgramName;
            Result.Rules = null;
            return Result;
        }
    };

}
