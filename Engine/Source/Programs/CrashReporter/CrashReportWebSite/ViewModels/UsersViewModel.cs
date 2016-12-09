// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.ViewModels
{
	/// <summary>
	/// The view model for the user administrative page.
	/// </summary>
	public class UsersViewModel
	{
		/// <summary>The currently selected user group.</summary>
		public string UserGroup { get; set; }

        public PagingInfo PagingInfo { get; set; }

		/// <summary>The currently selected user group.</summary>
		public string User { get; set; }

		/// <summary>The number of users in each group.</summary>
		public Dictionary<string, int> GroupCounts { get; set; }

        /// <summary>Select list of usergroup Ids</summary>
	    public List<SelectListItem> GroupSelectList { get; set; }

        /// <summary>
        /// List of users to display in page
        /// </summary>
        public List<UserViewModel> Users { get; set; } 
	}

    public class UserViewModel
    {
        public string Name { get; set; }

        public string UserGroup { get; set; }
    }
}