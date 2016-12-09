// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels;

namespace Tools.CrashReporter.CrashReportWebSite.ViewModels
{
	/// <summary>
	/// The view model for the crashes index page.
	/// </summary>
	public class CrashesViewModel
	{
		/// <summary>Dates are in milliseconds relative to this base (for ease of Javascript usage)</summary>
		public static readonly DateTime Epoch = new DateTime( 1970, 1, 1 );

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

		/// <summary>Callstack line as query for filtering.</summary>
		public string SearchQuery { get; set; }

		/// <summary>User name as query for filtering.</summary>
		public string UsernameQuery { get; set; }

		/// <summary>Epic ID or Machine ID as query for filtering.</summary>
		public string EpicIdOrMachineQuery { get; set; }

		/// <summary>Jira as query for crash filtering.</summary>
		public string JiraQuery { get; set; }

		/// <summary>Message/Summary or Description as query for filtering.</summary>
		public string MessageQuery { get; set; }

		/// <summary>Bugg Id as query for filtering.</summary>
		public string BuggId { get; set; }

		/// <summary>BuiltFromCL as query for filtering.</summary>
		public string BuiltFromCL { get; set; }

		/// <summary>The date of the earliest crash to display.</summary>
		public long DateFrom { get; set; }

		/// <summary>The date of the most recent crash to display.</summary>
		public long DateTo { get; set; }

		/// <summary>The name of the branch to filter by.</summary>
		public string BranchName { get; set; }

		/// <summary>The version to filter by.</summary>
		public string VersionName { get; set; }
        
        /// <summary>The version to filter by.</summary>
        public string EngineVersion { get; set; }

		/// <summary>The platform to filter by.</summary>
		public string PlatformName { get; set; }

        /// <summary>The Engine Mode by which to filter.</summary>
        public string EngineMode { get; set; }

		/// <summary>The name of the game to filter by.</summary>
		public string GameName { get; set; }

		/// <summary>A dictionary of group names vs. crash counts.</summary>
		public Dictionary<string, int> GroupCounts { get; set; }

		/// <summary>The user supplied parameters.</summary>
		public FormCollection FormCollection { get; set; }

		/// <summary>A collection of Branch names used in the drop down on the main search form</summary>
		public List<SelectListItem> BranchNames { get; set; }

		/// <summary>A collection of Version names used in the drop down on the main search form</summary>
		public List<SelectListItem> VersionNames { get; set; }

		/// <summary>A collection of Platform names used in the drop down on the main search form</summary>
		public List<SelectListItem> PlatformNames { get; set; }

        /// <summary>A collection of Engine Modes used in the drop down on the main search form</summary>
        public List<SelectListItem> EngineModes { get; set; }
        
        /// <summary>A collection of Engine Modes used in the drop down on the main search form</summary>
        public List<SelectListItem> EngineVersions { get; set; }

		/// <summary>The set of statuses a crash could have its status set to.</summary>
		public IEnumerable<string> SetStatus { get { return new List<string>( new string[] { "Unset", "Reviewed", "New", "Coder", "EngineQA", "GameQA" } ); } }

		/// <summary> The real user name, displayed only for searches. </summary>
		public string RealUserName { get; set; }

		/// <summary>Time spent in generating this site, formatted as a string.</summary>
		public string GenerationTime { get; set; }

		/// <summary>
		/// Default constructor, used by HomeController
		/// </summary>
		public CrashesViewModel()
		{
			DateTime FromDate = DateTime.Today.AddDays( -7 ).ToUniversalTime();
			DateTime ToDate = DateTime.Today.ToUniversalTime();
            DateFrom = (long)( FromDate - Epoch ).TotalMilliseconds;
			DateTo = (long)( ToDate - Epoch ).TotalMilliseconds;
			CrashType = "CrashesAsserts";
		}
	}
}