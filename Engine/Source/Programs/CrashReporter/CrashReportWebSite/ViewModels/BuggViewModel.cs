// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Tools.CrashReporter.CrashReportWebSite.DataModels;

namespace Tools.CrashReporter.CrashReportWebSite.ViewModels
{
	/// <summary>
	/// The view model for the bugg show page.
	/// </summary>
	public class BuggViewModel
	{
		/// <summary>The current Bugg to display details of.</summary>
		public Bugg Bugg { get; set; }

		/// <summary>A container for all the crashes associated with this Bugg.</summary>
		public List<Crash> Crashes { get; set; }

		/// <summary>The call stack common to all crashes in this Bugg.</summary>
		public CallStackContainer CallStack { get; set; }

		/// <summary></summary>
		public string SourceContext { get; set; }

		/// <summary>Time spent in generating this site, formatted as a string.</summary>
		public string GenerationTime { get; set; }

        public string LatestCrashSummary { get; set; }
	}
}