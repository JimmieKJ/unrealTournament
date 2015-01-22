// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.IO;

namespace APIDocTool
{
	public class APIFilter : APIPage
	{
		public List<APIFilter> Filters = new List<APIFilter>();
		public List<APIMember> Members = new List<APIMember>();
		
		public APIFilter(APIPage InParent, string InName)
			: base(InParent, InName)
		{
		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{
			Pages.AddRange(Filters);
			Pages.AddRange(Members);
		}

		public override IEnumerable<SitemapNode> CreateSitemapNodes()
		{
			SitemapNode Node = new SitemapNode(Name, SitemapLinkPath);
			Node.Children.AddRange(Filters.SelectMany(x => x.CreateSitemapNodes()));
			Node.Children.AddRange(Members.OfType<APIRecord>().SelectMany(x => x.CreateSitemapNodes()));
			if (Node.Children.Count == 0) Node.Children.Add(new SitemapNode(Name, SitemapLinkPath));
			yield return Node;
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			string FilterPath = Name;
			for(APIFilter Filter = Parent as APIFilter; Filter != null; Filter = Filter.Parent as APIFilter)
			{
				FilterPath = Filter.Name + "." + FilterPath;
			}
			Manifest.Add("Filter:" + FilterPath, this);

			foreach(APIFilter Filter in Filters)
			{
				Filter.AddToManifest(Manifest);
			}
			foreach(APIMember Member in Members)
			{
				Member.AddToManifest(Manifest);
			}
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, "");

				List<APIMember> FilteredMembers = Members;

				// Get the name of the withheld section
				APIModule Module = FindParent<APIModule>();
				if (Module != null)
				{
					string UnlistedEntries = Program.Settings.FindValue("Module." + Module.Name + ".Unlisted");
					if (UnlistedEntries != null)
					{
						HashSet<APIPage> UnlistedPages = new HashSet<APIPage>(UnlistedEntries.Split('\n').Select(x => Manifest.Find(x.Trim())));
						FilteredMembers = new List<APIMember>(FilteredMembers.Where(x => !UnlistedPages.Contains(x)));
					}
				}

				// Find all the records, sorted by name
				List<APIRecord> AllRecords = new List<APIRecord>(FilteredMembers.OfType<APIRecord>().Where(x => !x.bIsTemplateSpecialization).OrderBy(x => x.Name));

				// Find all the functions, sorted by name
				List<APIFunction> AllFunctions = new List<APIFunction>();
				AllFunctions.AddRange(FilteredMembers.OfType<APIFunction>().Where(x => !x.bIsTemplateSpecialization));
				AllFunctions.AddRange(FilteredMembers.OfType<APIFunctionGroup>().SelectMany(x => x.Children.OfType<APIFunction>()).Where(x => !x.bIsTemplateSpecialization));
				AllFunctions.Sort((x, y) => (x.Name.CompareTo(y.Name)));

				// Enter the module template
				Writer.EnterTag("[OBJECT:Filter]");

				// Write the class list
				Writer.EnterTag("[PARAM:filters]");
				APIFilter.WriteListSection(Writer, "filters", "Filters", Filters.OrderBy(x => x.Name));
				Writer.LeaveTag("[/PARAM]");

				// Write the class list
				Writer.EnterTag("[PARAM:classes]");
				Writer.WriteListSection("classes", "Classes", "Name", "Description", AllRecords.Select(x => x.GetListItem()));
				Writer.LeaveTag("[/PARAM]");

				// Write all the constants
				Writer.EnterTag("[PARAM:constants]");
				Writer.WriteListSection("constants", "Constants", "Name", "Description", FilteredMembers.OfType<APIConstant>().Select(x => x.GetListItem()));
				Writer.LeaveTag("[/PARAM]");

				// Write the typedef list
				Writer.EnterTag("[PARAM:typedefs]");
				Writer.WriteListSection("typedefs", "Typedefs", "Name", "Description", FilteredMembers.OfType<APITypeDef>().Select(x => x.GetListItem()));
				Writer.LeaveTag("[/PARAM]");

				// Write the enum list
				Writer.EnterTag("[PARAM:enums]");
				Writer.WriteListSection("enums", "Enums", "Name", "Description", FilteredMembers.OfType<APIEnum>().OrderBy(x => x.Name).Select(x => x.GetListItem()));
				Writer.LeaveTag("[/PARAM]");

				// Write the function list
				Writer.EnterTag("[PARAM:functions]");
				APIFunction.WriteListSection(Writer, "functions", "Functions", AllFunctions, true);
				Writer.LeaveTag("[/PARAM]");

				// Write the variable list
				Writer.EnterTag("[PARAM:variables]");
				APIVariable.WriteListSection(Writer, "variables", "Variables", FilteredMembers.OfType<APIVariable>());
				Writer.LeaveTag("[/PARAM]");

				// Close the module template
				Writer.LeaveTag("[/OBJECT]");
			}
		}

		public UdnFilterListItem GetFilterListItem()
		{
			return new UdnFilterListItem(Name, LinkPath);
		}

		public static void WriteListSection(UdnWriter Writer, string SectionId, string SectionTitle, IEnumerable<APIFilter> Categories)
		{
			APIFilter[] CategoryArray = Categories.ToArray();
			if (CategoryArray.Length > 0)
			{
				Writer.EnterSection(SectionId, SectionTitle);
				Writer.WriteFilterList(CategoryArray.Select(x => x.GetFilterListItem()).ToArray());
				Writer.LeaveSection();
			}
		}
	}
}
