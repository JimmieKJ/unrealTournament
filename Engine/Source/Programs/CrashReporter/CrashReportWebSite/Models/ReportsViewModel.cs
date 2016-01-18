// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// The view model for the crash summary page.
	/// </summary>
	public class ReportsViewModel
	{
		/// <summary>A container of sorted Buggs.</summary>
		public List<Bugg> Buggs { get; set; }

		/// <summary>A container of sorted Crashes.</summary>
		public List<Crash> Crashes { get; set; }

		/// <summary>The date of the earliest crash in a Bugg.</summary>
		public long DateFrom { get; set; }

		/// <summary>The date of the most recent crash in a Bugg.</summary>
		public long DateTo { get; set; }

		/// <summary></summary>
		public int TotalAnonymousCrashes { get; set; }

		/// <summary></summary>
		public int TotalUniqueAnonymousCrashes { get; set; }

		/// <summary></summary>
		public int TotalAffectedUsers { get; set; }

		/// <summary>Time spent in generating this site, formatted as a string.</summary>
		public string GenerationTime { get; set; }

		/// <summary>User input from the client.</summary>
		public FormCollection FormCollection { get; set; }
	}
}