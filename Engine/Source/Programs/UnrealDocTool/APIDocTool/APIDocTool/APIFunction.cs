// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.IO;
using System.Diagnostics;
using DoxygenLib;

namespace APIDocTool
{
	public enum APIFunctionType
	{
		Normal,
		Constructor,
		Destructor,
		UnaryOperator,
		BinaryOperator,
	}

	public struct APIFunctionKey
	{
		public string Name;
		public APIFunctionType Type;

		public APIFunctionKey(string InName, APIFunctionType InType)
		{
			Name = InName;
			Type = InType;
		}

		public static APIFunctionKey FromEntity(APIPage Parent, DoxygenEntity Entity)
		{
			string Name = Entity.Name;
			if (Parent != null && Parent is APIRecord)
			{
				if (Name == Parent.Name && (Parent is APIRecord) && Entity.Node.Attributes["static"].Value != "yes")
				{
					return new APIFunctionKey(Name, APIFunctionType.Constructor);
				}
				else if (Name == "~" + Parent.Name)
				{
					return new APIFunctionKey(Name, APIFunctionType.Destructor);
				}
			}
			if (Name.StartsWith("operator") && Name.Length > 8 && !Char.IsLetterOrDigit(Name[8]) && Name[8] != '_')
			{
				int NumParams = 0;
				using (XmlNodeList ParamNodeList = Entity.Node.SelectNodes("param"))
				{
					foreach (XmlNode ParamNode in ParamNodeList)
					{
						NumParams++;
					}
				}
				if ((Parent is APIRecord) && Entity.Node.Attributes["static"].Value != "yes")
				{
					NumParams++;
				}
				return new APIFunctionKey(Name, (NumParams == 1) ? APIFunctionType.UnaryOperator : APIFunctionType.BinaryOperator);
			}
			return new APIFunctionKey(Name, APIFunctionType.Normal);
		}

		public string GetLinkName()
		{
			switch (Type)
			{
				case APIFunctionType.Constructor:
					return "__ctor";
				case APIFunctionType.Destructor:
					return "__dtor";
				case APIFunctionType.UnaryOperator:
					return GetUnaryOperatorLinkName(Name);
				case APIFunctionType.BinaryOperator:
					return GetBinaryOperatorLinkName(Name);
				case APIFunctionType.Normal:
					break;
			}
			return Name;
		}

		public static string GetUnaryOperatorLinkName(string Name)
		{
			switch (Name)
			{
				case "operator+":
					return "op_pos";
				case "operator-":
					return "op_neg";
				case "operator++":
					return "op_preinc";
				case "operator--":
					return "op_predec";
				case "operator!":
					return "op_lognot";
				case "operator~":
					return "op_bitnot";
				case "operator*":
					return "op_deref";
				case "operator&":
					return "op_addressof";
				default:
					return Name;
			}
		}

		public static string GetBinaryOperatorLinkName(string Name)
		{
			switch (Name)
			{
				// Arithmetic
				case "operator+":
					return "op_add";
				case "operator-":
					return "op_sub";
				case "operator*":
					return "op_mul";
				case "operator/":
					return "op_div";
				case "operator%":
					return "op_mod";
				case "operator++":
					return "op_postinc";
				case "operator--":
					return "op_postdec";

				// Logical
				case "operator&&":
					return "op_logand";
				case "operator||":
					return "op_logor";

				// Bitwise
				case "operator&":
					return "op_bitand";
				case "operator|":
					return "op_bitor";
				case "operator^":
					return "op_bitxor";
				case "operator<<":
					return "op_lshift";
				case "operator>>":
					return "op_rshift";

				// Assignment
				case "operator=":
					return "op_assign";
				case "operator+=":
					return "op_add_assign";
				case "operator-=":
					return "op_sub_assign";
				case "operator*=":
					return "op_mul_assign";
				case "operator/=":
					return "op_div_assign";
				case "operator%=":
					return "op_mod_assign";
				case "operator&=":
					return "op_bitand_assign";
				case "operator|=":
					return "op_bitor_assign";
				case "operator^=":
					return "op_bitxor_assign";
				case "operator<<=":
					return "op_lshift_assign";
				case "operator>>=":
					return "op_rshift_assign";

				// Comparison
				case "operator==":
					return "op_cmp_eq";
				case "operator!=":
					return "op_cmp_ne";
				case "operator<":
					return "op_cmp_lt";
				case "operator>":
					return "op_cmp_gt";
				case "operator<=":
					return "op_cmp_lte";
				case "operator>=":
					return "op_cmp_gte";

				// Member
				case "operator[]":
					return "op_subscript";
				case "operator->":
					return "op_pm";
				case "operator->*":
					return "op_mem_pm";

				default:
					return Name;
			}
		}
	}

	public class APIFunctionParam
	{
		public readonly APIFunction Function;
		public readonly XmlNode Node;
		public readonly string Name;

		public APIFunctionParam(APIFunction InFunction, XmlNode InNode)
		{
			Function = InFunction;
			Node = InNode;

			// Set the name
			XmlNode NameNode = Node.SelectSingleNode("declname");
			if (NameNode != null) Name = NameNode.InnerText;
		}

		public string Type
		{
			get { return APIMember.ConvertToMarkdown(Node.SelectSingleNode("type")); }
		}

		public string TypeText
		{
			get { return Node.SelectSingleNode("type").InnerText; }
		}

		public string Definition
		{
			get { return (Name == null) ? Type : (Type + " " + Name); }
		}
	}

	public class APIFunctionParamSummary
	{
		public readonly APIFunction Function;
		public readonly XmlNode Node;
		public readonly List<string> Names = new List<string>();
		public readonly string Description;

		public APIFunctionParamSummary(APIFunction InFunction, XmlNode InNode)
		{
			Function = InFunction;
			Node = InNode;

			foreach (XmlNode NameNode in Node.SelectNodes("parameternamelist/parametername"))
			{
				Names.Add(NameNode.InnerText);
			}

			XmlNode DescriptionNode = Node.SelectSingleNode("parameterdescription");
			Description = DescriptionNode.InnerText.TrimStart('-', ' ');
		}

		public UdnListItem GetListItem()
		{
			return new UdnListItem(String.Join(", ", Names), Description, null);
		}
	}

    public class APIFunction : APIMember
    {
		public static Dictionary<string, APIFunction> TemplateFunctions = new Dictionary<string, APIFunction>();

		public readonly DoxygenEntity Entity;
		public readonly XmlNode Node;
		public readonly APIProtection Protection;
		public readonly APIFunctionType FunctionType;

		public string BriefDescription = "";
		public string FullDescription = "";
		public string ReturnDescription = "";
		public List<string> SeeAlso = new List<string>();
		public List<string> SnippetText = null;

		public APIEventParameters EventParameters;

		public List<APIFunctionParam> Parameters = new List<APIFunctionParam>();
		public List<APIFunctionParamSummary> ParameterSummaries = new List<APIFunctionParamSummary>();
		public string ReturnType;

		public bool IsVirtual = false;
		public bool IsStatic = false;

		public string TemplateSignature;
		public bool bIsTemplateSpecialization;
		public List<APIFunction> TemplateSpecializations = new List<APIFunction>();

		public List<APIFunction> Reimplements = new List<APIFunction>();
		public List<APIFunction> ReimplementedBy = new List<APIFunction>();
		public APIHierarchyNode HierarchyNode;

		public MetadataDirective MetadataDirective;

		public string[] SourceLines;

		public List<string> Warnings = new List<string>();

		public APIFunction(APIPage InParent, DoxygenEntity InEntity, APIFunctionKey InKey)
			: this(InParent, InEntity, InKey, Utility.MakeLinkPath(InParent.LinkPath, InKey.GetLinkName()))
		{
		}

		public APIFunction(APIPage InParent, DoxygenEntity InEntity, APIFunctionKey InKey, string InLinkPath)
            : base(InParent, InEntity.Name, InLinkPath)
        {
			Entity = InEntity;
			Node = Entity.Node;

			Protection = ParseProtection(Node);
			FunctionType = InKey.Type;
			AddRefLink(Entity.Node.Attributes["id"].Value, this);

			bIsTemplateSpecialization = Name.Contains('<');
			if (Node.SelectSingleNode("templateparamlist") != null && !bIsTemplateSpecialization && !TemplateFunctions.ContainsKey(FullName))
			{
				TemplateFunctions.Add(FullName, this);
			}
		}

		public APIMember ScopeParent
		{
			get { return ((Parent is APIFunctionGroup) ? Parent.Parent : Parent) as APIMember; }
		}

		public override string FullName
		{
			get { APIFunctionGroup ParentFunctionGroup = Parent as APIFunctionGroup; return (ParentFunctionGroup != null) ? ParentFunctionGroup.FullName : base.FullName; }
		}

		public APIFunction GetBaseImplementation()
		{
			APIFunction BaseFunction = this;
			while(BaseFunction.Reimplements.Count > 0)
			{
				BaseFunction = BaseFunction.Reimplements[0];
			}
			return BaseFunction;
		}

		public bool IsDeprecated()
		{
			return Name.EndsWith("_DEPRECATED");
		}

		public bool IsExecFunction()
		{
			return Name.Length >= 5 && Name.StartsWith("exec") && Char.IsUpper(Name[4]);
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			if (FunctionType != APIFunctionType.UnaryOperator && FunctionType != APIFunctionType.BinaryOperator)
			{
				Manifest.Add(FullName, this);
			}
		}

		public override void Link()
		{
			// Get the description
			ParseBriefAndFullDescription(Node, out BriefDescription, out FullDescription);

			// Try to link autogenerated exec functions to their original implementation
			if (Utility.IsNullOrWhitespace(BriefDescription) && Utility.IsNullOrWhitespace(FullDescription) && Name.Length >= 5 && Name.StartsWith("exec") && Char.IsUpper(Name[4]))
			{
				APIRecord ParentRecord = Parent as APIRecord;
				if(ParentRecord != null)
				{
					APIMember TargetMember = ParentRecord.Children.OfType<APIFunction>().FirstOrDefault(x => x.Name == Name.Substring(4));
					if (TargetMember == null)
					{
						TargetMember = ParentRecord.Children.OfType<APIFunctionGroup>().FirstOrDefault(x => x.Name == Name.Substring(4));
					}
					if (TargetMember != null)
					{
						BriefDescription = String.Format("Wrapper for [{0}]({1}).", TargetMember.Name, TargetMember.LinkPath);
					}
				}
			}

			// Get the @see directives
			ParseSeeAlso(Node, SeeAlso);

			string RealKeyName = (Entity.Parent != null) ? (Entity.Parent.Name + "::" + Entity.Name) : Entity.Name;
			SnippetText = APISnippets.LoadSnippetTextForSymbol(RealKeyName);

			// Get the modifiers
			IsVirtual = Node.Attributes.GetNamedItem("virt").InnerText == "virtual";
			IsStatic = Node.Attributes.GetNamedItem("static").InnerText == "yes";

			// Get the metadata
			MetadataDirective = MetadataCache.FindMetadataForMember(Node, "UFUNCTION");

			// Get the template parameters
			TemplateSignature = ParseTemplateSignature(Node);

			// If it's a specialization, add it to the parent class
			if (bIsTemplateSpecialization)
			{
				int StripIdx = FullName.LastIndexOf('<');
				if (StripIdx != -1)
				{
					APIFunction OriginalTemplateFunction;
					if (TemplateFunctions.TryGetValue(FullName.Substring(0, StripIdx), out OriginalTemplateFunction))
					{
						OriginalTemplateFunction.TemplateSpecializations.Add(this);
					}
				}
			}

			// Get the parameter declaration nodes
			XmlNodeList ParameterNodes = Node.SelectNodes("param");
			foreach (XmlNode ParameterNode in ParameterNodes)
			{
				Parameters.Add(new APIFunctionParam(this, ParameterNode));
			}

			// If the only parameter is "void", don't bother including it
			if (Parameters.Count == 1 && Parameters[0].Node.SelectSingleNode("type").InnerText == "void")
			{
				Parameters.Clear();
			}

			// Get the parameter description nodes
			XmlNodeList ParameterItemNodes = Node.SelectNodes("detaileddescription/para/parameterlist/parameteritem");
			foreach (XmlNode ParameterItemNode in ParameterItemNodes)
			{
				ParameterSummaries.Add(new APIFunctionParamSummary(this, ParameterItemNode));
			}

			// Get the return type
			XmlNode ReturnTypeNode = Node.SelectSingleNode("type");
			ReturnType = RemoveElaborations(ConvertToMarkdown(ReturnTypeNode));

			// Parse the reimplements list
			using (XmlNodeList ReimplementsList = Node.SelectNodes("reimplements"))
			{
				foreach (XmlNode ReimplementsNode in ReimplementsList)
				{
					APIFunction Function = ResolveRefLink(ReimplementsNode.Attributes["refid"].Value) as APIFunction;
					if(Function != null) Reimplements.Add(Function);
				}
			}

			// Parse the reimplemented-by list
			using (XmlNodeList ReimplementedByList = Node.SelectNodes("reimplementedby"))
			{
				foreach (XmlNode ReimplementedByNode in ReimplementedByList)
				{
					APIFunction Function = ResolveRefLink(ReimplementedByNode.Attributes["refid"].Value) as APIFunction;
					if (Function != null) ReimplementedBy.Add(Function);
				}
			}

			// Parse any other notes
			XmlNodeList SimpleNodes = Node.SelectNodes("detaileddescription/para/simplesect");
			foreach (XmlNode node in SimpleNodes)
			{
				switch (node.Attributes.GetNamedItem("kind").InnerText)
				{
					case "return":
						ReturnDescription = ConvertToMarkdown(node.SelectSingleNode("para"));
						break;
					case "warning":
						Warnings.Add(ConvertToMarkdown(node.SelectSingleNode("para")).TrimStart(':'));
						break;
				}
			}

			// Parse the source lines
			/*
			XmlNode LocationNode = Node.SelectSingleNode("location");
			if(LocationNode != null)
			{
				XmlAttribute BodyFileAttribute = LocationNode.Attributes["bodyfile"];
				if(BodyFileAttribute != null)
				{
					string BodyFile = BodyFileAttribute.Value;
					if(!BodyFile.ToLowerInvariant().Split('\\', '/').Any(x => Program.ExcludeSourceDirectoriesHash.Contains(x)))
					{
						SourceFile File = SourceFileCache.Read(BodyFile);
						if (File != null)
						{
							int BodyStart = Math.Min(Math.Max(Int32.Parse(LocationNode.Attributes["bodystart"].Value) - 1, 0), File.LineOffsets.Length - 1);
							int BodyStartOffset = File.LineOffsets[BodyStart];

							int BodyEnd = Math.Min(Int32.Parse(LocationNode.Attributes["bodyend"].Value), File.LineOffsets.Length - 1);
							int BodyEndOffset = File.LineOffsets[BodyEnd];

							SourceLines = File.Text.Substring(BodyStartOffset, BodyEndOffset - BodyStartOffset).Split('\n');
						}
					}
				}
			}
			 */
		}

		public override void PostLink()
		{
			// If we're missing a description for this function, copy it from whatever we reimplement
			List<APIFunction> FunctionStack = new List<APIFunction>{ this };
			while(Utility.IsNullOrWhitespace(BriefDescription) && Utility.IsNullOrWhitespace(FullDescription) && FunctionStack.Count > 0)
			{
				// Remove the top function from the stack
				APIFunction Function = FunctionStack.Last();
				FunctionStack.RemoveAt(FunctionStack.Count - 1);
				FunctionStack.AddRange(Function.Reimplements);

				// Copy its descriptions
				BriefDescription = Function.BriefDescription;
				FullDescription = Function.FullDescription;
			}

			// Create the hierarchy
			if (Reimplements.Count > 0 || ReimplementedBy.Count > 0)
			{
				APIHierarchyNode Node = new APIHierarchyNode(FullName + Utility.EscapeText("()"), false);
				Node.AddChildren(this);
				HierarchyNode = Node.AddRoot(this);
			}
		}

		static string GetNameFromNode(XmlNode Node)
		{
			XmlNode NameNode = Node.SelectSingleNode("name");
			return NameNode.InnerText;
		}

		private void WriteSyntax(UdnWriter Writer)
		{
			List<string> Lines = new List<string>();
			if(MetadataDirective != null)
			{
				Lines.AddRange(MetadataDirective.ToMarkdown());
			}
			if (!Utility.IsNullOrWhitespace(TemplateSignature))
			{
				Lines.Add(TemplateSignature);
			}

			string Keywords = "";
			if (IsVirtual)
			{
				Keywords += "virtual ";
			}
			if (IsStatic)
			{
				Keywords += "static ";
			}

			if (Parameters.Count > 0)
			{
				Lines.Add(Keywords + ReturnType + " " + Utility.EscapeText(Name));
				Lines.Add(Utility.EscapeText("("));
				for (int Idx = 0; Idx < Parameters.Count; Idx++)
				{
					string Separator = (Idx + 1 == Parameters.Count) ? "" : ",";
					Lines.Add(UdnWriter.TabSpaces + Parameters[Idx].Definition + Separator);
				}
				Lines.Add(Utility.EscapeText(")"));
			}
			else
			{
				Lines.Add(Keywords + ReturnType + " " + Utility.EscapeText(Name + "()"));
			}
			WriteNestedSimpleCode(Writer, Lines);
		}

		private void WriteIcons(UdnWriter Writer)
		{
			Writer.WriteIcon(Icons.Function[(int)Protection]);
			if (IsStatic)
			{
				Writer.WriteIcon(Icons.StaticFunction);
			}
			if (IsVirtual)
			{
				Writer.WriteIcon(Icons.VirtualFunction);
			}
			if (MetadataDirective != null)
			{
				Writer.WriteIcon(Icons.ReflectedFunction);
				MetadataDirective.WriteIcons(Writer);
			}
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(FullName, PageCrumbs, BriefDescription);

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

				// Write the virtual hierarchy
				if (HierarchyNode != null)
				{
					Writer.EnterSection("overrides", "Override Hierarchy");
					Writer.EnterTag("[REGION:hierarchy]");
					APIHierarchy.WriteHierarchy(Writer, HierarchyNode, "hrch");
					Writer.LeaveTag("[/REGION]");
					Writer.LeaveSection();
				}

				// Write the syntax
				Writer.EnterSection("syntax", "Syntax");
				WriteSyntax(Writer);
				Writer.LeaveSection();

				// Write the description
				if (!Utility.IsNullOrWhitespace(FullDescription))
				{
					Writer.EnterSection("description", "Remarks");
					Writer.WriteLine(FullDescription);
					Writer.LeaveSection();
				}

				// Enter the body
				Writer.EnterRegion("syntax");

				// Write the return type
				if(!Utility.IsNullOrWhitespace(ReturnDescription))
				{
					Writer.EnterSection("returns", "Returns");
					Writer.WriteLine(ReturnDescription);
					Writer.LeaveSection();
				}

				// Write the parameters
				if (ParameterSummaries.Count > 0)
				{
					Writer.WriteListSection("params", "Parameters", "Parameter", "Description", ParameterSummaries.Select(x => x.GetListItem()));
				}

				// Leave the body
				Writer.LeaveRegion();

				// Write the marshalling struct
				if (EventParameters != null)
				{
					Writer.EnterSection("marshalling", "Marshalling");
					Writer.WriteLine("May be called as an event using [{0}]({1}) as parameter frame.", EventParameters.Name, EventParameters.LinkPath);
					Writer.LeaveSection();
				}

				// Write the template specializations
				if(TemplateSpecializations.Count > 0)
				{
					Writer.EnterSection("specializations", "Specializations");
					foreach (APIFunction Specialization in TemplateSpecializations)
					{
						Writer.WriteLine("[{0}]({1})  ", Specialization.Name, Specialization.LinkPath);
					}
					Writer.LeaveSection();
				}

				//Write code snippets
				WriteSnippetSection(Writer, SnippetText);

				// Write the @see directives
				WriteSeeAlsoSection(Writer, SeeAlso);

				// Write the reference info
				WriteReferencesSection(Writer, Entity);
			}
        }

		public void WriteListItem(UdnWriter Writer, bool bWithType)
		{
			// Enter the object
			Writer.EnterObject(bWithType? "FunctionListItemWithType" : "FunctionListItem");

			// Get all the icons
			List<Icon> ItemIcons = new List<Icon>{ Icons.Function[(int)Protection] };
			if (IsStatic)
			{
				ItemIcons.Add(Icons.StaticFunction);
			}
			if (IsVirtual)
			{
				ItemIcons.Add(Icons.VirtualFunction);
			}
			if (MetadataDirective != null)
			{
				ItemIcons.Add(Icons.ReflectedFunction);
				ItemIcons.AddRange(MetadataDirective.Icons);
			}
			Writer.WriteParam("icons", ItemIcons);

			// Write the return type
			Writer.WriteParam("type", Markdown.Truncate(ReturnType, 12, "..."));

			// Write the name
			if(Parameters.Count == 0)
			{
				Writer.WriteParam("name", Name + "()");
				Writer.WriteParam("arguments", "");
			}
			else
			{
				Writer.WriteParam("name", Name);
				Writer.EnterParam("arguments");
				if (Parameters.Count > 0)
				{
					Writer.WriteEscapedLine("(  ");
					for (int Idx = 0; Idx < Parameters.Count; Idx++)
					{
						string Separator = (Idx + 1 == Parameters.Count) ? "" : ",";
						string Definition = Markdown.Truncate(APIMember.RemoveElaborations(Parameters[Idx].Definition), 35, "...");
						// Fix spacing around pointer/reference punctuation to match Epic code standards.
						if (Definition != null)
						{
							Definition = Definition.Replace(" *", "*").Replace(" &", "&");
						}
						Writer.WriteLine(UdnWriter.TabSpaces + Definition + Separator + "  ");
					}
					Writer.WriteEscapedLine(")  ");
				}
				Writer.LeaveParam();
			}

			// Write the other parameters
			Writer.WriteParam("link", "[RELATIVE:" + LinkPath + "]");
			Writer.WriteParam("description", BriefDescription);

			// Leave the object
			Writer.LeaveObject();
		}
		
		public static void WriteList(UdnWriter Writer, IEnumerable<APIFunction> Functions, bool bWithType)
		{
			Writer.WriteObject(bWithType? "FunctionListHeadWithType" : "FunctionListHead");
			foreach (APIFunction Function in Functions)
			{
				Function.WriteListItem(Writer, bWithType);
			}
			Writer.WriteObject("FunctionListTail");
		}

		public static bool WriteListSection(UdnWriter Writer, string SectionId, string SectionTitle, IEnumerable<APIFunction> Functions, bool bWithType)
		{
			APIFunction[] FunctionArray = Functions.ToArray();
			if (FunctionArray.Length > 0)
			{
				Writer.EnterSection(SectionId, SectionTitle);
				WriteList(Writer, Functions, bWithType);
				Writer.LeaveSection();
				return true;
			}
			return false;
		}
    }
}
