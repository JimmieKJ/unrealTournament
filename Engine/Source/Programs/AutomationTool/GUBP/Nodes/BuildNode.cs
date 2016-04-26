// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Xml.Serialization;
using UnrealBuildTool;

namespace AutomationTool
{
	public enum BuildNodePriority
	{
		Highest = -3,
		High = -2,
		AboveNormal = -1,
		Normal = 0,
		BelowNormal = +1,
		Low = +2,
		Lowest = +3
	}

	[DebuggerDisplay("{Name}")]
	public abstract class BuildNodeDefinition : ElementDefinition
	{
		[XmlAttribute]
		public string Name;

		[XmlAttribute, DefaultValue("")]
		public string DependsOn = "";

		[XmlAttribute, DefaultValue("")]
		public string AgentTypes = "";

		[XmlAttribute, DefaultValue(0)]
		public int FrequencyShift;

		[XmlAttribute, DefaultValue(0.0)]
		public float Priority;

		[XmlAttribute, DefaultValue("")]
		public string RunAfter = "";

		[XmlAttribute, DefaultValue("")]
		public string Notify = ""; // User names or email aliases separated by semicolons

		[XmlAttribute, DefaultValue(true)]
		public bool EmailSubmitters = true;

		[XmlAttribute, DefaultValue(true)]
		public bool NotifyOnWarnings = true;

		[XmlAttribute, DefaultValue(UnrealTargetPlatform.Win64)]
		public UnrealTargetPlatform AgentPlatform = UnrealTargetPlatform.Win64;

		[XmlAttribute, DefaultValue("")]
		public string AgentRequirements = "";

		[XmlAttribute, DefaultValue(0)]
		public int AgentMemoryRequirement;

		[XmlAttribute, DefaultValue(0)]
		public int TimeoutInMinutes;

		[XmlAttribute, DefaultValue(true)]
		public bool AddSubmittersToFailureEmails = true;

		[XmlAttribute, DefaultValue(false)]
		public bool SendSuccessEmail;

		[XmlAttribute, DefaultValue(false)]
		public bool IsParallelAgentShareEditor;

		[XmlAttribute, DefaultValue(false)]
		public bool IsSticky;

		[XmlAttribute, DefaultValue(false)]
		public bool IsTest;

		[XmlAttribute, DefaultValue("")]
		public string DisplayGroupName = "";

		public string AgentSharingGroup = "";

		/// <summary>
		/// Construct a build node from this definition.
		/// </summary>
		/// <returns>New instance of a build node</returns>
		public abstract BuildNode CreateNode();
	}

	[DebuggerDisplay("{Name}")]
	public abstract class BuildNode
	{
		public const int ExplicitFrequencyShift = 16;

		public readonly string Name;
		public UnrealTargetPlatform AgentPlatform = UnrealTargetPlatform.Win64;
		public string[] AgentTypes;
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
		public bool NotifyOnWarnings;
		public bool SendSuccessEmail;
		public bool IsParallelAgentShareEditor;
		public bool IsSticky;
		public bool IsTest;
		public bool CopyToSharedStorage;
		public string DisplayGroupName;

		public List<string> BuildProducts;

		public BuildNode(BuildNodeDefinition Definition)
		{
			Name = Definition.Name;
			AgentTypes = Definition.AgentTypes.Split(new char[]{ ';' }, StringSplitOptions.RemoveEmptyEntries);
			AgentPlatform = Definition.AgentPlatform;
			AgentRequirements = Definition.AgentRequirements;
			AgentSharingGroup = Definition.AgentSharingGroup;
			FrequencyShift = Definition.FrequencyShift;
			AgentMemoryRequirement = Definition.AgentMemoryRequirement;
			TimeoutInMinutes = Definition.TimeoutInMinutes;
			Priority = Definition.Priority;
			RecipientsForFailureEmails = Definition.Notify.Split(';');
			AddSubmittersToFailureEmails = Definition.AddSubmittersToFailureEmails;
			NotifyOnWarnings = Definition.NotifyOnWarnings;
			SendSuccessEmail = Definition.SendSuccessEmail;
			IsParallelAgentShareEditor = Definition.IsParallelAgentShareEditor;
			IsSticky = Definition.IsSticky;
			IsTest = Definition.IsTest;
			DisplayGroupName = Definition.DisplayGroupName;
		}

		public virtual void ArchiveBuildProducts(string SharedStorageDir, bool bWriteToSharedStorage)
		{
			TempStorage.StoreToTempStorage(Name, BuildProducts, SharedStorageDir, bWriteToSharedStorage);
		}

		public virtual void RetrieveBuildProducts(string SharedStorageDir)
		{
			CommandUtils.Log("***** Retrieving GUBP Node {0}", Name);
			try
			{
				BuildProducts = TempStorage.RetrieveFromTempStorage(SharedStorageDir, Name);
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, "Build Products cannot be found for node {0}", Name);
			}
		}

		public abstract bool DoBuild();

		public abstract bool DoFakeBuild();

		public TriggerNode ControllingTrigger
		{
			get { return (ControllingTriggers.Length == 0)? null : ControllingTriggers[ControllingTriggers.Length - 1]; }
		}

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
}
