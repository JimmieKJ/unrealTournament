using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;
using AutomationTool;

namespace AutomationTool
{
	/// <summary>
	/// Parameters for a compile task
	/// </summary>
	public class CompileTaskParameters
	{
		/// <summary>
		/// The target to compile
		/// </summary>
		[TaskParameter]
		public string Target;

		/// <summary>
		/// The configuration to compile
		/// </summary>
		[TaskParameter]
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// The platform to compile for
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Additional arguments for UnrealBuildTool
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;
	}

	/// <summary>
	/// Task which compiles a target with UnrealBuildTool
	/// </summary>
	[TaskElement("Compile", typeof(CompileTaskParameters))]
	public class CompileTask : CustomTask
	{
		/// <summary>
		/// List of targets to compile. As well as the target specifically added for this task, additional compile tasks may be merged with it.
		/// </summary>
		List<UE4Build.BuildTarget> Targets = new List<UE4Build.BuildTarget>();

		/// <summary>
		/// Construct a compile task
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CompileTask(CompileTaskParameters Parameters)
		{
			Targets.Add(new UE4Build.BuildTarget { TargetName = Parameters.Target, Platform = Parameters.Platform, Config = Parameters.Configuration, UBTArgs = "-nobuilduht " + (Parameters.Arguments ?? "") });
		}

		/// <summary>
		/// Allow this task to merge with other tasks within the same node. This can be useful to allow tasks to execute in parallel and reduce overheads.
		/// </summary>
		/// <param name="OtherTasks">Other tasks that this task can merge with. If a merge takes place, the other tasks should be removed from the list.</param>
		public override void Merge(List<CustomTask> OtherTasks)
		{
			for (int Idx = 0; Idx < OtherTasks.Count; Idx++)
			{
				CompileTask OtherCompileTask = OtherTasks[Idx] as CompileTask;
				if (OtherCompileTask != null)
				{
					Targets.AddRange(OtherCompileTask.Targets);
					OtherTasks.RemoveAt(Idx);
				}
			}
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
            UE4Build Builder = new UE4Build(Job.OwnerCommand);

            UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
			Agenda.Targets.AddRange(Targets);

			Builder.Build(Agenda, InDeleteBuildProducts: false, InUpdateVersionFiles: false, InForceNoXGE: CommandUtils.IsBuildMachine, InUseParallelExecutor: true);
            UE4Build.CheckBuildProducts(Builder.BuildProductFiles);

			BuildProducts.UnionWith(Builder.BuildProductFiles.Select(x => new FileReference(x)));
			return true;
		}
	}
}
