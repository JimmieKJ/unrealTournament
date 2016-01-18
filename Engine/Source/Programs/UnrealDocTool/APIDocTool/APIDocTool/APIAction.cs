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
		protected class TooltipLine
		{
			public TooltipLine()
			{
			}

			public TooltipLine(LineType InType)
			{
				Type = InType;
			}

			public TooltipLine(LineType InType, string InText)
			{
				Type = InType;
				Text = InText;
			}

			public enum LineType
			{
				Normal,
				Note,
				Count
			}

			public LineType Type = LineType.Count;
			public string Text;
		}

		protected readonly List<TooltipLine> TooltipData = new List<TooltipLine>();
		public readonly string TooltipNormalText;
		public readonly string CompactName;
		public readonly string NodeType;

		public readonly bool bShowAddPin;

		private List<APIActionPin> Pins;

		public APIAction(APIPage InParent, string InName, Dictionary<string, object> ActionProperties)
			: base(InParent, InName)
		{
			object Value;

			TooltipNormalText = "";
			TooltipLine.LineType CurrentLineType = TooltipLine.LineType.Count;		//Invalid
			if (ActionProperties.TryGetValue("Tooltip", out Value))
			{
				//Create an interleaved list of normal text and note regions. Also, store all normal text as a single block (i.e. notes stripped out) in a separate place.
				foreach (string Line in ((string)Value).Split('\n'))
				{
					string TrimmedLine = Line.Trim();

					if (TrimmedLine.StartsWith("@note"))
					{
						if (TrimmedLine.Length > 6)
						{
							if (CurrentLineType != TooltipLine.LineType.Note)
							{
								CurrentLineType = TooltipLine.LineType.Note;
								TooltipData.Add(new TooltipLine(CurrentLineType));
							}
							TooltipData[TooltipData.Count - 1].Text += (TrimmedLine.Substring(6) + '\n');
						}
					}
					else
					{
						if (CurrentLineType != TooltipLine.LineType.Normal)
						{
							CurrentLineType = TooltipLine.LineType.Normal;
							TooltipData.Add(new TooltipLine(CurrentLineType));
						}
						TooltipData[TooltipData.Count - 1].Text += (TrimmedLine + '\n');
						TooltipNormalText += (TrimmedLine + '\n');
					}
				}
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

				// Tooltip/description - Write this as interleaved text and notes
				foreach (TooltipLine TTL in TooltipData)
				{
					switch (TTL.Type)
					{
						case TooltipLine.LineType.Normal:
							Writer.WriteLine(TTL.Text);
							break;
						case TooltipLine.LineType.Note:
							Writer.EnterRegion("note");
							Writer.WriteLine(TTL.Text);
							Writer.LeaveRegion();
							break;
						default:
							//Error? Ignore this entry for now.
							break;
					}
				}

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
					// TODO: Remove this hack and reinstate the one-line version once UE-16475 is resolved.
					bool bAlreadyWroteOutputDelegate = false;
					for (int i = 0; i < Pins.Count; ++i)
					{
						APIActionPin Pin = Pins[i];
						if (!Pin.bInputPin)
						{
							if (Pin.GetTypeText() == "delegate")
							{
								if (bAlreadyWroteOutputDelegate)
								{
									continue;
								}
								bAlreadyWroteOutputDelegate = true;
							}
							Pin.WritePin(Writer);
						}
					}
					//WritePins(Writer, Pins.Where(x => !x.bInputPin));
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
			return new UdnListItem(Name, TooltipNormalText, LinkPath);
		}
	}
}
