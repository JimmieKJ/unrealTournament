// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels;

namespace Tools.CrashReporter.CrashReportWebSite.ViewModels
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

        /// <summary>The name of the branch to filter by.</summary>
        public string BranchName { get; set; }

        /// <summary>A collection of Branch names used in the drop down on the main search form</summary>
        public List<SelectListItem> BranchNames { get; set; }

        /// <summary>
        /// Default constructor, used by ReportsController
        /// </summary>
        public ReportsViewModel()
        {
            
        }
    }
}