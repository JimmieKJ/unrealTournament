// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	public class SitemapNode
	{
		public string Name;
		public string Local;
		public string ImageType = null;
		public List<SitemapNode> Children = new List<SitemapNode>();

		public SitemapNode(string InName, string InLocal)
		{
			Name = InName;
			Local = InLocal;
		}

		public void Write(StreamWriter Writer, string Indent)
		{
			// Write this item
			Writer.WriteLine(Indent + "<LI> <OBJECT type=\"text/sitemap\">");
			WriteParam(Writer, Indent + "      ", "name", Name);
			WriteParam(Writer, Indent + "      ", "local", Local);
			WriteParam(Writer, Indent + "      ", "imagetype", ImageType);
			Writer.WriteLine(Indent + "     </OBJECT>");

			// Write the children
			WriteList(Writer, Children, Indent);
		}

		public static void WriteList(StreamWriter Writer, List<SitemapNode> Nodes, string Indent)
		{
			if (Nodes.Count > 0)
			{
				Writer.WriteLine(Indent + "<UL>");
				foreach (SitemapNode Node in Nodes)
				{
					Node.Write(Writer, Indent + "  ");
				}
				Writer.WriteLine(Indent + "</UL>");
			}
		}

		private void WriteParam(StreamWriter Writer, string Indent, string Name, string Value)
		{
			if (Value != null)
			{
				Writer.WriteLine(Indent + "    <param name=\"{0}\" value=\"{1}\">", Name, Value);
			}
		}
	};

	public static class Sitemap
	{
		public static void WriteContentsFile(string OutputPath, List<SitemapNode> Nodes)
		{
			using (StreamWriter Writer = new StreamWriter(OutputPath))
			{
				WriteSitemapHeader(Writer);
				Writer.WriteLine("<OBJECT type=\"text/site properties\">");
				Writer.WriteLine("\t<param name=\"ImageType\" value=\"Folder\">");
				Writer.WriteLine("</OBJECT>");
				SitemapNode.WriteList(Writer, Nodes, "");
				WriteSitemapFooter(Writer);
			}
		}

		public static void WriteIndexFile(string OutputPath, List<SitemapNode> Nodes)
		{
			using (StreamWriter Writer = new StreamWriter(OutputPath))
			{
				WriteSitemapHeader(Writer);
				SitemapNode.WriteList(Writer, Nodes, "");
				WriteSitemapFooter(Writer);
			}
		}

		static void WriteSitemapHeader(StreamWriter Writer)
		{
			Writer.WriteLine("<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">");
			Writer.WriteLine("<HTML>");
			Writer.WriteLine("<HEAD>");
			Writer.WriteLine("<meta name=\"GENERATOR\" content=\"Microsoft&reg; HTML Help Workshop 4.1\">");
			Writer.WriteLine("<!-- Sitemap 1.0 -->");
			Writer.WriteLine("</HEAD><BODY>");
		}

		static void WriteSitemapFooter(StreamWriter Writer)
		{
			Writer.WriteLine("</BODY></HTML>");
		}
	}
}
