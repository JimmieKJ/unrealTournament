using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a spawn task
	/// </summary>
	public class SpawnTaskParameters
	{
		/// <summary>
		/// Executable to spawn
		/// </summary>
		[TaskParameter]
		public string Exe;

		/// <summary>
		/// Arguments for the newly created process
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;
	}

	/// <summary>
	/// Task which spawns an external executable
	/// </summary>
	[TaskElement("Spawn", typeof(SpawnTaskParameters))]
	public class SpawnTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		SpawnTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="Parameters">Parameters for the task</param>
		public SpawnTask(SpawnTaskParameters InParameters)
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
			CommandUtils.Run(Parameters.Exe, Parameters.Arguments);
			return true;
		}
	}
}
