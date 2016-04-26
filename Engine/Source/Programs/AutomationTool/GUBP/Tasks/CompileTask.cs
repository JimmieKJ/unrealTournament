// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Task definition for compiling a target with UnrealBuildTool
	/// </summary>
	public class CompileTaskDefinition : BuildTaskDefinition
	{
		/// <summary>
		/// The name of the target to compile
		/// </summary>
		[XmlAttribute]
		public string Target;

		/// <summary>
		/// The configuration to compile
		/// </summary>
		[XmlAttribute]
		public UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;

		/// <summary>
		/// The platform to be compiled for
		/// </summary>
		[XmlAttribute, DefaultValue(UnrealTargetPlatform.Unknown)]
		public UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;

		/// <summary>
		/// Extra arguments to pass to UBT
		/// </summary>
		[XmlAttribute]
		public string Arguments;

		/// <summary>
		/// Creates a task from this definition
		/// </summary>
		/// <returns>New instance of a build task</returns>
		public override BuildTask CreateTask()
		{
			return new CompileTask(Target, Platform, Configuration, Arguments);
		}
	}

	/// <summary>
	/// Task which compiles a target with UnrealBuildTool
	/// </summary>
	public class CompileTask : BuildTask
	{
		/// <summary>
		/// List of targets to compile. As well as the target specifically added for this task, additional compile tasks may be merged with it.
		/// </summary>
		List<UE4Build.BuildTarget> Targets = new List<UE4Build.BuildTarget>();

		/// <summary>
		/// Construct a compile task
		/// </summary>
		/// <param name="TargetName">Name of the target to compile</param>
		/// <param name="Platform">Platform to compile for</param>
		/// <param name="Configuration">Configuration to be compiled</param>
		/// <param name="ExtraArguments">Additional arguments to be passed to UnrealBuildTool</param>
		public CompileTask(string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string ExtraArguments = null)
		{
			Targets.Add(new UE4Build.BuildTarget { TargetName = TargetName, Platform = Platform, Config = Configuration, UBTArgs = ExtraArguments });
		}

		/// <summary>
		/// Allow this task to merge with other tasks within the same node. This can be useful to allow tasks to execute in parallel and reduce overheads.
		/// </summary>
		/// <param name="OtherTasks">Other tasks that this task can merge with. If a merge takes place, the other tasks should be removed from the list.</param>
		public override void Merge(List<BuildTask> OtherTasks)
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
		/// Execute the task and compile these targets.
		/// </summary>
		/// <param name="BuildProducts">Current list of build products for this node</param>
		/// <returns>True if the task succeeded</returns>
		public override bool Execute(List<string> BuildProducts)
		{
            UE4Build Builder = new UE4Build(null);

            UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
			Agenda.Targets.AddRange(Targets);

			Builder.Build(Agenda, InDeleteBuildProducts: false, InUpdateVersionFiles: false, InForceNoXGE: true, InForceUnity: true, InUseParallelExecutor: false);
            UE4Build.CheckBuildProducts(Builder.BuildProductFiles);

			BuildProducts.AddRange(Builder.BuildProductFiles);
			return true;
		}
	}
}
