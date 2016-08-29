using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
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
		[TaskParameter(ValidationType = TaskParameterValidationType.FileName)]
		public string Exe;

		/// <summary>
		/// Arguments for the newly created process
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// The minimum exit code which is treated as an error.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int ErrorLevel = 1;
	}

	/// <summary>
	/// Spawns an external executable and waits for it to complete.
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
		/// <param name="InParameters">Parameters for the task</param>
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
			ProcessResult Result = CommandUtils.Run(Parameters.Exe, Parameters.Arguments);
			return Result.ExitCode >= 0 && Result.ExitCode < Parameters.ErrorLevel;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
