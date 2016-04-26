// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;

namespace AutomationTool
{
	public class TriggerNodeDefinition : LegacyNodeDefinition
	{
		public new GUBP.WaitForUserInput Node;

		public TriggerNodeDefinition(GUBP InOwner, GUBP.WaitForUserInput InNode) : base(InOwner, InNode)
		{
			Node = InNode;
		}

		public override BuildNode CreateNode()
		{
			return new TriggerNode(this);
		}
	}

	public class TriggerNode : LegacyNode
	{
        public bool IsTriggered;
		public bool RequiresRecursiveWorkflow;
		public string StateName;
		public string DescriptionText;
		public string ActionText;

		public TriggerNode(TriggerNodeDefinition Definition) : base(Definition)
		{
			AddSubmittersToFailureEmails = false;

			StateName = Definition.Node.GetTriggerStateName();
			DescriptionText = Definition.Node.GetTriggerDescText();
			ActionText = Definition.Node.GetTriggerActionText();
			RequiresRecursiveWorkflow = Definition.Node.TriggerRequiresRecursiveWorkflow();
		}

		public void Activate()
		{
			IsTriggered = true;
			IsSticky = true;
		}
	}
}
