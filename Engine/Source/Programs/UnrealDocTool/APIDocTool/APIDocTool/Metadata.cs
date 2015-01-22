// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace APIDocTool
{
	public static class MetadataLookup
	{
		static string[] ReferenceNames =
		{
			"ClassGroup",
			"CustomConstructor",
			"NoExport",
			"Placeable",
			"NotPlaceable",
			"Deprecated",
			"NonTransient",
			"Config",
			"PerObjectConfig",
			"ConfigDoNotCheckDefaults",
			"EditInlineNew",
			"NotEditInlineNew",
			"HideDropDown",
			"ShowCategories",
			"HideCategories",
			"ShowFunctions",
			"HideFunctions",
			"AutoExpandCategories",
			"AutoCollapseCategories",
			"DontAutoCollapseCategories",
			"CollapseCategories",
			"DontCollapseCategories",
			"Meta",
		};

		static Dictionary<string, string> InvariantToReferenceNames;

		public static Dictionary<string, string> ClassTags;
		public static Dictionary<string, string> EnumTags;
		public static Dictionary<string, string> InterfaceTags;
		public static Dictionary<string, string> FunctionTags;
		public static Dictionary<string, string> PropertyTags;
		public static Dictionary<string, string> StructTags;
		public static Dictionary<string, string> MetaTags;

		static MetadataLookup()
		{
			Reset();
		}

		public static void BuildReferenceNameList()
		{
			// Build a list of all the names
			List<string> AllNames = new List<string>();
			AllNames.AddRange(ReferenceNames);
			AllNames.AddRange(ClassTags.Keys);
			AllNames.AddRange(EnumTags.Keys);
			AllNames.AddRange(InterfaceTags.Keys);
			AllNames.AddRange(FunctionTags.Keys);
			AllNames.AddRange(PropertyTags.Keys);
			AllNames.AddRange(StructTags.Keys);
			AllNames.AddRange(MetaTags.Keys);

			// Add them to the reference name map
			foreach(string Name in AllNames)
			{
				string InvariantName = Name.ToLowerInvariant();
				if (!InvariantToReferenceNames.ContainsKey(InvariantName))
				{
					InvariantToReferenceNames.Add(InvariantName, Name);
				}
			}
		}

		public static string GetReferenceName(string Name)
		{
			string ReferenceName;
			if (InvariantToReferenceNames.TryGetValue(Name.ToLowerInvariant(), out ReferenceName))
			{
				return ReferenceName;
			}
			else
			{
				return Name;
			}
		}

		public static void Reset()
		{
			InvariantToReferenceNames = new Dictionary<string, string>();

			ClassTags = new Dictionary<string, string>();
			EnumTags = new Dictionary<string, string>();
			InterfaceTags = new Dictionary<string, string>();
			FunctionTags = new Dictionary<string,string>();
			PropertyTags = new Dictionary<string,string>();
			StructTags = new Dictionary<string,string>();
			MetaTags = new Dictionary<string,string>();

			BuildReferenceNameList();
		}

		public static void Load(string InputPath)
		{
			Reset();

			XmlDocument Document = Utility.ReadXmlDocument(InputPath);

			XmlNode MetadataNode = Document.SelectSingleNode("metadata");
			ClassTags = LoadEntries(MetadataNode, "classtags", "classtag");
			EnumTags = LoadEntries(MetadataNode, "enumtags", "enumtag");
			InterfaceTags = LoadEntries(MetadataNode, "interfacetags", "interfacetag");
			FunctionTags = LoadEntries(MetadataNode, "functiontags", "functiontag");
			PropertyTags = LoadEntries(MetadataNode, "propertytags", "propertytag");
			StructTags = LoadEntries(MetadataNode, "structtags", "structtag");
			MetaTags = LoadEntries(MetadataNode, "metatags", "metatag");

			BuildReferenceNameList();
		}

		static Dictionary<string, string> LoadEntries(XmlNode BaseNode, string GroupName, string ItemName)
		{
			Dictionary<string, string> Entries = new Dictionary<string, string>();
			using (XmlNodeList NodeList = BaseNode.SelectNodes(GroupName + "/" + ItemName))
			{
				foreach (XmlNode Node in NodeList)
				{
					string Name = Node.Attributes["name"].Value;

					string Description = Node.Attributes["description"].Value;
					Entries.Add(GetReferenceName(Name), Description);
				}
			}
			return Entries;
		}

		public static void Save(string OutputPath)
		{
			// Create a formatting object for the index
			XmlWriterSettings Settings = new XmlWriterSettings();
			Settings.Indent = true;

			// Write an index for the parsed modules
			using (XmlWriter Writer = XmlWriter.Create(OutputPath, Settings))
			{
				Writer.WriteStartElement("metadata");
				SaveEntries(Writer, "classtags", "classtag", ClassTags);
				SaveEntries(Writer, "enumtags", "enumtag", EnumTags);
				SaveEntries(Writer, "interfacetags", "interfacetag", InterfaceTags);
				SaveEntries(Writer, "functiontags", "functiontag", FunctionTags);
				SaveEntries(Writer, "propertytags", "propertytag", PropertyTags);
				SaveEntries(Writer, "structtags", "structtag", StructTags);
				SaveEntries(Writer, "metatags", "metatag", MetaTags);
				Writer.WriteEndElement();
			}
		}

		static void SaveEntries(XmlWriter Writer, string GroupName, string ItemName, Dictionary<string, string> Entries)
		{
			Writer.WriteStartElement(GroupName);
			foreach(KeyValuePair<string, string> Entry in Entries)
			{
				Writer.WriteStartElement(ItemName);
				Writer.WriteAttributeString("name", Entry.Key);
				Writer.WriteAttributeString("description", Entry.Value);
				Writer.WriteEndElement();
			}
			Writer.WriteEndElement();
		}
	}

	public class MetadataNode
	{
		public readonly SourceToken Tag;
		public readonly Nullable<SourceToken> TokenValue;
		public readonly List<MetadataNode> ListValue;

		public MetadataNode(SourceToken InTag)
		{
			Tag = InTag;
		}

		public MetadataNode(SourceToken InTag, SourceToken InTokenValue)
		{
			Tag = InTag;
			TokenValue = InTokenValue;
		}

		public MetadataNode(SourceToken InTag, List<MetadataNode> InListValue)
		{
			Tag = InTag;
			ListValue = InListValue;
		}

		public string TagText
		{
			get { return MetadataLookup.GetReferenceName(Tag.ToString()); }
		}

		public string ValueText
		{
			get { return TokenValue.HasValue? MetadataLookup.GetReferenceName(TokenValue.ToString()) : (ListValue != null)? ("(" + String.Join(", ", ListValue) + ")") : null; }
		}

		public override string ToString()
		{
			string Result = TagText;

			string Value = ValueText;
			if(Value != null) Result += "=" + Value;
			return Result;
		}
	}

	public class MetadataListItem
	{
		public Icon Icon;
		public string Name;
		public List<string> Values = new List<string>();
		public string Description;
	}

	public class MetadataKeyword
	{
		public string Url;
		public Dictionary<string, string> NodeUrls = new Dictionary<string,string>();
	}

	public class MetadataDirective
	{
		public static Dictionary<string, Dictionary<string, int>> AllTags = new Dictionary<string, Dictionary<string, int>>();
		public static Dictionary<string, MetadataKeyword> AllKeywords = new Dictionary<string, MetadataKeyword>();

		public readonly SourceFile File;
		public readonly string Type;
		public readonly int Index;
		public readonly int EndIndex;
		public readonly int DefinitionIndex;
		public readonly List<MetadataNode> NodeList;

		public MetadataDirective(SourceFile InFile, int InIndex, int InEndIndex, int InDefinitionIndex, List<MetadataNode> InNodeList)
		{
			File = InFile;
			Index = InIndex;
			EndIndex = InEndIndex;
			DefinitionIndex = InDefinitionIndex;
			NodeList = InNodeList;

			Type = File.Tokens[Index].ToString();

			Dictionary<string, int> TagsForType;
			if(!AllTags.TryGetValue(Type, out TagsForType))
			{
				TagsForType = new Dictionary<string, int>();
				AllTags.Add(Type, TagsForType);
			}

			foreach(MetadataNode Node in NodeList)
			{
				int Count = 0;
				if(TagsForType.TryGetValue(Node.TagText, out Count))
				{
					Count++;
				}
				TagsForType[Node.TagText] = Count;
			}
		}

		public IEnumerable<string> ToMarkdown()
		{
			MetadataKeyword Keyword;
			if(AllKeywords.TryGetValue(Type, out Keyword))
			{
				StringBuilder Output = new StringBuilder();
				Output.AppendFormat("[{0}]({1})", Type, Keyword.Url);
				Output.Append(Utility.EscapeText("("));

				int Indent = Type.Length + 1;
				int HangingIndent = Indent;

				List<string> Lines = new List<string>();
				for(int Idx = 0; Idx < NodeList.Count; Idx++)
				{
					string NodeText = NodeList[Idx].ToString();

					if (Indent + NodeText.Length + 3 > 100)
					{
						yield return Output.ToString();
						Output.Clear();
						Output.Append(String.Concat(Enumerable.Repeat("&nbsp;", HangingIndent)));
						Indent = HangingIndent;
					}

					string NodeUrl;
					if(Keyword.NodeUrls.TryGetValue(NodeList[Idx].TagText, out NodeUrl))
					{
						Output.AppendFormat("[{0}]({1})", Utility.EscapeText(NodeText), NodeUrl);
					}
					else
					{
						Output.Append(Utility.EscapeText(NodeText));
					}
					Indent += NodeText.Length;

					if(Idx + 1 < NodeList.Count)
					{
						Output.Append(", ");
						Indent += 2;
					}
				}
				Output.Append(Utility.EscapeText(")"));
				yield return Output.ToString();
			}
			else
			{
				yield return Utility.EscapeText(ToString());
			}
		}

		public IEnumerable<Icon> Icons
		{
			get { return APIDocTool.Icons.GetMetadataIcons(Type, NodeList.Select(x => x.TagText)); }
		}

		public void WriteIcons(UdnWriter Writer)
		{
			List<Icon> IconList = APIDocTool.Icons.GetMetadataIcons(Type, NodeList.Select(x => x.TagText));
			foreach (Icon Icon in IconList)
			{
				Writer.WriteIcon(Icon);
			}
		}

		public static void CreateListItems(List<MetadataNode> Nodes, MetadataIcon[] TypeIcons, Dictionary<string, string> Descriptions, bool bMetaPrefix, List<MetadataListItem> ListItems)
		{
			foreach (MetadataNode Node in Nodes)
			{
				string TagName = Node.TagText;
				if (String.Compare(TagName, "Meta", true) == 0 && Node.ListValue != null)
				{
					CreateListItems(Node.ListValue, APIDocTool.Icons.MetaMetadataIcons, MetadataLookup.MetaTags, true, ListItems);
				}
				else
				{
					// Get the list name
					string ListName = bMetaPrefix ? ("Meta=(" + TagName + ")") : TagName;

					// Find or create a list item
					MetadataListItem ListItem = ListItems.FirstOrDefault(x => x.Name == ListName);
					if (ListItem == null)
					{
						ListItem = new MetadataListItem();
						ListItem.Icon = APIDocTool.Icons.GetMetadataIcon(TypeIcons, Node.TagText);
						ListItem.Name = ListName;
						Descriptions.TryGetValue(Node.TagText, out ListItem.Description);
						ListItems.Add(ListItem);
					}

					// Add all the values
					if(Node.TokenValue.HasValue)
					{
						ListItem.Values.Add(MetadataLookup.GetReferenceName(Node.TokenValue.ToString()));
					}
					if(Node.ListValue != null)
					{
						ListItem.Values.AddRange(Node.ListValue.Select(x => x.ToString()));
					}
				}
			}
		}

		public void WriteListSection(UdnWriter Writer, string SectionId, string SectionTitle, Dictionary<string, string> Descriptions)
		{
			if (NodeList.Count > 0)
			{
				// Create the list items
				List<MetadataListItem> ListItems = new List<MetadataListItem>();
				CreateListItems(NodeList, APIDocTool.Icons.GetMetadataIconsForType(Type), Descriptions, false, ListItems);

				// Enter the section and list
				Writer.EnterSection(SectionId, SectionTitle);
				Writer.WriteObject("MetadataListHead");

				// Write the items
				foreach (MetadataListItem ListItem in ListItems)
				{
					Writer.EnterObject("MetadataListItem");
					Writer.WriteParam("icon", ListItem.Icon);
					Writer.WriteParam("name", ListItem.Name);
					Writer.EnterParam("value");
					for (int Idx = 0; Idx < ListItem.Values.Count; Idx++)
					{
						Writer.WriteLine("{0}{1}  ", ListItem.Values[Idx], (Idx + 1 < ListItem.Values.Count) ? ", " : "");
					}
					Writer.LeaveParam();
					Writer.WriteParam("desc", ListItem.Description);
					Writer.LeaveObject();
				}

				// Leave the section and list
				Writer.WriteObject("MetadataListTail");
				Writer.LeaveSection();
			}
		}

		public override string ToString()
		{
			return Type + "(" + String.Join(", ", NodeList) + ")";
		}
	}

	public class MetadataFile
	{
		public SourceFile SourceFile;
		public List<MetadataDirective> Directives = new List<MetadataDirective>();

		public MetadataFile(SourceFile InSourceFile)
		{
			// Set the file
			SourceFile = InSourceFile;

			// Parse the directives
			SourceToken[] Tokens = SourceFile.Tokens;
			for (int Line = 0; Line + 1 < SourceFile.LineToToken.Length; Line++)
			{
				int Index = SourceFile.LineToToken[Line];
				if (Index != SourceFile.LineToToken[Line + 1])
				{
					MetadataDirective Directive = TryParseDirective(SourceFile, Tokens, Index);
					if(Directive != null)
					{
						Directives.Add(Directive);
					}
				}
			}
		}

		public MetadataDirective FindDirective(int Index)
		{
			foreach(MetadataDirective Directive in Directives)
			{
				if(Index >= Directive.Index && Index <= Directive.DefinitionIndex)
				{
					return Directive;
				}
			}
			return null;
		}

		public static MetadataDirective TryParseDirective(SourceFile File, SourceToken[] Tokens, int Index)
		{
			string TagText = Tokens[Index].Text;
			if(TagText == "UCLASS" || TagText == "UPROPERTY" || TagText == "UENUM" || TagText == "USTRUCT" || TagText == "UFUNCTION")
			{
				int DirectiveIndex = Index++;

				List<MetadataNode> NodeList = TryParseNodeList(Tokens, ref Index);
				if(NodeList != null)
				{
					// Save the end of the metadata, and the start of the definition
					int DirectiveEndIndex = Index;

					// Depending on the type, parse a definition
					int DefinitionIndex = Index;
					if(TagText == "UCLASS" || TagText == "USTRUCT" || TagText == "UENUM")
					{
						for(; Index < Tokens.Length && Tokens[Index].Text != ";"; Index++)
						{
							if(Tokens[Index].Text == "{")
							{
								DefinitionIndex = Index + 1;
								break;
							}
						}
					}

					// Create the definition
					return new MetadataDirective(File, DirectiveIndex, DirectiveEndIndex, DefinitionIndex, NodeList);
				}
			}
			return null;
		}

		public static MetadataNode TryParseNode(SourceToken[] Tokens, ref int Index)
		{
			MetadataNode Node = null;
			if(Tokens[Index].IsIdentifier)
			{
				int NewIndex = Index + 1;

				// Check whether it has a value
				if (Tokens[NewIndex].Text == "=" || Tokens[NewIndex].Text == "(")
				{
					if(Tokens[NewIndex].Text == "=") NewIndex++;

					List<MetadataNode> NodeList = TryParseNodeList(Tokens, ref NewIndex);
					if (NodeList != null)
					{
						Node = new MetadataNode(Tokens[Index], NodeList);
						Index = NewIndex;
					}
					else if(Tokens[NewIndex].IsIdentifier || Tokens[NewIndex].IsLiteral)
					{
						Node = new MetadataNode(Tokens[Index], Tokens[NewIndex]);
						Index = NewIndex + 1;
					}
				}
				else
				{
					Node = new MetadataNode(Tokens[Index]);
					Index = NewIndex;
				}
			}
			else if (Tokens[Index].IsLiteral)
			{
				Node = new MetadataNode(Tokens[Index]);
				Index++;
			}
			return Node;
		}

		public static List<MetadataNode> TryParseNodeList(SourceToken[] Tokens, ref int Index)
		{
			List<MetadataNode> Nodes = null;
			if(Tokens[Index].Text == "(")
			{
				Nodes = new List<MetadataNode>();

				int NewIndex = Index + 1;
				if (Tokens[NewIndex].Text != ")")
				{
					for (; ; )
					{
						// Read the next node
						MetadataNode Node = TryParseNode(Tokens, ref NewIndex);
						if (Node == null) throw new NotImplementedException();

						// Append it to the list
						Nodes.Add(Node);

						// Read the next delimiting comma
						if (Tokens[NewIndex].Text == ")") break;
						if (Tokens[NewIndex].Text != ",") throw new NotImplementedException();
						NewIndex++;
					}
				}
				Index = NewIndex + 1;
			}
			return Nodes;
		}
	}
	
	public static class MetadataCache
	{
		static Dictionary<SourceFile, MetadataFile> FileMap = new Dictionary<SourceFile,MetadataFile>();

		public static MetadataFile Read(string Path)
		{
			SourceFile File = SourceFileCache.Read(Path);
			return (File == null)? null : Read(File);
		}

		public static MetadataFile Read(SourceFile File)
		{
			MetadataFile Result;
			if(!FileMap.TryGetValue(File, out Result))
			{
				Result = new MetadataFile(File); 
				FileMap.Add(File, Result);
			}
			return Result;
		}

		public static MetadataDirective FindMetadataForMember(XmlNode Node)
		{
			XmlNode LocationNode = Node.SelectSingleNode("location");

			// Get the location of the definition
			string Path = LocationNode.Attributes.GetNamedItem("file").InnerText;
			int Line = int.Parse(LocationNode.Attributes.GetNamedItem("line").InnerText);
			int Column = int.Parse(LocationNode.Attributes.GetNamedItem("column").InnerText);

			// Try to get the token index at this position
			MetadataFile File = Read(Path);
			if (File != null)
			{
				int Index = File.SourceFile.GetTokenAt(Line, Column);
				return File.FindDirective(Index);
			}
			return null;
		}

		public static MetadataDirective FindMetadataForMember(XmlNode Node, string Type)
		{
			MetadataDirective Directive = FindMetadataForMember(Node);
			if (Directive != null && Directive.Type == Type)
			{
				return Directive;
			}
			return null;
		}
	}
}
