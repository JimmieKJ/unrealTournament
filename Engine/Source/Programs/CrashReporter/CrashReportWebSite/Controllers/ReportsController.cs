// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Mail;
using System.Web;
using System.Web.Mvc;
using System.Web.Routing;
using Tools.CrashReporter.CrashReportWebSite.DataModels;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.Models;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;
using Hangfire;
using FormCollection = System.Web.Mvc.FormCollection;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{

	//How hard would it be for you to create a CSV of:
	// Epic ID, 
	// buggs ID, 
	// engine version, 
	// 
	// # of crashes with that buggs ID for 
	//	this engine version and 
	//	user

    /// <summary>
	/// The controller to handle graphing of crashes per user group over time.
	/// </summary>
	public class ReportsController : Controller
    {
        #region Public Methods

        private IUnitOfWork _unitOfWork;

        /// <summary>
        /// 
        /// </summary>
        public ReportsController(IUnitOfWork unitOfWork)
        {
            _unitOfWork = unitOfWork;
        }

        /// <summary>Fake id for all user groups</summary>
		public static readonly int AllUserGroupId = -1;
        
		/// <summary>
		/// Get a report with the default form data and return the reports index view
		/// </summary>
		/// <param name="ReportsForm"></param>
		/// <returns></returns>
		public ActionResult Index( FormCollection ReportsForm )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				FormHelper FormData = new FormHelper( Request, ReportsForm, "JustReport" );

				// Handle 'CopyToJira' button
				int BuggIDToBeAddedToJira = 0;
				foreach( var Entry in ReportsForm )
				{
                    if (Entry.ToString().Contains("CopyToJira-"))
					{
                        int.TryParse(Entry.ToString().Substring("CopyToJira-".Length), out BuggIDToBeAddedToJira);
						break;
					}
				}
                
				var results = GetResults( FormData, BuggIDToBeAddedToJira );
				results.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", results );
			}
		}

	    /// <summary>
	    /// Return to the index view with a report for a specific branch
	    /// </summary>
	    /// <param name="ReportsForm"></param>
	    /// <returns></returns>
	    public ActionResult BranchReport(FormCollection ReportsForm)
	    {
            using (var logTimer = new FAutoScopedLogTimer(this.GetType().ToString(), bCreateNewLog: true))
            {
                var formData = new FormHelper(Request, ReportsForm, "JustReport");
                
                var buggIdToBeAddedToJira = 0;
                // Handle 'CopyToJira' button
                foreach (var Entry in ReportsForm)
                {
                    if (Entry.ToString().Contains("CopyToJira-"))
                    {
                        
                        int.TryParse(Entry.ToString().Substring("CopyToJira-".Length), out buggIdToBeAddedToJira);
                        break;
                    }
                }

                var results = GetResults(formData, buggIdToBeAddedToJira);
                results.GenerationTime = logTimer.GetElapsedSeconds().ToString("F2");
                return View("Index", results);
            }
        }

        /// <summary>
        /// Subscribe to a weekly email with a report for a specific branch over the last week.
        /// </summary>
        /// <returns>
        /// A partial view with a confirmation massge to be displayed in the source page.
        /// </returns>
        public PartialViewResult SubscribeToEmail(string branchName, string emailAddress)
        {
            var jobId = string.Format("{0}:{1}", branchName, emailAddress);

            RecurringJob.AddOrUpdate<ReportsController>(jobId, x => x.SendReportsEmail(emailAddress, branchName), Cron.Weekly);

            return PartialView("SignUpResponse", new EmailSubscriptionResponseModel () { Email = emailAddress, Branch = branchName } );
        }
        
        /// <summary>
        /// Unsubscribe from a weekly e-mail report for a branch
        /// </summary>
        /// <param name="jobId"></param>
        /// <returns></returns>
        public ActionResult UnsubscribeFromEmail(string jobId)
        {
            RecurringJob.RemoveIfExists(jobId);

            ViewData["BranchName"] = jobId.Split(':')[0];
            ViewData["EmailAddress"] = jobId.Split(':')[1];

            return View("Unsubscribe");
        }
        
        /// <summary>
        /// Private method called by hangfire to send individual report e-mails.
        /// </summary>
        /// <param name="emailAddress"></param>
        /// <param name="branchName"></param>
        public void SendReportsEmail(string emailAddress, string branchName)
        {
            var emailBody = string.Format("{0}" + Environment.NewLine + "{1}{2}%3A{3}",
                GetReportsEmailContents(branchName),
                "http://crashreporter.epicgames.net/Reports/UnsubscribeFromEmail?jobId=",
                branchName,
                emailAddress);
            var sMail = new SmtpClient();

            var message = new MailMessage("crashreporter@epicgames.com", emailAddress, "Weekly Crash Report : " + branchName, emailBody ) { IsBodyHtml = true };
            sMail.Send(message);
        }

        /// <summary>
        /// Test method - DELETE ME!
        /// </summary>
        public void GetJiraIssueSpec()
        {
            var jiraConnection = JiraConnection.Get();

            if (!jiraConnection.CanBeUsed())
                return;

            var jiraResponse = jiraConnection.JiraRequest(
                "/issue/createmeta?projectKeys=UE&issuetypeName=Bug&expand=projects.issuetypes.fields",
                JiraConnection.JiraMethod.GET, null, HttpStatusCode.OK);

            var projectSpec = JiraConnection.ParseJiraResponse(jiraResponse);

            var projects = ((ArrayList)projectSpec["projects"]);

            ArrayList issueTypes = null;

            foreach (var project in projects)
            {
                var kvp = (KeyValuePair<string, object>)project;
                if ( project == "issuetypes")
                {
                    issueTypes = kvp.Value as ArrayList;
                }

            }

            if (issueTypes != null)
            {
                var bug = issueTypes[0];


            }

            //var issuetypes = projdic["issuetypes"];

            //var issues = (Dictionary<string, object>) issuetypes;

            //var bug = (Dictionary<string, object>)issues.First().Value;

            //var fields = (Dictionary<string, object>)bug["fields"];
        }

        #endregion

        #region Private Methods
        
        private void AddBuggJiraMapping(Bugg NewBugg, ref HashSet<string> FoundJiras, ref Dictionary<string, List<Bugg>> JiraIDtoBugg)
        {
            string JiraID = NewBugg.TTPID;
            FoundJiras.Add("key = " + JiraID);

            bool bAdd = !JiraIDtoBugg.ContainsKey(JiraID);
            if (bAdd)
            {
                JiraIDtoBugg.Add(JiraID, new List<Bugg>());
            }

            JiraIDtoBugg[JiraID].Add(NewBugg);
        }

        /// <summary>
		/// Retrieve all Buggs matching the search criteria.
		/// </summary>
		/// <param name="FormData">The incoming form of search criteria from the client.</param>
		/// <param name="BuggIDToBeAddedToJira">ID of the bugg that will be added to JIRA</param>
		/// <returns>A view to display the filtered Buggs.</returns>
		private ReportsViewModel GetResults( FormHelper FormData, int BuggIDToBeAddedToJira )
		{
            var branchName = FormData.BranchName ?? "";
            var startDate = FormData.DateFrom;
            var endDate = FormData.DateTo;

            return GetResults(branchName, startDate, endDate, BuggIDToBeAddedToJira);
		}

	    /// <summary>
        /// Retrieve all Buggs matching the search criteria.
	    /// </summary>
	    /// <returns></returns>
        private ReportsViewModel GetResults(string branchName, DateTime startDate, DateTime endDate, int BuggIDToBeAddedToJira)
	    {
	        // It would be great to have a CSV export of this as well with buggs ID being the key I can then use to join them :)
	        // 
	        // Enumerate JIRA projects if needed.
	        // https://jira.ol.epicgames.net//rest/api/2/project
	        var JC = JiraConnection.Get();
	        var JiraComponents = JC.GetNameToComponents();
	        var JiraVersions = JC.GetNameToVersions();

	        using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer(this.GetType().ToString()))
	        {
	            string AnonumousGroup = "Anonymous";
	            //List<String> Users = FRepository.Get().GetUserNamesFromGroupName( AnonumousGroup );
	            int AnonymousGroupID = _unitOfWork.UserGroupRepository.First(data => data.Name == "Anonymous").Id;
	            List<int> AnonumousIDs =
                    _unitOfWork.UserGroupRepository.First(data => data.Name == "Anonymous").Users.Select(data => data.Id).ToList();
	            int AnonymousID = AnonumousIDs.First();

	            var CrashesQuery = _unitOfWork.CrashRepository.ListAll().Where(data => data.TimeOfCrash > startDate && data.TimeOfCrash <= endDate)
	                // Only crashes and asserts
	                .Where(Crash => Crash.CrashType == 1 || Crash.CrashType == 2)
	                // Only anonymous user
	                .Where(Crash => Crash.UserNameId.Value == AnonymousID);

	            // Filter by BranchName
	            if (!string.IsNullOrEmpty(branchName))
	            {
	                CrashesQuery =
	                    (
	                        from CrashDetail in CrashesQuery
	                        where CrashDetail.Branch.Equals(branchName)
	                        select CrashDetail
	                        );
	            }

	            var crashes = CrashesQuery.Select(data => new
	            {
                    ID = data.Id,
                    TimeOfCrash = data.TimeOfCrash.Value,
	                //UserID = Crash.UserNameId.Value, 
                    BuildVersion = data.BuildVersion,
                    JIRA = data.Jira,
                    Platform = data.PlatformName,
                    FixCL = data.FixedChangeList,
                    BuiltFromCL = data.ChangeListVersion,
                    Pattern = data.Pattern,
                    MachineID = data.ComputerName,
                    Branch = data.Branch,
                    Description = data.Description,
                    RawCallStack = data.RawCallStack,
	            }).ToList();
                var numCrashes = CrashesQuery.Count();

	            /*
                // Build patterns for crashes where patters is null.
                var CrashesWithoutPattern = FRepository.Get().Crashes
                    .FilterByDate( FRepository.Get().Crashes.ListAll(), startDate, endDate.AddDays( 1 ) )
                    // Only crashes and asserts
                    .Where( Crash => Crash.Pattern == null || Crash.Pattern == "" )
                    .Select( Crash => Crash )
                    .ToList();

                foreach( var Crash in CrashesWithoutPattern )
                {
                    Crash.BuildPattern();
                }
                */

	            // Total # of ALL (Anonymous) crashes in timeframe
	            int TotalAnonymousCrashes = numCrashes;

	            // Total # of UNIQUE (Anonymous) crashes in timeframe
	            HashSet<string> UniquePatterns = new HashSet<string>();
	            HashSet<string> UniqueMachines = new HashSet<string>();
	            Dictionary<string, int> PatternToCount = new Dictionary<string, int>();

	            //List<int> CrashesWithoutPattern = new List<int>();
	            //List<DateTime> CrashesWithoutPatternDT = new List<DateTime>();

	            foreach (var Crash in crashes)
	            {
	                if (string.IsNullOrEmpty(Crash.Pattern))
	                {
	                    //CrashesWithoutPattern.Add( Crash.ID );
	                    //CrashesWithoutPatternDT.Add( Crash.TimeOfCrash );
	                    continue;
	                }

	                UniquePatterns.Add(Crash.Pattern);
	                UniqueMachines.Add(Crash.MachineID);

	                bool bAdd = !PatternToCount.ContainsKey(Crash.Pattern);
	                if (bAdd)
	                {
	                    PatternToCount.Add(Crash.Pattern, 1);
	                }
	                else
	                {
	                    PatternToCount[Crash.Pattern]++;
	                }
	            }
	            var PatternToCountOrdered = PatternToCount.OrderByDescending(X => X.Value).ToList();
	            const int NumTopRecords = 200;
	            var PatternAndCount = PatternToCountOrdered.Take(NumTopRecords).ToDictionary(X => X.Key, Y => Y.Value);

	            int TotalUniqueAnonymousCrashes = UniquePatterns.Count;

	            // Total # of AFFECTED USERS (Anonymous) in timeframe
	            int TotalAffectedUsers = UniqueMachines.Count;

	            var RealBuggs = _unitOfWork.BuggRepository.ListAll().Where(Bugg => PatternAndCount.Keys.Contains(Bugg.Pattern));

	            // Build search string.
	            HashSet<string> FoundJiras = new HashSet<string>();
	            Dictionary<string, List<Bugg>> JiraIDtoBugg = new Dictionary<string, List<Bugg>>();

	            List<Bugg> Buggs = new List<Bugg>(NumTopRecords);
	            foreach (var Top in PatternAndCount)
	            {
	                Bugg NewBugg = RealBuggs.FirstOrDefault(X => X.Pattern == Top.Key);
	                if (NewBugg != null)
	                {
	                    using (
	                        FAutoScopedLogTimer TopTimer =
	                            new FAutoScopedLogTimer(string.Format("{0}:{1}", Buggs.Count + 1, NewBugg.Id)))
	                    {
	                        var CrashesForBugg = crashes.Where(Crash => Crash.Pattern == Top.Key).ToList();

	                        // Convert anonymous to full type;
	                        var FullCrashesForBugg = new List<Crash>(CrashesForBugg.Count);
	                        foreach (var Anon in CrashesForBugg)
	                        {
	                            FullCrashesForBugg.Add(new Crash()
	                            {
	                                Id = Anon.ID,
	                                TimeOfCrash = Anon.TimeOfCrash,
	                                BuildVersion = Anon.BuildVersion,
	                                Jira = Anon.JIRA,
	                                PlatformName = Anon.Platform,
	                                FixedChangeList = Anon.FixCL,
	                                ChangeListVersion = Anon.BuiltFromCL,
	                                Pattern = Anon.Pattern,
	                                ComputerName = Anon.MachineID,
	                                Branch = Anon.Branch,
	                                Description = Anon.Description,
	                                RawCallStack = Anon.RawCallStack,
	                            });
	                        }

	                        NewBugg.PrepareBuggForJira(FullCrashesForBugg);

	                        // Verify valid JiraID, this may be still a TTP 
	                        if (!string.IsNullOrEmpty(NewBugg.TTPID))
	                        {
	                            int TTPID = 0;
	                            int.TryParse(NewBugg.TTPID, out TTPID);

	                            if (TTPID == 0)
	                            {
	                                AddBuggJiraMapping(NewBugg, ref FoundJiras, ref JiraIDtoBugg);
	                            }
	                        }

	                        Buggs.Add(NewBugg);
	                    }
	                }
	                else
	                {
	                    FLogger.Global.WriteEvent("Bugg for pattern " + Top.Key + " not found");
	                }
	            }

	            if (BuggIDToBeAddedToJira > 0)
	            {
                    var Bugg = Buggs.Where(X => X.Id == BuggIDToBeAddedToJira).FirstOrDefault();
                    if (Bugg != null)
                    {
                        Bugg.CopyToJira();
                        AddBuggJiraMapping(Bugg, ref FoundJiras, ref JiraIDtoBugg);
                    }
	            }

	            if (JC.CanBeUsed())
	            {
	                var BuggsCopy = new List<Bugg>(Buggs);

	                HashSet<string> InvalidJiras = new HashSet<string>();

	                // Simple verification of JIRA
	                foreach (var Value in FoundJiras)
	                {
	                    if (Value.Length < 3 || !Value.Contains('-'))
	                    {
	                        InvalidJiras.Add(Value);
	                    }
	                }

	                foreach (var InvalidJira in InvalidJiras)
	                {
	                    FoundJiras.Remove(InvalidJira);
	                }

	                // Grab the data form JIRA.
	                string JiraSearchQuery = string.Join(" OR ", FoundJiras);

	                using (FAutoScopedLogTimer JiraResultsTimer = new FAutoScopedLogTimer("JiraResults"))
	                {
	                    bool bInvalid = false;
	                    var JiraResults = new Dictionary<string, Dictionary<string, object>>();
	                    try
	                    {
	                        if (!string.IsNullOrWhiteSpace(JiraSearchQuery))
	                        {
	                            JiraResults = JC.SearchJiraTickets(
	                                JiraSearchQuery,
	                                new string[]
	                                {
	                                    "key", // string
	                                    "summary", // string
	                                    "components", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                    "resolution",
	                                    // System.Collections.Generic.Dictionary`2[System.String,System.Object], name
	                                    "fixVersions", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                    "customfield_11200" // string
	                                });
	                        }
	                    }
	                    catch (System.Exception)
	                    {
	                        bInvalid = true;
	                    }

	                    // Invalid records have been found, find the broken using the slow path.
	                    if (bInvalid)
	                    {
	                        foreach (var Query in FoundJiras)
	                        {
	                            try
	                            {
	                                var TempResult = JC.SearchJiraTickets(
	                                    Query,
	                                    new string[]
	                                    {
	                                        "key", // string
	                                        "summary", // string
	                                        "components", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                        "resolution",
	                                        // System.Collections.Generic.Dictionary`2[System.String,System.Object], name
	                                        "fixVersions", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                        "customfield_11200" // string
	                                    });

	                                foreach (var Temp in TempResult)
	                                {
	                                    JiraResults.Add(Temp.Key, Temp.Value);
	                                }
	                            }
	                            catch (System.Exception)
	                            {

	                            }
	                        }
	                    }

	                    // Jira Key, Summary, Components, Resolution, Fix version, Fix changelist
	                    foreach (var Jira in JiraResults)
	                    {
	                        string JiraID = Jira.Key;

	                        string Summary = (string) Jira.Value["summary"];

	                        string ComponentsText = "";
	                        System.Collections.ArrayList Components =
	                            (System.Collections.ArrayList) Jira.Value["components"];
	                        foreach (Dictionary<string, object> Component in Components)
	                        {
	                            ComponentsText += (string) Component["name"];
	                            ComponentsText += " ";
	                        }

	                        Dictionary<string, object> ResolutionFields =
	                            (Dictionary<string, object>) Jira.Value["resolution"];
	                        string Resolution = ResolutionFields != null ? (string) ResolutionFields["name"] : "";

	                        string FixVersionsText = "";
	                        System.Collections.ArrayList FixVersions =
	                            (System.Collections.ArrayList) Jira.Value["fixVersions"];
	                        foreach (Dictionary<string, object> FixVersion in FixVersions)
	                        {
	                            FixVersionsText += (string) FixVersion["name"];
	                            FixVersionsText += " ";
	                        }

	                        int FixCL = Jira.Value["customfield_11200"] != null
	                            ? (int) (decimal) Jira.Value["customfield_11200"]
	                            : 0;

	                        List<Bugg> BuggsForJira;
	                        JiraIDtoBugg.TryGetValue(JiraID, out BuggsForJira);

	                        //var BuggsForJira = JiraIDtoBugg[JiraID];

	                        if (BuggsForJira != null)
	                        {
	                            foreach (Bugg Bugg in BuggsForJira)
	                            {
	                                Bugg.JiraSummary = Summary;
	                                Bugg.JiraComponentsText = ComponentsText;
	                                Bugg.JiraResolution = Resolution;
	                                Bugg.JiraFixVersionsText = FixVersionsText;
	                                if (FixCL != 0)
	                                {
	                                    Bugg.JiraFixCL = FixCL.ToString();
	                                }

	                                BuggsCopy.Remove(Bugg);
	                            }
	                        }
	                    }
	                }

	                // If there are buggs, we need to update the summary to indicate an error.
	                // Usually caused when bugg's project has changed.
	                foreach (var Bugg in BuggsCopy.Where(b => !string.IsNullOrEmpty(b.TTPID)))
	                {
	                    Bugg.JiraSummary = "JIRA MISMATCH";
	                    Bugg.JiraComponentsText = "JIRA MISMATCH";
	                    Bugg.JiraResolution = "JIRA MISMATCH";
	                    Bugg.JiraFixVersionsText = "JIRA MISMATCH";
	                    Bugg.JiraFixCL = "JIRA MISMATCH";
	                }
	            }

	            Buggs = Buggs.OrderByDescending(b => b.CrashesInTimeFrameGroup).ToList();

	            return new ReportsViewModel
	            {
	                Buggs = Buggs,
                    BranchName = branchName,
	                BranchNames = _unitOfWork.CrashRepository.GetBranchesAsListItems(),
	                DateFrom = (long) (startDate - CrashesViewModel.Epoch).TotalMilliseconds,
	                DateTo = (long) (endDate - CrashesViewModel.Epoch).TotalMilliseconds,
	                TotalAffectedUsers = TotalAffectedUsers,
	                TotalAnonymousCrashes = TotalAnonymousCrashes,
	                TotalUniqueAnonymousCrashes = TotalUniqueAnonymousCrashes
	            };
	        }
	    }

        /// <summary>
        /// Private method to get reports page for a branch and render to an HTML string
        /// </summary>
        /// <returns>HTML formatted string</returns>
	    private string GetReportsEmailContents(string branchName)
	    {
            var reportViewModel = GetResults(branchName, DateTime.Now.AddDays(-7), DateTime.Now, 0);
            return RenderViewToString("Reports", "~/Views/Reports/ViewReports.ascx", reportViewModel);
	    }

        /// <summary>
        /// Uses a "fake" HTTPContext to generate html output from a view or partial view.
        /// 
        /// Essentially this function returns view data at any time from any part of the program 
        /// without having to call into a view from a normal controller context.
        /// This is important to allow us to return view data asynchronously.
        /// </summary>
        /// <param name="controllerName">A string indicating the name of the controller that normally calls into a view</param>
        /// <param name="viewName">the path to the view in quiestion</param>
        /// <param name="viewData">the model to be passed to the view</param>
        /// <returns>HTML formatted string</returns>
        private static string RenderViewToString(string controllerName, string viewName, object viewData)
        {
            using (var writer = new StringWriter())
            {
                var routeData = new RouteData();
                routeData.Values.Add("controller", controllerName);
                var fakeControllerContext = new ControllerContext(new HttpContextWrapper(new HttpContext(new HttpRequest(null, "http://google.com", null), new HttpResponse(null))), routeData, new FakeController());
                var razorViewResult = ViewEngines.Engines.FindPartialView(fakeControllerContext, viewName);

                var viewContext = new ViewContext(fakeControllerContext, razorViewResult.View, new ViewDataDictionary(viewData), new TempDataDictionary(), writer);
                razorViewResult.View.Render(viewContext, writer);
                return writer.ToString();
            }
        }

        #endregion
    }

    /// <summary>
    /// Empty Controller class used as proxy 
    /// </summary>
    class FakeController : ControllerBase
    {
        protected override void ExecuteCore()
        {
        }
    }
}