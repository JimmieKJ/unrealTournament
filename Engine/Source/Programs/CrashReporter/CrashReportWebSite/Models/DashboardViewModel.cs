// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// The view model for the crash summary page.
	/// </summary>
	public class DashboardViewModel
	{
		/// <summary>An encoded table of crashes by week for the display plugin to use.</summary>
		public string CrashesByWeek { get; set; }
		/// <summary>An encoded table of crashes by day for the display plugin to use.</summary>
		public string CrashesByDay { get; set; }
		/// <summary>An encoded table of buggs by day for the display plugin to use.</summary>
		public string BuggsByDay { get; set; }

		/// <summary></summary>
		public string GenerationTime { get; set; }
	}
}