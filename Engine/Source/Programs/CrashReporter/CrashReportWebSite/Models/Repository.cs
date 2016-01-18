// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System;
using System.Data.Linq;
using System.Diagnostics;
using Tools.DotNETCommon;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// Globally accessible repository for crashes and buggs. 
	/// Requires a new instance of crashes or buggs, or both.
	/// Defines helper methods for the context.
	/// </summary>
	public class FRepository : IDisposable
	{
		private BuggRepository _Buggs;
		private CrashRepository _Crashes;

		/// <summary>
		/// Accesses the instance.
		/// </summary>
		public static FRepository Get( BuggRepository Buggs )
		{
			return new FRepository() { _Buggs = Buggs };
		}
		/// <summary>
		/// Accesses the instance.
		/// </summary>
		public static FRepository Get( CrashRepository Crashes )
		{
			return new FRepository() { _Crashes = Crashes };
		}
		/// <summary>
		/// Accesses the instance.
		/// </summary>
		public static FRepository Get( CrashRepository Crashes, BuggRepository Buggs )
		{
			return new FRepository() { _Crashes = Crashes, _Buggs = Buggs };
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
			if( Crashes != null )
			{
				Crashes.Dispose();
			}
			if( Buggs != null )
			{
				Buggs.Dispose();
			}
		}

		/// <summary>
		/// Accesses BuggRepository
		/// </summary>
		public BuggRepository Buggs
		{
			get
			{
				return _Buggs;
			}
		}

		/// <summary>
		/// Accesses CrashRepository
		/// </summary>
		public CrashRepository Crashes
		{
			get
			{
				return _Crashes;
			}
		}

		/// <summary>
		/// Accesses CrashReportDataContext
		/// </summary>
		public CrashReportDataContext Context
		{
			get
			{
				return Crashes != null ? Crashes.Context : (Buggs != null ? Buggs.Context : null);
			}
		}

		/// <summary>Gets a set of user's Ids from the group name</summary>
		public HashSet<int> GetUserIdsFromUserGroup( string UserGroup )
		{
			int UserGroupId = FindOrAddGroup( UserGroup );
			return GetUserIdsFromUserGroupId( UserGroupId );
		}

		/// <summary>Gets a set of user's Ids from the group Id</summary>
		public HashSet<int> GetUserIdsFromUserGroupId( int UserGroupId )
		{
			var UserIds = Context.Users.Where( X => X.UserGroupId == UserGroupId ).Select( X => X.Id );
			return new HashSet<int>( UserIds );
		}


		/// <summary>Gets a set of user's names from the group name</summary>
		public HashSet<string> GetUserNamesFromGroupName( string GroupName )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(UserGroup=" + GroupName + ")" ) )
			{
				int UserGroupId = FindOrAddGroup( GroupName );
				var UserNames = Context.Users.Where( X => X.UserGroupId == UserGroupId ).Select( X => X.UserName );
				return new HashSet<string>( UserNames );
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="UserGroupId"></param>
		/// <returns></returns>
		public List<string> GetUsersForGroupId( int UserGroupId )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(UserGroupId=" + UserGroupId + ")" ) )
			{
				List<string> Users = new List<string>();
				try
				{
					Users =
					(
						from UserDetail in Context.Users
						where UserDetail.UserGroupId == UserGroupId
						orderby UserDetail.UserName
						select UserDetail.UserName
					).ToList();
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetUsersForGroup: " + Ex.ToString() );
				}

				return Users;
			}
		}

		/// <summary>
		/// Find a user to the database, and add them if they do not exist.
		/// </summary>
		/// <param name="UserName">The user name to find or add.</param>
		/// <param name="UserGroupId">The id of the user group to add the user to.</param>
		/// <returns>The id of the found or added user.</returns>
		/// <remarks>All user interaction is done this way to remove any dependencies on pre-populated tables.</remarks>
		public int FindOrAddUser( string UserName, int UserGroupId = 1 )
		{
			int UserNameId = 0;
			try
			{
				IQueryable<int> UserNames = ( from UserDetail in Context.Users
											  where UserDetail.UserName.ToLower() == UserName.ToLower()
											  select UserDetail.Id );

				// If there is no existing user, add a new one
				if( UserNames.Count() == 0 )
				{
					User NewUser = new User();
					NewUser.UserName = UserName;
					NewUser.UserGroupId = UserGroupId;
					Context.Users.InsertOnSubmit( NewUser );

					Context.SubmitChanges();
					UserNameId = NewUser.Id;
				}
				else
				{
					UserNameId = UserNames.First();
				}
			}
			catch( Exception Ex )
			{
				FLogger.Global.WriteException( "FindOrAddUser: " + Ex.ToString() );
			}

			return UserNameId;
		}

		/// <summary>
		/// Find a user group, or add it if it doesn't exist. Return the id of the user group.
		/// </summary>
		/// <param name="UserGroupName">The user group name to find or add.</param>
		/// <returns>The id of the user group that was found or added.</returns>
		/// <remarks>All user group interaction is done this way to remove any dependencies on pre-populated tables.</remarks>
		public int FindOrAddGroup( string UserGroupName )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(GroupName=" + UserGroupName + ")" ) )
			{
				int UserGroupNameId = 0;
				try
				{
					IQueryable<int> UserGroups = ( from UserGroupDetail in Context.UserGroups
												   where UserGroupDetail.Name.ToLower() == UserGroupName.ToLower()
												   select UserGroupDetail.Id );

					// If there is no existing group, add a new one
					if( UserGroups.Count() == 0 )
					{
						UserGroup NewUserGroup = new UserGroup();
						NewUserGroup.Name = UserGroupName;
						Context.UserGroups.InsertOnSubmit( NewUserGroup );

						Context.SubmitChanges();
						UserGroupNameId = NewUserGroup.Id;
					}
					else
					{
						UserGroupNameId = UserGroups.First();
					}
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "FindOrAddUserGroup: " + Ex.ToString() );
				}

				return UserGroupNameId;
			}
		}

		/// <summary>
		/// Sets a user group for a given user.
		/// </summary>
		/// <param name="UserName">The user name to update.</param>
		/// <param name="UserGroupName">The user group to add the user to.</param>
		public void SetUserGroup( string UserName, string UserGroupName )
		{
			try
			{
				IQueryable<int> UserGroups = ( from UserGroupDetail in Context.UserGroups
											   where UserGroupDetail.Name.ToLower() == UserGroupName.ToLower()
											   select UserGroupDetail.Id );

				IQueryable<int> UserNames = ( from UserDetail in Context.Users
											  where UserDetail.UserName.ToLower() == UserName.ToLower()
											  select UserDetail.Id );

				if( UserGroups.Count() == 1 && UserNames.Count() == 1 )
				{
					string Query = "UPDATE Users SET UserGroupId = {0} WHERE Id = {1}";
					Context.ExecuteCommand( Query, UserGroups.First(), UserNames.First() );
				}
			}
			catch( Exception Ex )
			{
				FLogger.Global.WriteException( "AdSetUserGroupdUser: " + Ex.ToString() );
			}
		}

		/// <summary>
		/// Gets a container of crash counts per user group for all crashes.
		/// </summary>
		/// <returns>A dictionary of user group names, and the count of crashes for each group.</returns>
		public Dictionary<string, int> GetCountsByGroup()
		{
			// @TODO yrx 2014-11-06 Optimize?
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + " SQL OPT" ) )
			{
				Dictionary<string, int> Results = new Dictionary<string, int>();

				try
				{
					var GroupCounts =
					(
						from UserDetail in Context.Users
						join UserGroupDetail in Context.UserGroups on UserDetail.UserGroupId equals UserGroupDetail.Id
						group UserDetail by UserGroupDetail.Name into GroupCount
						select new { Key = GroupCount.Key, Count = GroupCount.Count() }
					);

					foreach( var GroupCount in GroupCounts )
					{
						Results.Add( GroupCount.Key, GroupCount.Count );
					}

					// Add in all groups, even though there are no crashes associated
					IEnumerable<string> UserGroups = ( from UserGroupDetail in Context.UserGroups select UserGroupDetail.Name );
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

		/// <summary>Returns a list of all users, from the AnalyticsDB.</summary>
		public List<UsersMapping> GetAnalyticsUsers()
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString()  ) )
			{
				var AllUsers = Context.ExecuteQuery<UsersMapping>( @"SELECT * FROM [analyticsdb-01.dmz.epicgames.net].[CrashReport].[dbo].[UsersMapping]" );
				return AllUsers.ToList();
			}
		}
	}
}
