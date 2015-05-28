// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// The view model for the Bugg index page.
	/// </summary>
	/// // @TODO yrx 2014-08-14 Add base class
	public class BuggsViewModel
	{
		/// <summary>Tooltip for "Crash Groups" tab</summary>
		public const string Tooltip = "Collections of crashes that have the exact same callstack.";

		/// <summary>A container of sorted Buggs.</summary>
		public IEnumerable<Bugg> Results { get; set; }

		/// <summary>Information to paginate the list of Buggs.</summary>	
		public PagingInfo PagingInfo { get; set; }
		
		/// <summary>The criterion to sort by e.g. Game.</summary>
		public string SortTerm { get; set; }
		
		/// <summary>Either ascending or descending.</summary>
		public string SortOrder { get; set; }
		
		/// <summary>The current user group name.</summary>
		public string UserGroup { get; set; }

		/// <summary>The type of report to filter by. e.g. Asserts.</summary>
		public string CrashType { get; set; }
		
		/// <summary>The query that filtered the results.</summary>
		public string SearchQuery { get; set; }
		
		/// <summary>The date of the earliest crash in a Bugg.</summary>
		public long DateFrom { get; set; }
		
		/// <summary>The date of the most recent crash in a Bugg.</summary>
		public long DateTo { get; set; }

		/// <summary>The build version of the most recent crash in a Bugg.</summary>
		public string BuildVersion { get; set; }

		/// <summary>A dictionary of the number of Buggs per user group.</summary>
		public SortedDictionary<string, int> GroupCounts { get; set; }

		/// <summary>The set of statuses a Bugg could have its status set to.</summary>
		public IEnumerable<string> SetStatus
		{
			get
			{
				return new List<string>
				(
					new string[] 
					{ 
						"Unset", 
						"Reviewed", 
						"New", 
						"Coder",
						"Tester" 
					}
				);
			}
		}
		
		/// <summary>User input from the client.</summary>
		public FormCollection FormCollection { get; set; }

		/// <summary> Build versions. </summary>
		public IEnumerable<string> BuildVersions
		{
			get 
			{
				return new List<string>(
					new string[] 
					{ 
						"",
						"4.1.0", 
						"4.2.0", 
						"4.2.1", 
						"4.3.0", 
						"4.3.1", 
						"4.4.0" 
					} );
			}
		}

		/// <summary></summary>
		public string GenerationTime { get; set; }
	}
}