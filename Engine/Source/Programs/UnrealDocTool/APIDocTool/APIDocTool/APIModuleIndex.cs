// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using DoxygenLib;
using System.IO;

namespace APIDocTool
{
	class APIModuleCategory
	{
		public string Name;
		public List<APIModule> Modules = new List<APIModule>();
		public List<APIModuleCategory> Categories = new List<APIModuleCategory>();
		public bool Expanded = false;

		public APIModuleCategory(string InName)
		{
			Name = InName;
		}

		public void AddModule(APIModule Module)
		{
			// Find the description and category
			string ModuleSettingsPath = "Module." + Module.Name;
			string CategoryText = Program.Settings.FindValueOrDefault(ModuleSettingsPath + ".Category", null);

			// Build a module from all the members
			APIModuleCategory Category = String.IsNullOrEmpty(CategoryText)? this : AddCategory(CategoryText);
			Category.Modules.Add(Module);
		}

		public APIModuleCategory AddCategory(string Path)
		{
			return AddCategory(Path.Split('|'), 0);
		}

		private APIModuleCategory AddCategory(string[] Path, int PathIdx)
		{
			// If we're at the end of the path, return this category
			if (PathIdx == Path.Length)
			{
				return this;
			}

			// Otherwise try to find an existing category
			foreach (APIModuleCategory Category in Categories)
			{
				if (String.Compare(Category.Name, Path[PathIdx], true) == 0)
				{
					return Category.AddCategory(Path, PathIdx + 1);
				}
			}

			// Otherwise create a new category, and add it to that
			APIModuleCategory NewCategory = new APIModuleCategory(Path[PathIdx]);
			Categories.Add(NewCategory);
			return NewCategory.AddCategory(Path, PathIdx + 1);
		}

		public bool IsEmpty
		{
			get { return Modules.Count == 0 && Categories.All(x => x.IsEmpty); }
		}

		public void Write(UdnWriter Writer, int Depth)
		{
			if (!IsEmpty)
			{
				// CSS region for indenting 
				Writer.EnterRegion("module-sections-list");

				// Find all the modules in this category
				if(Modules.Count > 0)
				{
					Writer.WriteList(Modules.OrderBy(x => x.Name).Select(x => x.GetListItem()));
				}

				// Write all the subcategories
				foreach (APIModuleCategory Category in Categories.OrderBy(x => x.Name))
				{
					Writer.WriteHeading(Depth, Category.Name);
					Writer.WriteLine();
					Category.Write(Writer, Depth + 1);
				}

				// End of CSS region
				Writer.LeaveRegion();
			}
		}

		public void WriteExpandable(UdnWriter Writer, string UniqueId)
		{
			// Top level modules
			if(Modules.Count > 0)
			{
				Writer.WriteList(Modules.OrderBy(x => x.Name).Select(x => x.GetListItem()));
			}

			// CSS region for indenting 
//			Writer.EnterRegion("module-sections-list");

			// Expandable section for each subcategory
			int UniqueIdSuffix = 1;
			foreach (APIModuleCategory Category in Categories.OrderBy(x => x.Name))
			{
				if(!Category.IsEmpty)
				{
					string NewUniqueId = String.Format("{0}_{1}", UniqueId, UniqueIdSuffix);

					Writer.EnterTag(Category.Expanded? "[OBJECT:ModuleSectionExpanded]" : "[OBJECT:ModuleSection]");

					Writer.WriteLine("[PARAMLITERAL:id]");
					Writer.WriteEscapedLine(NewUniqueId);
					Writer.WriteLine("[/PARAMLITERAL]");

					Writer.WriteLine("[PARAM:heading]");
					Writer.WriteEscapedLine(Category.Name);
					Writer.WriteLine("[/PARAM]");

					Writer.EnterTag("[PARAM:content]");
					Category.WriteExpandable(Writer, NewUniqueId);
					Writer.LeaveTag("[/PARAM]");

					Writer.LeaveTag("[/OBJECT]");

					UniqueIdSuffix++;
				}
			}

			// End of CSS region
//			Writer.LeaveRegion();
		}

		public override string ToString()
		{
			return Name;
		}
	}

	class APIModuleIndex : APIPage
	{
		public APIModuleCategory Category;
		public List<APIModule> Children = new List<APIModule>();

		public APIModuleIndex(APIPage Parent, APIModuleCategory InCategory)
			: base(Parent, InCategory.Name)
		{
			Category = InCategory;
		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{
			Pages.AddRange(Children);
		}

		public override IEnumerable<SitemapNode> CreateSitemapNodes()
		{
			SitemapNode Node = new SitemapNode(Name, SitemapLinkPath);
			Node.Children.AddRange(Children.OrderBy(x => x.Name).SelectMany(x => x.CreateSitemapNodes()));
			yield return Node;
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			Manifest.Add("ModuleIndex:" + Name, this);

			foreach (APIModule Child in Children)
			{
				Child.AddToManifest(Manifest);
			}
		}

		public void WriteModuleList(UdnWriter Writer, int Depth)
		{
			Category.Write(Writer, Depth);
		}

		public void WriteExpandableModuleList(UdnWriter Writer, string UniqueId)
		{
			Category.WriteExpandable(Writer, UniqueId);
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, "Module Index");
				Writer.EnterRegion("modules-list");
				WriteModuleList(Writer, 2);
				Writer.LeaveRegion();
			}
		}
	}
}
