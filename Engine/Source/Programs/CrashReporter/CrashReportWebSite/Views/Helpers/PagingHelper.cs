// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Web.Mvc;
using System.Text;

using Tools.CrashReporter.CrashReportWebSite.Models;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;

namespace Tools.CrashReporter.CrashReportWebSite.Views.Helpers
{
	/// <summary>
	/// Helper class to generate the correct links for next page, previous page etc.
	/// </summary>
	public static class PagingHelper
	{
		/// <summary>
		/// Create html with links to represent last page, next page, current page etc.
		/// </summary>
		/// <param name="Html">The html document writer for the current page.</param>
		/// <param name="WebPagingInfo">Information about the paging of the current page.</param>
		/// <param name="PageUrl">An Url to link to the correct page.</param>
		/// <returns>A string suitable for MVC to render.</returns>
		public static MvcHtmlString PageLinks( this HtmlHelper Html, PagingInfo WebPagingInfo, Func<int, string> PageUrl )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( "PagingHelper" ) )
			{
				StringBuilder ResultString = new StringBuilder();

				// Go to first page
				TagBuilder FirstTag = new TagBuilder( "a" ); // Construct an <a> Tag
				FirstTag.MergeAttribute( "href", PageUrl( WebPagingInfo.FirstPage ) );
				FirstTag.InnerHtml = "<<";

				ResultString.AppendLine( FirstTag.ToString() );

				// Go to previous page
				TagBuilder PreviousTag = new TagBuilder( "a" ); // Construct an <a> Tag
				PreviousTag.MergeAttribute( "href", PageUrl( WebPagingInfo.PreviousPageIndex ) );
				PreviousTag.InnerHtml = "<";

				ResultString.AppendLine( PreviousTag.ToString() );

				for( int PageIndex = WebPagingInfo.FirstPageIndex; PageIndex <= WebPagingInfo.LastPageIndex; PageIndex++ )
				{
					TagBuilder Tag = new TagBuilder( "a" ); // Construct an <a> Tag
					Tag.MergeAttribute( "href", PageUrl( PageIndex ) );
					Tag.InnerHtml = PageIndex.ToString();
					if( PageIndex == WebPagingInfo.CurrentPage )
					{
						Tag.AddCssClass( "selectedPage" );
					}

					ResultString.AppendLine( Tag.ToString() );
				}

				// Go to next page
				TagBuilder NextTag = new TagBuilder( "a" ); // Construct an <a> Tag
				NextTag.MergeAttribute( "href", PageUrl( WebPagingInfo.NextPageIndex ) );
				NextTag.InnerHtml = ">";

				ResultString.AppendLine( NextTag.ToString() );

				// Go to last page
				TagBuilder LastTag = new TagBuilder( "a" ); // Construct an <a> Tag
				LastTag.MergeAttribute( "href", PageUrl( WebPagingInfo.LastPage ) );
				LastTag.InnerHtml = ">>";

				ResultString.AppendLine( LastTag.ToString() );

				return MvcHtmlString.Create( ResultString.ToString() );
			}
		}
	}
}