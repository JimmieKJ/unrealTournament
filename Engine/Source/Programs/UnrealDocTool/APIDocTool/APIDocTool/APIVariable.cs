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
    public class APIVariable : APIMember
    {
		public readonly XmlNode Node;
		public readonly APIProtection Protection;

        public bool Mutable;
        public string Bitfield = "";
		public string ArgsString = "";

		public string BriefDescription = "";
		public string FullDescription = "";

		public MetadataDirective MetadataDirective;

		public string Definition = "";

		public string Type = "";
		public string IsolatedType = "";
		public string AbbreviatedType = "";

		public bool IsMutable = false;
		public bool IsStatic = false;

		public List<string> Warnings = new List<string>();
		public List<string> SnippetText = null;

		public APIVariable(APIPage InParent, XmlNode InNode)
			: base(InParent, InNode.SelectSingleNode("name").InnerText)
		{
			Node = InNode;
			Protection = ParseProtection(Node);
			AddRefLink(Node.Attributes["id"].InnerText, this);
		}

		public bool IsDeprecated()
		{
			return Name.EndsWith("_DEPRECATED");
		}

		public override void Link()
		{
			// Parse the metadata
			MetadataDirective = MetadataCache.FindMetadataForMember(Node, "UPROPERTY");

			ParseBriefAndFullDescription(Node, out BriefDescription, out FullDescription);

            XmlNode BitfieldNode = Node.SelectSingleNode("bitfield");
            if (BitfieldNode != null)
            {
                Bitfield = BitfieldNode.InnerText;
            }
			IsMutable = Node.Attributes.GetNamedItem("mutable").InnerText == "yes";
			IsStatic = Node.Attributes.GetNamedItem("static").InnerText == "yes";

			XmlNode DefNode = Node.SelectSingleNode("definition");
			if (DefNode != null)
			{
				Definition = ConvertToMarkdown(DefNode);
			}

			XmlNode type = Node.SelectSingleNode("type");
			Type = APIMember.RemoveElaborations(ConvertToMarkdown(type));

			XmlNode ArgsStringNode = Node.SelectSingleNode("argsstring");
			if(ArgsStringNode != null)
			{
				ArgsString = ConvertToMarkdown(ArgsStringNode);
			}

			IsolatedType = Type;
			if(!String.IsNullOrEmpty(ArgsString))
			{
				IsolatedType += ArgsString;
			}
			if(!String.IsNullOrEmpty(Bitfield))
			{
				IsolatedType += ": " + Bitfield;
			}
			AbbreviatedType = Markdown.Truncate(IsolatedType, 15, "...");

			XmlNodeList SimpleNodes = Node.SelectNodes("detaileddescription/para/simplesect");
			foreach (XmlNode node in SimpleNodes)
			{
				switch (node.Attributes.GetNamedItem("kind").InnerText)
				{
					case "warning":
						Warnings.Add(ConvertToMarkdown(node.SelectSingleNode("para")).TrimStart(':'));
						break;
				}
			}

			SnippetText = APISnippets.LoadSnippetTextForSymbol(FullName);			
		}

		private void WriteDefinition(UdnWriter Writer)
		{
			List<string> Lines = new List<string>();

			if (MetadataDirective != null)
			{
				Lines.AddRange(MetadataDirective.ToMarkdown());
			}

			StringBuilder Definition = new StringBuilder();
			if (IsStatic) Definition.Append("static ");
			if (IsMutable) Definition.Append("mutable ");
			Definition.Append(Type + " " + Name);
			if (Bitfield != "") Definition.Append(": " + Bitfield);
			if (ArgsString != "") Definition.Append(ArgsString);
			Definition.Append("  ");
			Lines.Add(Definition.ToString());

			WriteNestedSimpleCode(Writer, Lines);
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
        {
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, BriefDescription);

				// Write the warnings
				if (Warnings.Count > 0)
				{
					Writer.EnterTag("[REGION:warning]");
					Writer.WriteLine("**Warnings**");
					foreach (string Warning in Warnings)
					{
						Writer.WriteLine("* " + Warning);
					}
					Writer.LeaveTag("[/REGION]");
				}

				// Write the syntax
				Writer.EnterSection("syntax", "Syntax");
				WriteDefinition(Writer);
				Writer.LeaveSection();

				// Write the description
				if (!Utility.IsNullOrWhitespace(FullDescription))
				{
					Writer.EnterSection("description", "Remarks");
					Writer.WriteLine(FullDescription);
					Writer.LeaveSection();
				}

				//Write code snippets
				WriteSnippetSection(Writer, SnippetText);
			}
        }

		public override bool ShouldOutputPage()
		{
			return (BriefDescription != FullDescription || IsolatedType != AbbreviatedType);
		}

		public void WriteListItem(UdnWriter Writer)
		{
			bool OutputPage = ShouldOutputPage();

			// Enter the object
			Writer.EnterObject(OutputPage? "VariableListItem" : "VariableListItemNoLink");

			// Get all the icons
			List<Icon> Icons = new List<Icon>();
			Icons.Add(APIDocTool.Icons.Variable[(int)Protection]);
			if (IsStatic)
			{
				Icons.Add(APIDocTool.Icons.StaticVariable);
			}
			if (MetadataDirective != null)
			{
				Icons.Add(APIDocTool.Icons.ReflectedVariable);
				Icons.AddRange(MetadataDirective.Icons);
			}
			Writer.WriteParam("icons", Icons);

			// Write the type
			Writer.WriteParam("type", AbbreviatedType);
			Writer.WriteParam("name", Name);
			if(OutputPage)
			{
				Writer.WriteParam("link", "[RELATIVE:" + LinkPath + "]");
			}
			Writer.WriteParam("description", BriefDescription);

			// Leave the object
			Writer.LeaveObject();
		}
		
		public static void WriteList(UdnWriter Writer, IEnumerable<APIVariable> Variables)
		{
			Writer.WriteObject("VariableListHead");
			foreach (APIVariable Variable in Variables)
			{
				Variable.WriteListItem(Writer);
			}
			Writer.WriteObject("VariableListTail");
		}

		public static bool WriteListSection(UdnWriter Writer, string SectionId, string SectionTitle, IEnumerable<APIVariable> Variables)
		{
			APIVariable[] VariableArray = Variables.ToArray();
			if (VariableArray.Length > 0)
			{
				Writer.EnterSection(SectionId, SectionTitle);
				WriteList(Writer, Variables);
				Writer.LeaveSection();
				return true;
			}
			return false;
		}
	}
}
