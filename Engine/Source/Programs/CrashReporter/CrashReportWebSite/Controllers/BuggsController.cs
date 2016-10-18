// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Linq.Expressions;
using System.Web;
using System.Web.Mvc;
using Microsoft.Practices.ObjectBuilder2;
using Tools.CrashReporter.CrashReportWebSite.DataModels;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// A controller to handle the Bugg data.
	/// </summary>
	public class BuggsController : Controller
	{
        private IUnitOfWork _unitOfWork;

		/// <summary>
		/// An empty constructor.
		/// </summary>
		public BuggsController(IUnitOfWork unitOfWork)
		{
		    _unitOfWork = unitOfWork;
		}

		/// <summary>
		/// The Index action.
		/// </summary>
		/// <param name="BuggsForm">The form of user data passed up from the client.</param>
		/// <returns>The view to display a list of Buggs on the client.</returns>
		public ActionResult Index( FormCollection BuggsForm )
		{
			using (var logTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ))
			{
				var formData = new FormHelper( Request, BuggsForm, "CrashesInTimeFrameGroup" );
			    var results = GetResults( formData );
				foreach (var bugg in results.Results)
				{
					// Populate function calls.
				    //Bugg.FunctionCalls = new CallStackContainer(Bugg.Crashes.First()).GetFunctionCalls();
				}
				results.GenerationTime = logTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", results );
			}
		}

		/// <summary>
		/// The Show action.
		/// </summary>
		/// <param name="BuggsForm">The form of user data passed up from the client.</param>
		/// <param name="id">The unique id of the Bugg.</param>
		/// <returns>The view to display a Bugg on the client.</returns>
		public ActionResult Show( FormCollection BuggsForm, int id )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(BuggId=" + id + ")", bCreateNewLog: true ) )
			{
				// Handle 'CopyToJira' button
				int BuggIDToBeAddedToJira = 0;
				foreach( var Entry in BuggsForm )
				{
					if( Entry.ToString().Contains( "CopyToJira-" ) )
					{
                        int.TryParse(Entry.ToString().Substring("CopyToJira-".Length), out BuggIDToBeAddedToJira);
						break;
					}
				}

				// Set the display properties based on the radio buttons
				bool DisplayModuleNames = false;
				if( BuggsForm["DisplayModuleNames"] == "true" )
				{
					DisplayModuleNames = true;
				}

				bool DisplayFunctionNames = false;
				if( BuggsForm["DisplayFunctionNames"] == "true" )
				{
					DisplayFunctionNames = true;
				}

				bool DisplayFileNames = false;
				if( BuggsForm["DisplayFileNames"] == "true" )
				{
					DisplayFileNames = true;
				}

				bool DisplayFilePathNames = false;
				if( BuggsForm["DisplayFilePathNames"] == "true" )
				{
					DisplayFilePathNames = true;
					DisplayFileNames = false;
				}

				bool DisplayUnformattedCallStack = false;
				if( BuggsForm["DisplayUnformattedCallStack"] == "true" )
				{
					DisplayUnformattedCallStack = true;
				}

				// Create a new view and populate with crashes
				List<Crash> Crashes = null;
				BuggViewModel Model = new BuggViewModel();
				Bugg newBugg = _unitOfWork.BuggRepository.GetById( id );
				if( newBugg == null )
				{
					return RedirectToAction( "Index" );
				}

				Crashes = newBugg.Crashes.OrderByDescending(data => data.TimeOfCrash).ToList();

				using (FAutoScopedLogTimer GetCrashesTimer = new FAutoScopedLogTimer( "Bugg.PrepareBuggForJira" ))
				{
					if (Crashes.Count > 0)
					{
						newBugg.PrepareBuggForJira( Crashes );

						if (BuggIDToBeAddedToJira != 0)
						{
							newBugg.CopyToJira();
						}
					}
				}

				using( FAutoScopedLogTimer JiraResultsTimer = new FAutoScopedLogTimer( "Bugg.GrabJira" ) )
				{
					var JC = JiraConnection.Get();
					bool bValidJira = false;

					// Verify valid JiraID, this may be still a TTP
					if( !string.IsNullOrEmpty( newBugg.TTPID ) )
					{
						var jira = 0;
                        int.TryParse(newBugg.TTPID, out jira);

                        if (jira == 0)
						{
							//AddBuggJiraMapping( NewBugg, ref FoundJiras, ref JiraIDtoBugg );
							bValidJira = true;
						}
					}

					if( JC.CanBeUsed() && bValidJira )
					{
						// Grab the data form JIRA.
						string JiraSearchQuery = "key = " + newBugg.TTPID;
						var JiraResults = new Dictionary<string, Dictionary<string, object>>();

						try
						{
							JiraResults = JC.SearchJiraTickets(
							JiraSearchQuery,
							new string[] 
							{ 
								"key",				// string
								"summary",			// string
								"components",		// System.Collections.ArrayList, Dictionary<string,object>, name
								"resolution",		// System.Collections.Generic.Dictionary`2[System.String,System.Object], name
								"fixVersions",		// System.Collections.ArrayList, Dictionary<string,object>, name
								"customfield_11200" // string
							} );

						}
						catch (System.Exception)
						{
                            newBugg.JiraSummary = "JIRA MISMATCH";
                            newBugg.JiraComponentsText = "JIRA MISMATCH";
                            newBugg.JiraResolution = "JIRA MISMATCH";
                            newBugg.JiraFixVersionsText = "JIRA MISMATCH";
                            newBugg.JiraFixCL = "JIRA MISMATCH";
						}

						// Jira Key, Summary, Components, Resolution, Fix version, Fix changelist
						foreach( var jira in JiraResults )
						{
							string jiraId = jira.Key;

							var summary = (string)jira.Value["summary"];

							string ComponentsText = "";
							System.Collections.ArrayList Components = (System.Collections.ArrayList)jira.Value["components"];
							foreach( Dictionary<string, object> Component in Components )
							{
								ComponentsText += (string)Component["name"];
								ComponentsText += " ";
							}

							Dictionary<string, object> ResolutionFields = (Dictionary<string, object>)jira.Value["resolution"];
							string Resolution = ResolutionFields != null ? (string)ResolutionFields["name"] : "";

							string FixVersionsText = "";
							System.Collections.ArrayList FixVersions = (System.Collections.ArrayList)jira.Value["fixVersions"];
							foreach( Dictionary<string, object> FixVersion in FixVersions )
							{
								FixVersionsText += (string)FixVersion["name"];
								FixVersionsText += " ";
							}

							int FixCL = jira.Value["customfield_11200"] != null ? (int)(decimal)jira.Value["customfield_11200"] : 0;

                            //Conversion to ado.net entity framework

                            newBugg.JiraSummary = summary;
                            newBugg.JiraComponentsText = ComponentsText;
                            newBugg.JiraResolution = Resolution;
                            newBugg.JiraFixVersionsText = FixVersionsText;
							if( FixCL != 0 )
							{
								newBugg.FixedChangeList = FixCL.ToString();
							}
							
							break;
						}
					}
				}

				// Apply any user settings
				if( BuggsForm.Count > 0 )
				{
					if( !string.IsNullOrEmpty( BuggsForm["SetStatus"] ) )
					{
						newBugg.Status = BuggsForm["SetStatus"];
                        newBugg.Crashes.ForEach(data =>data.Status = BuggsForm["SetStatus"]);
					}

					if( !string.IsNullOrEmpty( BuggsForm["SetFixedIn"] ) )
					{
						newBugg.FixedChangeList = BuggsForm["SetFixedIn"];
                        newBugg.Crashes.ForEach(data => data.FixedChangeList = BuggsForm["SetFixedIn"]);
					}

					if( !string.IsNullOrEmpty( BuggsForm["SetTTP"] ) )
					{
						newBugg.TTPID = BuggsForm["SetTTP"];
                        newBugg.Crashes.ForEach(data => data.Jira = BuggsForm["SetTTP"]);
					}

                    _unitOfWork.BuggRepository.Update(newBugg);
                    _unitOfWork.Save();
				}

				// Set up the view model with the crash data
				Model.Crashes = Crashes;
			    Model.Bugg = newBugg;

				Crash NewCrash = Model.Crashes.FirstOrDefault();
				if( NewCrash != null )
				{
					using( FScopedLogTimer LogTimer2 = new FScopedLogTimer( "CallstackTrimming" ) )
					{
						CallStackContainer CallStack = new CallStackContainer( NewCrash );

						// Set callstack properties
						CallStack.bDisplayModuleNames = DisplayModuleNames;
						CallStack.bDisplayFunctionNames = DisplayFunctionNames;
						CallStack.bDisplayFileNames = DisplayFileNames;
						CallStack.bDisplayFilePathNames = DisplayFilePathNames;
						CallStack.bDisplayUnformattedCallStack = DisplayUnformattedCallStack;

						Model.CallStack = CallStack;

						// Shorten very long function names.
						foreach( CallStackEntry Entry in Model.CallStack.CallStackEntries )
						{
							Entry.FunctionName = Entry.GetTrimmedFunctionName( 128 );
						}

						Model.SourceContext = NewCrash.SourceContext;
					}

					Model.LatestCrashSummary = NewCrash.Summary;
				}
                Model.LatestCrashSummary = NewCrash.Summary;
                Model.Bugg.LatestCrashSummary = NewCrash.Summary;
				Model.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Show", Model );
			}
		}

        /// <summary>
        /// Retrieve all Buggs matching the search criteria.
        /// </summary>
        /// <param name="FormData">The incoming form of search criteria from the client.</param>
        /// <returns>A view to display the filtered Buggs.</returns>
        public BuggsViewModel GetResults(FormHelper FormData)
        {
            using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer(this.GetType().ToString()))
            {
                // Right now we take a Result IQueryable starting with ListAll() Buggs then we widdle away the result set by tacking on
                // Linq queries. Essentially it's Results.ListAll().Where().Where().Where().Where().Where().Where()
                // Could possibly create more optimized queries when we know exactly what we're querying
                // The downside is that if we add more parameters each query may need to be updated.... Or we just add new case statements
                // The other downside is that there's less code reuse, but that may be worth it.

                var fromDate = FormData.DateFrom;
                var toDate = FormData.DateTo.AddDays(1);

                IQueryable<Bugg> results = null;
                var skip = (FormData.Page - 1) * FormData.PageSize;
                var take = FormData.PageSize;

                var userGroupId = _unitOfWork.UserGroupRepository.First(data => data.Name == FormData.UserGroup).Id;

                // Get every Bugg.
                var resultsAll = _unitOfWork.BuggRepository.ListAll();

                // Look at all Buggs that are still 'open' i.e. the last crash occurred in our date range.
                results = FilterByDate(resultsAll, FormData.DateFrom, FormData.DateTo);

                // Filter results by build version.
                if (!string.IsNullOrEmpty(FormData.VersionName))
                {
                    results = FilterByBuildVersion(results, FormData.VersionName);
                }

                // Filter by BranchName
                if (!string.IsNullOrEmpty(FormData.BranchName))
                {
                    results =
                        results.Where(
                            data => data.Crashes.Any(da => da.Branch.Equals(FormData.BranchName)));
                }

                // Filter by PlatformName
                if (!string.IsNullOrEmpty(FormData.PlatformName))
                {
                    results =
                        results.Where(
                            data => data.Crashes.Any(da => da.PlatformName.Equals(FormData.PlatformName)));
                }

                // Run at the end
                if (!string.IsNullOrEmpty(FormData.SearchQuery))
                {
                    results = FilterByCallstack(results, FormData.SearchQuery);
                }

                // Filter by Crash Type
                if (FormData.CrashType != "All")
                {
                    switch (FormData.CrashType)
                    {
                        case "Crashes":
                            results = results.Where(buggInstance => buggInstance.CrashType == 1);
                            break;
                        case "Assert":
                            results = results.Where(buggInstance => buggInstance.CrashType == 2);
                            break;
                        case "Ensure":
                            results = results.Where(buggInstance => buggInstance.CrashType == 3);
                            break;
                        case "CrashesAsserts":
                            results = results.Where(buggInstance => buggInstance.CrashType == 1 || buggInstance.CrashType == 2);
                            break;
                    }
                }

                // Filter by user group if present
                if(!string.IsNullOrEmpty(FormData.UserGroup))
                    results = FilterByUserGroup(results, FormData.UserGroup);

                if (!string.IsNullOrEmpty(FormData.JiraId))
                {
                    results = FilterByJira(results, FormData.JiraId);
                }
                
                // Grab just the results we want to display on this page
                var totalCountedRecords = results.Count();
                
                var crashesByUserGroupCounts = _unitOfWork.CrashRepository.ListAll().Where(data =>
                   data.User.UserGroupId == userGroupId && 
                   data.TimeOfCrash >= fromDate &&
                   data.TimeOfCrash <= toDate &&
                   data.PatternId != null)
                   .GroupBy(data => data.PatternId)
                   .Select(data => new { data.Key, Count = data.Count() })
                   .OrderByDescending(data => data.Count)
                   .ToDictionary(data => data.Key, data => data.Count);

                var crashesInTimeFrameCounts = _unitOfWork.CrashRepository.ListAll().Where(data =>
                    data.TimeOfCrash >= fromDate &&
                    data.TimeOfCrash <= toDate
                    && data.PatternId != null)
                    .GroupBy(data => data.PatternId)
                    .Select(data => new { data.Key, Count = data.Count() })
                    .OrderByDescending(data => data.Count)
                    .ToDictionary(data => data.Key, data => data.Count);

                var affectedUsers = _unitOfWork.CrashRepository.ListAll().Where(data =>
                   data.User.UserGroupId == userGroupId &&
                   data.TimeOfCrash >= fromDate &&
                   data.TimeOfCrash <= toDate &&
                   data.PatternId != null).Select(data => new {PatternId = data.PatternId, ComputerName = data.ComputerName}).Distinct()
                   .GroupBy(data => data.PatternId)
                   .Select(data => new { data.Key, Count = data.Count() })
                   .OrderByDescending(data => data.Count)
                   .ToDictionary(data => data.Key, data => data.Count);
                
                results = GetSortedResults(results, FormData.SortTerm, FormData.SortTerm == "descending",
                FormData.DateFrom, FormData.DateTo, FormData.UserGroup);
                
                var sortedResultsList = 
                    results.ToList();

                // Get UserGroup ResultCounts
                var groups = results.SelectMany(data => data.UserGroups).GroupBy(data => data.Name).Select(data => new { Key = data.Key, Count = data.Count()}).ToDictionary(x => x.Key, y => y.Count);
                var groupCounts = new SortedDictionary<string, int>(groups);
                
                foreach (var bugg in sortedResultsList)
                {
                    if (bugg.PatternId.HasValue && crashesByUserGroupCounts.ContainsKey(bugg.PatternId.Value))
                    {
                        bugg.CrashesInTimeFrameGroup = crashesByUserGroupCounts[bugg.PatternId.Value];
                    }
                    else
                        bugg.CrashesInTimeFrameGroup = 0;

                    if (bugg.PatternId.HasValue && crashesInTimeFrameCounts.ContainsKey(bugg.PatternId.Value))
                    {
                        bugg.CrashesInTimeFrameAll = crashesInTimeFrameCounts[bugg.PatternId.Value];
                    }
                    else
                        bugg.CrashesInTimeFrameAll = 0;

                    if (bugg.PatternId.HasValue && affectedUsers.ContainsKey(bugg.PatternId.Value))
                    {
                        bugg.NumberOfUniqueMachines = affectedUsers[bugg.PatternId.Value];
                    }
                    else
                        bugg.NumberOfUniqueMachines = 0;
                }

                sortedResultsList = sortedResultsList.OrderByDescending(data => data.CrashesInTimeFrameGroup).Skip(skip)
                        .Take(totalCountedRecords >= skip + take ? take : totalCountedRecords).ToList();

                foreach (var bugg in sortedResultsList)
                {
                    var crash =
                        _unitOfWork.CrashRepository.First(data => data.Buggs.Any(bg => bg.Id == bugg.Id));
                    bugg.FunctionCalls = crash.GetCallStack().GetFunctionCalls();
                }

                return new BuggsViewModel(_unitOfWork.CrashRepository)
                {
                    Results = sortedResultsList,
                    PagingInfo = new PagingInfo { CurrentPage = FormData.Page, PageSize = FormData.PageSize, TotalResults = totalCountedRecords },
                    SortTerm = FormData.SortTerm,
                    SortOrder = FormData.SortOrder,
                    UserGroup = FormData.UserGroup,
                    CrashType = FormData.CrashType,
                    SearchQuery = FormData.SearchQuery,
                    DateFrom = (long)(FormData.DateFrom - CrashesViewModel.Epoch).TotalMilliseconds,
                    DateTo = (long)(FormData.DateTo - CrashesViewModel.Epoch).TotalMilliseconds,
                    VersionName = FormData.VersionName,
                    PlatformName = FormData.PlatformName,
                    BranchName = FormData.BranchName,
                    GroupCounts = groupCounts,
                    Jira = FormData.JiraId,
                };
            }
        }

        /// <summary>
        /// Filter a set of Buggs by a jira ticket.
        /// </summary>
        /// <param name="results">The unfiltered set of Buggs</param>
        /// <param name="jira">The ticket by which to filter the list of buggs</param>
        /// <returns></returns>
        public IQueryable<Bugg> FilterByJira(IQueryable<Bugg> results, string jira)
        {
            return results.Where(bugg => bugg.TTPID == jira);
        }

        /// <summary>
        /// Filter a set of Buggs to a date range.
        /// </summary>
        /// <param name="results">The unfiltered set of Buggs.</param>
        /// <param name="dateFrom">The earliest date to filter by.</param>
        /// <param name="dateTo">The latest date to filter by.</param>
        /// <returns>The set of Buggs between the earliest and latest date.</returns>
        public IQueryable<Bugg> FilterByDate(IQueryable<Bugg> results, DateTime dateFrom, DateTime dateTo)
        {
            dateTo = dateTo.AddDays(1);
            var buggsInTimeFrame =
                results.Where(bugg => (bugg.TimeOfFirstCrash >= dateFrom && bugg.TimeOfFirstCrash <= dateTo) ||
                    (bugg.TimeOfLastCrash >= dateFrom && bugg.TimeOfLastCrash <= dateTo) || (bugg.TimeOfFirstCrash <= dateFrom && bugg.TimeOfLastCrash >= dateTo));

            return buggsInTimeFrame;
        }

        /// <summary>
        /// Filter a set of Buggs by build version.
        /// </summary>
        /// <param name="results">The unfiltered set of Buggs.</param>
        /// <param name="buildVersion">The build version to filter by.</param>
        /// <returns>The set of Buggs that matches specified build version</returns>
        public IQueryable<Bugg> FilterByBuildVersion(IQueryable<Bugg> results, string buildVersion)
        {
            if (!string.IsNullOrEmpty(buildVersion))
            {
                results = results.Where(data => data.BuildVersion.Contains(buildVersion));
            }
            return results;
        }

        /// <summary>
        /// Filter a set of Buggs by user group name.
        /// </summary>
        /// <param name="setOfBuggs">The unfiltered set of Buggs.</param>
        /// <param name="groupName">The user group name to filter by.</param>
        /// <returns>The set of Buggs by users in the requested user group.</returns>
        public IQueryable<Bugg> FilterByUserGroup(IQueryable<Bugg> setOfBuggs, string groupName)
        {
            return setOfBuggs.Where(data => data.UserGroups.Any(ug => ug.Name == groupName));
        }

        /// <summary>
        /// Sort the container of Buggs by the requested criteria.
        /// </summary>
        /// <param name="results">A container of unsorted Buggs.</param>
        /// <param name="sortTerm">The term to sort by.</param>
        /// <param name="bSortDescending">Whether to sort by descending or ascending.</param>
        /// <param name="dateFrom">The date of the earliest Bugg to examine.</param>
        /// <param name="dateTo">The date of the most recent Bugg to examine.</param>
        /// <param name="groupName">The user group name to filter by.</param>
        /// <returns>A sorted container of Buggs.</returns>
        public IOrderedQueryable<Bugg> GetSortedResults(IQueryable<Bugg> results, string sortTerm, bool bSortDescending, DateTime dateFrom, DateTime dateTo, string groupName)
        {
            IOrderedQueryable<Bugg> orderedResults = null;

            try
            {
                switch (sortTerm)
                {
                    case "Id":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.Id, bSortDescending);
                        break;

                    case "BuildVersion":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.BuildVersion, bSortDescending);
                        break;

                    case "LatestCrash":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.TimeOfLastCrash, bSortDescending);
                        break;

                    case "FirstCrash":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.TimeOfFirstCrash, bSortDescending);
                        break;

                    case "NumberOfCrashes":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.NumberOfCrashes, bSortDescending);
                        break;

                    case "NumberOfUsers":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.NumberOfUniqueMachines, bSortDescending);
                        break;
                    case "Pattern":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.Pattern, bSortDescending);
                        break;

                    case "CrashType":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.CrashType, bSortDescending);
                        break;

                    case "Status":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.Status, bSortDescending);
                        break;

                    case "FixedChangeList":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.FixedChangeList, bSortDescending);
                        break;

                    case "TTPID":
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.TTPID, bSortDescending);
                        break;
                    default:
                        orderedResults = EnumerableOrderBy(results, buggCrashInstance => buggCrashInstance.Id, bSortDescending);
                        break;

                }
                return orderedResults;
            }
            catch (Exception ex)
            {
                Debug.WriteLine("Exception in GetSortedResults: " + ex.ToString());
            }
            return orderedResults;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <typeparam name="TKey"></typeparam>
        /// <param name="query"></param>
        /// <param name="predicate"></param>
        /// <param name="bDescending"></param>
        /// <returns></returns>
        public IOrderedQueryable<Bugg> EnumerableOrderBy<TKey>(IQueryable<Bugg> query, Expression<Func<Bugg, TKey>> predicate, bool bDescending)
        {
            return bDescending ? query.OrderByDescending(predicate) : query.OrderBy(predicate);
        }

        /// <summary>
        /// Filter the list of Buggs by a callstack entry.
        /// </summary>
        public IQueryable<Bugg> FilterByCallstack(IQueryable<Bugg> results, string callstackEntry)
        {
            try
            {
                var queryString = HttpUtility.HtmlDecode(callstackEntry.ToString()) ?? "";

                // Take out terms starting with a -
                var terms = queryString.Split("-, ;+".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
                var allFuncionCallIds = new HashSet<int>();
                foreach (var term in terms)
                {
                    var functionCallIds = _unitOfWork.FunctionRepository.Get(functionCallInstance => functionCallInstance.Call.Contains(term)).Select(x => x.Id).ToList();
                    foreach (var id in functionCallIds)
                    {
                        allFuncionCallIds.Add(id);
                    }
                }

                // Search for all function ids. OR operation, not very efficient, but for searching for one function should be ok.
                results = allFuncionCallIds.Aggregate(results, (current, id) => current.Where(x => x.Pattern.Contains(id + "+") || x.Pattern.Contains("+" + id)));
            }
            catch (Exception ex)
            {
                Debug.WriteLine("Exception in Search: " + ex.ToString());
            }
            return results;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _unitOfWork.Dispose();
            }
        }
	}
}
