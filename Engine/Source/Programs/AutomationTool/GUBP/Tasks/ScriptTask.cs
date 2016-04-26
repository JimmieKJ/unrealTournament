// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using Microsoft.CSharp;
using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace AutomationTool
{
	/// <summary>
	/// Supported languages for script tasks
	/// </summary>
	public enum ScriptTaskLanguage
	{
		Batch,
		[XmlEnum(Name="C#")]
		CSharp,
	}

	/// <summary>
	/// Task definition which can contain arbitrary script code of a given language
	/// </summary>
	public class ScriptTaskDefinition : BuildTaskDefinition
	{
		/// <summary>
		/// Language for the script block
		/// </summary>
		[XmlAttribute]
		public ScriptTaskLanguage Language;

		/// <summary>
		/// The script to run
		/// </summary>
		[XmlText]
		public string Text;

		/// <summary>
		/// Creates a task from this definition
		/// </summary>
		/// <returns>New instance of a build task</returns>
		public override BuildTask CreateTask()
		{
			return new ScriptTask(Language, Text);
		}
	}

	/// <summary>
	/// Task which can execute script code in a given language
	/// </summary>
	public class ScriptTask : BuildTask
	{
		/// <summary>
		/// Language for the script block
		/// </summary>
		ScriptTaskLanguage Language;

		/// <summary>
		/// The script to run
		/// </summary>
		string Text;

		/// <summary>
		/// Construct a script task
		/// </summary>
		/// <param name="InLanguage">Language for the script code</param>
		/// <param name="InText">Script text to run</param>
		public ScriptTask(ScriptTaskLanguage InLanguage, string InText)
		{
			Language = InLanguage;
			Text = InText;
		}

		/// <summary>
		/// Execute the node
		/// </summary>
		/// <param name="BuildProducts">List of build products to run</param>
		/// <returns>True if the task succeeded</returns>
		public override bool Execute(List<string> BuildProducts)
		{
			if(Language == ScriptTaskLanguage.Batch)
			{
				// Write the script to a batch file
				string FileName = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Intermediate", "GUBP", "Temp.bat");
				CommandUtils.CreateDirectory(Path.GetDirectoryName(FileName));
				CommandUtils.WriteAllText(FileName, Text);

				// Run it through the command processor
				int ExitCode;
				CommandUtils.RunAndLog(Path.Combine(Environment.SystemDirectory, "cmd.exe"), "/Q /C " + CommandUtils.MakePathSafeToUseWithCommandLine(FileName), out ExitCode);
				return ExitCode == 0;
			}
			else if(Language == ScriptTaskLanguage.CSharp)
			{
				// Build the full C# source file containing the given fragment. We assume it's a function body.
				StringBuilder FullScript = new StringBuilder();
				FullScript.AppendLine("using System;");
				FullScript.AppendLine("using System.IO;");
				FullScript.AppendLine("using System.Collections.Generic;");
				FullScript.AppendLine("using System.Linq;");
				FullScript.AppendLine("using AutomationTool;");
				FullScript.AppendLine();
				FullScript.AppendLine("public static class TaskHarness");
				FullScript.AppendLine("{");
				FullScript.AppendLine("\tpublic static void Execute()");
				FullScript.AppendLine("\t{");
				FullScript.AppendLine(Text);
				FullScript.AppendLine("\t}");
				FullScript.AppendLine("}");

				// Compile it into an in-memory assembly
				CompilerParameters Parameters = new CompilerParameters();
				Parameters.GenerateExecutable = false;
				Parameters.GenerateInMemory = true;
				Parameters.ReferencedAssemblies.Add("mscorlib.dll");
				Parameters.ReferencedAssemblies.Add("System.dll");
				Parameters.ReferencedAssemblies.Add("System.Core.dll");
				Parameters.ReferencedAssemblies.Add("System.Data.Linq.dll");
				Parameters.ReferencedAssemblies.Add(typeof(CommandUtils).Assembly.Location);

				CSharpCodeProvider Compiler = new CSharpCodeProvider();
				CompilerResults Results = Compiler.CompileAssemblyFromSource(Parameters, FullScript.ToString());

				if(Results.Errors.HasErrors)
				{
					throw new AutomationException("Failed to compile C# script:\n" + String.Join("\n", Results.Errors.OfType<CompilerError>().Select(x => x.ToString())));
				}

				// Execute the code
				Type CompiledType = Results.CompiledAssembly.GetType("TaskHarness");
				try
				{
					CompiledType.GetMethod("Execute").Invoke(null, new object[0]);
				}
				catch(Exception Ex)
				{
					throw new AutomationException(Ex, "Task script threw an exception.\n");
				}
				return true;
			}
			else
			{
				throw new NotImplementedException(String.Format("Script language {0} has not been implemented", Language));
			}
		}
	}
}
