// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// The view model for the user administrative page.
	/// </summary>
	public class UsersViewModel
	{
		/// <summary>The currently selected user group.</summary>
		public string UserGroup { get; set; }

		/// <summary>The currently selected user group.</summary>
		public HashSet<string> Users { get; set; }

		/// <summary>The number of users in each group.</summary>
		public Dictionary<string, int> GroupCounts { get; set; }
	}
}