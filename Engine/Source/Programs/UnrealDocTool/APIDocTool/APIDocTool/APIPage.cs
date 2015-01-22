// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	public abstract class APIPage
	{
		public readonly APIPage Parent;
		public readonly string Name;
		public readonly string LinkPath;

		public APIPage(APIPage InParent, string InName)
			: this(InParent, InName, Utility.MakeLinkPath((InParent == null)? "" : InParent.LinkPath, InName))
		{
		}

		public APIPage(APIPage InParent, string InName, string InLinkPath)
		{
			Parent = InParent;
			Name = InName;
			LinkPath = InLinkPath;
		}

		public string PageCrumbs
		{
			get { return (Parent == null) ? "%ROOT%" : (Parent.PageCrumbs + ", " + Parent.LinkPath); }
		}

		public string SitemapLinkPath
		{
			get { return "INT\\" + LinkPath + "\\index.html"; }
		}

		public T FindParent<T>() where T : APIPage
		{
			for(APIPage NextPage = Parent; NextPage != null; NextPage = NextPage.Parent)
			{
				if(NextPage is T)
				{
					return (T)NextPage;
				}
			}
			return null;
		}

		public bool HasParent(APIPage InParent)
		{
			for (APIPage Page = Parent; Page != null; Page = Page.Parent)
			{
				if (Page == InParent)
				{
					return true;
				}
			}
			return false;
		}

		public IEnumerable<APIPage> GatherPages()
		{
			// Visit all the pages and collect all their references
			HashSet<APIPage> PendingSet = new HashSet<APIPage>{ this };
			HashSet<APIPage> VisitedSet = new HashSet<APIPage>();
			while (PendingSet.Count > 0)
			{
				APIPage Page = PendingSet.First();

				List<APIPage> ReferencedPages = new List<APIPage>();
				Page.GatherReferencedPages(ReferencedPages);

				foreach (APIPage ReferencedPage in ReferencedPages)
				{
					if (!VisitedSet.Contains(ReferencedPage))
					{
						PendingSet.Add(ReferencedPage);
					}
				}

				PendingSet.Remove(Page);
				VisitedSet.Add(Page);
			}
			return VisitedSet;
		}

		public virtual bool ShouldOutputPage()
		{
			return true;
		}

		public virtual IEnumerable<SitemapNode> CreateSitemapNodes()
		{
			if(ShouldOutputPage())
			{
				yield return new SitemapNode(Name, SitemapLinkPath);
			}
		}

		public abstract void WritePage(UdnManifest Manifest, string OutputPath);

		public abstract void GatherReferencedPages(List<APIPage> Pages);

		public abstract void AddToManifest(UdnManifest Manifest);
	}
}
