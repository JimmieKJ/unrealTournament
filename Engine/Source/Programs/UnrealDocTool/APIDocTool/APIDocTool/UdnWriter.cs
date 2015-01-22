// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace APIDocTool
{
	public interface IUdnListItem
	{
		void WriteUdnListItem(UdnWriter Writer);
	}

	public struct UdnListItem
	{
		public string Name;
		public string Description;
		public string Link;

		public UdnListItem(string InName, string InDescription, string InLink)
		{
			Name = InName;
			Description = InDescription;
			Link = InLink;
		}
	}

	public struct UdnIconListItem
	{
		public List<Icon> Icons;
		public string Name;
		public string Description;
		public string Link;

		public UdnIconListItem(IEnumerable<Icon> InIcons, string InName, string InDescription, string InLink)
		{
			Icons = new List<Icon>(InIcons);
			Name = InName;
			Description = InDescription;
			Link = InLink;
		}
	}

	public struct UdnFilterListItem
	{
		public string Name;
		public string Link;

		public UdnFilterListItem(string InName, string InLink)
		{
			Name = InName;
			Link = InLink;
		}
	}

	public class UdnWriter : IDisposable
	{
		public const string Space = "&nbsp;";
		public const string TabSpaces = "&nbsp;&nbsp;&nbsp;&nbsp;";

		private bool IsNewLine = true;
		private FileStream UnderlyingStream;
		private StringWriter UnderlyingWriter;
		private byte[] Data;

		public UdnWriter(string Path)
		{
			// Open a file stream and buffer the current data
			UnderlyingStream = new FileStream(Path, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
			Data = new byte[UnderlyingStream.Length];
			if(UnderlyingStream.Read(Data, 0, Data.Length) != Data.Length) throw new InvalidOperationException();

			// Create a new StringWriter to build the output
			UnderlyingWriter = new StringWriter();
		}

		public void Flush()
		{
			byte[] NewData = System.Text.Encoding.UTF8.GetBytes(UnderlyingWriter.ToString());
			if(!Enumerable.SequenceEqual(Data, NewData))
			{
				UnderlyingStream.SetLength(NewData.Length);
				UnderlyingStream.Seek(0, SeekOrigin.Begin);
				UnderlyingStream.Write(NewData, 0, NewData.Length);
				Data = NewData;
			}
		}

		public void Dispose()
		{
			Flush();
			UnderlyingStream.Dispose();
		}

		public int Indent
		{
			get; set;
		}

		public void Write(char Character)
		{
			WriteIndent();
			UnderlyingWriter.Write(Character);
		}

		public void Write(string Text)
		{
			WriteIndent();
			UnderlyingWriter.Write(Text);
		}

		public void Write(string Format, params object[] Args)
		{
			WriteIndent();
			UnderlyingWriter.Write(Format, Args);
		}

		public void Write(UdnManifest Manifest, string Text)
		{
			Write(Manifest.FormatString(Text));
		}

		public void WriteLine()
		{
			UnderlyingWriter.WriteLine();
			IsNewLine = true;
		}

		public void WriteLine(string Text)
		{
			Write(Text);
			WriteLine();
		}

		public void WriteLine(string Format, params object[] Args)
		{
			Write(Format, Args);
			WriteLine();
		}

		public void WriteLine(UdnManifest Manifest, string Text)
		{
			Write(Manifest, Text);
			WriteLine();
		}

		public void WriteIcon(Icon InIcon)
		{
			WriteLine("![{0}]({1} \"{0}\")(w:18 convert:false)", InIcon.AltText, InIcon.Path);
		}

		public void WritePageHeader(string Title, string Crumbs, string Description)
		{
			WriteLine("Title: {0}", Title.Replace("<", "&lt;").Replace(">", "&gt;"));
			WriteLine("Crumbs: {0}", Crumbs);
			WriteLine("Description: {0}", Description.Replace('\n', ' ').Replace('\r', ' '));
			WriteLine("Availability: Public");
			WriteLine();
		}

		public void WriteHeading(int Level, string Name)
		{
			WriteLine();
			WriteLine();

			for(int Idx = 0; Idx < Level; Idx++)
			{
				Write("#");
			}
			WriteLine(" {0}", Name);

			WriteLine();
			WriteLine();
		}

		public void WriteEscaped(string Text)
		{
			if (Text != null)
			{
				Write(Utility.EscapeText(Text));
			}
		}

		public void WriteEscapedLine(string Text)
		{
			if (Text != null)
			{
				WriteEscaped(Text);
				WriteLine();
			}
		}

		public void WriteEscapedLine(string Format, params object[] Args)
		{
			WriteEscapedLine(String.Format(Format, Args));
		}

		public void WriteListItem(string Template, string Cell2, string Cell3)
		{
			WriteLine("[OBJECT:{0}]", Template);
			Indent++;

			WriteLine("[PARAM:cell2]");
			WriteLine(Cell2);
			WriteLine("[/PARAM]");

			WriteLine("[PARAM:cell3]");
			WriteLine(Cell3);
			WriteLine("[/PARAM]");

			Indent--;
			WriteLine("[/OBJECT]");
		}

		public void WriteListItem(string Template, string Cell1, string Cell2, string Cell3)
		{
			WriteLine("[OBJECT:{0}]", Template);
			Indent++;

			WriteLine("[PARAM:cell1]");
			WriteLine(Cell1);
			WriteLine("[/PARAM]");

			WriteLine("[PARAM:cell2]");
			WriteLine(Cell2);
			WriteLine("[/PARAM]");

			WriteLine("[PARAM:cell3]");
			WriteLine(Cell3);
			WriteLine("[/PARAM]");

			Indent--;
			WriteLine("[/OBJECT]");
		}

		public void EnterTag(string Text)
		{
			WriteLine(Text);
			Indent++;
		}

		public void EnterTag(string Format, params object[] Args)
		{
			WriteLine(Format, Args);
			Indent++;
		}

		public void LeaveTag(string Text)
		{
			Indent--;
			WriteLine(Text);
		}

		public void LeaveTag(string Format, params object[] Args)
		{
			Indent--;
			WriteLine(Format, Args);
		}

		public void EnterRegion(string Name)
		{
			EnterTag("[REGION:{0}]", Name);
		}

		public void LeaveRegion()
		{
			LeaveTag("[/REGION]");
		}

		public void WriteRegion(string Name, string Text)
		{
			EnterRegion(Name);
			WriteLine(Text);
			LeaveRegion();
		}

		public void WriteRegion(string Name, Icon Icon)
		{
			EnterRegion(Name);
			WriteIcon(Icon);
			LeaveRegion();
		}

		public void EnterObject(string Name)
		{
			EnterTag("[OBJECT:{0}]", Name);
		}

		public void LeaveObject()
		{
			LeaveTag("[/OBJECT]");
		}

		public void EnterParam(string Name)
		{
			EnterTag("[PARAM:{0}]", Name);
		}

		public void LeaveParam()
		{
			LeaveTag("[/PARAM]");
		}

		public void EnterSection(string ID, string Heading)
		{
			EnterTag("[OBJECT:Section]");

			WriteLine("[PARAMLITERAL:id]");
			WriteEscapedLine(ID);
			WriteLine("[/PARAMLITERAL]");

			WriteLine("[PARAM:heading]");
			WriteEscapedLine(Heading);
			WriteLine("[/PARAM]");

			EnterTag("[PARAM:content]");
		}

		public void LeaveSection()
		{
			LeaveTag("[/PARAM]");
			LeaveTag("[/OBJECT]");
		}

		public void EnterList(string CssRegion, string HeaderTemplate, string HeaderColumn1, string HeaderColumn2)
		{
			EnterTag("[REGION:{0}]", CssRegion);
			WriteListItem(HeaderTemplate, HeaderColumn1, HeaderColumn2);
		}

		public void EnterList(string CssRegion, string HeaderTemplate, string HeaderColumn1, string HeaderColumn2, string HeaderColumn3)
		{
			EnterTag("[REGION:{0}]", CssRegion);
			WriteListItem(HeaderTemplate, HeaderColumn1, HeaderColumn2, HeaderColumn3);
		}

		public void LeaveList()
		{
			LeaveTag("[/REGION]");
		}

		public void EnterListSection(string SectionId, string SectionHeading, string HeaderTemplate, string HeaderColumn1, string HeaderColumn2, string HeaderColumn3)
		{
			EnterSection(SectionId, SectionHeading);
			EnterList("members", HeaderTemplate, HeaderColumn1, HeaderColumn2, HeaderColumn3);
		}

		public void LeaveListSection()
		{
			LeaveList();
			LeaveSection();
		}

		public void WriteObject(string Name, params object[] Params)
		{
			EnterTag("[OBJECT:{0}]", Name);
			for(int Idx = 0; Idx < Params.Length; Idx += 2)
			{
				string ParamName = (string)Params[Idx];
				object ParamValue = Params[Idx + 1];
				if(ParamValue is string)
				{
					WriteParam(ParamName, (string)ParamValue);
				}
				else if(ParamValue is Icon)
				{
					WriteParam(ParamName, (Icon)ParamValue);
				}
				else if(ParamValue is IEnumerable<Icon>)
				{
					WriteParam(ParamName, (IEnumerable<Icon>)ParamValue);
				}
				else
				{
					throw new InvalidDataException("Unknown parameter type");
				}
			}
			LeaveTag("[/OBJECT]");
		}

		public void WriteParam(string Name, string Value)
		{
			EnterParam(Name);
			WriteLine(Value);
			LeaveParam();
		}

		public void WriteParam(string Name, Icon Icon)
		{
			EnterParam(Name);
			WriteIcon(Icon);
			LeaveParam();
		}

		public void WriteParam(string Name, IEnumerable<Icon> Icons)
		{
			EnterParam(Name);
			foreach (Icon Icon in Icons)
			{
				WriteIcon(Icon);
			}
			LeaveParam();
		}

		public void WriteParamLiteral(string Name, string Value)
		{
			EnterTag("[PARAMLITERAL:{0}]", Name);
			WriteLine(Value);
			LeaveTag("[/PARAMLITERAL]");
		}

		public void WriteList(IEnumerable<UdnListItem> Items)
		{
			// Write the head
			WriteObject("MemberListHeadBlank");

			// Write the body
			foreach (UdnListItem Item in Items)
			{
				if (Item.Link == null)
				{
					WriteObject("MemberListItem", "name", Item.Name, "desc", Item.Description);
				}
				else
				{
					WriteObject("MemberListItemLinked", "name", Item.Name, "link", "[RELATIVE:" + Item.Link + "]", "desc", Item.Description);
				}
			}

			// Write the tail
			WriteObject("MemberListTail");
		}

		public void WriteList(string NameColumn, string DescColumn, IEnumerable<UdnListItem> Items)
		{
			// Write the head
			WriteObject("MemberListHead", "name", NameColumn, "desc", DescColumn);

			// Write the body
			foreach (UdnListItem Item in Items)
			{
				if (Item.Link == null)
				{
					WriteObject("MemberListItem", "name", Item.Name, "desc", Item.Description);
				}
				else
				{
					WriteObject("MemberListItemLinked", "name", Item.Name, "link", "[RELATIVE:" + Item.Link + "]", "desc", Item.Description);
				}
			}

			// Write the tail
			WriteObject("MemberListTail");
		}

		public void WriteList(IEnumerable<UdnIconListItem> Items)
		{
			// Write the head
			WriteObject("MemberIconListHeadBlank");

			// Write the body
			foreach (UdnIconListItem Item in Items)
			{
				if (Item.Link == null)
				{
					WriteObject("MemberIconListItem", "icon", Item.Icons, "name", Item.Name, "desc", Item.Description);
				}
				else
				{
					WriteObject("MemberIconListItemLinked", "icon", Item.Icons, "name", Item.Name, "link", "[RELATIVE:" + Item.Link + "]", "desc", Item.Description);
				}
			}

			// Write the tail
			WriteObject("MemberIconListTail");
		}

		public void WriteList(string NameColumn, string DescColumn, IEnumerable<UdnIconListItem> Items)
		{
			// Write the head
			WriteObject("MemberIconListHead", "name", NameColumn, "desc", DescColumn);

			// Write the body
			foreach (UdnIconListItem Item in Items)
			{
				if (Item.Link == null)
				{
					WriteObject("MemberIconListItem", "icon", Item.Icons, "name", Item.Name, "desc", Item.Description);
				}
				else
				{
					WriteObject("MemberIconListItemLinked", "icon", Item.Icons, "name", Item.Name, "link", "[RELATIVE:" + Item.Link + "]", "desc", Item.Description);
				}
			}

			// Write the tail
			WriteObject("MemberIconListTail");
		}

		public void WriteFilterList(UdnFilterListItem[] FilterListItems)
		{
			EnterRegion("filter-list");
			foreach(UdnFilterListItem FilterListItem in FilterListItems)
			{
				WriteObject("FilterItem", "name", FilterListItem.Name, "link", "[RELATIVE:" + FilterListItem.Link + "]");
			}
			EnterRegion("filter-last");
			LeaveRegion();
			LeaveRegion();
		}

		public void WriteListSection(string SectionId, string SectionName, string NameColumn, string DescColumn, IEnumerable<UdnListItem> Items)
		{
			UdnListItem[] ItemArray = Items.ToArray();
			if(ItemArray.Length > 0)
			{
				EnterSection(SectionId, SectionName);
				WriteList(NameColumn, DescColumn, ItemArray);
				LeaveSection();
			}
		}

		public void WriteListSection(string SectionId, string SectionName, string NameColumn, string DescColumn, IEnumerable<UdnIconListItem> Items)
		{
			UdnIconListItem[] ItemArray = Items.ToArray();
			if (ItemArray.Length > 0)
			{
				EnterSection(SectionId, SectionName);
				WriteList(NameColumn, DescColumn, ItemArray);
				LeaveSection();
			}
		}

		private void WriteIndent()
		{
			if (IsNewLine)
			{
				for (int Idx = 0; Idx < Indent; Idx++)
				{
					UnderlyingWriter.Write("    ");
				}
				IsNewLine = false;
			}
		}
	}
}
