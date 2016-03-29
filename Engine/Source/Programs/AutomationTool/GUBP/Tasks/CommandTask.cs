// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace AutomationTool
{
	/// <summary>
	/// Definition for a task which calls another UAT command
	/// </summary>
	public class CommandTaskDefinition : BuildTaskDefinition
	{
		/// <summary>
		/// The command name to execute
		/// </summary>
		[XmlAttribute]
		public string Name;

		/// <summary>
		/// Arguments to be passed to the command
		/// </summary>
		[XmlAttribute]
		public string Arguments;

		/// <summary>
		/// Creates a task from this definition
		/// </summary>
		/// <returns>New instance of a build task</returns>
		public override BuildTask CreateTask()
		{
			return new CommandTask(Name, Arguments);
		}
	}

	/// <summary>
	/// Implements a task which calls another UAT command
	/// </summary>
	public class CommandTask : BuildTask
	{
		/// <summary>
		/// The command name to execute
		/// </summary>
		string Name;

		/// <summary>
		/// Arguments to be passed to the command
		/// </summary>
		string Arguments;

		/// <summary>
		/// Construct a new CommandTask.
		/// </summary>
		/// <param name="InName">Name of the command to run</param>
		/// <param name="InArguments">Command line arguments for the command</param>
		public CommandTask(string InName, string InArguments)
		{
			Name = InName;
			Arguments = InArguments;
		}

		/// <summary>
		/// Execute this node.
		/// </summary>
		/// <param name="BuildProducts">List of build products created after executing tasks so far. Includes previous tasks.</param>
		/// <returns>Whether the node succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		public override bool Execute(List<string> BuildProducts)
		{
			CommandUtils.RunUAT(CommandUtils.CmdEnv, String.Format("{0} {1}", Name, Arguments));
			return true;
		}
	}
}
