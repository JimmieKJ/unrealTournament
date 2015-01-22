// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIMemberIndex : APIPage
	{
		public class Entry
		{
			public string Name;
			public string SortName;
			public string LinkPath;

			public Entry(string InName, string InLinkPath)
			{
				Name = InName;
				SortName = Name;
				LinkPath = InLinkPath;
			}

			public Entry(APIMember Member)
			{
				Name = Member.FullName;
				SortName = Name;
				LinkPath = Member.LinkPath;
			}
		}

		public class Category
		{
			public string Name;
			public List<Entry> Entries = new List<Entry>();

			public Category(string InName)
			{
				Name = InName;
			}
		}

		public const string CharacterCategories = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_";

		public string Description;
		public List<Category> Categories = new List<Category>();

		public APIMemberIndex(APIPage InParent, string InName, string InDescription)
			: base(InParent, InName)
		{
			Description = InDescription;

			for (int Idx = 0; Idx < CharacterCategories.Length; Idx++)
			{
				Categories.Add(new Category(CharacterCategories.Substring(Idx, 1)));
			}
		}

		public void AddToDefaultCategory(Entry InEntry)
		{
			int CategoryIdx = CharacterCategories.IndexOf(Char.ToUpper(InEntry.SortName[0]));
			if (CategoryIdx == -1)
			{
				Console.WriteLine("No default category for {0}", InEntry.SortName);
			}
			else
			{
				Categories[CategoryIdx].Entries.Add(InEntry);
			}
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, Description);

				// Create an array of links for each letter
				string[] CategoryLinks = new string[Categories.Count];
				for (int Idx = 0; Idx < Categories.Count; Idx++)
				{
					if (Categories[Idx].Entries.Count == 0)
					{
						CategoryLinks[Idx] = Categories[Idx].Name;
					}
					else
					{
						CategoryLinks[Idx] = String.Format("[{0}](#idx_{0})", Categories[Idx].Name);
					}
				}

				// Write the link section
				Writer.WriteLine("[REGION:memberindexlinks]");
				Writer.WriteLine(String.Join(" &middot; ", CategoryLinks));
				Writer.WriteLine("[/REGION]");

				// Write each letter
				for(int Idx = 0; Idx < Categories.Count; Idx++)
				{
					if (Categories[Idx].Entries.Count > 0)
					{
						Writer.WriteLine();
						Writer.WriteLine("(#idx_{0})", Categories[Idx].Name);
						Writer.WriteHeading(2, Categories[Idx].Name);
						foreach (Entry Entry in Categories[Idx].Entries)
						{
							Writer.WriteLine("[REGION:memberindexitem]");
							Writer.WriteLine("[{0}]({1})", Entry.Name, Entry.LinkPath);
							Writer.WriteLine("[/REGION]");
						}
					}
				}
			}
		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			Manifest.Add("MemberIndex:" + Name, this);
		}
	}
}
