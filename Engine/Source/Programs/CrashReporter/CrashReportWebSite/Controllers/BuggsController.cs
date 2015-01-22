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
		/// <summary></summary>
		private BuggRepository LocalBuggRepository = new BuggRepository();
		/// <summary></summary>
		private CrashRepository LocalCrashRepository = new CrashRepository();

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
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				FormHelper FormData = new FormHelper( Request, BuggsForm, "CrashesInTimeFrame" );
				BuggsViewModel Results = LocalBuggRepository.GetResults( FormData );
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
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(BuggId=" + id + ")" ) )
			{
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
				Bugg Bugg = new Bugg();

				BuggViewModel Model = new BuggViewModel();
				Bugg = LocalBuggRepository.GetBugg( id );
				if( Bugg == null )
				{
					return RedirectToAction( "" );
				}

				Crashes = Bugg.GetCrashes().ToList();

				// Apply any user settings
				if( BuggsForm.Count > 0 )
				{
					if( !string.IsNullOrEmpty( BuggsForm["SetStatus"] ) )
					{
						Bugg.Status = BuggsForm["SetStatus"];
						LocalCrashRepository.SetBuggStatus( Bugg.Status, id );
					}

					if( !string.IsNullOrEmpty( BuggsForm["SetFixedIn"] ) )
					{
						Bugg.FixedChangeList = BuggsForm["SetFixedIn"];
						LocalCrashRepository.SetBuggFixedChangeList( Bugg.FixedChangeList, id );
					}

					if( !string.IsNullOrEmpty( BuggsForm["SetTTP"] ) )
					{
						Bugg.TTPID = BuggsForm["SetTTP"];
						LocalCrashRepository.SetBuggTTPID( Bugg.TTPID, id );
					}

					if( !string.IsNullOrEmpty( BuggsForm["Description"] ) )
					{
						Bugg.Description = BuggsForm["Description"];
					}

					// <STATUS>
				}

				// Set up the view model with the crash data
				Model.Bugg = Bugg;
				Model.Crashes = Crashes;

				Crash NewCrash = Model.Crashes.FirstOrDefault();
				if( NewCrash != null )
				{
					CallStackContainer CallStack = new CallStackContainer( NewCrash );

					// Set callstack properties
					CallStack.bDisplayModuleNames = DisplayModuleNames;
					CallStack.bDisplayFunctionNames = DisplayFunctionNames;
					CallStack.bDisplayFileNames = DisplayFileNames;
					CallStack.bDisplayFilePathNames = DisplayFilePathNames;
					CallStack.bDisplayUnformattedCallStack = DisplayUnformattedCallStack;

					Model.CallStack = CallStack;

					NewCrash.CallStackContainer = NewCrash.GetCallStack();
				}

				using( FScopedLogTimer LogTimer2 = new FScopedLogTimer( "BuggsController.Show.PopulateUserInfo" + "(id=" + id + ")" ) )
				{
					// Add in the users for each crash in the Bugg
					foreach( Crash CrashInstance in Model.Crashes )
					{
						LocalCrashRepository.PopulateUserInfo( CrashInstance );
					}
				}
				return View( "Show", Model );
			}	
		}
	}
}
