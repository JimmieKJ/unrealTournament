// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// The view model for the crashes index page.
	/// </summary>
	public class CrashesViewModel
	{
		/// <summary>Dates are in milliseconds relative to this base (for ease of Javascript usage)</summary>
		public static readonly DateTime Epoch = new DateTime(1970, 1, 1);

		/// <summary>A container for all the crashes that pass the current filters.</summary>
		public IEnumerable<Crash> Results { get; set; }

		/// <summary>Information to properly paginate a large number of crashes.</summary>
		public PagingInfo PagingInfo { get; set; }

		/// <summary>Whether to sort ascending or descending.</summary>
		public string SortOrder { get; set; }

		/// <summary>The term to sort by.</summary>
		public string SortTerm { get; set; }

		/// <summary>The user group to filter by.</summary>
		public string UserGroup { get; set; }

		/// <summary>The type of report to filter by. e.g. Asserts.</summary>
		public string CrashType { get; set; }

		/// <summary>The query for advanced crash filtering.</summary>
		public string SearchQuery { get; set; }

		/// <summary>The date of the earliest crash to display.</summary>
		public long DateFrom { get; set; }

		/// <summary>The date of the most recent crash to display.</summary>
		public long DateTo { get; set; }

		/// <summary>The name of the branch to filter by.</summary>
		public string BranchName { get; set; }

		/// <summary>The name of the game to filter by.</summary>
		public string GameName { get; set; }

		/// <summary>A dictionary of group names vs. crash counts.</summary>
		public Dictionary<string, int> GroupCounts { get; set; }

		/// <summary>The user supplied parameters.</summary>
		public FormCollection FormCollection { get; set; }

		/// <summary>The set of statuses a crash could have its status set to.</summary>
		public IEnumerable<string> SetStatus { get { return new List<string>( new string[] { "Unset", "Reviewed", "New", "Coder", "EngineQA", "GameQA" } ); } }

		/// <summary> The real user name, displayed only for searches. </summary>
		public string RealUserName { get; set; }
	}
}