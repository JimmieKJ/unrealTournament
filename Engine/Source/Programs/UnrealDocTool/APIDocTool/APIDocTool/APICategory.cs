// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APICategory : APIPage
	{
		static private Dictionary<string, string> CategoryTooltips = new Dictionary<string, string>();

		static public void LoadTooltips(Dictionary<string,object> Categories)
		{
			foreach (var Category in Categories)
			{
				CategoryTooltips.Add(Category.Key,(string)((Dictionary<string,object>)Category.Value)["Tooltip"]);
			}
		}

		public readonly Dictionary<string, APICategory> SubCategories;
		public readonly List<APIAction> Actions;
		public readonly bool bIsRootCategory;

		public APICategory(APIPage InParent, string InName, bool bInIsRootCategory = false)
			: base(InParent, InName)
		{
			SubCategories = new Dictionary<string, APICategory>();
			Actions = new List<APIAction>();

			bIsRootCategory = bInIsRootCategory;
		}

		public APICategory GetSubCategory(string CategoryName)
		{
			APICategory ReturnValue;
			if (SubCategories.TryGetValue(CategoryName, out ReturnValue) == false)
			{
				ReturnValue = new APICategory(this, CategoryName);
				SubCategories.Add(CategoryName, ReturnValue);
			}
			return ReturnValue;
		}

		public void AddAction(APIAction Action)
		{
			Actions.Add(Action);
		}

		public override IEnumerable<SitemapNode> CreateSitemapNodes()
		{
			SitemapNode Node = new SitemapNode(Name, SitemapLinkPath);
			Node.Children.AddRange(SubCategories.OrderBy(x => x.Value.Name).SelectMany(x => x.Value.CreateSitemapNodes()));
			yield return Node;
		}

		public void WriteSitemapContents(string OutputPath)
		{
			List<SitemapNode> RootNodes = new List<SitemapNode>();
			if (bIsRootCategory)
			{
				RootNodes.Add(new SitemapNode("Contents", SitemapLinkPath));
			}
			foreach (APIPage SubCategory in SubCategories.Values)
			{
				RootNodes.AddRange(SubCategory.CreateSitemapNodes());
			}
			foreach (APIPage Action in Actions)
			{
				RootNodes.AddRange(Action.CreateSitemapNodes());
			}
			Sitemap.WriteContentsFile(OutputPath, RootNodes);
		}

		public void WriteSitemapIndex(string OutputPath)
		{
			// Gather all the pages
			List<APIPage> AllPages = new List<APIPage>(GatherPages());

			// Find the shortest link path to each member
			Dictionary<string, List<APIMember>> MemberIndexLinks = new Dictionary<string, List<APIMember>>();
			foreach (APIMember Member in AllPages.OfType<APIMember>())
			{
				// Find or create the list of members for this link path
				List<APIMember> Members;
				if (!MemberIndexLinks.TryGetValue(Member.FullName, out Members))
				{
					Members = new List<APIMember>();
					MemberIndexLinks.Add(Member.FullName, Members);
				}

				// Remove any members 
				Members.RemoveAll(x => x.HasParent(Member));
				if (!Members.Any(x => Member.HasParent(x))) Members.Add(Member);
			}

			// Create all the root pages
			List<SitemapNode> RootNodes = new List<SitemapNode>();
			foreach (APIPage Page in AllPages)
			{
				if (!(Page is APIMember))
				{
					RootNodes.Add(new SitemapNode(Page.Name, Page.SitemapLinkPath));
				}
			}
			foreach (KeyValuePair<string, List<APIMember>> MemberIndexLink in MemberIndexLinks)
			{
				foreach (APIMember Member in MemberIndexLink.Value)
				{
					RootNodes.Add(new SitemapNode(MemberIndexLink.Key, Member.SitemapLinkPath));
				}
			}

			// Write the index file
			Sitemap.WriteIndexFile(OutputPath, RootNodes);
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{

				if (bIsRootCategory)
				{
					Writer.WritePageHeader("Unreal Engine Blueprint API Reference", PageCrumbs, "Unreal Engine Blueprint API Reference");

					Writer.WriteHeading(2, "Disclaimer");

					Writer.WriteLine("The Blueprint API reference is an early work in progress, and some information may be missing or out of date. It strives to reflect all available nodes, but it is not guaranteed to be an exhaustive list.");
					Writer.WriteLine("For tutorials, walkthroughs and detailed guides to using Blueprints in UE4, please see the [Introduction to Blueprints](http://docs.unrealengine.com/latest/INT/Engine/Blueprints/GettingStarted/index.html) on the web.");
				}
				else
				{
					Writer.WritePageHeader(Name, PageCrumbs, Name);
				}

				if (Actions.Count > 0)
				{
					Writer.WriteHeading(2, "Actions");
					Writer.WriteList(Actions.OrderBy(x => x.Name).Select(x => x.GetListItem()));
				}

				if (SubCategories.Count > 0)
				{
					Writer.WriteHeading(2, "Categories");
					Writer.WriteList(SubCategories.OrderBy(x => x.Key).Select(x => x.Value.GetListItem()));
				}
			}

		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{
			Pages.AddRange(SubCategories.Values);
			Pages.AddRange(Actions);
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			Manifest.Add(Name, this);
			foreach (APIPage SubCategory in SubCategories.Values)
			{
				SubCategory.AddToManifest(Manifest);
			}
			foreach (APIPage Action in Actions)
			{
				Action.AddToManifest(Manifest);
			}
		}

		public UdnListItem GetListItem()
		{
			string Tooltip;
			if (!CategoryTooltips.TryGetValue(Name, out Tooltip))
			{
				Tooltip = "";
			}

			return new UdnListItem(Name, Tooltip, LinkPath);
		}

	}
}
