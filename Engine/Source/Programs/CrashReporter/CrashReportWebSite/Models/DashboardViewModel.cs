// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;

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

		/// <summary>Time spent in generating this site, formatted as a string.</summary>
		public string GenerationTime { get; set; }

		/// <summary>Engine versions.</summary>
		public List<string> EngineVersions;
	}
}