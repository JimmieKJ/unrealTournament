// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Web.Mvc;
using System.Text;

using Tools.CrashReporter.CrashReportWebSite.Models;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;

namespace Tools.CrashReporter.CrashReportWebSite.Views.Helpers
{
	/// <summary>
	/// A helper class for writing html code.
	/// </summary>
	public static class UrlHelperExtension
	{
		/// <summary>
		/// Create the html required to create the links for table headers to control the sorting of crashes.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="HeaderName">The table column header name.</param>
		/// <param name="SortTerm">The sort term to use when sorting by the column.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString TableHeader( this UrlHelper Helper, string HeaderName, string SortTerm, CrashesViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			string URL = Helper.Action( "Index", new { 
														controller = "Crashes",
														Page = Model.PagingInfo.CurrentPage,
														SortTerm = SortTerm,
														PreviousOrder = Model.SortOrder,
														PreviousTerm = Model.SortTerm,
														CrashType = Model.CrashType,
														UserGroup = Model.UserGroup,
														SearchQuery = Model.SearchQuery,
														UsernameQuery = Model.UsernameQuery,
														EpicIdOrMachineQuery = Model.EpicIdOrMachineQuery,
														MessageQuery = Model.MessageQuery,
														BuggId = Model.BuggId,
														BuiltFromCL = Model.BuiltFromCL,
														JiraQuery = Model.JiraQuery,
														DateFrom = Model.DateFrom,
														DateTo = Model.DateTo,
														BranchName = Model.BranchName,
														VersionName = Model.VersionName,
														PlatformName = Model.PlatformName,
														GameName = Model.GameName
													} );

			Tag.MergeAttribute( "href", URL );
			string SortArrow = "";
			if( Model.SortTerm == SortTerm )
			{
				if( Model.SortOrder == "Descending" )
				{
					SortArrow = "<img border=0 src='../../Content/Images/SortDescending.png' />";
				}
				else
				{
					SortArrow = "<img border=0 src='../../Content/Images/SortAscending.png' />";
				}
			}
			else
			{
				SortArrow = "<img border=0 src='../../Content/Images/SortPlaceHolder.png' />";
			}

			Tag.InnerHtml = "<span>" + HeaderName + "&nbsp;" + SortArrow + "</span>";
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to make a link for a callstack line for a crash.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="CallStack">A line of a callstack to wrap in a link.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString CallStackSearchLink( this UrlHelper Helper, string CallStack, CrashesViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			string URL = Helper.Action( "Index", new { 
														controller = "Crashes",
														SortTerm = Model.SortTerm,
														SortOrder = Model.SortOrder,
														CrashType = Model.CrashType,
														UserGroup = Model.UserGroup,
														SearchQuery = CallStack,
														UsernameQuery = Model.UsernameQuery,
														EpicIdOrMachineQuery = Model.EpicIdOrMachineQuery,
														MessageQuery = Model.MessageQuery,
														BuggId = Model.BuggId,
														BuiltFromCL = Model.BuiltFromCL,
														JiraQuery = Model.JiraQuery,
														DateFrom = Model.DateFrom,
														DateTo = Model.DateTo,
														BranchName = Model.BranchName,
														VersionName = Model.VersionName,
														PlatformName = Model.PlatformName,
														GameName = Model.GameName
													} );

			Tag.MergeAttribute( "href", URL );
			Tag.InnerHtml = CallStack;
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to change the current user group to filter crashes by.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="UserGroup">The user group to generate a link for.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString UserGroupLink( this UrlHelper Helper, string UserGroup, CrashesViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			string URL = Helper.Action( "Index", new { 
														controller = "Crashes",
														SortTerm = Model.SortTerm,
														SortOrder = Model.SortOrder,
														CrashType = Model.CrashType,
														UserGroup = UserGroup,
														SearchQuery = Model.SearchQuery,
														UsernameQuery = Model.UsernameQuery,
														EpicIdOrMachineQuery = Model.EpicIdOrMachineQuery,
														MessageQuery = Model.MessageQuery,
														BuggId = Model.BuggId,
														BuiltFromCL = Model.BuiltFromCL,
														JiraQuery = Model.JiraQuery,
														DateFrom = Model.DateFrom,
														DateTo = Model.DateTo,
														BranchName = Model.BranchName,
														VersionName = Model.VersionName,
														PlatformName = Model.PlatformName,
														GameName = Model.GameName
													} );

			Tag.MergeAttribute( "href", URL );
			Tag.InnerHtml = UserGroup;
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to create the links for table headers to control the sorting of Buggs.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="HeaderName">The table column header name.</param>
		/// <param name="SortTerm">The sort term to use when sorting by this column.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString TableHeader( this UrlHelper Helper, string HeaderName, string SortTerm, BuggsViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" ); // Construct an <a> Tag

			string URL = Helper.Action( "Index", new { 
														controller = "Buggs",
														Page = Model.PagingInfo.CurrentPage,
														SortTerm = SortTerm,
														PreviousOrder = Model.SortOrder,
														PreviousTerm = Model.SortTerm,
														UserGroup = Model.UserGroup,
														SearchQuery = Model.SearchQuery,
														DateFrom = Model.DateFrom,
														DateTo = Model.DateTo,
														VersionName = Model.VersionName,
														CrashType = Model.CrashType,
													} );

			Tag.MergeAttribute( "href", URL );
			string SortArrow = "";
			if( Model.SortTerm == SortTerm )
			{
				if( Model.SortOrder == "Descending" )
				{
					SortArrow = "<img border=0 src='../../Content/Images/SortDescending.png' />";
				}
				else
				{
					SortArrow = "<img border=0 src='../../Content/Images/SortAscending.png' />";
				}
			}
			else
			{
				SortArrow = "<img border=0 src='../../Content/Images/SortPlaceHolder.png' />";
			}

			Tag.InnerHtml = "<span>" + HeaderName + "&nbsp;" + SortArrow + "</span>";
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to make a link for a callstack line for a Bugg.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="CallStack">A line of callstack to make a link for.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString CallStackSearchLink( this UrlHelper Helper, string CallStack, BuggsViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			string URL = Helper.Action( "Index", new { 
														controller = "Buggs",
														SortTerm = Model.SortTerm,
														SortOrder = Model.SortOrder,
														UserGroup = Model.UserGroup,
														SearchQuery = CallStack,
														DateFrom = Model.DateFrom,
														DateTo = Model.DateTo,
														VersionName = Model.VersionName,
														CrashType = Model.CrashType,
													} );

			Tag.MergeAttribute( "href", URL );
			Tag.InnerHtml = CallStack;
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to change the current user group to filter Buggs by.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="UserGroup">The user group to generate a link for.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString UserGroupLink( this UrlHelper Helper, string UserGroup, BuggsViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			string URL = Helper.Action( "Index", new { 
														controller = "Buggs",
														SortTerm = Model.SortTerm,
														SortOrder = Model.SortOrder,
														UserGroup = UserGroup,
														SearchQuery = Model.SearchQuery,
														DateFrom = Model.DateFrom,
														DateTo = Model.DateTo,
														VersionName = Model.VersionName,
                                                        CrashType = Model.CrashType,
                                                        JiraId = Model.Jira,
                                                        PlatformName = Model.PlatformName,
                                                        BranchName = Model.BranchName
													} );

			Tag.MergeAttribute( "href", URL );
			Tag.InnerHtml = UserGroup;
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to create the links for table headers to control the sorting of a Bugg.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="HeaderName">The table column header name.</param>
		/// <param name="SortTerm">The sort term to use when sorting by this column.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString TableHeader( this UrlHelper Helper, string HeaderName, string SortTerm, BuggViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			Tag.MergeAttribute( "href", "/Buggs/Show/" + Model.Bugg.Id );
			Tag.InnerHtml = "<span>" + HeaderName + "</span>";
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to generate a link for a line of callstack in a Bugg.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="CallStack">A line of callstack to make a link for.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString CallStackSearchLink( this UrlHelper Helper, string CallStack, BuggViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			Tag.MergeAttribute( "href", "/Buggs/Show/" + Model.Bugg.Id );
			Tag.InnerHtml = CallStack;
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}

		/// <summary>
		/// Create the html required to change the current user group to filter users by.
		/// </summary>
		/// <param name="Helper">The Url helper object.</param>
		/// <param name="UserGroup">The user group to generate a link for.</param>
		/// <param name="Model">The view model for the current page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString UserGroupLink( this UrlHelper Helper, string UserGroup, UsersViewModel Model )
		{
			StringBuilder Result = new StringBuilder();
			TagBuilder Tag = new TagBuilder( "a" );

			Tag.MergeAttribute( "href", "/Users/Index/" + UserGroup );
			Tag.InnerHtml = UserGroup;
			Result.AppendLine( Tag.ToString() );

			return MvcHtmlString.Create( Result.ToString() );
		}
	}
}