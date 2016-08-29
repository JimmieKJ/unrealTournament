using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;
using AutomationTool;
using System.Xml;

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

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Compiles a target with UnrealBuildTool.
	/// </summary>
	[TaskElement("Compile", typeof(CompileTaskParameters))]
	public class CompileTask : CustomTask
	{
		/// <summary>
		/// List of targets to compile. As well as the target specifically added for this task, additional compile tasks may be merged with it.
		/// </summary>
		List<UE4Build.BuildTarget> Targets = new List<UE4Build.BuildTarget>();

		/// <summary>
		/// Mapping of receipt filename to its corresponding tag name
		/// </summary>
		Dictionary<UE4Build.BuildTarget, string> TargetToTagName = new Dictionary<UE4Build.BuildTarget,string>();

		/// <summary>
		/// Construct a compile task
		/// </summary>
		/// <param name="Parameters">Parameters for this task</param>
		public CompileTask(CompileTaskParameters Parameters)
		{
			UE4Build.BuildTarget Target = new UE4Build.BuildTarget { TargetName = Parameters.Target, Platform = Parameters.Platform, Config = Parameters.Configuration, UBTArgs = "-nobuilduht " + (Parameters.Arguments ?? "") };
			if(!String.IsNullOrEmpty(Parameters.Tag))
			{
				TargetToTagName.Add(Target, Parameters.Tag);
			}
			Targets.Add(Target);
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
					foreach(KeyValuePair<UE4Build.BuildTarget, string> TargetTagName in OtherCompileTask.TargetToTagName)
					{
						TargetToTagName.Add(TargetTagName.Key, TargetTagName.Value);
					}
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
			// Create the agenda
            UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
			Agenda.Targets.AddRange(Targets);

			// Build everything
			Dictionary<UE4Build.BuildTarget, BuildManifest> TargetToManifest = new Dictionary<UE4Build.BuildTarget,BuildManifest>();
            UE4Build Builder = new UE4Build(Job.OwnerCommand);
			try
			{
				Builder.Build(Agenda, InDeleteBuildProducts: null, InUpdateVersionFiles: false, InForceNoXGE: false, InUseParallelExecutor: true, InTargetToManifest: TargetToManifest);
			}
			catch (CommandUtils.CommandFailedException)
			{
				return false;
			}
			UE4Build.CheckBuildProducts(Builder.BuildProductFiles);

			// Tag all the outputs
			foreach(KeyValuePair<UE4Build.BuildTarget, string> TargetTagName in TargetToTagName)
			{
				BuildManifest Manifest;
				if(!TargetToManifest.TryGetValue(TargetTagName.Key, out Manifest))
				{
					throw new AutomationException("Missing manifest for target {0} {1} {2}", TargetTagName.Key.TargetName, TargetTagName.Key.Platform, TargetTagName.Key.Config);
				}

				foreach(string TagName in SplitDelimitedList(TargetTagName.Value))
				{
					HashSet<FileReference> FileSet = FindOrAddTagSet(TagNameToFileSet, TagName);
					FileSet.UnionWith(Manifest.BuildProducts.Select(x => new FileReference(x)));
					FileSet.UnionWith(Manifest.LibraryBuildProducts.Select(x => new FileReference(x)));
				}
			}

			// Add everything to the list of build products
			BuildProducts.UnionWith(Builder.BuildProductFiles.Select(x => new FileReference(x)));
			BuildProducts.UnionWith(Builder.LibraryBuildProductFiles.Select(x => new FileReference(x)));
			return true;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			foreach (UE4Build.BuildTarget Target in Targets)
			{
				CompileTaskParameters Parameters = new CompileTaskParameters();
				Parameters.Target = Target.TargetName;
				Parameters.Platform = Target.Platform;
				Parameters.Configuration = Target.Config;
				Parameters.Arguments = Target.UBTArgs;

				string TagName;
				if (TargetToTagName.TryGetValue(Target, out TagName))
				{
					Parameters.Tag = TagName;
				}

				Write(Writer, Parameters);
			}
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
			return TargetToTagName.Values.SelectMany(x => SplitDelimitedList(x));
		}
	}
}
