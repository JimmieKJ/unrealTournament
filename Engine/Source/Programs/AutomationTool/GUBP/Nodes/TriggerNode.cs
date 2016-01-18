using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;

namespace AutomationTool
{
	public class TriggerNodeTemplate : LegacyNodeTemplate
	{
		public new GUBP.WaitForUserInput Node;

		public TriggerNodeTemplate(GUBP InOwner, GUBP.WaitForUserInput InNode) : base(InOwner, InNode)
		{
			Node = InNode;
		}

		public override BuildNode Instantiate()
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

		public TriggerNode(TriggerNodeTemplate Template) : base(Template)
		{
			AddSubmittersToFailureEmails = false;

			StateName = Template.Node.GetTriggerStateName();
			DescriptionText = Template.Node.GetTriggerDescText();
			ActionText = Template.Node.GetTriggerActionText();
			RequiresRecursiveWorkflow = Template.Node.TriggerRequiresRecursiveWorkflow();
		}

		public void Activate()
		{
			IsTriggered = true;
			IsSticky = true;
		}
	}
}
