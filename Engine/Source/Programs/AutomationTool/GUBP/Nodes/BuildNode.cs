using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool
{
	[DebuggerDisplay("{Name}")]
	public abstract class BuildNodeTemplate
	{
		public string Name;
		public UnrealTargetPlatform AgentPlatform = UnrealTargetPlatform.Win64;
		public string AgentRequirements;
		public string AgentSharingGroup;
		public int FrequencyShift;
		public int AgentMemoryRequirement;
		public int TimeoutInMinutes;
		public float Priority;
		public string InputDependencyNames;
		public string OrderDependencyNames;
		public string RecipientsForFailureEmails;
		public bool AddSubmittersToFailureEmails;
		public bool SendSuccessEmail;
		public bool IsParallelAgentShareEditor;
		public bool IsSticky;
		public bool IsTest;
		public string DisplayGroupName;
        public string GameNameIfAnyForFullGameAggregateNode;

		public abstract BuildNode Instantiate();
	}

	[DebuggerDisplay("{Name}")]
	public abstract class BuildNode
	{
		public readonly string Name;
		public UnrealTargetPlatform AgentPlatform = UnrealTargetPlatform.Win64;
		public string AgentRequirements;
		public string AgentSharingGroup;
		public int FrequencyShift;
		public int AgentMemoryRequirement;
		public int TimeoutInMinutes;
		public float Priority;

		public HashSet<BuildNode> InputDependencies;
		public HashSet<BuildNode> OrderDependencies;
		public TriggerNode[] ControllingTriggers;
		public bool IsComplete;
		public string[] RecipientsForFailureEmails;
		public bool AddSubmittersToFailureEmails;
		public bool SendSuccessEmail;
		public bool IsParallelAgentShareEditor;
		public bool IsSticky;
		public bool IsTest;
		public string DisplayGroupName;
        public string GameNameIfAnyForFullGameAggregateNode;

		public List<string> BuildProducts;

		public BuildNode(BuildNodeTemplate Template)
		{
			Name = Template.Name;
			AgentPlatform = Template.AgentPlatform;
			AgentRequirements = Template.AgentRequirements;
			AgentSharingGroup = Template.AgentSharingGroup;
			FrequencyShift = Template.FrequencyShift;
			AgentMemoryRequirement = Template.AgentMemoryRequirement;
			TimeoutInMinutes = Template.TimeoutInMinutes;
			Priority = Template.Priority;
			RecipientsForFailureEmails = Template.RecipientsForFailureEmails.Split(';');
			AddSubmittersToFailureEmails = Template.AddSubmittersToFailureEmails;
			SendSuccessEmail = Template.SendSuccessEmail;
			IsParallelAgentShareEditor = Template.IsParallelAgentShareEditor;
			IsSticky = Template.IsSticky;
			IsTest = Template.IsTest;
			DisplayGroupName = Template.DisplayGroupName;
            GameNameIfAnyForFullGameAggregateNode = Template.GameNameIfAnyForFullGameAggregateNode;
		}

		public virtual void ArchiveBuildProducts(TempStorageNodeInfo TempStorageNodeInfo, bool bLocalOnly)
		{
			TempStorage.StoreToTempStorage(TempStorageNodeInfo, BuildProducts, bLocalOnly, CommandUtils.CmdEnv.LocalRoot);
		}

		public virtual void RetrieveBuildProducts(TempStorageNodeInfo TempStorageNodeInfo)
		{
			CommandUtils.Log("***** Retrieving GUBP Node {0} from {1}", Name, TempStorageNodeInfo.GetRelativeDirectory());
			try
			{
				BuildProducts = TempStorage.RetrieveFromTempStorage(TempStorageNodeInfo, CommandUtils.CmdEnv.LocalRoot);
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, "Build Products cannot be found for node {0}", Name);
			}
		}

		public abstract void DoBuild();

		public abstract void DoFakeBuild();

		public string ControllingTriggerDotName
		{
			get { return String.Join(".", ControllingTriggers.Select(x => x.Name)); }
		}

		public bool DependsOn(BuildNode Node)
		{
			return OrderDependencies.Contains(Node);
		}

		public override string ToString()
		{
			System.Diagnostics.Trace.TraceWarning("Implicit conversion from NodeInfo to string\n{0}", Environment.StackTrace);
			return Name;
		}
	}

	[DebuggerDisplay("{Definition.Name}")]
	class BuildNodePair
	{
		public readonly BuildNodeTemplate Template;
		public readonly BuildNode Node;

		public BuildNodePair(BuildNodeTemplate InTemplate)
		{
			Template = InTemplate;
			Node = InTemplate.Instantiate();
		}
	}
}
