// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;
using System.Web.Helpers;

namespace Tools.CrashReporter.CrashReportWebSite.ViewModels
{
	/// <summary>
	/// A helper class to extract parameters passed from web pages
	/// </summary>
	public class FormHelper
	{
		/// <summary>The user search query.</summary>
		public string SearchQuery = "";

		/// <summary>User name as query for filtering.</summary>
		public string UsernameQuery = "";

		/// <summary>Epic ID or Machine ID as query for filtering.</summary>
		public string EpicIdOrMachineQuery = "";

		/// <summary>Jira as query for crash filtering.</summary>
		public string JiraQuery = "";

        /// <summary> Jira Id for crash/bugg filtering </summary>
	    public string JiraId = "";

		/// <summary>Message/Summary or Description as query for filtering.</summary>
		public string MessageQuery = "";

		/// <summary>BuiltFromCL as query for filtering.</summary>
		public string BuiltFromCL = "";

		/// <summary>BuggId as query for filtering.</summary>
		public string BuggId = "";

		/// <summary>The page to display from the list.</summary>
		public int Page = 1;

		/// <summary>The number of items to display per page.</summary>
		public int PageSize = 100;

		/// <summary>The term to sort by. e.g. Jira.</summary>
		public string SortTerm = "";

		/// <summary>The types of crashes we wish to see.</summary>
		public string CrashType = "CrashesAsserts";

		/// <summary>Whether to sort ascending or descending.</summary>
		public string SortOrder = "Descending";

		/// <summary>The previous sort order.</summary>
		public string PreviousOrder = "";

		/// <summary>The user group to filter by.</summary>
		public string UserGroup = "General";

		/// <summary>The earliest date we are interested in.</summary>
		public DateTime DateFrom = DateTime.UtcNow;

		/// <summary>The most recent date we are interested in.</summary>
		public DateTime DateTo = DateTime.UtcNow;

		/// <summary>The branch name to filter by.</summary>
		public string BranchName = "";

		/// <summary>The version name to filter by.</summary>
		public string VersionName = "";

		/// <summary>The platform name to filter by.</summary>
		public string PlatformName = "";

		/// <summary>The game to filter by.</summary>
		public string GameName = "";

		private string PreviousTerm = "";

	    public string EngineMode = "";

		/// <summary>
		/// Intelligently extract parameters from a web request that could be in a form or a query string (GET vs. POST).
		/// </summary>
		/// <param name="Request">The owning HttpRequest.</param>
		/// <param name="Form">The form passed to the controller.</param>
		/// <param name="Key">The parameter we wish to retrieve a value for.</param>
		/// <param name="DefaultValue">The default value to set if the parameter is null or empty.</param>
		/// <param name="Result">The value of the requested parameter.</param>
		/// <returns>true if the parameter was found to be not null or empty.</returns>
		private bool GetFormParameter( HttpRequestBase Request, FormCollection Form, string Key, string DefaultValue, out string Result )
		{
			Result = DefaultValue;

			string Value = "";
			if (Form.Count == 0)
			{
				Value = Request.QueryString[Key];
			}
			else
			{
				Value = Form[Key];
			}

			if (!string.IsNullOrEmpty( Value ))
			{
				Result = Value.Trim();
				return true;
			}

			return false;
		}

		/// <summary>
		/// Same as GetFormParameter, but ignores validation. Only used for search query.
		/// </summary>
		/// <param name="Request">The owning HttpRequest.</param>
		/// <param name="Form">The form passed to the controller.</param>
		/// <param name="Key">The parameter we wish to retrieve a value for.</param>
		/// <param name="DefaultValue">The default value to set if the parameter is null or empty.</param>
		/// <param name="Result">The value of the requested parameter.</param>
		/// <returns>true if the parameter was found to be not null or empty.</returns>
		private bool GetFormParameterUnvalidated( HttpRequestBase Request, FormCollection Form, string Key, string DefaultValue, out string Result )
		{
			var Collection = Request.Unvalidated;

			Result = DefaultValue;

			string Value = "";
			if (Form.Count == 0)
			{
				Value = Collection.QueryString[Key];
			}
			else
			{
				Value = Form[Key];
			}

			if (!string.IsNullOrEmpty( Value ))
			{
				Result = Value.Trim();
				return true;
			}

			return false;
		}

		/// <summary>
		/// Attempt to read a form item as a date in milliseconds since 1970
		/// </summary>
		/// <param name="Request">The request that contains parameters if the form does not.</param>
		/// <param name="Form">The form that contains parameters if the request does not.</param>
		/// <param name="Key">Name of item in form/request</param>
		/// <param name="Date">Result, only modified on succes</param>
		void TryParseDate( HttpRequestBase Request, FormCollection Form, string Key, ref DateTime Date )
		{
			string MillisecondsString;
			if (!GetFormParameter( Request, Form, Key, "", out MillisecondsString ))
			{
				return;
			}

			try
			{
				Date = CrashesViewModel.Epoch.AddMilliseconds( long.Parse( MillisecondsString ) );
			}
			catch (FormatException)
			{
			}
		}

		/// <summary>
		/// Extract all the parameters from the client user input.
		/// </summary>
		/// <param name="Request">The request that contains parameters if the form does not.</param>
		/// <param name="Form">The form that contains parameters if the request does not.</param>
		/// <param name="DefaultSortTerm">The default sort term to use.</param>
		public FormHelper( HttpRequestBase Request, FormCollection Form, string DefaultSortTerm )
		{
			// Set up Default values if there is no QueryString and set values to the Query string if it is there.
			GetFormParameterUnvalidated( Request, Form, "SearchQuery", SearchQuery, out SearchQuery );
			GetFormParameter( Request, Form, "UsernameQuery", UsernameQuery, out UsernameQuery );
			GetFormParameter( Request, Form, "EpicIdOrMachineQuery", EpicIdOrMachineQuery, out EpicIdOrMachineQuery );
			GetFormParameter( Request, Form, "JiraQuery", JiraQuery, out JiraQuery );
            GetFormParameter( Request, Form, "JiraId", JiraId, out JiraId);
			GetFormParameter( Request, Form, "MessageQuery", MessageQuery, out MessageQuery );
			GetFormParameter( Request, Form, "BuggId", BuggId, out BuggId );
			GetFormParameter( Request, Form, "BuiltFromCL", BuiltFromCL, out BuiltFromCL );
			GetFormParameter( Request, Form, "SortTerm", DefaultSortTerm, out SortTerm );
			GetFormParameter( Request, Form, "CrashType", CrashType, out CrashType );
			GetFormParameter( Request, Form, "UserGroup", UserGroup, out UserGroup );
			GetFormParameter( Request, Form, "BranchName", BranchName, out BranchName );
			GetFormParameter( Request, Form, "VersionName", VersionName, out VersionName );
			GetFormParameter( Request, Form, "PlatformName", PlatformName, out PlatformName );
			GetFormParameter( Request, Form, "GameName", GameName, out GameName );
            GetFormParameter(Request, Form, "EngineMode", EngineMode, out EngineMode);


			string PageString = Page.ToString();
			if (GetFormParameter( Request, Form, "Page", PageString, out PageString ))
			{
				if (!int.TryParse( PageString, out Page ) || Page < 1)
				{
					Page = 1;
				}
			}

			string PageSizeString = PageSize.ToString();
			if (GetFormParameter( Request, Form, "PageSize", PageSizeString, out PageSizeString ))
			{
				if (!int.TryParse( PageSizeString, out PageSize ) || PageSize < 1)
				{
					PageSize = 100;
				}
			}

			GetFormParameter( Request, Form, "SortOrder", SortOrder, out SortOrder );
			GetFormParameter( Request, Form, "PreviousOrder", PreviousOrder, out PreviousOrder );
			GetFormParameter( Request, Form, "PreviousTerm", PreviousTerm, out PreviousTerm );
			
			DateFrom = DateTime.Today.AddDays( -7 ).ToUniversalTime();
			TryParseDate( Request, Form, "DateFrom", ref DateFrom );

			DateTo = DateTime.Today.ToUniversalTime();
			TryParseDate( Request, Form, "DateTo", ref DateTo );

			// Set the sort order 
			if (PreviousOrder == "Descending" && PreviousTerm == SortTerm)
			{
				SortOrder = "Ascending";
			}
			else if (PreviousOrder == "Ascending" && PreviousTerm == SortTerm)
			{
				SortOrder = "Descending";
			}
			else if (string.IsNullOrEmpty( PreviousOrder ))
			{
				// keep SortOrder Where it's at.
			}
			else
			{
				SortOrder = "Descending";
			}
		}
	}
}