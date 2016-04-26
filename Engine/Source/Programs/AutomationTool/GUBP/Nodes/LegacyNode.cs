// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool
{
	public class LegacyNodeDefinition : BuildNodeDefinition
	{
		public GUBP Owner;
		public GUBP.GUBPNode Node;

		public LegacyNodeDefinition(GUBP InOwner, GUBP.GUBPNode InNode)
		{
			Owner = InOwner;
			Node = InNode;

			// Copy the definition from the node
			Name = Node.GetFullName();
			AgentTypes = String.Join(";", Node.GetAgentTypes());
			AgentRequirements = Node.ECAgentString();
			AgentSharingGroup = Node.AgentSharingGroup;
			AgentMemoryRequirement = Node.AgentMemoryRequirement();
			TimeoutInMinutes = Node.TimeoutInMinutes();
			SendSuccessEmail = Node.SendSuccessEmail();
			NotifyOnWarnings = Node.NotifyOnWarnings();
			Priority = Node.Priority();
			IsSticky = Node.IsSticky();
			DependsOn = String.Join(";", Node.FullNamesOfDependencies);
			RunAfter = String.Join(";", Node.FullNamesOfPseudodependencies);
			IsTest = Node.IsTest();
			DisplayGroupName = Node.GetDisplayGroupName();
		}

		public override void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			throw new NotImplementedException();
		}

		public override BuildNode CreateNode()
		{
			return new LegacyNode(this);
		}
	}

	public class LegacyNode : BuildNode
	{
		GUBP Owner;
		GUBP.GUBPNode Node;

		public LegacyNode(LegacyNodeDefinition Definition)
			: base(Definition)
		{
			Owner = Definition.Owner;
			Node = Definition.Node;
		}

		public override void RetrieveBuildProducts(string SharedStorageDir)
		{
			base.RetrieveBuildProducts(SharedStorageDir);

			Node.BuildProducts = BuildProducts;
		}

		public override bool DoBuild()
		{
			Node.AllDependencyBuildProducts = new List<string>();
			foreach (BuildNode InputDependency in InputDependencies)
			{
				foreach (string BuildProduct in InputDependency.BuildProducts)
				{
					Node.AddDependentBuildProduct(BuildProduct);
				}
			}

			Node.DoBuild(Owner);
			BuildProducts = Node.BuildProducts;
			return true;
		}

		public override bool DoFakeBuild()
		{
			Node.DoFakeBuild(Owner);
			BuildProducts = Node.BuildProducts;
			return true;
		}
	}
}
