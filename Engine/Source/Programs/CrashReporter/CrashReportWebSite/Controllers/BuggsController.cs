// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Web.Mvc;

using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// A controller to handle the Bugg data.
	/// </summary>
	public class BuggsController : Controller
	{
		/// <summary>
		/// An empty constructor.
		/// </summary>
		public BuggsController()
		{
		}

		/// <summary>
		/// The Index action.
		/// </summary>
		/// <param name="BuggsForm">The form of user data passed up from the client.</param>
		/// <returns>The view to display a list of Buggs on the client.</returns>
		public ActionResult Index( FormCollection BuggsForm )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				BuggRepository Buggs = new BuggRepository();

				FormHelper FormData = new FormHelper( Request, BuggsForm, "CrashesInTimeFrameGroup" );
				BuggsViewModel Results = Buggs.GetResults( FormData );
				foreach( var Bugg in Results.Results )
				{
					// Populate function calls.
					Bugg.GetFunctionCalls();
				}
				Results.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", Results );
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
					if( Entry.ToString().Contains( Bugg.JiraSubmitName ) )
					{
						int.TryParse( Entry.ToString().Substring( Bugg.JiraSubmitName.Length ), out BuggIDToBeAddedToJira );
						break;
					}
				}

				BuggRepository Buggs = new BuggRepository();

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
				Bugg NewBugg = Buggs.GetBugg( id );
				if( NewBugg == null )
				{
					return RedirectToAction( "" );
				}

				Crashes = NewBugg.GetCrashes();

				using( FAutoScopedLogTimer GetCrashesTimer = new FAutoScopedLogTimer( "Bugg.PrepareBuggForJira" ) )
				{
					NewBugg.PrepareBuggForJira( Crashes );

					if( BuggIDToBeAddedToJira != 0 )
					{
						NewBugg.CopyToJira();
					}			
				}

				using( FAutoScopedLogTimer JiraResultsTimer = new FAutoScopedLogTimer( "Bugg.GrabJira" ) )
				{
					var JC = JiraConnection.Get();
					bool bValidJira = false;

					// Verify valid JiraID, this may be still a TTP 
					if( !string.IsNullOrEmpty( NewBugg.TTPID ) )
					{
						int TTPID = 0;
						int.TryParse( NewBugg.TTPID, out TTPID );

						if( TTPID == 0 )
						{
							//AddBuggJiraMapping( NewBugg, ref FoundJiras, ref JiraIDtoBugg );
							bValidJira = true;
						}
					}

					if( JC.CanBeUsed() && bValidJira )
					{
						// Grab the data form JIRA.
						string JiraSearchQuery = "key = " + NewBugg.TTPID;

						var JiraResults = JC.SearchJiraTickets(
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


						// Jira Key, Summary, Components, Resolution, Fix version, Fix changelist
						foreach( var Jira in JiraResults )
						{
							string JiraID = Jira.Key;

							string Summary = (string)Jira.Value["summary"];

							string ComponentsText = "";
							System.Collections.ArrayList Components = (System.Collections.ArrayList)Jira.Value["components"];
							foreach( Dictionary<string, object> Component in Components )
							{
								ComponentsText += (string)Component["name"];
								ComponentsText += " ";
							}

							Dictionary<string, object> ResolutionFields = (Dictionary<string, object>)Jira.Value["resolution"];
							string Resolution = ResolutionFields != null ? (string)ResolutionFields["name"] : "";

							string FixVersionsText = "";
							System.Collections.ArrayList FixVersions = (System.Collections.ArrayList)Jira.Value["fixVersions"];
							foreach( Dictionary<string, object> FixVersion in FixVersions )
							{
								FixVersionsText += (string)FixVersion["name"];
								FixVersionsText += " ";
							}

							int FixCL = Jira.Value["customfield_11200"] != null ? (int)(decimal)Jira.Value["customfield_11200"] : 0;

							NewBugg.JiraSummary = Summary;
							NewBugg.JiraComponentsText = ComponentsText;
							NewBugg.JiraResolution = Resolution;
							NewBugg.JiraFixVersionsText = FixVersionsText;
							if( FixCL != 0 )
							{
								NewBugg.JiraFixCL = FixCL.ToString();
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
						NewBugg.Status = BuggsForm["SetStatus"];
						Buggs.SetBuggStatus( NewBugg.Status, id );
					}

					if( !string.IsNullOrEmpty( BuggsForm["SetFixedIn"] ) )
					{
						NewBugg.FixedChangeList = BuggsForm["SetFixedIn"];
						Buggs.SetBuggFixedChangeList( NewBugg.FixedChangeList, id );
					}

					if( !string.IsNullOrEmpty( BuggsForm["SetTTP"] ) )
					{
						NewBugg.TTPID = BuggsForm["SetTTP"];
						Buggs.SetJIRAForBuggAndCrashes( NewBugg.TTPID, id );
					}

					// <STATUS>
				}

				// Set up the view model with the crash data
				Model.Bugg = NewBugg;
				Model.Crashes = Crashes;

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

					Model.Bugg.LatestCrashSummary = NewCrash.Summary;
				}

				Model.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Show", Model );
			}
		}
	}
}
