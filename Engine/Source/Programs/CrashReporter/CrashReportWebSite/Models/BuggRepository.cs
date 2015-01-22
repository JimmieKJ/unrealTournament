// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.Linq.SqlClient;
using System.Diagnostics;
using System.Linq;
using System.Web;
using Naspinski.IQueryableSearch;
using System.Data.Linq;
using System.Linq.Dynamic;
using System.Reflection;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// The class to handle the processing of bucketed crashes a.k.a. Buggs.
	/// </summary>
	public class BuggRepository : IDisposable
	{
		private const string DefaultUserGroup = "General";

		private readonly CrashReportDataContext BuggsDataContext;

		/// <summary>
		/// The default constructor to create a data context to access the database.
		/// </summary>
		public BuggRepository()
		{
			BuggsDataContext = new CrashReportDataContext();
		}

		/// <summary>
		/// Implementing Dispose.
		/// </summary>
		public void Dispose()
		{
			Dispose( true );
			GC.SuppressFinalize( this );
		}

		/// <summary>
		/// Disposes the resources.
		/// </summary>
		/// <param name="Disposing">true if the Dispose call is from user code, and not system code.</param>
		protected virtual void Dispose( bool Disposing )
		{
			BuggsDataContext.Dispose();
		}

		/// <summary>
		/// A accessor for the data context.
		/// </summary>
		/// <returns>The data context used to interface with the database.</returns>
		public CrashReportDataContext GetDataContext()
		{
			return BuggsDataContext;
		}

		/// <summary>
		/// Get a Bugg from an id
		/// </summary>
		/// <param name="Id">The id of a Bugg.</param>
		/// <returns>A Bugg representing a group of crashes.</returns>
		public Bugg GetBugg( int Id )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(" + Id + ")" ) )
			{
				Bugg Result = null;

				try
				{
					Result =
					(
						from BuggDetail in BuggsDataContext.Buggs
						where BuggDetail.Id == Id
						select BuggDetail
					).FirstOrDefault();
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetBugg: " + Ex.ToString() );
				}

				return Result;
			}
		}

		/// <summary>
		/// Get the id of a function from its name.
		/// </summary>
		/// <param name="FunctionCallName">The name of the function to look up.</param>
		/// <returns>The unique id of the function name, or zero if none is found.</returns>
		public int GetFunctionCallId( string FunctionCallName )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				try
				{
					FunctionCall FunctionCall = BuggsDataContext.FunctionCalls.Where( FunctionCallInstance => FunctionCallInstance.Call.Contains( FunctionCallName ) ).First();
					return FunctionCall.Id;
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetFunctionCallId: " + Ex.ToString() );
				}

				return 0;
			}
		}

		/// <summary>
		/// Get all the function names in a callstack pattern.
		/// </summary>
		/// <param name="Pattern">The pattern to expand to a full callstack.</param>
		/// <returns>A list of callstack lines.</returns>
		public List<string> GetFunctionCalls( string Pattern )
		{
			CachedDataService CachedResults = new CachedDataService( HttpContext.Current.Cache, this );
			List<string> FunctionCalls = CachedResults.GetFunctionCalls( Pattern );

			return FunctionCalls;
		}

		/// <summary>
		/// Retrieves a list of function names from a list of function name ids.
		/// Primarily used to fill GetFunctionCalls in CachedDataService
		/// </summary>
		/// <param name="Ids">A list of unique function name ids.</param>
		/// <returns>A list of strings that make up a callstack.</returns>
		public List<string> GetFunctionCalls( List<int> Ids )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Ids.Count=" + Ids.Count + ")" ) )
			{
				List<string> FunctionCalls = new List<string>();
				try
				{
					List<FunctionCall> Funcs = BuggsDataContext.FunctionCalls.Where( FuncCall => Ids.Contains( FuncCall.Id ) ).ToList();
					// Order by Ids
					foreach( int Id in Ids )
					{
						var Found = Funcs.Find( FC => FC.Id == Id );
						FunctionCalls.Add( Found.Call );
					}
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetFunctionCalls: " + Ex.ToString() );
				}

				return FunctionCalls;
			}
		}

		/// <summary>
		/// Associate a set of crashes and their owners with a Bugg.
		/// </summary>
		/// <param name="Bugg">The Bugg to get the data.</param>
		/// <param name="Crashes">The set of crashes to add to the Bugg.</param>
		public void UpdateBuggData( Bugg Bugg, IEnumerable<Crash> Crashes )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString()+ "(" + Bugg.Id + ")" ) )
			{
				FLogger.WriteEvent( "UpdateBuggData Bugg.Id=" + Bugg.Id );

				DateTime? TimeOfFirstCrash = null;
				DateTime? TimeOfLastCrash = null;
				string BuildVersion = null;
				List<int> UserNameIds = new List<int>();
				int CrashCount = 0;

				// Set or return min date max date while we're iterating through the crashes
				bool bHasChanges = false;
				try
				{
					foreach( Crash CurrentCrash in Crashes )
					{
						CrashCount++;
						if( TimeOfFirstCrash == null || CurrentCrash.TimeOfCrash < TimeOfFirstCrash )
						{
							TimeOfFirstCrash = CurrentCrash.TimeOfCrash;
						}

						if( TimeOfLastCrash == null || CurrentCrash.TimeOfCrash > TimeOfLastCrash )
						{
							TimeOfLastCrash = CurrentCrash.TimeOfCrash;
						}

						if( BuildVersion == null || CurrentCrash.BuildVersion.CompareTo( BuildVersion ) > 0 )
						{
							BuildVersion = CurrentCrash.BuildVersion;
						}

						// Handle user count
						if( Bugg.Id != 0 )
						{
							if( !UserNameIds.Contains( CurrentCrash.User.Id ) )
							{
								// Add username to local users variable that we will use later to set user count and associate users and user groups to the bugg.
								UserNameIds.Add( CurrentCrash.User.Id );
							}
						}

						CurrentCrash.FixedChangeList = Bugg.FixedChangeList;
						CurrentCrash.TTPID = Bugg.TTPID;
						CurrentCrash.Status = Bugg.Status;

						if( Bugg.Id != 0 )
						{
							bHasChanges |= LinkCrashToBugg( Bugg, CurrentCrash );
						}
					}

					if( CrashCount > 1 )
					{
						Bugg.TimeOfLastCrash = TimeOfLastCrash;
						Bugg.TimeOfFirstCrash = TimeOfFirstCrash;
						Bugg.NumberOfUsers = UserNameIds.Count();
						Bugg.NumberOfCrashes = CrashCount;
						Bugg.BuildVersion = BuildVersion;

						foreach( int UserNameId in UserNameIds )
						{
							// Link user to Bugg
							bHasChanges |= LinkUserToBugg( Bugg, UserNameId );
						}
					}

					if( bHasChanges )
					{
						BuggsDataContext.SubmitChanges();
					}
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in UpdateBuggData: " + Ex.ToString() );
				}

			}
		}

		/// <summary>
		/// Associate a crash with a Bugg.
		/// </summary>
		/// <param name="Bugg">The Bugg to add the crash reference to.</param>
		/// <param name="CurrentCrash">The crash to add to the Bugg.</param>
		/// <returns>true if a new relationship was added, false otherwise.</returns>
		public bool LinkCrashToBugg( Bugg Bugg, Crash CurrentCrash )
		{
			try
			{
				// Make sure we don't already have this relationship
				if( BuggsDataContext.Buggs_Crashes.Where( BuggInstance => BuggInstance.CrashId == CurrentCrash.Id && BuggInstance.BuggId == Bugg.Id ).Count() < 1 )
				{
					// We don't so create the relationship
					Buggs_Crash NewBugg = new Buggs_Crash();
					NewBugg.CrashId = CurrentCrash.Id;
					NewBugg.BuggId = Bugg.Id;
					BuggsDataContext.Buggs_Crashes.InsertOnSubmit( NewBugg );
					return true;
				}
			}
			catch( Exception Ex )
			{
				Debug.WriteLine( "Exception in LinkCrashToBugg: " + Ex.ToString() );
			}

			return false;
		}

		/// <summary>
		/// Associate a user with a Bugg.
		/// </summary>
		/// <param name="Bugg">The Bugg to associate the user with.</param>
		/// <param name="UserNameId">The id of a user to associate with the Bugg.</param>
		/// <returns>true if a new relationship was added, false otherwise.</returns>
		public bool LinkUserToBugg( Bugg Bugg, int UserNameId )
		{
			try
			{
				int BuggUserCount = BuggsDataContext.Buggs_Users.Where( BuggUserInstance => BuggUserInstance.BuggId == Bugg.Id && BuggUserInstance.UserNameId == UserNameId ).Count();
				if( BuggUserCount < 1 )
				{
					Buggs_User NewBuggsUser = new Buggs_User();
					NewBuggsUser.BuggId = Bugg.Id;
					NewBuggsUser.UserNameId = UserNameId;

					BuggsDataContext.Buggs_Users.InsertOnSubmit( NewBuggsUser );
					return true;
				}
			}
			catch( Exception Ex )
			{
				Debug.WriteLine( "Exception in LinkCrashToBugg: " + Ex.ToString() );
			}

			return false;
		}

		/// <summary>
		/// Return all known Buggs.
		/// </summary>
		/// <returns>A container of all known Buggs.</returns>
		public IQueryable<Bugg> ListAll()
		{
			IQueryable<Bugg> Buggs =
			(
				from BuggDetail in BuggsDataContext.Buggs
				select BuggDetail
			);

			return Buggs;
		}

		/// <summary>
		/// Filter the list of Buggs by a search query.
		/// </summary>
		/// <param name="Results">The unfiltered set of Buggs.</param>
		/// <param name="Query">The query to use as a filter.</param>
		/// <returns>A filtered set of Buggs.</returns>
		public IQueryable<Bugg> Search( IQueryable<Bugg> Results, string Query )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Query=" + Query + ")" ) )
			{
				// Also may want to revisit how we search since this could get inefficient for a big search set.
				IQueryable<Bugg> Buggs;
				try
				{
					string QueryString = HttpUtility.HtmlDecode( Query.ToString() );
					if( QueryString == null )
					{
						QueryString = "";
					}

					// Take out terms starting with a - 
					string[] Terms = QueryString.Split( "-, ;+".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					string TermsToUse = "";
					foreach( string Term in Terms )
					{
						string CallId = GetFunctionCallId( Term ).ToString();
						if( !TermsToUse.Contains( CallId ) )
						{
							TermsToUse = TermsToUse + "+" + CallId;
						}
					}

					Buggs = (IQueryable<Bugg>)Results.Search( new string[] { "Pattern" }, TermsToUse.Split( "+".ToCharArray() ) );
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in Search: " + Ex.ToString() );
					Buggs = Results;
				}
				return Buggs;
			}
		}

		/// <summary>
		/// Retrieve all Buggs matching the search criteria.
		/// </summary>
		/// <param name="FormData">The incoming form of search criteria from the client.</param>
		/// <returns>A view to display the filtered Buggs.</returns>
		public BuggsViewModel GetResults( FormHelper FormData )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				// Right now we take a Result IQueryable starting with ListAll() Buggs then we widdle away the result set by tacking on 
				// Linq queries. Essentially it's Results.ListAll().Where().Where().Where().Where().Where().Where()
				// Could possibly create more optimized queries when we know exactly what we're querying
				// The downside is that if we add more parameters each query may need to be updated.... Or we just add new case statements
				// The other downside is that there's less code reuse, but that may be worth it.

				IQueryable<Bugg> Results = null;
				int Skip = ( FormData.Page - 1 ) * FormData.PageSize;
				int Take = FormData.PageSize;

				// Get every Bugg.
				Results = ListAll();

				// Look at all Buggs that are still 'open' i.e. the last crash occurred in our date range.
				Results = FilterByDate( Results, FormData.DateFrom, FormData.DateTo );

				// Filter results by build version.
				Results = FilterByBuildVersion( Results, FormData.BuildVersion );

				// Run at the end
				if( !string.IsNullOrEmpty( FormData.SearchQuery ) )
				{
					Results = Search( Results, FormData.SearchQuery );
				}

				// Filter by Crash Type
				if( FormData.CrashType != "All" )
				{
					switch( FormData.CrashType )
					{
						case "Crashes":
							Results = Results.Where( BuggInstance => BuggInstance.CrashType == 1 );
							break;
						case "Assert":
							Results = Results.Where( BuggInstance => BuggInstance.CrashType == 2 );
							break;
						case "Ensure":
							Results = Results.Where( BuggInstance => BuggInstance.CrashType == 3 );
							break;
						case "CrashesAsserts":
							Results = Results.Where( BuggInstance => BuggInstance.CrashType == 1 || BuggInstance.CrashType == 2 );
							break;
					}
				}

				// Get UserGroup ResultCounts
				Dictionary<string, int> GroupCounts = GetCountsByGroup( Results );

				// Filter by user group if present
				Results = FilterByUserGroup( Results, FormData.UserGroup );

				// Pass in the results and return them sorted properly
				IEnumerable<Bugg> SortedResults = GetSortedResults( Results, FormData.SortTerm, ( FormData.SortOrder == "Descending" ), FormData.DateFrom, FormData.DateTo );

				// Grab just the results we want to display on this page
				SortedResults = SortedResults.Skip( Skip ).Take( Take ).ToList();

				return new BuggsViewModel
				{
					Results = SortedResults,
					PagingInfo = new PagingInfo { CurrentPage = FormData.Page, PageSize = FormData.PageSize, TotalResults = Results.Count() },
					SortTerm = FormData.SortTerm,
					SortOrder = FormData.SortOrder,
					UserGroup = FormData.UserGroup,
					CrashType = FormData.CrashType,
					SearchQuery = FormData.SearchQuery,
					DateFrom = (long)( FormData.DateFrom - CrashesViewModel.Epoch ).TotalMilliseconds,
					DateTo = (long)( FormData.DateTo - CrashesViewModel.Epoch ).TotalMilliseconds,
					BuildVersion = FormData.BuildVersion,
					GroupCounts = GroupCounts,
				};
			}
		}

		/// <summary>
		/// Bucket the Buggs by user group.
		/// </summary>
		/// <param name="Buggs">A list of Buggs to bucket.</param>
		/// <returns>A dictionary of user group names, and the count of Buggs for each group.</returns>
		public Dictionary<string, int> GetCountsByGroup( IQueryable<Bugg> Buggs )
		{
			// @TODO yrx 2014-11-06 Optimize
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				Dictionary<string, int> Results = new Dictionary<string, int>();

				try
				{
					Results =
					(
						from BuggDetail in Buggs
						join BuggsUserGroupDetail in BuggsDataContext.Buggs_UserGroups on BuggDetail.Id equals BuggsUserGroupDetail.BuggId
						join UserGroupDetail in BuggsDataContext.UserGroups on BuggsUserGroupDetail.UserGroupId equals UserGroupDetail.Id
						group 0 by UserGroupDetail.Name into GroupCount
						select new { Key = GroupCount.Key, Count = GroupCount.Count() }
					).ToDictionary( x => x.Key, y => y.Count );

					// Add in all groups, even though there are no crashes associated
					IEnumerable<string> UserGroups = ( from UserGroupDetail in BuggsDataContext.UserGroups select UserGroupDetail.Name );
					foreach( string UserGroupName in UserGroups )
					{
						if( !Results.Keys.Contains( UserGroupName ) )
						{
							Results[UserGroupName] = 0;
						}
					}
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetCountsByGroup: " + Ex.ToString() );
				}

				return Results;
		}
		}

		/// <summary>
		/// Filter a set of Buggs to a date range.
		/// </summary>
		/// <param name="Results">The unfiltered set of Buggs.</param>
		/// <param name="DateFrom">The earliest date to filter by.</param>
		/// <param name="DateTo">The latest date to filter by.</param>
		/// <returns>The set of Buggs between the earliest and latest date.</returns>
		public IQueryable<Bugg> FilterByDate( IQueryable<Bugg> Results, DateTime DateFrom, DateTime DateTo )
		{
			IQueryable<Bugg> BuggsInTimeFrame = Results.Where( Bugg => Bugg.TimeOfLastCrash >= DateFrom && Bugg.TimeOfLastCrash <= AddOneDayToDate( DateTo ) );
			return BuggsInTimeFrame;			
		}

		/// <summary>
		/// Filter a set of Buggs by build version.
		/// </summary>
		/// <param name="Results">The unfiltered set of Buggs.</param>
		/// <param name="BuildVersion">The build version to filter by.</param>
		/// <returns>The set of Buggs that matches specified build version</returns>
		public IQueryable<Bugg> FilterByBuildVersion( IQueryable<Bugg> Results, string BuildVersion )
		{
			IQueryable<Bugg> BuggsForBuildVersion = Results;

			// Filter by BuildVersion
			if( !string.IsNullOrEmpty( BuildVersion ) )
			{
				BuggsForBuildVersion =
				(
					from MyBugg in Results
					where MyBugg.BuildVersion.Contains( BuildVersion )
					select MyBugg
				);
			}
			return BuggsForBuildVersion;	
		} 

		/// <summary>
		/// Filter a set of Buggs by user group name.
		/// </summary>
		/// <param name="SetOfBuggs">The unfiltered set of Buggs.</param>
		/// <param name="UserGroup">The user group name to filter by.</param>
		/// <returns>The set of Buggs by users in the requested user group.</returns>
		public IQueryable<Bugg> FilterByUserGroup( IQueryable<Bugg> SetOfBuggs, string UserGroup )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(UserGroup=" + UserGroup + ")" ) )
			{
				IQueryable<Bugg> NewSetOfBuggs = null;

				try
				{
					NewSetOfBuggs =
					(
						from BuggDetail in SetOfBuggs
						join BuggsUserGroupDetail in BuggsDataContext.Buggs_UserGroups on BuggDetail.Id equals BuggsUserGroupDetail.BuggId
						join UserGroupDetail in BuggsDataContext.UserGroups on BuggsUserGroupDetail.UserGroupId equals UserGroupDetail.Id
						where UserGroupDetail.Name.Contains( UserGroup )
						select BuggDetail
					);

				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in FilterByUserGroup: " + Ex.ToString() );
				}


				return NewSetOfBuggs;
			}
		}

		private DateTime AddOneDayToDate( DateTime Date )
		{
			return Date.AddDays( 1 );
		}

		/// <summary>
		/// Sort the container of Buggs by the requested criteria.
		/// </summary>
		/// <param name="Results">A container of unsorted Buggs.</param>
		/// <param name="SortTerm">The term to sort by.</param>
		/// <param name="bSortDescending">Whether to sort by descending or ascending.</param>
		/// <param name="DateFrom">The date of the earliest Bugg to examine.</param>
		/// <param name="DateTo">The date of the most recent Bugg to examine.</param>
		/// <returns>A sorted container of Buggs.</returns>
		public IQueryable<Bugg> GetSortedResults( IQueryable<Bugg> Results, string SortTerm, bool bSortDescending, DateTime DateFrom, DateTime DateTo )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				try
				{

					var IntermediateResults =
					(
						from BuggCrashDetail in BuggsDataContext.Buggs_Crashes
						where BuggCrashDetail.Crash.TimeOfCrash >= DateFrom && BuggCrashDetail.Crash.TimeOfCrash <= AddOneDayToDate( DateTo )
						group BuggCrashDetail by BuggCrashDetail.BuggId into CrashesGrouped
						join BuggDetail in Results on CrashesGrouped.Key equals BuggDetail.Id
						select new { Bugg = BuggDetail, Count = CrashesGrouped.Count() }
					);

					using( FScopedLogTimer LogTimer2 = new FScopedLogTimer( "BuggRepository.GetSortedResults.CrashesInTimeFrame" ) )
					{
						foreach( var Result in IntermediateResults )
						{
							Result.Bugg.CrashesInTimeFrame = Result.Count;
						}
					}


					switch( SortTerm )
					{
						case "CrashesInTimeFrame":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Count, bSortDescending );
							break;

						case "Id":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.Id, bSortDescending );
							break;

						case "BuildVersion":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.BuildVersion, bSortDescending );
							break;

						case "LatestCrash":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.TimeOfLastCrash, bSortDescending );
							break;

						case "FirstCrash":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.TimeOfFirstCrash, bSortDescending );
							break;

						case "NumberOfCrashes":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.NumberOfCrashes, bSortDescending );
							break;

						case "NumberOfUsers":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.NumberOfUsers, bSortDescending );
							break;

						case "Pattern":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.Pattern, bSortDescending );
							break;

						case "Status":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.Status, bSortDescending );
							break;

						case "FixedChangeList":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.FixedChangeList, bSortDescending );
							break;

						case "TTPID":
							IntermediateResults = CrashRepository.OrderBy( IntermediateResults, BuggCrashInstance => BuggCrashInstance.Bugg.TTPID, bSortDescending );
							break;

					}
					return IntermediateResults.Select( x => x.Bugg );
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetSortedResults: " + Ex.ToString() );
				}
				return Results;
		}
	}
}
}
