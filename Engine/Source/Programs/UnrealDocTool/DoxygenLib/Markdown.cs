// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace DoxygenLib
{
	public class MarkdownWriter
	{
		StringBuilder Buffer = new StringBuilder();
		bool bWantNewParagraph;
		bool bIsNewLine;
		string Indent;
		Stack<string> IndentStack = new Stack<string>();

		public MarkdownWriter(string InIndent)
		{
			Indent = InIndent;
		}

		public void AddIndent(string ExtraIndent)
		{
			IndentStack.Push(Indent);
			Indent += ExtraIndent;
		}

		public void RemoveIndent()
		{
			Indent = IndentStack.Pop();
		}

		public void WriteParagraph()
		{
			bWantNewParagraph = true;
		}

		public void Write(string Text)
		{
			FlushParagraph();
			if(bIsNewLine)
			{
				Buffer.Append(Indent);
				bIsNewLine = false;
			}
			Buffer.Append(Text);
		}

		public void WriteFormat(string Format, params object[] Args)
		{
			FlushParagraph();
			if (bIsNewLine)
			{
				Buffer.Append(Indent);
				bIsNewLine = false;
			}
			Buffer.AppendFormat(Format, Args);
		}

		public void WriteLine()
		{
			if (!bIsNewLine)
			{
				Buffer.AppendLine();
				bIsNewLine = true;
			}
		}

		public void WriteLink(string LinkText, string LinkUrl)
		{
			WriteFormat("[{0}]({1})", LinkText, LinkUrl);
		}

		void FlushParagraph()
		{
			if (bWantNewParagraph)
			{
				if (!bIsNewLine)
				{
					Buffer.AppendLine();
					bIsNewLine = true;
				}
				Buffer.AppendLine();
				bWantNewParagraph = false;
			}
		}

		public override string ToString()
		{
			return Buffer.ToString();
		}
	}

	public static class Markdown
	{
		const bool bDocToolCanParseExternalLinks = true;

		public delegate string ResolveLinkDelegate(string Id);

		/** 
		 * Parses markdown text out of an XML node 
		 * 
		 * @param child			xml node we want to parse into Markdown 
		 * @param Indent		indenting prefix for each newline of text in the converted markdown
		 * @param ResolveLink	delegate to call to resolve doxygen id's into link paths. may be null.
		 * @return				a string containing the text from the child node 
		 */
		public static string ParseXml(XmlNode Node, string Indent, ResolveLinkDelegate ResolveLink)
		{
			MarkdownWriter Writer = new MarkdownWriter(Indent);
			ConvertNodeContents(Node, Writer, ResolveLink);
			return Writer.ToString();
		}

		/** 
		 * Parses markdown text out of an XML node 
		 * 
		 * @param child			xml node we want to parse into Markdown 
		 * @param Indent		indenting prefix for each newline of text in the converted markdown
		 * @param Output		storage for the output string
		 * @param ResolveLink	delegate to call to resolve doxygen id's into link paths. may be null.
		 */
		static void ConvertNode(XmlNode Node, MarkdownWriter Writer, ResolveLinkDelegate ResolveLink)
		{
			switch (Node.Name)
			{
				case "ulink":
					string LinkText = Node.InnerText;
					string LinkUrl = Node.Attributes.GetNamedItem("url").InnerText;

					const string WebPrefix = "docs.unrealengine.com/latest/INT/";
					int WebPrefixIdx = LinkUrl.IndexOf(WebPrefix);
					if (WebPrefixIdx != -1 && LinkText.Contains("docs.unrealengine.com"))
					{
						LinkText = "";
						LinkUrl = LinkUrl.Substring(WebPrefixIdx + WebPrefix.Length);

						int AnchorIdx = LinkUrl.LastIndexOf('#');
						if(AnchorIdx == -1)
						{
							LinkUrl = RemoveDefaultPageName(LinkUrl);
						}
						else
						{
							LinkUrl = RemoveDefaultPageName(LinkUrl.Substring(0, AnchorIdx)) + LinkUrl.Substring(AnchorIdx);
						}
					}
					Writer.WriteLink(LinkText, LinkUrl);
					break;
				case "ref":
					XmlAttribute RefAttribute = Node.Attributes["refid"];
					if (RefAttribute != null && ResolveLink != null)
					{
						string LinkPath = ResolveLink(RefAttribute.Value);
						if (LinkPath != null)
						{
							Writer.WriteLink(Node.InnerText, LinkPath);
							break;
						}
					}
					Writer.Write(Node.InnerText);
					break;
				case "bold":
					Writer.Write("**");
					ConvertNodeContents(Node, Writer, ResolveLink);
					Writer.Write("**");
					break;
				case "emphasis":
					Writer.Write("_");
					ConvertNodeContents(Node, Writer, ResolveLink);
					Writer.Write("_");
					break;
				case "computeroutput":
					Writer.Write("`");
					ConvertNodeContents(Node, Writer, ResolveLink);
					Writer.Write("_");
					break;
				case "parameterlist":
					break;
				case "parameteritem":
					break;
				case "parameternamelist":
					break;
				case "parameterdescription":
					ConvertNodeContents(Node, Writer, ResolveLink);
					break;
				case "xrefsect":
					break;
				case "simplesect":
					XmlAttribute KindAttribute = Node.Attributes["kind"];
					if (KindAttribute == null || KindAttribute.Value != "see")
					{
						ConvertNodeContents(Node, Writer, ResolveLink);
					}
					break;
				case "para":
					Writer.WriteParagraph();
					ConvertNodeContents(Node, Writer, ResolveLink);
					break;
				case "itemizedlist":
					Writer.WriteLine();
					foreach (XmlNode ListItemNode in Node.SelectNodes("listitem"))
					{
						XmlNodeList ParaNodes = ListItemNode.SelectNodes("para");
						if (ParaNodes.Count > 0)
						{
							// Write the first node
							Writer.Write("* ");
							Writer.AddIndent(" ");
							ConvertNodeContents(ParaNodes[0], Writer, ResolveLink);

							// Write anything else as an actual paragraph
							for (int Idx = 1; Idx < ParaNodes.Count; Idx++)
							{
								Writer.WriteLine();
								ConvertNodeContents(ParaNodes[Idx], Writer, ResolveLink);
							}

							// Finish the line
							Writer.RemoveIndent();
							Writer.WriteLine();
						}
					}
					break;
				default:
					ConvertNodeContents(Node, Writer, ResolveLink);
					break;
			}
		}

		/** 
		 * Removes the default page name from a URL (index.html)
		 * 
		 * @param LinkUrl			URL for the page
		 * @return URL with the page name removed
		 */
		static string RemoveDefaultPageName(string LinkUrl)
		{
			string DefaultPageName = "/index.html";
			if (LinkUrl.EndsWith(DefaultPageName))
			{
				return LinkUrl.Substring(0, LinkUrl.Length - DefaultPageName.Length + 1);
			}
			return LinkUrl;
		}

		/** 
		 * Converts all children of the given XML node
		 * 
		 * @param Node			Xml node we want to parse all children into Markdown 
		 * @param Writer		Output for markdown text
		 * @param ResolveLink	Delegate to call to resolve doxygen id's into link paths. may be null.
		 */
		static void ConvertNodeContents(XmlNode Node, MarkdownWriter Writer, ResolveLinkDelegate ResolveLink)
		{
			if (Node.HasChildNodes)
			{
				foreach (XmlNode ChildNode in Node.ChildNodes)
				{
					ConvertNode(ChildNode, Writer, ResolveLink);
				}
			}
			else
			{
				Writer.Write(EscapeXmlText(Node.InnerText));
			}
		}

		/** 
		 * Parses Markdown from a Doxygen code node
		 * 
		 * @param Node			Xml node we want to parse into Markdown 
		 * @param ResolveLink	Delegate to call to resolve doxygen id's into link paths. may be null.
		 * @return				Converted text
		 */
		public static string ParseXmlCodeLine(XmlNode Node, ResolveLinkDelegate ResolveLink)
		{
			StringBuilder Builder = new StringBuilder();
			ParseXmlCodeLine(Node, Builder, ResolveLink);
			return Builder.ToString();
		}

		/** 
		 * Parses Markdown from a Doxygen code node
		 * 
		 * @param Node			Xml node we want to parse into Markdown 
		 * @param Output		StringBuilder to receive the output
		 * @param ResolveLink	Delegate to call to resolve doxygen id's into link paths. may be null.
		 */
		static void ParseXmlCodeLine(XmlNode Node, StringBuilder Output, ResolveLinkDelegate ResolveLink)
		{
			switch(Node.Name)
			{
				case "sp":
					Output.Append(' ');
					return;
				case "ref":
					XmlAttribute RefAttribute = Node.Attributes["refid"];

					string LinkPath = (RefAttribute != null && ResolveLink != null)? ResolveLink(RefAttribute.Value) : null;
					if(LinkPath != null)
					{
						Output.Append("[");
					}

					ParseXmlCodeLineChildren(Node, Output, ResolveLink);

					if(LinkPath != null)
					{
						Output.AppendFormat("]({0})", LinkPath);
					}
					break;
				case "#text":
					Output.Append(EscapeText(Node.InnerText));
					break;
				default:
					ParseXmlCodeLineChildren(Node, Output, ResolveLink);
					break;
			}
		}

		/** 
		 * Parses Markdown from the children of a Doxygen code node
		 * 
		 * @param Node			Xml node we want to parse into Markdown 
		 * @param Output		StringBuilder to receive the output
		 * @param ResolveLink	Delegate to call to resolve doxygen id's into link paths. may be null.
		 */
		static void ParseXmlCodeLineChildren(XmlNode Node, StringBuilder Output, ResolveLinkDelegate ResolveLink)
		{
			foreach(XmlNode ChildNode in Node.ChildNodes)
			{
				ParseXmlCodeLine(ChildNode, Output, ResolveLink);
			}
		}

		/** 
		 * Escape a string to be valid markdown.
		 * 
		 * @param Text			String to escape
		 * @return				Escaped string
		 */
		public static string EscapeText(string Text)
		{
			StringBuilder Result = new StringBuilder();
			for (int Idx = 0; Idx < Text.Length; Idx++)
			{
				if (Text[Idx] == '&')
				{
					Result.Append("&amp;");
				}
				else if (Text[Idx] == '<')
				{
					Result.Append("&lt;");
				}
				else if (Text[Idx] == '>')
				{
					Result.Append("&gt;");
				}
				else if (Text[Idx] == '"')
				{
					Result.Append("&quot;");
				}
				else if (Text[Idx] == '\t')
				{
					Result.Append("    ");
				}
				else if(Char.IsLetterOrDigit(Text[Idx]) || Text[Idx] == '_' || Text[Idx] == ' ' || Text[Idx] == ',' || Text[Idx] == '.')
				{
					Result.Append(Text[Idx]);
				}
				else
				{
					Result.AppendFormat("&#{0:00};", (int)Text[Idx]);
				}
			}
			return Result.ToString();
		}

		/** 
		 * Escape xml text to markdown text.
		 * 
		 * @param Text			String to escape
		 * @return				Escaped string
		 */
		public static string EscapeXmlText(string Text)
		{
			StringBuilder Result = new StringBuilder();
			for (int Idx = 0; Idx < Text.Length; Idx++)
			{
				if (Text[Idx] == '!')
				{
					Result.Append("&#33;");
				}
				else if(Text[Idx] == '*')
				{
					Result.Append("&#42;");
				}
				else
				{
					Result.Append(Text[Idx]);
				}
			}
			return Result.ToString();
		}

		/** 
		 * Truncates a line to a given width, respecting embedded links.
         * 
		 * @param Text			Text to truncate
         * @param MaxLength     Maximum number of characters in the output
         * @param Suffix        Suffix to append if the string is truncated
         * @return				Truncated string
		 */
        public static string Truncate(string Text, int MaxLength, string Suffix)
        {
            StringBuilder Output = new StringBuilder();
            
            int RemainingCharacters = MaxLength;
            for (int Idx = 0; Idx < Text.Length; )
            {
                int NextIdx = TruncateEscapedCharacter(Text, Idx, ref RemainingCharacters, Output);
                if(NextIdx == -1)
                {
                    NextIdx = TruncateLink(Text, Idx, ref RemainingCharacters, Output);
                    if(NextIdx == -1)
                    {
                        NextIdx = Idx + 1;
                        if(RemainingCharacters > 0)
                        {
                            Output.Append(Text[Idx]);
                        }
                        RemainingCharacters--;
                    }
                }
                Idx = NextIdx;
            }
            if(RemainingCharacters < 0)
            {
                Output.Append(Suffix);
            }

            return Output.ToString();
        }

        private static int TruncateEscapedCharacter(string Text, int Idx, ref int RemainingCharacters, StringBuilder Output)
        {
            // Check it starts with an ampersand
            if (Text[Idx] == '&')
            {
                // Skip past any sequence of letters
                int NewIdx = Idx + 1;
                while (NewIdx < Text.Length && Char.IsLetter(Text[NewIdx]))
                {
                    NewIdx++;
                }

                // Check the whole thing is valid
                if (NewIdx != Idx + 1 && NewIdx < Text.Length && Text[NewIdx] == ';')
                {
                    // Append the markdown
                    if (RemainingCharacters > 0)
                    {
                        Output.Append(Text.Substring(Idx, NewIdx + 1 - Idx));
                    }

                    // Move to the next character
                    RemainingCharacters--;
                    return NewIdx + 1;
                }
            }
            return -1;
        }

        private static int TruncateLink(string Text, int Idx, ref int RemainingCharacters, StringBuilder Output)
        {
            // Check if starts with a bracket
            if(Text[Idx] == '[')
            {
                // Find the end of the description
                int LabelStart = Idx + 1;
                int LabelEnd = Text.IndexOf(']', LabelStart);
                if(LabelEnd != -1 && LabelEnd + 1 < Text.Length && Text[LabelEnd + 1] == '(')
                {
                    // Find the end of the link
                    int LinkStart = LabelEnd + 2;
                    int LinkEnd = Text.IndexOf(')', LinkStart);
                    if(LinkEnd != -1)
                    {
                        if(RemainingCharacters > 0)
                        {
                            Output.AppendFormat("[{0}]({1})", Text.Substring(LabelStart, Math.Min(LabelEnd - LabelStart, RemainingCharacters)), Text.Substring(LinkStart, LinkEnd - LinkStart));
                        }
                        RemainingCharacters -= LabelEnd - LabelStart;
                        return LinkEnd + 1;
                    }
                }
            }
            return -1;
        }

		public static int MeasureText(string Text)
		{
			int Length = 0;
			for(int Idx = 0; Idx < Text.Length; Idx = SkipCharacter(Text, Idx))
			{
				Length++;
			}
			return Length;
		}

		public static int SkipCharacter(string Text, int Idx)
		{
			if(Text[Idx] == '&')
			{
				// Try to read a sequence of characters naming the entity
				int EndIdx = Idx + 1;
				while(EndIdx < Text.Length && Char.IsLetter(Text[EndIdx]))
				{
					EndIdx++;
				}

				// Check it finishes with a semicolon
				if(EndIdx < Text.Length && Text[EndIdx] == ';')
				{
					return EndIdx + 1;
				}
			}
			else if(Text[Idx] == '[')
			{
				// Find the end of the link
				int EndIdx = Text.IndexOf(']', Idx + 1);
				if(EndIdx != -1 && EndIdx + 1 < Text.Length && Text[EndIdx + 1] == '(')
				{
					EndIdx = Text.IndexOf(')', EndIdx + 1);
					if(EndIdx != -1)
					{
						return EndIdx + 1;
					}
				}
			}
			return Idx + 1;
		}
	}
}
