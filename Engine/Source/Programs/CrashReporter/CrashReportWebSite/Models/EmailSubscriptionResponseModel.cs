using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
    /// <summary>
    /// The View Model for email reports sign up response
    /// This is used for the partial view used in the partial page update response.
    /// </summary>
    public class EmailSubscriptionResponseModel
    {
        /// <summary>
        /// The email address to which the weekly report will be sent.
        /// </summary>
        public string Email { get; set; }

        /// <summary>
        /// The branch for which the report should be selected.
        /// </summary>
        public string Branch { get; set; }
    };
}