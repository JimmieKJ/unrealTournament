// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.Entity.Validation;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Web;
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.Properties;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;
using Tools.DotNETCommon.XmlHandler;
using Tools.CrashReporter.CrashReportCommon;
using System.Data.SqlClient;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// The controller to handle the crash data.
	/// </summary>
	[HandleError]
	public class CrashesController : Controller
	{
        //Ugly instantiation of crash repository will replace with dependency injection BEFORE this gets anywhere near live.
	    private IUnitOfWork _unitOfWork;
	    private SlackWriter _slackWriter;
        
        /// <summary> Special user name, currently used to mark crashes from UE4 releases. </summary>
        const string UserNameAnonymous = "Anonymous";

		/// <summary>
		/// An empty constructor.
		/// </summary>
		public CrashesController(IUnitOfWork unitOfWork)
		{
		    _unitOfWork = unitOfWork;

		    _slackWriter = new SlackWriter()
		    {
		        WebhookUrl = Settings.Default.SlackWebhookUrl,
		        Channel = Settings.Default.SlackChannel,
		        Username = Settings.Default.SlackUsername,
		        IconEmoji = Settings.Default.SlackEmoji
		    };
		}

		/// <summary>
		/// Display a summary list of crashes based on the search criteria.
		/// </summary>
		/// <param name="crashesForm">A form of user data passed up from the client.</param>
		/// <returns>A view to display a list of crash reports.</returns>
		public ActionResult Index( FormCollection crashesForm )
		{
            using( var logTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				// Handle any edits made in the Set form fields
				foreach( var Entry in crashesForm )
				{
					int Id = 0;
					if( int.TryParse( Entry.ToString(), out Id ) )
					{
                        Crash currentCrash = _unitOfWork.CrashRepository.GetById(Id);
						if( currentCrash != null )
						{
							if( !string.IsNullOrEmpty( crashesForm["SetStatus"] ) )
							{
								currentCrash.Status = crashesForm["SetStatus"];
							}

							if( !string.IsNullOrEmpty( crashesForm["SetFixedIn"] ) )
							{
								currentCrash.FixedChangeList = crashesForm["SetFixedIn"];
							}

							if( !string.IsNullOrEmpty( crashesForm["SetTTP"] ) )
							{
								currentCrash.Jira = crashesForm["SetTTP"];
							}
						}
					}

				    _unitOfWork.Save();
				}

				// <STATUS>

				// Parse the contents of the query string, and populate the form
				var formData = new FormHelper( Request, crashesForm, "TimeOfCrash" );
				var result = GetResults( formData );
                result.BranchNames = _unitOfWork.CrashRepository.GetBranchesAsListItems();
                result.VersionNames = _unitOfWork.CrashRepository.GetVersionsAsListItems();
                result.PlatformNames = _unitOfWork.CrashRepository.GetPlatformsAsListItems();
                result.EngineModes = _unitOfWork.CrashRepository.GetEngineModesAsListItems();
                result.EngineVersions = _unitOfWork.CrashRepository.GetEngineVersionsAsListItems();

				// Add the FromCollection to the CrashesViewModel since we don't need it for the get results function but we do want to post it back to the page.
				result.FormCollection = crashesForm;
				result.GenerationTime = logTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", result );
			}
		}

		/// <summary>
		/// Show detailed information about a crash.
		/// </summary>
		/// <param name="crashesForm">A form of user data passed up from the client.</param>
		/// <param name="id">The unique id of the crash we wish to show the details of.</param>
		/// <returns>A view to show crash details.</returns>
		public ActionResult Show( FormCollection crashesForm, int id )
		{
			using( var logTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(CrashId=" + id + ")", bCreateNewLog: true ) )
			{
				CallStackContainer currentCallStack = null;

				// Update the selected crash based on the form contents
                var currentCrash = _unitOfWork.CrashRepository.GetById(id);
                
				if( currentCrash == null )
				{
					return RedirectToAction( "Index" );
				}

				string FormValue;

				FormValue = crashesForm["SetStatus"];
				if( !string.IsNullOrEmpty( FormValue ) )
				{
					currentCrash.Status = FormValue;
				}

				FormValue = crashesForm["SetFixedIn"];
				if( !string.IsNullOrEmpty( FormValue ) )
				{
					currentCrash.FixedChangeList = FormValue;
				}

				FormValue = crashesForm["SetTTP"];
				if( !string.IsNullOrEmpty( FormValue ) )
				{
					currentCrash.Jira = FormValue;
				}

				// Valid to set description to an empty string
				FormValue = crashesForm["Description"];
				if( FormValue != null )
				{
					currentCrash.Description = FormValue;
				}

				currentCallStack = new CallStackContainer( currentCrash );
			    currentCrash.Module = currentCallStack.GetModuleName();

				//Set call stack properties
				currentCallStack.bDisplayModuleNames = true;
				currentCallStack.bDisplayFunctionNames = true;
				currentCallStack.bDisplayFileNames = true;
				currentCallStack.bDisplayFilePathNames = true;
				currentCallStack.bDisplayUnformattedCallStack = false;

				currentCrash.CallStackContainer = new CallStackContainer(currentCrash);

				var Model = new CrashViewModel { Crash = currentCrash, CallStack = currentCallStack };
				Model.GenerationTime = logTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Show", Model );
			}
		}

		/// <summary>
		/// Add a crash passed in the payload as Xml to the database.
		/// </summary>
		/// <param name="id">Unused.</param>
		/// <returns>The row id of the newly added crash.</returns>
		public ActionResult AddCrash( int id )
		{
            var newCrashResult = new CrashReporterResult();
            CrashDescription newCrash;
			newCrashResult.ID = -1;
			string payloadString;

            //Read the request payload
			try
			{
			    using (var reader = new StreamReader(Request.InputStream, Request.ContentEncoding))
			    {
			        payloadString = reader.ReadToEnd();
			        if (string.IsNullOrEmpty(payloadString))
			        {
			            FLogger.Global.WriteEvent(string.Format("Add Crash Failed : Payload string empty"));
			        }
			    }
			}
			catch (Exception ex)
			{
                var messageBuilder = new StringBuilder();
                messageBuilder.AppendLine("Error Reading Crash Payload");
                messageBuilder.AppendLine("Exception was:");
                messageBuilder.AppendLine(ex.ToString());

                FLogger.Global.WriteException(messageBuilder.ToString());

                newCrashResult.Message = messageBuilder.ToString();
                newCrashResult.bSuccess = false;
                    
                return Content(XmlHandler.ToXmlString<CrashReporterResult>(newCrashResult), "text/xml");
			}

            // De-serialize the payload string
			try
			{
			    newCrash = XmlHandler.FromXmlString<CrashDescription>(payloadString);
			}
			catch (Exception ex)
			{
                var messageBuilder = new StringBuilder();
                messageBuilder.AppendLine("Error Reading CrashDescription XML");
                messageBuilder.AppendLine("Exception was: ");
                messageBuilder.AppendLine(ex.ToString());

                FLogger.Global.WriteException(messageBuilder.ToString());

                newCrashResult.Message = messageBuilder.ToString();
                newCrashResult.bSuccess = false;

                return Content(XmlHandler.ToXmlString<CrashReporterResult>(newCrashResult), "text/xml");
			}
                
            //Add crash to database
			try
			{
			    var crash = CreateCrash(newCrash);
			    newCrashResult.ID = crash.Id;
			    newCrashResult.bSuccess = true;
			}
			catch (DbEntityValidationException dbentEx)
			{
                var messageBuilder = new StringBuilder();
                messageBuilder.AppendLine("Exception was:");
                messageBuilder.AppendLine(dbentEx.ToString());

			    var innerEx = dbentEx.InnerException;
			    while (innerEx != null)
			    {
			        messageBuilder.AppendLine("Inner Exception : " + innerEx.Message);
			        innerEx = innerEx.InnerException;
			    }

			    if (dbentEx.EntityValidationErrors != null)
			    {
			        messageBuilder.AppendLine("Validation Errors : ");
			        foreach (var valErr in dbentEx.EntityValidationErrors)
			        {
			            messageBuilder.AppendLine(valErr.ValidationErrors.Select(data => data.ErrorMessage).Aggregate((current, next) => current + "; /n" + next));
			        }
			    }

                messageBuilder.AppendLine("Received payload was:");
                messageBuilder.AppendLine(payloadString);

                FLogger.Global.WriteException(messageBuilder.ToString());
                _slackWriter.Write(messageBuilder.ToString());

                newCrashResult.Message = messageBuilder.ToString();
                newCrashResult.bSuccess = false;
			}
			catch (SqlException sqlExc)
			{
				if (sqlExc.Number == -2)//If this is an sql timeout log the timeout and try again.
				{
					FLogger.Global.WriteEvent(string.Format( "AddCrash: Timeout" ));
				}
				else
				{
                    var messageBuilder = new StringBuilder();
                    messageBuilder.AppendLine("Exception was:");
                    messageBuilder.AppendLine(sqlExc.ToString());
                    messageBuilder.AppendLine("Received payload was:");
                    messageBuilder.AppendLine(payloadString);

                    FLogger.Global.WriteException(messageBuilder.ToString());

					newCrashResult.Message = messageBuilder.ToString();
                    newCrashResult.bSuccess = false;
				}
			}
			catch (Exception ex)
			{
				var messageBuilder = new StringBuilder();
				messageBuilder.AppendLine("Exception was:");
				messageBuilder.AppendLine(ex.ToString());
				messageBuilder.AppendLine("Received payload was:");
				messageBuilder.AppendLine(payloadString);

                FLogger.Global.WriteException(messageBuilder.ToString());

				newCrashResult.Message = messageBuilder.ToString();
				newCrashResult.bSuccess = false;
			}

			var returnResult = XmlHandler.ToXmlString<CrashReporterResult>( newCrashResult );
                
			return Content( returnResult, "text/xml" );
		}

        /// <summary>
        /// Gets a list of crashes filtered based on our form data.
        /// </summary>
        /// <param name="formData"></param>
        /// <returns></returns>
        public CrashesViewModel GetResults(FormHelper formData)
        {
            List<Crash> results = null;
            IQueryable<Crash> resultsQuery = null;

            var skip = (formData.Page - 1) * formData.PageSize;
            var take = formData.PageSize;

            resultsQuery = ConstructQueryForFiltering(formData);

            // Filter by data and get as enumerable.
            resultsQuery = FilterByDate(resultsQuery, formData.DateFrom, formData.DateTo);

            // Filter by BuggId
            if (!string.IsNullOrEmpty(formData.BuggId))
            {
                var buggId = 0;
                var bValid = int.TryParse(formData.BuggId, out buggId);

                if (bValid)
                {
                    var newBugg = _unitOfWork.BuggRepository.GetById(buggId);

                    if (newBugg != null)
                    {
                        resultsQuery = resultsQuery.Where(data => data.PatternId == newBugg.PatternId);
                    }
                }
            }

            var countsQuery = resultsQuery.GroupBy(data => data.User.UserGroup).Select(data => new { Key = data.Key.Name, Count = data.Count()});
            var groupCounts = countsQuery.OrderBy(data => data.Key).ToDictionary(data => data.Key, data => data.Count);

            // Filter by user group if present
            var userGroupId = !string.IsNullOrEmpty(formData.UserGroup) ? _unitOfWork.UserGroupRepository.First(data => data.Name == formData.UserGroup).Id : 1;
            resultsQuery = resultsQuery.Where(data => data.User.UserGroupId == userGroupId);

            var orderedQuery = GetSortedQuery(resultsQuery, formData.SortTerm ?? "TimeOfCrash", formData.SortOrder == "Descending");

            // Grab just the results we want to display on this page
            results = orderedQuery.Skip(skip).Take(take).ToList();

            // Get the Count for pagination
            var resultCount = 0;
            resultCount = orderedQuery.Count();

            // Process call stack for display
            foreach (var crashInstance in results)
            {
                // Put call stacks into an list so we can access them line by line in the view
                crashInstance.CallStackContainer = new CallStackContainer(crashInstance);
            }

            return new CrashesViewModel
            {
                Results = results,
                PagingInfo = new PagingInfo { CurrentPage = formData.Page, PageSize = formData.PageSize, TotalResults = resultCount },
                SortOrder = formData.SortOrder,
                SortTerm = formData.SortTerm,
                UserGroup = formData.UserGroup,
                CrashType = formData.CrashType,
                SearchQuery = formData.SearchQuery,
                UsernameQuery = formData.UsernameQuery,
                EpicIdOrMachineQuery = formData.EpicIdOrMachineQuery,
                MessageQuery = formData.MessageQuery,
                BuiltFromCL = formData.BuiltFromCL,
                BuggId = formData.BuggId,
                JiraQuery = formData.JiraQuery,
                DateFrom = (long)(formData.DateFrom - CrashesViewModel.Epoch).TotalMilliseconds,
                DateTo = (long)(formData.DateTo - CrashesViewModel.Epoch).TotalMilliseconds,
                BranchName = formData.BranchName,
                VersionName = formData.VersionName,
                PlatformName = formData.PlatformName,
                GameName = formData.GameName,
                GroupCounts = groupCounts
            };
        }

	    /// <summary>
	    /// Returns a lambda expression used to sort our crash data.
	    /// </summary>
	    /// <param name="resultsQuery">A query filter on the crashes entity.</param>
	    /// <param name="sortTerm">Sort term identifying the field on which to sort.</param>
	    /// <param name="sortDescending">bool indicating sort order</param>
	    /// <returns></returns>
	    public IOrderedQueryable<Crash> GetSortedQuery(IQueryable<Crash> resultsQuery, string sortTerm, bool sortDescending)
	    {
            switch (sortTerm)
            {
                case "Id":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.Id) : resultsQuery.OrderBy(data => data.Id);
                    break;
                case "TimeOfCrash":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.TimeOfCrash) : resultsQuery.OrderBy(crashInstance => crashInstance.TimeOfCrash);
                    break;
                case "UserName":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.UserName) : resultsQuery.OrderBy(crashInstance => crashInstance.UserName);
                    break;
                case "RawCallStack":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.RawCallStack) : resultsQuery.OrderBy(crashInstance => crashInstance.RawCallStack);
                    break;
                case "GameName":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.GameName) : resultsQuery.OrderBy(crashInstance => crashInstance.GameName);
                    break;
                case "EngineMode":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.EngineMode) : resultsQuery.OrderBy(crashInstance => crashInstance.EngineMode);
                    break;
                case "FixedChangeList":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.FixedChangeList) : resultsQuery.OrderBy(crashInstance => crashInstance.FixedChangeList);
                    break;
                case "TTPID":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.Jira) : resultsQuery.OrderBy(crashInstance => crashInstance.Jira);
                    break;
                case "Branch":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.Branch) : resultsQuery.OrderBy(crashInstance => crashInstance.Branch);
                    break;
                case "ChangeListVersion":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.ChangeListVersion) : resultsQuery.OrderBy(crashInstance => crashInstance.ChangeListVersion);
                    break;
                case "ComputerName":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.ComputerName) : resultsQuery.OrderBy(crashInstance => crashInstance.ComputerName);
                    break;
                case "PlatformName":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.PlatformName) : resultsQuery.OrderBy(crashInstance => crashInstance.PlatformName);
                    break;
                case "Status":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.Status) : resultsQuery.OrderBy(crashInstance => crashInstance.Status);
                    break;
                case "Module":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.Module) : resultsQuery.OrderBy(crashInstance => crashInstance.Module);
                    break;
                case "Summary":
                    return sortDescending ? resultsQuery.OrderByDescending(data => data.Summary) : resultsQuery.OrderBy(data => data.Summary);
            }
	        return null;
	    }

        /// <summary>Constructs query for filtering.</summary>
        private IQueryable<Crash> ConstructQueryForFiltering(FormHelper formData)
        {
            //Instead of returning a queryable and filtering it here we should construct a filtering expression and pass that to the repository.
            //I don't like that data handling is taking place in the controller.
            var results = _unitOfWork.CrashRepository.ListAll();

            // Grab Results 
            var queryString = HttpUtility.HtmlDecode(formData.SearchQuery);
            if (!string.IsNullOrEmpty(queryString))
            {
                if (!string.IsNullOrEmpty(queryString))
                {
                    //We only use SearchQuery now for CallStack searching - if there's a SearchQuery value and a Username value, we need to get rid of the 
                    //Username so that we can create a broader search range
                    formData.UsernameQuery = "";
                }
                results = results.Where(data => data.RawCallStack.Contains(formData.SearchQuery));
            }

            // Filter by Crash Type
            if (formData.CrashType != "All")
            {
                switch (formData.CrashType)
                {
                    case "Crashes":
                        results = results.Where(crashInstance => crashInstance.CrashType == 1);
                        break;
                    case "Assert":
                        results = results.Where(crashInstance => crashInstance.CrashType == 2);
                        break;
                    case "Ensure":
                        results = results.Where(crashInstance => crashInstance.CrashType == 3);
                        break;
                    case "CrashesAsserts":
                        results = results.Where(crashInstance => crashInstance.CrashType == 1 || crashInstance.CrashType == 2);
                        break;
                }
            }

            // JRX Restore EpicID/UserName searching
            if (!string.IsNullOrEmpty(formData.UsernameQuery))
            {
                var decodedUsername = HttpUtility.HtmlDecode(formData.UsernameQuery).ToLower();
                var user = _unitOfWork.UserRepository.First(data => data.UserName.Contains(decodedUsername));
                if (user != null)
                {
                    var userId = user.Id; 
                    results = (
                        from crashDetail in results
                        where crashDetail.UserNameId == userId
                        select crashDetail
                        );
                }
            }

            // Start Filtering the results
            if (!string.IsNullOrEmpty(formData.EpicIdOrMachineQuery))
            {
                var decodedEpicOrMachineId = HttpUtility.HtmlDecode(formData.EpicIdOrMachineQuery).ToLower();
                results =
                        (
                            from crashDetail in results
                            where crashDetail.EpicAccountId.Equals(decodedEpicOrMachineId) || crashDetail.ComputerName.Equals(decodedEpicOrMachineId)
                            select crashDetail
                        );
            }

            if (!string.IsNullOrEmpty(formData.JiraQuery))
            {
                var decodedJira = HttpUtility.HtmlDecode(formData.JiraQuery).ToLower();
                results =
                    (
                        from crashDetail in results
                        where crashDetail.Jira.Contains(decodedJira)
                        select crashDetail
                    );
            }

            // Filter by BranchName
            if (!string.IsNullOrEmpty(formData.BranchName))
            {
                results =
                    (
                        from crashDetail in results
                        where crashDetail.Branch.Equals(formData.BranchName)
                        select crashDetail
                    );
            }

            // Filter by VersionName
            if (!string.IsNullOrEmpty(formData.VersionName))
            {
                results =
                    (
                        from crashDetail in results
                        where crashDetail.BuildVersion.Equals(formData.VersionName)
                        select crashDetail
                    );
            }

            //Filter by Engine Version
            if (!string.IsNullOrEmpty(formData.EngineVersion))
            {
                results =
                    (
                        from crashDetail in results
                        where crashDetail.EngineVersion.Equals(formData.EngineVersion)
                        select crashDetail
                    );
            }

            // Filter by VersionName
            if (!string.IsNullOrEmpty(formData.EngineMode))
            {
                results =
                    (
                        from crashDetail in results
                        where crashDetail.EngineMode.Equals(formData.EngineMode)
                        select crashDetail
                    );
            }

            // Filter by PlatformName
            if (!string.IsNullOrEmpty(formData.PlatformName))
            {
                results =
                    (
                        from crashDetail in results
                        where crashDetail.PlatformName.Contains(formData.PlatformName)
                        select crashDetail
                    );
            }

            // Filter by GameName
            if (!string.IsNullOrEmpty(formData.GameName))
            {
                var DecodedGameName = HttpUtility.HtmlDecode(formData.GameName).ToLower();

                if (DecodedGameName.StartsWith("-"))
                {
                    results =
                    (
                        from crashDetail in results
                        where !crashDetail.GameName.Contains(DecodedGameName.Substring(1))
                        select crashDetail
                    );
                }
                else
                {
                    results =
                    (
                        from crashDetail in results
                        where crashDetail.GameName.Contains(DecodedGameName)
                        select crashDetail
                    );
                }
            }

            // Filter by MessageQuery
            if (!string.IsNullOrEmpty(formData.MessageQuery))
            {
                results =
                (
                    from crashDetail in results
                    where crashDetail.Summary.Contains(formData.MessageQuery) || crashDetail.Description.Contains(formData.MessageQuery)
                    select crashDetail
                );
            }

            // Filter by BuiltFromCL
            if (!string.IsNullOrEmpty(formData.BuiltFromCL))
            {
                var builtFromCl = 0;
                var bValid = int.TryParse(formData.BuiltFromCL, out builtFromCl);

                if (bValid)
                {
                    results =
                        (
                            from crashDetail in results
                            where crashDetail.ChangeListVersion.Equals(formData.BuiltFromCL)
                            select crashDetail
                        );
                }
            }

            return results;
        }

        /// <summary>
        /// Filter a set of crashes to a date range.
        /// </summary>
        /// <param name="Results">The unfiltered set of crashes.</param>
        /// <param name="DateFrom">The earliest date to filter by.</param>
        /// <param name="DateTo">The latest date to filter by.</param>
        /// <returns>The set of crashes between the earliest and latest date.</returns>
        public IQueryable<Crash> FilterByDate(IQueryable<Crash> Results, DateTime DateFrom, DateTime DateTo)
        {
            using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer(this.GetType().ToString() + " SQL"))
            {
                DateTo = DateTo.AddDays(1);

                IQueryable<Crash> CrashesInTimeFrame = Results
                    .Where(MyCrash => MyCrash.TimeOfCrash >= DateFrom && MyCrash.TimeOfCrash <= DateTo);
                return CrashesInTimeFrame;
            }
        }

	    /// <summary>
	    /// Create a new crash data model object and insert it into the database
	    /// </summary>
	    /// <param name="description"></param>
	    /// <returns></returns>
	    private Crash CreateCrash(CrashDescription description)
	    {
	        var newCrash = new Crash
	        {
	            Branch = description.BranchName,
	            BaseDir = description.BaseDir,
	            BuildVersion = description.EngineVersion,
                EngineVersion = description.BuildVersion,
	            ChangeListVersion = description.BuiltFromCL.ToString(),
	            CommandLine = description.CommandLine,
	            EngineMode = description.EngineMode,
	            ComputerName = description.MachineGuid
	        };

	        //if there's a valid username assign the associated UserNameId else use "anonymous".
	        var userName = (!string.IsNullOrEmpty(description.UserName)) ? description.UserName : UserNameAnonymous;
	        var user = _unitOfWork.UserRepository.GetByUserName(userName);

	        if (user != null)
	        {
	            newCrash.UserNameId = user.Id;
	            newCrash.UserName = user.UserName;
	        }
	        else
	        {
	            newCrash.User = new User() {UserName = description.UserName, UserGroupId = 5};
	        }

	        //If there's a valid EpicAccountId assign that.
	        if (!string.IsNullOrEmpty(description.EpicAccountId))
	        {
	            newCrash.EpicAccountId = description.EpicAccountId;
	        }

	        newCrash.Description = "";
	        if (description.UserDescription != null)
	        {
	            newCrash.Description = string.Join(Environment.NewLine, description.UserDescription);
	        }

	        newCrash.EngineMode = description.EngineMode;
	        newCrash.GameName = description.GameName;
	        newCrash.LanguageExt = description.Language; //Converted by the crash process.
	        newCrash.PlatformName = description.Platform;

	        if (description.ErrorMessage != null)
	        {
	            newCrash.Summary = string.Join("\n", description.ErrorMessage);
	        }

	        if (description.CallStack != null)
	        {
	            newCrash.RawCallStack = string.Join("\n", description.CallStack);
	        }

	        if (description.SourceContext != null)
	        {
	            newCrash.SourceContext = string.Join("\n", description.SourceContext);
	        }

	        newCrash.TimeOfCrash = description.TimeofCrash;
	        newCrash.Processed = description.bAllowToBeContacted;

	        newCrash.Jira = "";
	        newCrash.FixedChangeList = "";
	        newCrash.ProcessFailed = description.bProcessorFailed;

	        // Set the crash type
	        newCrash.CrashType = 1;

	        //if we have a crash type set the crash type
	        if (!string.IsNullOrEmpty(description.CrashType))
	        {
	            switch (description.CrashType.ToLower())
	            {
	                case "crash":
	                    newCrash.CrashType = 1;
	                    break;
	                case "assert":
	                    newCrash.CrashType = 2;
	                    break;
	                case "ensure":
	                    newCrash.CrashType = 3;
	                    break;
	                case "":
	                case null:
	                default:
	                    newCrash.CrashType = 1;
	                    break;
	            }
	        }
	        else //else fall back to the old behavior and try to determine type from RawCallStack
	        {
	            if (newCrash.RawCallStack != null)
	            {
	                if (newCrash.RawCallStack.Contains("FDebug::AssertFailed"))
	                {
	                    newCrash.CrashType = 2;
	                }
	                else if (newCrash.RawCallStack.Contains("FDebug::Ensure"))
	                {
	                    newCrash.CrashType = 3;
	                }
	                else if (newCrash.RawCallStack.Contains("FDebug::OptionallyLogFormattedEnsureMessageReturningFalse"))
	                {
	                    newCrash.CrashType = 3;
	                }
	                else if (newCrash.RawCallStack.Contains("NewReportEnsure"))
	                {
	                    newCrash.CrashType = 3;
	                }
	            }
	        }

	        // As we're adding it, the status is always new
	        newCrash.Status = "New";
	        newCrash.UserActivityHint = description.UserActivityHint;
	        try
	        {
	            BuildPattern(newCrash);
	        }
	        catch (Exception ex)
	        {
                FLogger.Global.WriteException("Error in Create Crash Build Pattern Method");
	            throw;
	        }

            if(newCrash.CommandLine == null)
                newCrash.CommandLine = "";

            _unitOfWork.Dispose();
            _unitOfWork = new UnitOfWork(new CrashReportEntities());
	        var callStackRepository = _unitOfWork.CallstackRepository;

	        try
	        {
	            var crashRepo = _unitOfWork.CrashRepository;
                //if we don't have any callstack data then insert the crash and return
	            if (string.IsNullOrEmpty(newCrash.Pattern))
	            {
	                crashRepo.Save(newCrash);
                    _unitOfWork.Save();
	                return newCrash;
	            }

                //If this isn't a new pattern then link it to our crash data model
                if (callStackRepository.Any(data => data.Pattern == newCrash.Pattern))
	            {
                    var callstackPattern = callStackRepository.First(data => data.Pattern == newCrash.Pattern);
	                newCrash.PatternId = callstackPattern.id;
	            }
	            else
	            {
	                //if this is a new callstack pattern then insert into data model and create a new bugg.
	                var callstackPattern = new CallStackPattern { Pattern = newCrash.Pattern };
	                callStackRepository.Save(callstackPattern);
                    _unitOfWork.Save();
	                newCrash.PatternId = callstackPattern.id;
	            }

                //Mask out the line number and File path from our error message.
                var errorMessageString = description.ErrorMessage != null ? String.Join("", description.ErrorMessage) : "";

                //Create our masking regular expressions
                var fileRegex = new Regex(@"(\[File:).*?(])");//Match the filename out the file name
                var lineRegex = new Regex(@"(\[Line:).*?(])");//Match the line no.

                /**
                 * Regex to match ints of two characters or longer
                 * First term ((?<=\s)|(-)) : Positive look behind, match if preceeded by whitespace or if first character is '-'
                 * Second term (\d{3,}) match three or more decimal chracters in a row.
                 * Third term (?=(\s|$)) positive look ahead match if followed by whitespace or end of line/file.
                 */
                var intRegex = new Regex(@"-?\d{3,}");
                
                /**
                 * Regular expression for masking out floats
                 */
                var floatRegex = new Regex(@"-?\d+\.\d+");

                /**
                 * Regular expression for masking out hexadecimal numbers
                 */
                var hexRegex = new Regex(@"0x[\da-fA-F]+");

                //mask out terms matches by our regex's
                var trimmedError = fileRegex.Replace(errorMessageString, "");
                trimmedError = lineRegex.Replace(trimmedError, "");
                trimmedError = floatRegex.Replace(trimmedError, "");
                trimmedError = hexRegex.Replace(trimmedError, "");
                trimmedError = intRegex.Replace(trimmedError, "");
                if(trimmedError.Length > 450)
                    trimmedError = trimmedError.Substring(0, 450);//error message is used as an index - trim to max index length
                
                //Check to see if the masked error message is unique
	            ErrorMessage errorMessage = null;
                if (_unitOfWork.ErrorMessageRepository.Any(data => data.Message.ToLower().Contains(trimmedError.ToLower())))
                {
                    errorMessage =
                        _unitOfWork.ErrorMessageRepository.First(data => data.Message.ToLower().Contains(trimmedError.ToLower()));
                }
                else
                {
                    //if it's a new message then add it to the database.
                    errorMessage = new ErrorMessage() { Message = trimmedError };
                    _unitOfWork.ErrorMessageRepository.Save(errorMessage);
                    _unitOfWork.Save();
                }

	            //Check for an existing bugg with this pattern and error message / no error message
                if ( _unitOfWork.BuggRepository.Any(data =>
                    (data.PatternId == newCrash.PatternId) && (data.ErrorMessageId == errorMessage.Id || data.ErrorMessageId == null) ))
                {
                    //if a bugg exists for this pattern update the bugg data
                    var bugg = _unitOfWork.BuggRepository.First(data => data.PatternId == newCrash.PatternId);
                    bugg.PatternId = newCrash.PatternId;

                    bugg.CrashType = newCrash.CrashType;
                    bugg.ErrorMessageId = errorMessage.Id;

	                //also update the bugg data while we're here
	                bugg.TimeOfLastCrash = newCrash.TimeOfCrash;
	                
	                if (String.Compare(newCrash.BuildVersion, bugg.BuildVersion,
	                        StringComparison.Ordinal) != 1)
	                    bugg.BuildVersion = newCrash.BuildVersion;

                    _unitOfWork.Save();

	                //if a bugg exists update this crash from the bugg
	                //buggs are authoritative in this case
	                newCrash.Buggs.Add(bugg);
	                newCrash.Jira = bugg.TTPID;
	                newCrash.FixedChangeList = bugg.FixedChangeList;
	                newCrash.Status = bugg.Status;
	                _unitOfWork.CrashRepository.Save(newCrash);
	                _unitOfWork.Save();
	            }
	            else
	            {
	                //if there's no bugg for this pattern create a new bugg and insert into the data store.
	                var bugg = new Bugg();
	                bugg.TimeOfFirstCrash = newCrash.TimeOfCrash;
	                bugg.TimeOfLastCrash = newCrash.TimeOfCrash;
	                bugg.TTPID = newCrash.Jira;
                    bugg.Pattern = newCrash.Pattern;
	                bugg.PatternId = newCrash.PatternId;
	                bugg.NumberOfCrashes = 1;
	                bugg.NumberOfUsers = 1;
	                bugg.NumberOfUniqueMachines = 1;
	                bugg.BuildVersion = newCrash.BuildVersion;
	                bugg.CrashType = newCrash.CrashType;
	                bugg.Status = newCrash.Status;
                    bugg.FixedChangeList = newCrash.FixedChangeList;
	                bugg.ErrorMessageId = errorMessage.Id;
                    newCrash.Buggs.Add(bugg);
                    _unitOfWork.BuggRepository.Save(bugg);
                    _unitOfWork.CrashRepository.Save(newCrash);
                    _unitOfWork.Save();
	            }
	        }
            catch (DbEntityValidationException dbentEx)
            {
                var messageBuilder = new StringBuilder();
                messageBuilder.AppendLine("Db Entity Validation Exception Exception was:");
                messageBuilder.AppendLine(dbentEx.ToString());

                var innerEx = dbentEx.InnerException;
                while (innerEx != null)
                {
                    messageBuilder.AppendLine("Inner Exception : " + innerEx.Message);
                    innerEx = innerEx.InnerException;
                }

                if (dbentEx.EntityValidationErrors != null)
                {
                    messageBuilder.AppendLine("Validation Errors : ");
                    foreach (var valErr in dbentEx.EntityValidationErrors)
                    {
                        messageBuilder.AppendLine(valErr.ValidationErrors.Select(data => data.ErrorMessage).Aggregate((current, next) => current + "; /n" + next));
                    }
                }
                FLogger.Global.WriteException(messageBuilder.ToString());
                _slackWriter.Write("Create Crash Exception : " + messageBuilder.ToString());
                throw;
            }
	        catch (Exception ex)
	        {
                var messageBuilder = new StringBuilder();
                messageBuilder.AppendLine("Create Crash Exception : ");
                messageBuilder.AppendLine(ex.Message.ToString());
                var innerEx = ex.InnerException;
                while (innerEx != null)
                {
                    messageBuilder.AppendLine("Inner Exception : " + innerEx.Message);
                    innerEx = innerEx.InnerException;
                }

	            FLogger.Global.WriteException("Create Crash Exception : " +
	                                          messageBuilder.ToString());
                _slackWriter.Write("Create Crash Exception : " + messageBuilder.ToString());
	            throw;
	        }

            return newCrash;
	    }

	    /// <summary>
	    /// Create call stack pattern and either insert into database or match to existing.
	    /// Update Associate Buggs.
	    /// </summary>
	    /// <param name="newCrash"></param>
	    private void BuildPattern(Crash newCrash)
	    {
	        var callstack = new CallStackContainer(newCrash);
	        newCrash.Module = callstack.GetModuleName();
	        if (newCrash.PatternId == null)
	        {
	            var patternList = new List<string>();

	            try
	            {
	                foreach (var entry in callstack.CallStackEntries.Take(CallStackContainer.MaxLinesToParse))
	                {
	                    FunctionCall currentFunctionCall;

	                    var csEntry = entry;
                        var functionCall = _unitOfWork.FunctionRepository.First(f => f.Call == csEntry.FunctionName);
                        if (functionCall != null)
                        {
                            currentFunctionCall = functionCall;
                        }
	                    else
	                    {
	                        currentFunctionCall = new FunctionCall { Call = csEntry.FunctionName };
	                        _unitOfWork.FunctionRepository.Save(currentFunctionCall);
	                        _unitOfWork.Save();
	                    }

	                    patternList.Add(currentFunctionCall.Id.ToString());
	                }

	                newCrash.Pattern = string.Join("+", patternList);
	            }
	            catch (Exception ex)
	            {
                    var messageBuilder = new StringBuilder();
                    FLogger.Global.WriteException("Build Pattern exception: " + ex.Message.ToString(CultureInfo.InvariantCulture));
                    messageBuilder.AppendLine("Exception was:");
                    messageBuilder.AppendLine(ex.ToString());
                    while (ex.InnerException != null)
                    {
                        ex = ex.InnerException;
                        messageBuilder.AppendLine(ex.ToString());
                    }

                    _slackWriter.Write("Build Pattern Exception : " + ex.Message.ToString(CultureInfo.InvariantCulture));
                    throw;
	            }
	        }
	    }

	    protected override void Dispose(bool disposing)
	    {
	        if (disposing)
	        {
	            if (_unitOfWork != null)
	            {
	                _unitOfWork.Dispose();
	                _unitOfWork = null;
	            }
	        }

	        base.Dispose(disposing);
	    }
	}
}