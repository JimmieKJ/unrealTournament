// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using DoxygenLib;
using System.Xml;

namespace APIDocTool
{
	public class APIEventParameters : APIMember
	{
		public APIMember AttachedTo;
		public DoxygenEntity Entity;

		public APIEventParameters(APIPage InParent, APIMember InAttachedTo, DoxygenEntity InEntity)
			: base(InParent, InEntity.Name)
		{
			AttachedTo = InAttachedTo;
			Entity = InEntity;
			AddRefLink(Entity.Id, this);
		}

		public override void Link()
		{
		}

		public void WriteSyntax(UdnWriter Writer)
		{
			List<string> Lines = new List<string>();
			Lines.Add(String.Format("{0} {1}", Entity.Node.Attributes["kind"].Value, Entity.Name));
			Lines.Add("&#40;");
			using (XmlNodeList MemberNodeList = Entity.Node.SelectNodes("sectiondef/memberdef"))
			{
				foreach (XmlNode MemberNode in MemberNodeList)
				{
					if (MemberNode.Attributes["kind"].Value == "variable")
					{
						Lines.Add(UdnWriter.TabSpaces + String.Format("{0} {1};", ConvertToMarkdown(MemberNode.SelectSingleNode("type")), MemberNode.SelectSingleNode("name").InnerText));
					}
					else if(MemberNode.Attributes["kind"].Value == "function")
					{
						Lines.Add(UdnWriter.TabSpaces);
						Lines.Add(UdnWriter.TabSpaces + "// " + ConvertToMarkdown(MemberNode.SelectSingleNode("briefdescription")).Replace('\r', ' ').Replace('\n', ' ').Trim());
						Lines.Add(UdnWriter.TabSpaces + ConvertFunctionNodeToMarkdown(MemberNode));
					}
				}
			}
			Lines.Add("&#41;");
			WriteNestedSimpleCode(Writer, Lines);
		}

		public string ConvertFunctionNodeToMarkdown(XmlNode Node)
		{
			string Name = Node.SelectSingleNode("name").InnerText;
			string Type = ConvertToMarkdown(Node.SelectSingleNode("type"));
			string Args = ConvertToMarkdown(Node.SelectSingleNode("argsstring"));
			return Utility.IsNullOrWhitespace(Type)? (Name + Args + ";") : (Type + Name + Args + ";");
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, "");
				Writer.EnterTag("[OBJECT:Class]");

				// Write the brief description
				Writer.WriteLine("[PARAM:briefdesc]");
				Writer.WriteLine("Parameter struct for [{0}]({1})", AttachedTo.FullName, AttachedTo.LinkPath);
				Writer.WriteLine("[/PARAM]");

				// Write the hierarchy
				Writer.EnterTag("[PARAM:hierarchy]");
				Writer.LeaveTag("[/PARAM]");

				// Write the record definition
				Writer.EnterTag("[PARAM:syntax]");
				Writer.EnterSection("syntax", "Syntax");
				WriteSyntax(Writer);
				Writer.LeaveSection();
				Writer.LeaveTag("[/PARAM]");

				// Write the metadata section
				Writer.WriteLine("[PARAM:meta]");
				Writer.WriteLine("[/PARAM]");

				// Write all the specializations
				Writer.EnterTag("[PARAM:specializations]");
				Writer.LeaveTag("[/PARAM]");

				// Write all the typedefs
				Writer.EnterTag("[PARAM:typedefs]");
				Writer.LeaveTag("[/PARAM]");

				// Write all the constructors
				Writer.EnterTag("[PARAM:constructors]");
				Writer.LeaveTag("[/PARAM]");

				// Write all the destructors
				Writer.EnterTag("[PARAM:destructors]");
				Writer.LeaveTag("[/PARAM]");

				// Write all the enums
				Writer.EnterTag("[PARAM:enums]");
				Writer.LeaveTag("[/PARAM]");

				// Write all the inner structures
				Writer.EnterTag("[PARAM:classes]");
				Writer.LeaveTag("[/PARAM]");

				// Write all the constants
				Writer.EnterTag("[PARAM:constants]");
				Writer.LeaveTag("[/PARAM]");

				// Write all the variables
				Writer.EnterTag("[PARAM:variables]");
				Writer.LeaveTag("[/PARAM]");

				// Write the functions
				Writer.EnterTag("[PARAM:methods]");
				Writer.LeaveTag("[/PARAM]");

				// Write the operator list
				Writer.EnterTag("[PARAM:operators]");
				Writer.LeaveTag("[/PARAM]");

				// Write class description
				Writer.EnterTag("[PARAM:description]");
				Writer.LeaveTag("[/PARAM]");

				// Write the declaration file
				Writer.EnterTag("[PARAM:declaration]");
				Writer.LeaveTag("[/PARAM]");

				Writer.LeaveTag("[/OBJECT]");
			}
		}
	}
}
