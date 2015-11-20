// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data;
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
		/// <summary>
		/// Context
		/// </summary>
		public CrashReportDataContext Context;

		/// <summary>
		/// The default constructor.
		/// </summary>
		public BuggRepository()
		{
			Context = new CrashReportDataContext();
		}

		/// <summary> Submits enqueue changes to the database. </summary>
		public void SubmitChanges()
		{
			Context.SubmitChanges();
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
			Context.Dispose();
		}
	
		private const string DefaultUserGroup = "General";

		/// <summary>
		/// Sets the status for all crashes in a Bugg.
		/// </summary>
		/// <param name="Status">The new status of all crashes.</param>
		/// <param name="BuggId">The id of the Bugg to update the crashes for.</param>
		public void SetBuggStatus( string Status, int BuggId )
		{
			try
			{
				string Query = "UPDATE Crashes SET Status = {0} WHERE Id IN ( SELECT CrashId FROM Buggs_Crashes WHERE BuggId = {1} )";
				Context.ExecuteCommand( Query, Status, BuggId );

				Query = "UPDATE Buggs SET Status = {0} WHERE id = {1}";
				Context.ExecuteCommand( Query, Status, BuggId );
			}
			catch( Exception Ex )
			{
				FLogger.Global.WriteException( "SetBuggStatus: " + Ex.ToString() );
			}
		}


		/// <summary>
		/// Sets the fixed in changelist for all crashes in a Bugg.
		/// </summary>
		/// <param name="FixedChangeList">A string representing a revision.</param>
		/// <param name="BuggId">The id of the Bugg to update the crashes for.</param>
		public void SetBuggFixedChangeList( string FixedChangeList, int BuggId )
		{
			try
			{
				string Query = "UPDATE Crashes SET FixedChangeList = {0} WHERE Id IN ( SELECT CrashId FROM Buggs_Crashes WHERE BuggId = {1} )";
				Context.ExecuteCommand( Query, FixedChangeList, BuggId );

				Query = "UPDATE Buggs SET FixedChangeList = {0} WHERE id = {1}";
				Context.ExecuteCommand( Query, FixedChangeList, BuggId );
			}
			catch( Exception Ex )
			{
				FLogger.Global.WriteException( "SetBuggFixedChangeList: " + Ex.ToString() );
			}
		}

		/// <summary>
		/// Sets the JIRA for all crashes in a Bugg.
		/// </summary>
		/// <param name="JIRA">A string representing a TTP.</param>
		/// <param name="BuggId">The id of the Bugg to update the crashes for.</param>
		public void SetJIRAForBuggAndCrashes( string JIRA, int BuggId )
		{
			try
			{
				using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( "SetJIRAForBuggAndCrashes (" + BuggId + ")" ) )
				{
					string Query = "UPDATE Crashes SET TTPID = {0} WHERE Id IN ( SELECT CrashId FROM Buggs_Crashes WHERE BuggId = {1} )";
					Context.ExecuteCommand( Query, JIRA, BuggId );

					Query = "UPDATE Buggs SET TTPID = {0} WHERE id = {1}";
					Context.ExecuteCommand( Query, JIRA, BuggId );
				}
			}
			catch( Exception Ex )
			{
				FLogger.Global.WriteException( "SetBuggTTPID: " + Ex.ToString() );
			}
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
						from BuggDetail in Context.Buggs
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
		/// Get the list of id of a function from its name.
		/// </summary>
		/// <param name="FunctionCallName">The name of the function to look up.</param>
		public List<int> GetFunctionCallIds( string FunctionCallName )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				try
				{
					return Context.FunctionCalls.Where( FunctionCallInstance => FunctionCallInstance.Call.Contains( FunctionCallName ) ).Select( X => X.Id ).ToList();
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetFunctionCallId: " + Ex.ToString() );
				}

				return new List<int>();
			}
		}

		/// <summary>
		/// Associate a set of crashes and their owners with a Bugg.
		/// NOT USED, FIX!
		/// </summary>
		/// <param name="Bugg">The Bugg to get the data.</param>
		/// <param name="Crashes">The set of crashes to add to the Bugg.</param>
		public void UpdateBuggData( Bugg Bugg, IEnumerable<Crash> Crashes )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(" + Bugg.Id + ")" ) )
			{
				FLogger.Global.WriteEvent( "UpdateBuggData Bugg.Id=" + Bugg.Id );

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
						CurrentCrash.Jira = Bugg.Jira;
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
						SubmitChanges();
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
				if( Context.Buggs_Crashes.Where( BuggInstance => BuggInstance.CrashId == CurrentCrash.Id && BuggInstance.BuggId == Bugg.Id ).Count() < 1 )
				{
					// We don't so create the relationship
					Buggs_Crash NewBugg = new Buggs_Crash();
					NewBugg.CrashId = CurrentCrash.Id;
					NewBugg.BuggId = Bugg.Id;
					Context.Buggs_Crashes.InsertOnSubmit( NewBugg );
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
				int BuggUserCount = Context.Buggs_Users.Where( BuggUserInstance => BuggUserInstance.BuggId == Bugg.Id && BuggUserInstance.UserNameId == UserNameId ).Count();
				if( BuggUserCount < 1 )
				{
					Buggs_User NewBuggsUser = new Buggs_User();
					NewBuggsUser.BuggId = Bugg.Id;
					NewBuggsUser.UserNameId = UserNameId;

					Context.Buggs_Users.InsertOnSubmit( NewBuggsUser );
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
			return Context.Buggs.AsQueryable();
		}

		/// <summary>
		/// Filter the list of Buggs by a callstack entry.
		/// </summary>
		public IEnumerable<Bugg> FilterByCallstack( IEnumerable<Bugg> Results, string CallstackEntry )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Query=" + CallstackEntry + ")" ) )
			{
				// Also may want to revisit how we search since this could get inefficient for a big search set.
				IEnumerable<Bugg> Buggs;
				try
				{
					string QueryString = HttpUtility.HtmlDecode( CallstackEntry.ToString() );
					if( QueryString == null )
					{
						QueryString = "";
					}

					// Take out terms starting with a - 
					string[] Terms = QueryString.Split( "-, ;+".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					HashSet<int> AllFuncionCallIds = new HashSet<int>();
					foreach( string Term in Terms )
					{
						List<int> FunctionCallIds = GetFunctionCallIds( Term );
						foreach( int Id in FunctionCallIds )
						{
							AllFuncionCallIds.Add( Id );
						};
					}

					HashSet<Bugg> TemporaryResults = new HashSet<Bugg>();
					// Search for all function ids. OR operation, not very efficient, but for searching for one function should be ok.
					foreach (int Id in AllFuncionCallIds)
					{
						var BuggsForFunc = Results.Where( X => X.Pattern.Contains( Id + "+" ) || X.Pattern.Contains( "+" + Id ) ).ToList();
						
						foreach(Bugg Bugg in BuggsForFunc)
						{
							TemporaryResults.Add( Bugg );
						}
					}

					Buggs = TemporaryResults.ToList();
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

				IEnumerable<Bugg> Results = null;
				int Skip = ( FormData.Page - 1 ) * FormData.PageSize;
				int Take = FormData.PageSize;

				// Get every Bugg.
				var ResultsAll = ListAll();

				// Look at all Buggs that are still 'open' i.e. the last crash occurred in our date range.
				Results = FilterByDate( ResultsAll, FormData.DateFrom, FormData.DateTo );

				// Filter results by build version.
				Results = FilterByBuildVersion( Results, FormData.VersionName );

				// Run at the end
				if( !string.IsNullOrEmpty( FormData.SearchQuery ) )
				{
					Results = FilterByCallstack( Results, FormData.SearchQuery );
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
				SortedDictionary<string, int> GroupCounts = GetCountsByGroup( Results );

				// Filter by user group if present
				Results = FilterByUserGroup( Results, FormData.UserGroup );

				// Pass in the results and return them sorted properly
				IEnumerable<Bugg> SortedResults = GetSortedResults( Results, FormData.SortTerm, ( FormData.SortOrder == "Descending" ), FormData.DateFrom, FormData.DateTo, FormData.UserGroup );

				// Grab just the results we want to display on this page

				var SortedResultsList = SortedResults.ToList();
				var TotalCountedRecords = SortedResultsList.Count();

				SortedResultsList = SortedResultsList.GetRange( Skip, TotalCountedRecords >= Skip + Take ? Take : TotalCountedRecords );

				return new BuggsViewModel
				{
					Results = SortedResultsList,
					PagingInfo = new PagingInfo { CurrentPage = FormData.Page, PageSize = FormData.PageSize, TotalResults = Results.Count() },
					SortTerm = FormData.SortTerm,
					SortOrder = FormData.SortOrder,
					UserGroup = FormData.UserGroup,
					CrashType = FormData.CrashType,
					SearchQuery = FormData.SearchQuery,
					DateFrom = (long)( FormData.DateFrom - CrashesViewModel.Epoch ).TotalMilliseconds,
					DateTo = (long)( FormData.DateTo - CrashesViewModel.Epoch ).TotalMilliseconds,
					VersionName = FormData.VersionName,
					GroupCounts = GroupCounts,
				};
			}
		}

		/// <summary>
		/// Bucket the Buggs by user group.
		/// </summary>
		/// <param name="Buggs">A list of Buggs to bucket.</param>
		/// <returns>A dictionary of user group names, and the count of Buggs for each group.</returns>
		public SortedDictionary<string, int> GetCountsByGroup( IEnumerable<Bugg> Buggs )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + " SQL OPT" ) )
			{
				Dictionary<string, int> Results = new Dictionary<string, int>();
				try
				{
					Results =
					(
						from BuggDetail in Buggs
						join BuggsUserGroupDetail in Context.Buggs_UserGroups on BuggDetail.Id equals BuggsUserGroupDetail.BuggId
						join UserGroupDetail in Context.UserGroups on BuggsUserGroupDetail.UserGroupId equals UserGroupDetail.Id
						group 0 by UserGroupDetail.Name into GroupCount
						select new { Key = GroupCount.Key, Count = GroupCount.Count() }
					).ToDictionary( x => x.Key, y => y.Count );

					// Add in all groups, even though there are no crashes associated
					IEnumerable<string> UserGroups = ( from UserGroupDetail in Context.UserGroups select UserGroupDetail.Name );
					foreach( string UserGroupName in UserGroups )
					{
						if( !Results.Keys.Contains( UserGroupName ) )
						{
							Results[UserGroupName] = 0;
						}
					}

					/*
					//
					var UsersIDsAndGroupIDs = Context.Users.Select( User => new { UserId = User.Id, UserGroupId = User.UserGroupId } ).ToList();
					var UserGroupArray = Context.UserGroups.ToList();
					UserGroupArray.Sort( ( UG1, UG2 ) => UG1.Name.CompareTo( UG2.Name ) );

					// Initialize all groups to 0.
					foreach( var UserGroup in UserGroupArray )
					{
						Results.Add( UserGroup.Name, 0 );
					}

					Dictionary<int, string> UserIdToGroupName = new Dictionary<int, string>();
					foreach( var UserIds in UsersIDsAndGroupIDs )
					{
						// Find group name for the user id.
						string UserGroupName = UserGroupArray.Where( UG => UG.Id == UserIds.UserGroupId ).First().Name;
						UserIdToGroupName.Add( UserIds.UserId, UserGroupName );
					}

					HashSet<int> UserNameIds = new HashSet<int>();
					foreach( Bugg Bugg in Buggs )
					{
						var CrashList =
						(
							from BuggCrash in Context.Buggs_Crashes
							where BuggCrash.BuggId == Bugg.Id
							select BuggCrash.Crash.UserNameId.GetValueOrDefault()
						).ToList();

						UserNameIds.UnionWith( CrashList );
					}

					foreach( int UserId in UserNameIds )
					{
						string UserGroupName = UserIdToGroupName[UserId];
						Results[UserGroupName]++;
					}*/
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetCountsByGroup: " + Ex.ToString() );
				}

				SortedDictionary<string, int> SortedResults = new SortedDictionary<string, int>( Results );
				return SortedResults;
			}
		}

		/// <summary>
		/// Filter a set of Buggs to a date range.
		/// </summary>
		/// <param name="Results">The unfiltered set of Buggs.</param>
		/// <param name="DateFrom">The earliest date to filter by.</param>
		/// <param name="DateTo">The latest date to filter by.</param>
		/// <returns>The set of Buggs between the earliest and latest date.</returns>
		public IEnumerable<Bugg> FilterByDate( IQueryable<Bugg> Results, DateTime DateFrom, DateTime DateTo )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + " SQL" ) )
			{
				var DateTo1 = DateTo.AddDays( 1 );

				IQueryable<Bugg> BuggsInTimeFrame = Results
					.Where( Bugg =>
					( Bugg.TimeOfFirstCrash <= DateFrom && Bugg.TimeOfLastCrash >= DateTo1 ) ||
					( Bugg.TimeOfFirstCrash >= DateFrom && Bugg.TimeOfLastCrash <= DateTo1 ) ||
					( Bugg.TimeOfFirstCrash >= DateFrom && Bugg.TimeOfFirstCrash <= DateTo1 && Bugg.TimeOfLastCrash >= DateTo1 ) ||
					( Bugg.TimeOfFirstCrash <= DateFrom && Bugg.TimeOfLastCrash <= DateTo1 && Bugg.TimeOfLastCrash >= DateFrom )
					);

				IEnumerable<Bugg> BuggsInTimeFrameEnumerable = BuggsInTimeFrame.ToList();
				return BuggsInTimeFrameEnumerable;
			}
		}

		/// <summary>
		/// Filter a set of Buggs by build version.
		/// </summary>
		/// <param name="Results">The unfiltered set of Buggs.</param>
		/// <param name="BuildVersion">The build version to filter by.</param>
		/// <returns>The set of Buggs that matches specified build version</returns>
		public IEnumerable<Bugg> FilterByBuildVersion( IEnumerable<Bugg> Results, string BuildVersion )
		{
			IEnumerable<Bugg> BuggsForBuildVersion = Results;

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
		/// <param name="GroupName">The user group name to filter by.</param>
		/// <returns>The set of Buggs by users in the requested user group.</returns>
		public IEnumerable<Bugg> FilterByUserGroup( IEnumerable<Bugg> SetOfBuggs, string GroupName )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(GroupName=" + GroupName + ") SQL" ) )
			{
				IQueryable<Bugg> NewSetOfBuggs = null;
				IQueryable<Bugg> SetOfBuggsQueryable = SetOfBuggs.AsQueryable();

				try
				{
					NewSetOfBuggs =
					(
						from BuggDetail in SetOfBuggsQueryable
						join BuggsUserGroupDetail in Context.Buggs_UserGroups on BuggDetail.Id equals BuggsUserGroupDetail.BuggId
						join UserGroupDetail in Context.UserGroups on BuggsUserGroupDetail.UserGroupId equals UserGroupDetail.Id
						where UserGroupDetail.Name.Contains( GroupName )
						select BuggDetail
					);

				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in FilterByUserGroup: " + Ex.ToString() );
				}


				return NewSetOfBuggs.AsEnumerable();
			}
		}

		

		/// <summary>
		/// 
		/// </summary>
		/// <typeparam name="TKey"></typeparam>
		/// <param name="Query"></param>
		/// <param name="Predicate"></param>
		/// <param name="bDescending"></param>
		/// <returns></returns>
		public IEnumerable<Bugg> EnumerableOrderBy<TKey>( IEnumerable<Bugg> Query, Func<Bugg, TKey> Predicate, bool bDescending )
		{
			var Ordered = bDescending ? Query.OrderByDescending( Predicate ) : Query.OrderBy( Predicate );
			return Ordered;
		}

		/// <summary>
		/// Sort the container of Buggs by the requested criteria.
		/// </summary>
		/// <param name="Results">A container of unsorted Buggs.</param>
		/// <param name="SortTerm">The term to sort by.</param>
		/// <param name="bSortDescending">Whether to sort by descending or ascending.</param>
		/// <param name="DateFrom">The date of the earliest Bugg to examine.</param>
		/// <param name="DateTo">The date of the most recent Bugg to examine.</param>
		/// <param name="GroupName">The user group name to filter by.</param>
		/// <returns>A sorted container of Buggs.</returns>
		public IEnumerable<Bugg> GetSortedResults( IEnumerable<Bugg> Results, string SortTerm, bool bSortDescending, DateTime DateFrom, DateTime DateTo, string GroupName )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				try
				{
					// Get the group id and grab all buggs for the specified group.
					HashSet<string> UserNamesForUserGroup = FRepository.Get( this ).GetUserNamesFromGroupName( GroupName );

					// Simplified query.
					var BuggIdToCountMapGroup = new Dictionary<int, int>();
					var BuggIdToCountMapRest = new Dictionary<int, int>();
					var BuggIdToMachineSet = new Dictionary<int, HashSet<string>>();
					Dictionary<string, int> MachineIdToCountMap = new Dictionary<string, int>();
					List<Buggs_Crash> BuggsFromDate = null;
					Dictionary<int, string> CrashToUser = null;
					Dictionary<int, string> CrashToMachine = null;


					// Get all buggs from the date range.
					using( FScopedLogTimer LogTimer1 = new FScopedLogTimer( "BuggRepository.GetSortedResults.BuggsFromDate SQL" ) )
					{
						BuggsFromDate =
								(
									from BuggCrash in Context.Buggs_Crashes
									where BuggCrash.Crash.TimeOfCrash >= DateFrom && BuggCrash.Crash.TimeOfCrash <= DateTo.AddDays( 1 )
									select BuggCrash
								).AsEnumerable().ToList();

						var CrashesWithIdUserMachine =
						(
							from Crash in Context.Crashes
							where Crash.TimeOfCrash >= DateFrom && Crash.TimeOfCrash <= DateTo.AddDays( 1 )
							select new { Id = Crash.Id, UserName = Crash.UserName, MachineId = Crash.MachineId }
						);

						CrashToUser = CrashesWithIdUserMachine.ToDictionary( x => x.Id, y => y.UserName );
						CrashToMachine = CrashesWithIdUserMachine.ToDictionary( x => x.Id, y => y.MachineId );
					}


					using( FScopedLogTimer LogTimer0 = new FScopedLogTimer( "BuggRepository.GetSortedResults.Filtering" ) )
					{
						// This calculates total crashes for selected group and all groups.
						foreach( Buggs_Crash BuggCrash in BuggsFromDate )
						{
							string MachineId;
							CrashToMachine.TryGetValue( BuggCrash.CrashId, out MachineId );

							string UserName = CrashToUser[BuggCrash.CrashId];
							bool bValidForGroup = UserNamesForUserGroup.Contains( UserName );
							if( !bValidForGroup )
							{
								int CountRest = 0;
								bool bFoundRestKey = BuggIdToCountMapRest.TryGetValue( BuggCrash.BuggId, out CountRest );
								if( bFoundRestKey )
								{
									BuggIdToCountMapRest[BuggCrash.BuggId]++;
								}
								else
								{
									BuggIdToCountMapRest.Add( BuggCrash.BuggId, 1 );
								}

								continue;
							}

							int Count = 0;
							bool bFoundKey = BuggIdToCountMapGroup.TryGetValue( BuggCrash.BuggId, out Count );
							if( bFoundKey )
							{
								BuggIdToCountMapGroup[BuggCrash.BuggId]++;
							}
							else
							{
								BuggIdToCountMapGroup.Add( BuggCrash.BuggId, 1 );
							}

							if( MachineId != null && MachineId.Length > 0 )
							{
								HashSet<string> MachineSet = null;
								bool bFoundMachineKey = BuggIdToMachineSet.TryGetValue( BuggCrash.BuggId, out MachineSet );
								if( !bFoundMachineKey )
								{
									BuggIdToMachineSet.Add( BuggCrash.BuggId, new HashSet<string>() );
								}

								BuggIdToMachineSet[BuggCrash.BuggId].Add( MachineId );
							}
						}
					}

					using( FScopedLogTimer LogTimer2 = new FScopedLogTimer( "BuggRepository.GetSortedResults.CrashesInTimeFrame" ) )
					{
						foreach( var Result in Results )
						{
							int GroupCount = 0;
							BuggIdToCountMapGroup.TryGetValue( Result.Id, out GroupCount );
							Result.CrashesInTimeFrameGroup = GroupCount;

							int GroupRest = 0;
							BuggIdToCountMapRest.TryGetValue( Result.Id, out GroupRest );
							Result.CrashesInTimeFrameAll = GroupCount + GroupRest;
						}
					}

					using( FScopedLogTimer LogTimer2 = new FScopedLogTimer( "BuggRepository.GetSortedResults.UniqueMachineCrashesInTimeFrame" ) )
					{
						foreach( var Result in Results )
						{
							HashSet<string> MachineSet = null;
							bool bFoundKey = BuggIdToMachineSet.TryGetValue( Result.Id, out MachineSet );
							if( bFoundKey )
							{
								Result.NumberOfUniqueMachines = MachineSet.Count;
							}
							else
							{
								Result.NumberOfUniqueMachines = 0;
							}
						}
					}

					switch( SortTerm )
					{
						case "CrashesInTimeFrameGroup":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.CrashesInTimeFrameGroup, bSortDescending );
							break;

						case "CrashesInTimeFrameAll":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.CrashesInTimeFrameAll, bSortDescending );
							break;

						case "Id":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.Id, bSortDescending );
							break;

						case "BuildVersion":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.BuildVersion, bSortDescending );
							break;

						case "LatestCrash":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.TimeOfLastCrash, bSortDescending );
							break;

						case "FirstCrash":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.TimeOfFirstCrash, bSortDescending );
							break;

						case "NumberOfCrashes":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.NumberOfCrashes, bSortDescending );
							break;


						case "NumberOfUsers":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.NumberOfUniqueMachines, bSortDescending );
							break;

						case "Pattern":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.Pattern, bSortDescending );
							break;

						case "CrashType":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.CrashType, bSortDescending );
							break;

						case "Status":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.Status, bSortDescending );
							break;

						case "FixedChangeList":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.FixedChangeList, bSortDescending );
							break;

						case "TTPID":
							Results = EnumerableOrderBy( Results, BuggCrashInstance => BuggCrashInstance.Jira, bSortDescending );
							break;

					}
					return Results;
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
