// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Web.Mvc;

using Tools.DotNETCommon.XmlHandler;
using Tools.CrashReporter.CrashReportCommon;
using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// The controller to handle the crash data.
	/// </summary>
	[HandleError]
	public class CrashesController : Controller
	{
		/// <summary>
		/// An empty constructor.
		/// </summary>
		public CrashesController()
		{
		}

		/// <summary>
		/// Display a summary list of crashes based on the search criteria.
		/// </summary>
		/// <param name="CrashesForm">A form of user data passed up from the client.</param>
		/// <returns>A view to display a list of crash reports.</returns>
		public ActionResult Index( FormCollection CrashesForm )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				CrashRepository Crashes = new CrashRepository();

				// Handle any edits made in the Set form fields
				foreach( var Entry in CrashesForm )
				{
					int Id = 0;
					if( int.TryParse( Entry.ToString(), out Id ) )
					{
						Crash CurrentCrash = Crashes.GetCrash( Id );
						if( CurrentCrash != null )
						{
							if( !string.IsNullOrEmpty( CrashesForm["SetStatus"] ) )
							{
								CurrentCrash.Status = CrashesForm["SetStatus"];
							}

							if( !string.IsNullOrEmpty( CrashesForm["SetFixedIn"] ) )
							{
								CurrentCrash.FixedChangeList = CrashesForm["SetFixedIn"];
							}

							if( !string.IsNullOrEmpty( CrashesForm["SetTTP"] ) )
							{
								CurrentCrash.TTPID = CrashesForm["SetTTP"];
							}
						}
					}

					Crashes.SubmitChanges();
				}

				// <STATUS>

				// Parse the contents of the query string, and populate the form
				FormHelper FormData = new FormHelper( Request, CrashesForm, "TimeOfCrash" );
				CrashesViewModel Result = Crashes.GetResults( FormData );

				// Add the FromCollection to the CrashesViewModel since we don't need it for the get results function but we do want to post it back to the page.
				Result.FormCollection = CrashesForm;
				Result.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", Result );
			}
		}

		/// <summary>
		/// Show detailed information about a crash.
		/// </summary>
		/// <param name="CrashesForm">A form of user data passed up from the client.</param>
		/// <param name="id">The unique id of the crash we wish to show the details of.</param>
		/// <returns>A view to show crash details.</returns>
		public ActionResult Show( FormCollection CrashesForm, int id )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(CrashId=" + id + ")", bCreateNewLog: true ) )
			{
				CrashRepository Crashes = new CrashRepository();

				CallStackContainer CurrentCallStack = null;

				// Update the selected crash based on the form contents
				Crash CurrentCrash = Crashes.GetCrash( id );

				if( CurrentCrash == null )
				{
					return RedirectToAction( "" );
				}

				string FormValue;

				FormValue = CrashesForm["SetStatus"];
				if( !string.IsNullOrEmpty( FormValue ) )
				{
					CurrentCrash.Status = FormValue;
				}

				FormValue = CrashesForm["SetFixedIn"];
				if( !string.IsNullOrEmpty( FormValue ) )
				{
					CurrentCrash.FixedChangeList = FormValue;
				}

				FormValue = CrashesForm["SetTTP"];
				if( !string.IsNullOrEmpty( FormValue ) )
				{
					CurrentCrash.TTPID = FormValue;
				}

				// Valid to set description to an empty string
				FormValue = CrashesForm["Description"];
				if( FormValue != null )
				{
					CurrentCrash.Description = FormValue;
				}

				CurrentCallStack = new CallStackContainer( CurrentCrash );

				// Set callstack properties
				CurrentCallStack.bDisplayModuleNames = true;
				CurrentCallStack.bDisplayFunctionNames = true;
				CurrentCallStack.bDisplayFileNames = true;
				CurrentCallStack.bDisplayFilePathNames = true;
				CurrentCallStack.bDisplayUnformattedCallStack = false;

				CurrentCrash.CallStackContainer = CurrentCrash.GetCallStack();

				// Populate the crash with the correct user data
				Crashes.PopulateUserInfo( CurrentCrash );
				Crashes.SubmitChanges();

				var Model = new CrashViewModel { Crash = CurrentCrash, CallStack = CurrentCallStack };
				Model.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
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
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(NewCrashId=" + id + ")" ) )
			{
				CrashRepository Crashes = new CrashRepository();

				CrashReporterResult NewCrashResult = new CrashReporterResult();
				NewCrashResult.ID = -1;

				try
				{
					using( StreamReader Reader = new StreamReader( Request.InputStream, Request.ContentEncoding ) )
					{
						string Result = Reader.ReadToEnd();
						CrashDescription NewCrash = XmlHandler.FromXmlString<CrashDescription>( Result );
						NewCrashResult.ID = Crashes.AddNewCrash( NewCrash );
						NewCrashResult.bSuccess = true;
					}
				}
				catch( Exception Ex )
				{
					NewCrashResult.Message = Ex.ToString();
					NewCrashResult.bSuccess = false;
				}

				string ReturnResult = XmlHandler.ToXmlString<CrashReporterResult>( NewCrashResult );
				return Content( ReturnResult, "text/xml" );
			}
		}
	}
}
