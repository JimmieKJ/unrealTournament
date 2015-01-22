// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.CodeDom.Compiler;
using Microsoft.CSharp;
using Microsoft.Win32;
using System.Reflection;
using System.Diagnostics;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Exception thrown by PreprocessScriptFile.
	/// </summary>
	class CompilationException : AutomationException
	{
		public CompilationException(string Filename, int StartLine, int StartColumn, int EndLine, int EndColumn, string Message, params string[] Args)
			: base(String.Format("Compilation Failed.\n>{0}({1},{2},{3},{4}): error: {5}", Path.GetFullPath(Filename), StartLine + 1, StartColumn + 1, EndLine + 1, EndColumn + 1,
			String.Format(Message, Args)))
		{
		}
	}

	/// <summary>
	/// Compiles and loads script assemblies.
	/// </summary>
	class ScriptCompiler
	{
		#region Fields
				
		private CaselessDictionary<Type> ScriptCommands;
#if DEBUG
		const string BuildConfig = "Debug";
#else
		const string BuildConfig = "Development";
#endif
		const string DefaultScriptsDLLName = "AutomationScripts.Automation.dll";

		#endregion

		#region Compilation

		public ScriptCompiler()
		{
		}


		/// <summary>
		/// Finds and/or compiles all script files and assemblies.
		/// </summary>
		/// <param name="AdditionalScriptsFolders">Additional script fodlers to look for source files in.</param>
		public void FindAndCompileAllScripts(List<string> AdditionalScriptsFolders)
		{
			bool DoCompile = true;
			if (GlobalCommandLine.NoCompile)
			{
				DoCompile = false;
			}

			// Change to Engine\Source (if exists) to properly discover all UBT classes
			var OldCWD = Environment.CurrentDirectory;
			var UnrealBuildToolCWD = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source");
			if (Directory.Exists(UnrealBuildToolCWD))
			{
				Environment.CurrentDirectory = UnrealBuildToolCWD;
			}
			// Register all the classes inside UBT
			Log.TraceVerbose("Registering UBT Classes.");
			UnrealBuildTool.UnrealBuildTool.RegisterAllUBTClasses();
			Environment.CurrentDirectory = OldCWD;

			// Compile only if not disallowed.
			if (DoCompile && !String.IsNullOrEmpty(CommandUtils.CmdEnv.MsBuildExe))
			{
				CleanupScriptsAssemblies();
				FindAndCompileScriptModules(AdditionalScriptsFolders);
			}

			var ScriptAssemblies = new List<Assembly>();
			LoadPreCompiledScriptAssemblies(ScriptAssemblies);

			// Setup platforms
			Platform.InitializePlatforms(ScriptAssemblies.ToArray());

			// Instantiate all the automation classes for interrogation
			Log.TraceVerbose("Creating commands.");
			ScriptCommands = new CaselessDictionary<Type>();
			foreach (var CompiledScripts in ScriptAssemblies)
			{
				foreach (var ClassType in CompiledScripts.GetTypes())
				{
					if (ClassType.IsSubclassOf(typeof(BuildCommand)) && ClassType.IsAbstract == false)
					{
						if (ScriptCommands.ContainsKey(ClassType.Name) == false)
						{
							ScriptCommands.Add(ClassType.Name, ClassType);
						}
						else
						{
							Log.TraceWarning("Unable to add command {0} twice. Previous: {1}, Current: {2}", ClassType.Name,
								ClassType.AssemblyQualifiedName, ScriptCommands[ClassType.Name].AssemblyQualifiedName);
						}
					}
				}
			}
		}

		private static void FindAndCompileScriptModules(List<string> AdditionalScriptsFolders)
		{
			var OldCWD = Environment.CurrentDirectory;
			var UnrealBuildToolCWD = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source");

			// Convert script folders to be relative to UnrealBuildTool's expected CWD
			var RemappedAdditionalScriptFolders = new List<string>();
			foreach (var CurFolder in AdditionalScriptsFolders)
			{
				RemappedAdditionalScriptFolders.Add(UnrealBuildTool.Utils.MakePathRelativeTo(CurFolder, UnrealBuildToolCWD));
			}

			Environment.CurrentDirectory = UnrealBuildToolCWD;

			// Configure the rules compiler
			// Get all game folders and convert them to build subfolders.
			var AllGameFolders = UnrealBuildTool.UEBuildTarget.DiscoverAllGameFolders();
			var BuildFolders = new List<string>(AllGameFolders.Count);
			foreach (var Folder in AllGameFolders)
			{
				var GameBuildFolder = CommandUtils.CombinePaths(Folder, "Build");
				if (Directory.Exists(GameBuildFolder))
				{
					BuildFolders.Add(GameBuildFolder);
				}
			}
			RemappedAdditionalScriptFolders.AddRange(BuildFolders);

			Log.TraceVerbose("Discovering game folders.");
			UnrealBuildTool.RulesCompiler.SetAssemblyNameAndGameFolders("UnrealAutomationToolRules", AllGameFolders);

			var DiscoveredModules = UnrealBuildTool.RulesCompiler.FindAllRulesSourceFiles(UnrealBuildTool.RulesCompiler.RulesFileType.AutomationModule, RemappedAdditionalScriptFolders);
			var ModulesToCompile = new List<string>(DiscoveredModules.Count);
			foreach (var ModuleFilename in DiscoveredModules)
			{
				if (HostPlatform.Current.IsScriptModuleSupported(CommandUtils.GetFilenameWithoutAnyExtensions(ModuleFilename)))
				{
					ModulesToCompile.Add(ModuleFilename);
				}
				else
				{
					CommandUtils.LogVerbose("Script module {0} filtered by the Host Platform and will not be compiled.", ModuleFilename);
				}
			}
			CompileModules(ModulesToCompile);

			Environment.CurrentDirectory = OldCWD;
		}

		/// <summary>
		/// Compiles all script modules.
		/// </summary>
		/// <param name="Modules">Module project filenames.</param>
		/// <param name="CompiledModuleFilenames">The resulting compiled module assembly filenames.</param>
		private static void CompileModules(List<string> Modules)
		{
			Log.TraceInformation("Building script modules");
			// Make sure DefaultScriptsDLLName is compiled first
			var DefaultScriptsProjName = Path.ChangeExtension(DefaultScriptsDLLName, "csproj");
			foreach (var ModuleName in Modules)
			{
				if (ModuleName.IndexOf(DefaultScriptsProjName, StringComparison.InvariantCultureIgnoreCase) >= 0)
				{
					Log.TraceInformation("Building script module: {0}", ModuleName);
					try
					{
						CompileScriptModule(ModuleName);
					}
					catch (Exception Ex)
					{
						CommandUtils.Log(TraceEventType.Error, Ex);
						throw new AutomationException("Failed to compile module {0}", ModuleName);
					}
					break;
				}
			}

			// Second pass, compile everything else
			foreach (var ModuleName in Modules)
			{
				if (ModuleName.IndexOf(DefaultScriptsProjName, StringComparison.InvariantCultureIgnoreCase) < 0)
				{
					Log.TraceInformation("Building script module: {0}", ModuleName);
					try
					{
						CompileScriptModule(ModuleName);
					}
					catch (Exception Ex)
					{
						CommandUtils.Log(TraceEventType.Error, Ex);
						throw new AutomationException("Failed to compile module {0}", ModuleName);
					}
				}
			}
		}

		/// <summary>
		/// Builds a script module (csproj file)
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <returns></returns>
		private static bool CompileScriptModule(string ProjectFile)
		{
			if (!ProjectFile.EndsWith(".csproj", StringComparison.InvariantCultureIgnoreCase))
			{
				throw new AutomationException(String.Format("Unable to build Project {0}. Not a valid .csproj file.", ProjectFile));
			}
			if (!CommandUtils.FileExists(ProjectFile))
			{
				throw new AutomationException(String.Format("Unable to build Project {0}. Project file not found.", ProjectFile));
			}

			var CmdLine = String.Format("\"{0}\" /verbosity:quiet /nologo /target:Build /property:Configuration={1} /property:Platform=AnyCPU /p:TreatWarningsAsErrors=true /p:BuildProjectReferences=true",
				ProjectFile, BuildConfig);

			// Compile the project
			var Result = CommandUtils.Run(CommandUtils.CmdEnv.MsBuildExe, CmdLine);
			if (Result.ExitCode != 0)
			{
				throw new AutomationException(String.Format("Failed to build \"{0}\":{1}{2}", ProjectFile, Environment.NewLine, Result.Output));
			}
			else
			{
				// Remove .Automation.csproj and copy to target dir
				Log.TraceVerbose("Successfully compiled {0}", ProjectFile);
			}
			return Result.ExitCode == 0;
		}

		/// <summary>
		/// Loads all precompiled assemblies (DLLs that end with *Scripts.dll).
		/// </summary>
		/// <param name="OutScriptAssemblies">List to store all loaded assemblies.</param>
		private static void LoadPreCompiledScriptAssemblies(List<Assembly> OutScriptAssemblies)
		{
			CommandUtils.Log("Loading precompiled script DLLs");

			bool DefaultScriptsDLLFound = false;
			var ScriptsLocation = GetScriptAssemblyFolder();
			if (CommandUtils.DirectoryExists(ScriptsLocation))
			{
				var ScriptDLLFiles = Directory.GetFiles(ScriptsLocation, "*.Automation.dll", SearchOption.AllDirectories);

				CommandUtils.Log("Found {0} script DLL(s).", ScriptDLLFiles.Length);
				foreach (var ScriptsDLLFilename in ScriptDLLFiles)
				{

					if (!HostPlatform.Current.IsScriptModuleSupported(CommandUtils.GetFilenameWithoutAnyExtensions(ScriptsDLLFilename)))
					{
						CommandUtils.LogVerbose("Script module {0} filtered by the Host Platform and will not be loaded.", ScriptsDLLFilename);
						continue;
					}
					// Load the assembly into our app domain
					CommandUtils.Log("Loading script DLL: {0}", ScriptsDLLFilename);
					try
					{
						var Dll = AppDomain.CurrentDomain.Load(AssemblyName.GetAssemblyName(ScriptsDLLFilename));
						OutScriptAssemblies.Add(Dll);
						// Check if this is the default scripts DLL.
						if (!DefaultScriptsDLLFound && String.Compare(Path.GetFileName(ScriptsDLLFilename), DefaultScriptsDLLName, true) == 0)
						{
							DefaultScriptsDLLFound = true;
						}
					}
					catch (Exception Ex)
					{
						throw new AutomationException("Failed to load script DLL: {0}: {1}", ScriptsDLLFilename, Ex.Message);
					}
				}
			}
			else
			{
				CommandUtils.LogError("Scripts folder {0} does not exist!", ScriptsLocation);
			}

			// The default scripts DLL is required!
			if (!DefaultScriptsDLLFound)
			{
				throw new AutomationException("{0} was not found or could not be loaded, can't run scripts.", DefaultScriptsDLLName);
			}
		}

		private void CleanupScriptsAssemblies()
		{
			Log.TraceInformation("Cleaning up script DLL folder");
			CommandUtils.DeleteDirectory(GetScriptAssemblyFolder());
		}

		private static string GetScriptAssemblyFolder()
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries", "DotNET", "AutomationScripts");
		}

		#endregion

		#region Properties

		public CaselessDictionary<Type> Commands
		{
			get { return ScriptCommands; }
		}

		#endregion
	}
}
