// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	public class UdnManifest
	{
		Dictionary<string, APIPage> Entries = new Dictionary<string, APIPage>();
		Dictionary<string, List<APIPage>> ConflictEntries = new Dictionary<string, List<APIPage>>();

		public UdnManifest(APIPage Page)
		{
			Page.AddToManifest(this);
		}

		public void Add(string Name, APIPage Page)
		{
			APIPage ExistingPage;
			if(Entries.TryGetValue(Name, out ExistingPage))
			{
				if (ExistingPage != Page)
				{
					List<APIPage> ConflictPages;
					if (!ConflictEntries.TryGetValue(Name, out ConflictPages))
					{
						ConflictPages = new List<APIPage>{ ExistingPage };
						ConflictEntries.Add(Name, ConflictPages);
					}
					if (!ConflictPages.Contains(Page))
					{
						ConflictPages.Add(Page);
					}
				}
			}
			else
			{
				Entries.Add(Name, Page);
			}
		}

		public APIPage Find(string Name)
		{
			APIPage Result;
			if(!Entries.TryGetValue(Name, out Result))
			{
				Result = null;
			}
			return Result;
		}

		public string FormatString(string Text)
		{
			StringBuilder Output = new StringBuilder();
			for (int Idx = 0; Idx < Text.Length; Idx++)
			{
				if (Text[Idx] == '{')
				{
					if (Text[Idx + 1] == '{')
					{
						Output.Append(Text[++Idx]);
					}
					else
					{
						int EndIdx = Text.IndexOf('}', Idx + 1);
						string LinkText = Text.Substring(Idx + 1, EndIdx - Idx - 1);
						string LinkName = LinkText;

						int LinkNameIdx = LinkText.IndexOf('|');
						if (LinkNameIdx != -1)
						{
							LinkName = LinkText.Substring(LinkNameIdx + 1);
							LinkText = LinkText.Substring(0, LinkNameIdx);
						}

						APIPage LinkPage = Find(LinkName);
						if (LinkPage != null)
						{
							Output.AppendFormat("[{0}]({1})", LinkText, LinkPage.LinkPath);
						}
						else
						{
							Output.Append(LinkText);
						}
						Idx = EndIdx;
					}
				}
				else
				{
					Output.Append(Text[Idx]);
				}
			}
			return Output.ToString();
		}

		public void PrintConflicts()
		{
			foreach(KeyValuePair<string, List<APIPage>> ConflictEntry in ConflictEntries)
			{
				Console.WriteLine("Conflicting manifest entry for {0}:", ConflictEntry.Key);
				for (int Idx = 0; Idx < ConflictEntry.Value.Count; Idx++)
				{
					if (Idx == 0)
					{
						Console.WriteLine("   Could be: {0}", ConflictEntry.Value[Idx].LinkPath);
					}
					else
					{
						Console.WriteLine("         or: {0}", ConflictEntry.Value[Idx].LinkPath);
					}
				}
			}
		}

		public void Write(string OutputPath)
		{ 
			// Write the remaining entries to the output file
			using (StreamWriter Writer = new StreamWriter(OutputPath))
			{
				foreach (KeyValuePair<string, APIPage> Entry in Entries.OrderBy(x => x.Key))
				{
					APIPage TargetPage = Entry.Value;
					while(!TargetPage.ShouldOutputPage())
					{
						TargetPage = TargetPage.Parent;
					}
					Writer.WriteLine("{0}, {1}", Entry.Key, TargetPage.LinkPath);
				}
			}
		}
	}
}
