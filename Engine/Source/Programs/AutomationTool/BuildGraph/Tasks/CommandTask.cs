using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a task which calls another UAT command
	/// </summary>
	public class CommandTaskParameters
	{
		/// <summary>
		/// The command name to execute
		/// </summary>
		[TaskParameter]
		public string Name;

		/// <summary>
		/// Arguments to be passed to the command
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;
	}

	/// <summary>
	/// Implements a task which calls another UAT command
	/// </summary>
	[TaskElement("Command", typeof(CommandTaskParameters))]
	public class CommandTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		CommandTaskParameters Parameters;

		/// <summary>
		/// Construct a new CommandTask.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CommandTask(CommandTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>True if the task succeeded</returns>
		public override bool Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			CommandUtils.RunUAT(CommandUtils.CmdEnv, String.Format("{0} {1}", Parameters.Name, Parameters.Arguments ?? ""));
			return true;
		}
	}
}
