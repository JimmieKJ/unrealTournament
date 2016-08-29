// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;

namespace Tools.CrashReporter.CrashReportWebSite.ViewModels
{
	/// <summary>
	/// A class to handle pagination of large lists.
	/// </summary>
	public class PagingInfo
	{
		/// <summary>The current page of elements to display.</summary>
		public int CurrentPage { get; set; }
		/// <summary>The total number of elements.</summary>
		public int TotalResults { get; set; }
		/// <summary>The number of elements per page.</summary>
		public int PageSize { get; set; }

		/// <summary>
		/// The total number of pages.
		/// </summary>
		public int PageCount
		{
			// return total number of pages
			get 
			{ 
				return ( int )Math.Ceiling( ( float )TotalResults / ( float )PageSize ); 
			}
		}

		/// <summary>
		/// Return the Index for the first result in the paging list to display.
		/// </summary>
		public int FirstPageIndex
		{
			get 
			{
				if( CurrentPage >= PagingListSize )
				{
					return CurrentPage;
				}

				return 1;
			}
		}

		/// <summary>
		/// Return the Index for the last result in the paging list to display.
		/// </summary>
		public int LastPageIndex
		{
			get 
			{
				if( ( ( FirstPageIndex - 1 ) + PagingListSize ) < LastPage )
				{
					return ( FirstPageIndex - 1 ) + PagingListSize;
				}

				return LastPage;
			}
		}

		/// <summary>
		/// Return the number of pages to display on the screen
		/// </summary>
		public int PagingListSize
		{
			get 
			{ 
				return 10; 
			}
		}

		/// <summary>
		/// Return the Index for the previous result in the paging list to display.
		/// </summary>
		public int PreviousPageIndex
		{
			get 
			{
				if( CurrentPage - 1 > 1 )
				{
					return CurrentPage - 1;
				}

				return 1;
			}
		}

		/// <summary>
		/// Return the Index for the previous result in the paging list to display.
		/// </summary>
		public int NextPageIndex
		{
			get 
			{
				if( CurrentPage + 1 <= LastPage )
				{
					return CurrentPage + 1;
				}

				return LastPage;
			}
		}

		/// <summary>
		/// Return the Index for the first result in the paging list to display.
		/// </summary>
		public int FirstPage
		{
			get 
			{ 
				return 1; 
			}
		}

		/// <summary>
		/// Return the Index for the last result.
		/// </summary>
		public int LastPage
		{
			get 
			{
				return PageCount; 
			}
		}
	}
}