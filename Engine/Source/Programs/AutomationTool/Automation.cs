// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Reflection;
using System.Diagnostics;
using UnrealBuildTool;

namespace AutomationTool
{
	#region Command info

	/// <summary>
	/// Command to execute info.
	/// </summary>
	class CommandInfo
	{
		public string CommandName;
		public List<string> Arguments = new List<string>();

		public override string ToString()
		{
			string Result = CommandName;
			Result += "(";
			for (int Index = 0; Index < Arguments.Count; ++Index)
			{
				if (Index > 0)
				{
					Result += ", ";
				}
				Result += Arguments[Index];
			}
			Result += ")";
			return Result;
		}
	}

	#endregion

	#region Command Line helpers

	/// <summary>
	/// Helper class for automatically registering command line params.
	/// </summary>
	public class CommandLineArg
	{
		public delegate void OnSetDelegate();
		public string Name;
		public OnSetDelegate SetDelegate;

		/// <summary>
		/// True of this arg was set in the command line.
		/// </summary>
		public bool IsSet { get; private set; }

		public CommandLineArg(string InName, OnSetDelegate InSet = null)
		{
			Name = InName;
			SetDelegate = InSet;
			GlobalCommandLine.RegisteredArgs.Add(Name, this);
		}

		public void Set()
		{
			IsSet = true;
			if (SetDelegate != null)
			{
				SetDelegate();
			}
		}

		public override string ToString()
		{
			return String.Format("{0}, {1}", Name, IsSet);
		}

		/// <summary>
		/// Returns true of this arg was set in the command line.
		/// </summary>
		public static implicit operator bool(CommandLineArg Arg)
		{
			return Arg.IsSet;
		}
	}

	/// <summary>
	/// Global command line parameters.
	/// </summary>
	public static class GlobalCommandLine
	{
		/// <summary>
		/// List of all registered command line parameters (global, not command-specific)
		/// </summary>
		public static CaselessDictionary<CommandLineArg> RegisteredArgs = new CaselessDictionary<CommandLineArg>();

		public static CommandLineArg CompileOnly = new CommandLineArg("-CompileOnly");
		public static CommandLineArg Verbose = new CommandLineArg("-Verbose");
		public static CommandLineArg Submit = new CommandLineArg("-Submit");
		public static CommandLineArg NoSubmit = new CommandLineArg("-NoSubmit");
		public static CommandLineArg ForceLocal = new CommandLineArg("-ForceLocal");
		public static CommandLineArg NoP4 = new CommandLineArg("-NoP4");
		public static CommandLineArg P4 = new CommandLineArg("-P4");
		public static CommandLineArg Preprocess = new CommandLineArg("-Preprocess");
		public static CommandLineArg NoCompile = new CommandLineArg("-NoCompile");
		public static CommandLineArg NoCompileEditor = new CommandLineArg("-NoCompileEditor");
		public static CommandLineArg Help = new CommandLineArg("-Help");
		public static CommandLineArg List = new CommandLineArg("-List");
		public static CommandLineArg Rocket = new CommandLineArg("-Rocket");
		public static CommandLineArg NoKill = new CommandLineArg("-NoKill");
		public static CommandLineArg Installed = new CommandLineArg("-Installed");
		public static CommandLineArg UTF8Output = new CommandLineArg("-UTF8Output");
		public static CommandLineArg NoAutoSDK = new CommandLineArg("-NoAutoSDK");
		public static CommandLineArg IgnoreJunk = new CommandLineArg("-ignorejunk");
		/// <summary>
		/// Force initialize static members by calling this.
		/// </summary>
		public static void Init()
		{
			Log.TraceVerbose("Registered {0} command line parameters.", RegisteredArgs.Count);
		}
	}

	#endregion

	[Help(
@"Executes scripted commands

AutomationTool.exe [-verbose] [-compileonly] [-p4] Command0 [-Arg0 -Arg1 -Arg2 …] Command1 [-Arg0 -Arg1 …] Command2 [-Arg0 …] Commandn … [EnvVar0=MyValue0 … EnvVarn=MyValuen]"
)]
	[Help("verbose", "Enables verbose logging")]
	[Help("nop4", "Disables Perforce functionality (default if not run on a build machine)")]
	[Help("p4", "Enables Perforce functionality (default if run on a build machine)")]
	[Help("compileonly", "Does not run any commands, only compiles them")]
	[Help("forcelocal", "Forces local execution")]
	[Help("help", "Displays help")]
	[Help("list", "Lists all available commands")]
	[Help("submit", "Allows UAT command to submit changes")]
	[Help("nosubmit", "Prevents any submit attempts")]
	[Help("nokill", "Does not kill any spawned processes on exit")]
	[Help("ignorejunk", "Prevents UBT from cleaning junk files")]
	static class Automation
	{
		#region Command line parsing

		/// <summary>
		/// Parses command line parameter.
		/// </summary>
		/// <param name="ParamIndex">Parameter index</param>
		/// <param name="CommandLine">Command line</param>
		/// <param name="CurrentCommand">Recently parsed command</param>
		/// <param name="OutAdditionalScriptsFolders">Additional script locations</param>
		/// <returns>True if the parameter has been successfully parsed.</returns>
		private static void ParseParam(string CurrentParam, CommandInfo CurrentCommand, List<string> OutAdditionalScriptsFolders)
		{
			bool bGlobalParam = false;
			foreach (var RegisteredParam in GlobalCommandLine.RegisteredArgs)
			{
				if (String.Compare(CurrentParam, RegisteredParam.Key, true) == 0)
				{
					// Register and exit, we're done here.
					RegisteredParam.Value.Set();
					bGlobalParam = true;
					break;
				}
			}

			// The parameter was not found in the list of global parameters, continue looking...
			if (CurrentParam.StartsWith("-ScriptDir=", StringComparison.InvariantCultureIgnoreCase))
			{
				var ScriptDir = CurrentParam.Substring(CurrentParam.IndexOf('=') + 1);
				if (Directory.Exists(ScriptDir))
				{
					OutAdditionalScriptsFolders.Add(ScriptDir);
					Log.TraceVerbose("Found additional script dir: {0}", ScriptDir);
				}
				else
				{
					throw new AutomationException("Specified ScriptDir doesn't exist: {0}", ScriptDir);
				}
			}
			else if (CurrentParam.StartsWith("-"))
			{
				if (CurrentCommand != null)
				{
					CurrentCommand.Arguments.Add(CurrentParam.Substring(1));
				}
				else if (!bGlobalParam)
				{
					throw new AutomationException("Unknown parameter {0} in the command line that does not belong to any command.", CurrentParam);
				}
			}
			else if (CurrentParam.Contains("="))
			{
				// Environment variable
				int ValueStartIndex = CurrentParam.IndexOf('=') + 1;
				string EnvVarName = CurrentParam.Substring(0, ValueStartIndex - 1);
				if (String.IsNullOrEmpty(EnvVarName))
				{
					throw new AutomationException("Unable to parse environment variable that has no name. Error when parsing command line param {0}", CurrentParam);
				}
				string EnvVarValue = CurrentParam.Substring(ValueStartIndex);
				CommandUtils.SetEnvVar(EnvVarName, EnvVarValue);
			}
		}

		/// <summary>
		/// Parse the command line and create a list of commands to execute.
		/// </summary>
		/// <param name="CommandLine">Command line</param>
		/// <param name="OutCommandsToExecute">List containing the names of commands to execute</param>
		/// <param name="OutAdditionalScriptsFolders">Optional list of additional paths to look for scripts in</param>
		private static void ParseCommandLine(string[] CommandLine, List<CommandInfo> OutCommandsToExecute, List<string> OutAdditionalScriptsFolders)
		{
			// Initialize global command line parameters
			GlobalCommandLine.Init();

			Log.TraceInformation("Parsing command line: {0}", String.Join(" ", CommandLine));

			CommandInfo CurrentCommand = null;
			for (int Index = 0; Index < CommandLine.Length; ++Index)
			{
				var Param = CommandLine[Index];
				if (Param.StartsWith("-") || Param.Contains("="))
				{
					ParseParam(CommandLine[Index], CurrentCommand, OutAdditionalScriptsFolders);
				}
				else
				{
					CurrentCommand = new CommandInfo();
					CurrentCommand.CommandName = CommandLine[Index];
					OutCommandsToExecute.Add(CurrentCommand);
				}
			}

			// Validate
			var Result = OutCommandsToExecute.Count > 0 || GlobalCommandLine.Help || GlobalCommandLine.CompileOnly || GlobalCommandLine.List;
			if (OutCommandsToExecute.Count > 0)
			{
				Log.TraceVerbose("Found {0} scripts to execute:", OutCommandsToExecute.Count);
				foreach (var Command in OutCommandsToExecute)
				{
					Log.TraceVerbose("  " + Command.ToString());
				}
			}
			else if (!Result)
			{
				throw new AutomationException("Failed to find scripts to execute in the command line params.");
			}
			if (GlobalCommandLine.NoP4 && GlobalCommandLine.P4)
			{
				throw new AutomationException("{0} and {1} can't be set simultaneously.", GlobalCommandLine.NoP4.Name, GlobalCommandLine.P4.Name);
			}
			if (GlobalCommandLine.NoSubmit && GlobalCommandLine.Submit)
			{
				throw new AutomationException("{0} and {1} can't be set simultaneously.", GlobalCommandLine.NoSubmit.Name, GlobalCommandLine.Submit.Name);
			}

			if (GlobalCommandLine.Rocket)
			{
				UnrealBuildTool.UnrealBuildTool.bRunningRocket = true;
			}
		}

		#endregion

		#region Main Program

		/// <summary>
		/// Main method.
		/// </summary>
		/// <param name="CommandLine">Command line</param>
		public static void Process(string[] CommandLine)
		{
			// Initial check for local or build machine runs BEFORE we parse the command line (We need this value set
			// in case something throws the exception while parsing the command line)
			IsBuildMachine = !String.IsNullOrEmpty(Environment.GetEnvironmentVariable("uebp_LOCAL_ROOT"));

			// Scan the command line for commands to execute.
			var CommandsToExecute = new List<CommandInfo>();
			var AdditionalScriptsFolders = new List<string>();
			ParseCommandLine(CommandLine, CommandsToExecute, AdditionalScriptsFolders);

			// Check for build machine override (force local)
			IsBuildMachine = GlobalCommandLine.ForceLocal ? false : IsBuildMachine;
			Log.TraceInformation("IsBuildMachine={0}", IsBuildMachine);
			Environment.SetEnvironmentVariable("IsBuildMachine", IsBuildMachine ? "1" : "0");

			// should we kill processes on exit
			ShouldKillProcesses = !GlobalCommandLine.NoKill;
			Log.TraceInformation("ShouldKillProcesses={0}", ShouldKillProcesses);

			if (CommandsToExecute.Count == 0 && GlobalCommandLine.Help)
			{
				DisplayHelp();
				return;
			}

			// Disable AutoSDKs if specified on the command line
			if (GlobalCommandLine.NoAutoSDK)
			{
				UEBuildPlatform.bAllowAutoSDKSwitching = false;
			}

			// Setup environment
			Log.TraceInformation("Setting up command environment.");
			CommandUtils.InitCommandEnvironment();

			// Change CWD to UE4 root.
			Environment.CurrentDirectory = CommandUtils.CmdEnv.LocalRoot;

			// Fill in the project info
			UnrealBuildTool.UProjectInfo.FillProjectInfo();

			// Clean rules folders up
			ProjectUtils.CleanupFolders();

			// Compile scripts.
			Log.TraceInformation("Compiling scripts.");
			ScriptCompiler Compiler = new ScriptCompiler();
			Compiler.FindAndCompileAllScripts(AdditionalScriptsFolders: AdditionalScriptsFolders);

			if (GlobalCommandLine.CompileOnly)
			{
				Log.TraceInformation("Compilation successful, exiting (CompileOnly)");
				return;
			}

			if (GlobalCommandLine.List)
			{
				ListAvailableCommands(Compiler.Commands);
				return;
			}

			if (GlobalCommandLine.Help)
			{
				DisplayHelp(CommandsToExecute, Compiler.Commands);
				return;
			}

			// Enable or disable P4 support
			CommandUtils.InitP4Support(CommandsToExecute, Compiler.Commands);
			if (CommandUtils.P4Enabled)
			{
				Log.TraceInformation("Setting up Perforce environment.");
				CommandUtils.InitP4Environment();
				CommandUtils.InitDefaultP4Connection();
			}

			// Find and execute commands.
			Execute(CommandsToExecute, Compiler.Commands);

			return;
		}

		/// <summary>
		/// Execute commands specified in the command line.
		/// </summary>
		/// <param name="CommandsToExecute"></param>
		/// <param name="Commands"></param>
		private static void Execute(List<CommandInfo> CommandsToExecute, CaselessDictionary<Type> Commands)
		{
			for (int CommandIndex = 0; CommandIndex < CommandsToExecute.Count; ++CommandIndex)
			{
				var CommandInfo = CommandsToExecute[CommandIndex];
				Log.TraceVerbose("Attempting to execute {0}", CommandInfo.ToString());
				Type CommandType;
				if (!Commands.TryGetValue(CommandInfo.CommandName, out CommandType))
				{
					throw new AutomationException("Failed to find command {0}", CommandInfo.CommandName);
				}
				else
				{
					BuildCommand Command = (BuildCommand)Activator.CreateInstance(CommandType);
					Command.Params = CommandInfo.Arguments.ToArray();
					Command.Execute();
					// dispose of the class if necessary
					{
						var CommandDisposable = Command as IDisposable;
						if (CommandDisposable != null)
						{
							CommandDisposable.Dispose();
						}
					}

					// Make sure there's no directories on the stack.
					CommandUtils.ClearDirStack();
				}
			}

			Log.TraceInformation("Script execution successful, exiting.");
		}

		#endregion

		#region Help

		/// <summary>
		/// Display help for the specified commands (to execute)
		/// </summary>
		/// <param name="CommandsToExecute">List of commands specified in the command line.</param>
		/// <param name="Commands">All discovered command objects.</param>
		private static void DisplayHelp(List<CommandInfo> CommandsToExecute, CaselessDictionary<Type> Commands)
		{
			for (int CommandIndex = 0; CommandIndex < CommandsToExecute.Count; ++CommandIndex)
			{
				var CommandInfo = CommandsToExecute[CommandIndex];
				Type CommandType;
				if (Commands.TryGetValue(CommandInfo.CommandName, out CommandType) == false)
				{
					Log.TraceError("Help: Failed to find command {0}", CommandInfo.CommandName);
				}
				else
				{
					CommandUtils.Help(CommandType);
				}
			}
		}

		/// <summary>
		/// Display AutomationTool.exe help.
		/// </summary>
		private static void DisplayHelp()
		{
			CommandUtils.LogHelp(typeof(Automation));
		}

		/// <summary>
		/// List all available commands.
		/// </summary>
		/// <param name="Commands">All vailable commands.</param>
		private static void ListAvailableCommands(CaselessDictionary<Type> Commands)
		{
			string Message = Environment.NewLine;
			Message += "Available commands:" + Environment.NewLine;
			foreach (var AvailableCommand in Commands)
			{
				Message += String.Format("  {0}{1}", AvailableCommand.Key, Environment.NewLine);
			}
			CommandUtils.Log(Message);
		}

		#endregion

		#region Properties

		/// <summary>
		/// True if this process is running on a build machine, false if locally.
		/// </summary>
		/// <remarks>
		/// The reason one this property exists in Automation class and not BuildEnvironment is that
		/// it's required long before BuildEnvironment is initialized.
		/// </remarks>
		public static bool IsBuildMachine
		{
			get
			{
				if (!bIsBuildMachine.HasValue)
				{
					throw new AutomationException("Trying to access IsBuildMachine property before it was initialized.");
				}
				return (bool)bIsBuildMachine;				
			}
			private set
			{
				bIsBuildMachine = value;
			}
		}
		private static bool? bIsBuildMachine;

		public static bool ShouldKillProcesses
		{
			get
			{
				if (!bShouldKillProcesses.HasValue)
				{
					throw new AutomationException("Trying to access ShouldKillProcesses property before it was initialized.");
				}
				return (bool)bShouldKillProcesses;
			}
			private set
			{
				bShouldKillProcesses = value;
			}
		}
		private static bool? bShouldKillProcesses;
		#endregion
	}

}
