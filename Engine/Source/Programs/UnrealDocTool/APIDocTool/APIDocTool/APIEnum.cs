// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.IO;
using DoxygenLib;

namespace APIDocTool
{
	public class APIEnum : APIMember
    {
		public readonly DoxygenEntity Entity;
		public readonly XmlNode Node;
		public APIProtection Protection;

		public MetadataDirective MetadataDirective;

		public string BriefDescription = "";
		public string FullDescription = "";

		public string Definition = "";

		public List<APIEnumValue> Values = new List<APIEnumValue>();

		public APIEnum(APIPage InParent, DoxygenEntity InEntity, string InName)
			: base(InParent, GetCleanName(InName))
        {
			// Set the defaults
			Entity = InEntity;
			Node = InEntity.Node;
			Protection = ParseProtection(Node);

			// Read the values
			using(XmlNodeList ValueNodeList = Node.SelectNodes("enumvalue"))
			{
				foreach(XmlNode ValueNode in ValueNodeList)
				{
					APIEnumValue Value = new APIEnumValue(ValueNode);
					AddRefLink(Value.Id, this);
					Values.Add(Value);
				}
			}

			// Add it as a link target
			AddRefLink(Node.Attributes["id"].InnerText, this);
        }

		public static string GetCleanName(string Name)
		{
			int Index = Name.IndexOf("::@");
			if(Index != -1)
			{
				Name = Name.Substring(0, Index + 2);
			}
			return Name;
		}

		public override void Link()
        {
			// Read the metadata
			MetadataDirective = MetadataCache.FindMetadataForMember(Node);

			// Get the description
			ParseBriefAndFullDescription(Node, out BriefDescription, out FullDescription);

			// If there isn't one and we're in a namespace, use that instead
			if(String.IsNullOrEmpty(BriefDescription) && String.IsNullOrEmpty(FullDescription) && Entity.NamespaceNode != null)
			{
				ParseBriefAndFullDescription(Entity.NamespaceNode, out BriefDescription, out FullDescription);
			}

			// Link all the values
			foreach (APIEnumValue Value in Values)
			{
				Value.Link();
			}
        }

		private void WriteDefinition(UdnWriter Writer)
		{
			List<string> Lines = new List<string>();

			Lines.Add(String.Format("enum {0}", ShortName.StartsWith("@")? "" : ShortName));
			Lines.Add(Utility.EscapeText("{"));
			if (Values.Count > 0)
			{
				int NamePadding = Values.Max(x => x.Name.Length) + 4;
				foreach (APIEnumValue Value in Values)
				{
					string ValueLine = UdnWriter.TabSpaces + Value.Name;
					if (Value.Initializer != null)
					{
						for (int Idx = 0; Idx < NamePadding - Value.Name.Length; Idx++) ValueLine += UdnWriter.Space;
						ValueLine += Value.Initializer;
					}
					Lines.Add(ValueLine + ",");
				}
			}
			Lines.Add(Utility.EscapeText("}"));

			WriteNestedSimpleCode(Writer, Lines);
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(AnnotateAnonymousNames(Name), PageCrumbs, BriefDescription);

				// Write the syntax
				Writer.EnterSection("syntax", "Syntax");
				WriteDefinition(Writer);
				Writer.LeaveSection();

				// Write the enum values
				Writer.WriteListSection("values", "Values", "Name", "Description", Values.Select(x => x.GetListItem()));

				// Write the description
				if (!Utility.IsNullOrWhitespace(FullDescription))
				{
					Writer.EnterSection("description", "Remarks");
					Writer.WriteLine(FullDescription);
					Writer.LeaveSection();
				}

				// Write the references
				WriteReferencesSection(Writer, Entity);
			}
        }

		public UdnIconListItem GetListItem()
		{
			List<Icon> SummaryIcons = new List<Icon>() { Icons.Enum[(int)Protection] };
			if (MetadataDirective != null)
			{
				SummaryIcons.Add(Icons.ReflectedEnum);
				SummaryIcons.AddRange(MetadataDirective.Icons);
			}
			return new UdnIconListItem(SummaryIcons, Name, BriefDescription, LinkPath);
		}
	}
}
