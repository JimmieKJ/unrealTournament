// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.IO;
using DoxygenLib;

namespace APIDocTool
{
    public class APIModule : APIFilter
    {
		public readonly string Description;

		public APIModule(APIPage InParent, string InName, string InDescription) 
			: base(InParent, InName)
        {
			Description = InDescription;
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			base.AddToManifest(Manifest);
			Manifest.Add("Module:" + Name, this);
		}

		public static int IndexOfEnd(string InputStr, string SubStr)
		{
			int Idx = InputStr.IndexOf(SubStr);
			return (Idx == -1)? -1 : Idx + SubStr.Length;
		}

		public static APIModule Build(APIPage InParent, DoxygenModule InModule)
		{
			// Find the description and category
			string ModuleSettingsPath = "Module." + InModule.Name;
			string Description = Program.Settings.FindValueOrDefault(ModuleSettingsPath + ".Description", "");

			// Get the filter settings
			IniSection FilterSection = Program.Settings.FindSection(ModuleSettingsPath + ".Filter");
			IniSection WithholdSection = Program.Settings.FindSection(ModuleSettingsPath + ".Withhold");

			// Build a module from all the members
			APIModule Module = new APIModule(InParent, InModule.Name, Description);

			// Normalize the base directory
			string NormalizedBaseSrcDir = Path.GetFullPath(InModule.BaseSrcDir).ToLowerInvariant();
			if (!NormalizedBaseSrcDir.EndsWith("\\")) NormalizedBaseSrcDir += "\\";

			// Separate the members into categories, based on their path underneath the module source directory
			Dictionary<APIFilter, List<DoxygenEntity>> MembersByFilter = new Dictionary<APIFilter,List<DoxygenEntity>>();
			foreach (DoxygenEntity Entity in InModule.Entities)
			{
				string FilterPath = null;

				// Try to get the filter path from the ini section
				if (FilterSection != null)
				{
					FilterPath = FilterSection.Find(Entity.Name);
				}

				// If we didn't have one set explicitly, generate one from the subdirectory
				if(FilterPath == null)
				{
					string EntityFile = String.IsNullOrEmpty(Entity.File)? "" : Path.GetFullPath(Entity.File);
					if(EntityFile.ToLowerInvariant().StartsWith(NormalizedBaseSrcDir))
					{
						int MinIndex = EntityFile.IndexOf('\\', NormalizedBaseSrcDir.Length);
						if(MinIndex == -1)
						{
							FilterPath = "";
						}
						else if (IsVisibleFolder(EntityFile.Substring(NormalizedBaseSrcDir.Length, MinIndex - NormalizedBaseSrcDir.Length)))
						{
							int MaxIndex = EntityFile.LastIndexOf('\\');
							FilterPath = EntityFile.Substring(MinIndex + 1, Math.Max(MaxIndex - MinIndex - 1, 0));
						}
					}
				}

				// Add this entity to the right filters
				if(FilterPath != null)
				{
					// Create all the categories for this entry
					APIFilter ParentFilter = Module;
					if (FilterPath.Length > 0)
					{
						string[] Folders = FilterPath.Split('\\');
						for (int Idx = 0; Idx < Folders.Length; Idx++)
						{
							APIFilter NextFilter = ParentFilter.Filters.FirstOrDefault(x => String.Compare(x.Name, Folders[Idx], true) == 0);
							if (NextFilter == null)
							{
								NextFilter = new APIFilter(ParentFilter, Folders[Idx]);
								ParentFilter.Filters.Add(NextFilter);
							}
							ParentFilter = NextFilter;
						}
					}

					// Add this entity to the pending list for this filter
					Utility.AddToDictionaryList(ParentFilter, Entity, MembersByFilter);
				}
			}

			// Try to fixup all of the filters
			foreach (KeyValuePair<APIFilter, List<DoxygenEntity>> Members in MembersByFilter)
			{
				APIFilter Filter = Members.Key;
				Filter.Members.AddRange(APIMember.CreateChildren(Filter, Members.Value));
			}
			return Module;
		}

		private static bool IsVisibleFolder(string Name)
		{
			return String.Compare(Name, "Classes", true) == 0 || String.Compare(Name, "Public") == 0;
		}

		public UdnListItem GetListItem()
		{
			return new UdnListItem(Name, Description, LinkPath);
		}
    }
}
