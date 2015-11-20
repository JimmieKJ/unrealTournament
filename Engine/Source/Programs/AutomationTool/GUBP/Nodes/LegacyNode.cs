using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool
{
	public class LegacyNodeTemplate : BuildNodeTemplate
	{
		public GUBP Owner;
		public GUBP.GUBPNode Node;

		public LegacyNodeTemplate(GUBP InOwner, GUBP.GUBPNode InNode)
		{
			Owner = InOwner;
			Node = InNode;

			SetStandardProperties(InNode, this);
		}

		public static void SetStandardProperties(GUBP.GUBPNode Node, BuildNodeTemplate Definition)
		{
			Definition.Name = Node.GetFullName();
			Definition.AgentRequirements = Node.ECAgentString();
			Definition.AgentSharingGroup = Node.AgentSharingGroup;
			Definition.AgentMemoryRequirement = Node.AgentMemoryRequirement();
			Definition.TimeoutInMinutes = Node.TimeoutInMinutes();
			Definition.SendSuccessEmail = Node.SendSuccessEmail();
			Definition.Priority = Node.Priority();
			Definition.IsSticky = Node.IsSticky();
			Definition.InputDependencyNames = String.Join(";", Node.FullNamesOfDependencies);
			Definition.OrderDependencyNames = String.Join(";", Node.FullNamesOfPseudodependencies);
			Definition.IsTest = Node.IsTest();
			Definition.DisplayGroupName = Node.GetDisplayGroupName();
            Definition.GameNameIfAnyForFullGameAggregateNode = Node.GameNameIfAnyForFullGameAggregateNode();
		}

		public override BuildNode Instantiate()
		{
			return new LegacyNode(this);
		}
	}

	public class LegacyNode : BuildNode
	{
		GUBP Owner;
		GUBP.GUBPNode Node;

		public LegacyNode(LegacyNodeTemplate Template)
			: base(Template)
		{
			Owner = Template.Owner;
			Node = Template.Node;
		}

		public override void RetrieveBuildProducts(TempStorageNodeInfo TempStorageNodeInfo)
		{
			base.RetrieveBuildProducts(TempStorageNodeInfo);

			Node.BuildProducts = BuildProducts;
		}

		public override void DoBuild()
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
		}

		public override void DoFakeBuild()
		{
			Node.DoFakeBuild(Owner);
			BuildProducts = Node.BuildProducts;
		}
	}
}
