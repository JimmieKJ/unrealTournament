// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIAction : APIPage
	{
		public readonly string Tooltip;
		public readonly string CompactName;
		public readonly string NodeType;

		public readonly bool bShowAddPin;

		private List<APIActionPin> Pins;

		public APIAction(APIPage InParent, string InName, Dictionary<string, object> ActionProperties)
			: base(InParent, InName)
		{
			object Value;
			
			if (ActionProperties.TryGetValue("Tooltip", out Value))
			{
				Tooltip = String.Concat(((string)Value).Split('\n').Select(Line => Line.Trim() + '\n'));
			}
			else
			{
				Tooltip = "";
			}

			if (ActionProperties.TryGetValue("CompactTitle", out Value))
			{
				CompactName = (string)Value;
			}

			if (ActionProperties.TryGetValue("NodeType", out Value))
			{
				NodeType = (string)Value;
			}
			else
			{
				NodeType = "function";
			}

			if (ActionProperties.TryGetValue("Pins", out Value))
			{
				Pins = new List<APIActionPin>();

				foreach (var Pin in (Dictionary<string, object>)Value)
				{
					Pins.Add(new APIActionPin(this, APIActionPin.GetPinName((Dictionary<string, object>)Pin.Value), (Dictionary<string, object>)Pin.Value));
				}
			}

			if (ActionProperties.TryGetValue("ShowAddPin", out Value))
			{
				bShowAddPin = Convert.ToBoolean((string)Value);
			}
		}

		private void WritePins(UdnWriter Writer, IEnumerable<APIActionPin> Pins)
		{
			foreach(var Pin in Pins)
			{
				Pin.WritePin(Writer);
			}
		}

		private void WritePinObjects(UdnWriter Writer, IEnumerable<APIActionPin> Pins)
		{
			foreach(var Pin in Pins)
			{
				Pin.WriteObject(Writer, CompactName == null);
			}
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, Name);

				// Tooltip/description
				Writer.WriteLine(Tooltip);

				if (Pins != null && Pins.Count() > 0)
				{
					// Visualization of the node
					Writer.EnterRegion("graph");
					Writer.EnterObject("BlueprintNode");
					if (CompactName != null)
					{
						Writer.WriteParamLiteral("type", "compact");
						Writer.WriteParamLiteral("title", CompactName);
					}
					else
					{
						Writer.WriteParamLiteral("type", NodeType);
						Writer.WriteParamLiteral("title", Name);
					}
					Writer.EnterParam("inputs");
					WritePinObjects(Writer, Pins.Where(x => x.bInputPin));
					Writer.LeaveParam();
					Writer.EnterParam("outputs");
					WritePinObjects(Writer, Pins.Where(x => !x.bInputPin && x.GetTypeText() != "delegate"));

					if (bShowAddPin)
					{
						Writer.EnterObject("BlueprintPin");
						Writer.WriteParamLiteral("type", "addpin");
						Writer.WriteParamLiteral("id", "AddPin");
						Writer.WriteParamLiteral("title", "Add pin");
						Writer.LeaveObject();
					}

					Writer.LeaveParam();
					Writer.LeaveObject();
					Writer.LeaveRegion();

					// Inputs
					Writer.EnterSection("inputs", "Inputs");
					Writer.WriteObject("MemberIconListHeadBlank");
					WritePins(Writer, Pins.Where(x => x.bInputPin));
					Writer.WriteObject("MemberIconListTail");
					Writer.LeaveSection();

					// Outputs
					Writer.EnterSection("outputs", "Outputs");
					Writer.WriteObject("MemberIconListHeadBlank");
					WritePins(Writer, Pins.Where(x => !x.bInputPin));
					Writer.WriteObject("MemberIconListTail");
					Writer.LeaveSection();
				}
			}
		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{
			/*if (Pins != null)
			{
				Pages.AddRange(Pins);
			}*/
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			Manifest.Add(Name, this);
			/*if (Pins != null)
			{
				foreach (APIPage Pin in Pins)
				{
					Pin.AddToManifest(Manifest);
				}
			}*/
		}

		public UdnListItem GetListItem()
		{
			return new UdnListItem(Name, Tooltip, LinkPath);
		}
	}
}
