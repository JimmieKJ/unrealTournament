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
	public abstract class APIConstant : APIMember
	{
		public readonly DoxygenEntity Entity;

		public string BriefDescription;
		public string FullDescription;

		public APIConstant(APIPage InParent, DoxygenEntity InEntity, string InName)
			: base(InParent, InName)
		{
			Entity = InEntity;
		}

		protected abstract void WriteDefinition(UdnWriter Writer);

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, BriefDescription);

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

				// Write the references
				WriteReferencesSection(Writer, Entity);
			}
		}

		public UdnListItem GetListItem()
		{
			return new UdnListItem(Name, BriefDescription, LinkPath);
		}
	}

	public class APIConstantEnum : APIConstant
	{
		public readonly XmlNode EnumNode;
		public readonly XmlNode ValueNode;
		public string Initializer;

		public APIConstantEnum(APIPage InParent, DoxygenEntity InEnumEntity, XmlNode InValueNode, string InName)
			: base(InParent, InEnumEntity, InName)
		{
			EnumNode = InEnumEntity.Node;
			ValueNode = InValueNode;
		}

		public override void Link()
		{
			// Get the description
			ParseBriefAndFullDescription(EnumNode, out BriefDescription, out FullDescription);
			if (Utility.IsNullOrWhitespace(BriefDescription) && Utility.IsNullOrWhitespace(FullDescription))
			{
				ParseBriefAndFullDescription(ValueNode, out BriefDescription, out FullDescription);
			}

			// Get the initializer
			XmlNode InitializerNode = ValueNode.SelectSingleNode("initializer");
			Initializer = (InitializerNode == null) ? "" : InitializerNode.InnerText;
		}

		public static IEnumerable<APIConstant> Read(APIPage Parent, DoxygenEntity Entity)
		{
			XmlNode EnumNode = Entity.Node;
			using (XmlNodeList ValueNodeList = EnumNode.SelectNodes("enumvalue"))
			{
				foreach (XmlNode ValueNode in ValueNodeList)
				{
					string Name = ValueNode.SelectSingleNode("name").InnerText;
					APIConstant Constant = new APIConstantEnum(Parent, Entity, ValueNode, Name);
					yield return Constant;
				}
			}
		}

		protected override void WriteDefinition(UdnWriter Writer)
		{
			List<string> Lines = new List<string>();

			Lines.Add("enum");
			Lines.Add(Utility.EscapeText("{"));
			Lines.Add(UdnWriter.TabSpaces + Name + " " + Initializer);
			Lines.Add(Utility.EscapeText("}"));

			WriteNestedSimpleCode(Writer, Lines);
		}
	}

	public class APIConstantVariable : APIConstant
	{
		public readonly XmlNode Node;

		public string Type;
		public string Initializer;

		public APIConstantVariable(APIPage InParent, DoxygenEntity InEntity)
			: base(InParent, InEntity, InEntity.Name)
		{
			Node = Entity.Node;
		}

		public override void Link()
		{
			// Get the description
			ParseBriefAndFullDescription(Node, out BriefDescription, out FullDescription);

			// Get the type
			XmlNode TypeNode = Node.SelectSingleNode("type");
			Type = ConvertToMarkdown(TypeNode);

			// Get the initializer
			XmlNode InitializerNode = Node.SelectSingleNode("initializer");
			Initializer = (InitializerNode == null) ? null : InitializerNode.InnerText;
		}

		protected override void WriteDefinition(UdnWriter Writer)
		{
			string Definition = "static " + Type + " " + Name;
			if(Initializer != null) Definition += " " + Initializer;
			Definition += ";";
			WriteNestedSimpleCode(Writer, new List<string>{ Definition });
		}
	}
}
