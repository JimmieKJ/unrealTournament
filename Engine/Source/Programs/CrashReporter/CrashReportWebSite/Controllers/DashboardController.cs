// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// 
	/// </summary>
	public struct FCrashMinimal
	{
		/// <summary>
		/// 
		/// </summary>
		public readonly DateTime TimeOfCrash;

		/// <summary>
		///	
		/// </summary>
		public readonly int UserId;

		/// <summary>
		/// 
		/// </summary>
		public readonly string EngineVersion;

		/// <summary>
		/// 
		/// </summary>
		/// <param name="InTimeOfCrash"></param>
		/// <param name="InUserId"></param>
		/// <param name="InEngineVersion"></param>
		public FCrashMinimal( DateTime InTimeOfCrash, int InUserId, string InEngineVersion )
		{
			TimeOfCrash = InTimeOfCrash;
			UserId = InUserId;
			EngineVersion = InEngineVersion;
		}
	}

	/// <summary>
	/// The controller to handle graphing of crashes per user group over time.
	/// </summary>
	public class DashboardController : Controller
	{
		/// <summary>Fake id for all user groups</summary>
		public static readonly int AllUserGroupId = -1;

        //
        CrashReportEntities _entities = new CrashReportEntities();
	    private CrashRepository _Crashes;
	    private BuggRepository _Buggs;

	    public DashboardController()
	    {
	        //Hideous temporary code
            //no! Unity! Dependency injection! Single Context wrapper!
            _Crashes = new CrashRepository(_entities);
            _Buggs = new BuggRepository(_entities);
	    }

		/// <summary>
		/// Return a dictionary of crashes per group grouped by week.
		/// </summary>
		/// <param name="Crashes">A set of crashes to interrogate.</param>
		/// <param name="UserGroupId">The id of the user group to interrogate.</param>
		/// <returns>A dictionary of week vs. crash count.</returns>
		public Dictionary<DateTime, int> GetWeeklyCountsByGroup( List<FCrashMinimal> Crashes, int UserGroupId )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(UserGroupId=" + UserGroupId + ")" ) )
			{
				Dictionary<DateTime, int> Results = new Dictionary<DateTime, int>();

			    var UsersIds = new HashSet<int>(_entities.Users.Distinct().Select(data => data.Id));

				// Trim crashes to user group.
				if( UserGroupId != DashboardController.AllUserGroupId )
				{
					Crashes = Crashes.Where( Crash => UsersIds.Contains( Crash.UserId ) ).ToList();
				}

				try
				{
					Results =
					(
						from CrashDetail in Crashes
						group CrashDetail by CrashDetail.TimeOfCrash.AddDays( -(int)CrashDetail.TimeOfCrash.DayOfWeek ).Date into GroupCount
						orderby GroupCount.Key
						select new { Count = GroupCount.Count(), Date = GroupCount.Key }
					).ToDictionary( x => x.Date, y => y.Count );
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetWeeklyCountsByGroup: " + Ex.ToString() );
				}

				return Results;
			}
		}

		/// <summary>
		/// Return a dictionary of crashes per version grouped by week.
		/// </summary>
		/// <param name="Crashes">A set of crashes to interrogate.</param>
		/// <param name="EngineVersion">Engine version</param>
		/// <param name="AnonymousID">Anonymous id</param>
		/// <returns>A dictionary of week vs. crash count.</returns>
		public Dictionary<DateTime, int> GetWeeklyCountsByVersion( List<FCrashMinimal> Crashes, string EngineVersion, int AnonymousID )
		{
			using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(EngineVersion=" + EngineVersion + ")" ))
			{
				Dictionary<DateTime, int> Results = new Dictionary<DateTime, int>();

				Crashes = Crashes
					.Where( Crash => Crash.UserId == AnonymousID )
					.Where( Crash => Crash.EngineVersion.Contains( EngineVersion ) )
					.ToList();

				try
				{
					Results =
					(
						from CrashDetail in Crashes
						group CrashDetail by CrashDetail.TimeOfCrash.AddDays( -(int)CrashDetail.TimeOfCrash.DayOfWeek ).Date into VersionCount
						orderby VersionCount.Key
						select new { Count = VersionCount.Count(), Date = VersionCount.Key }
					).ToDictionary( x => x.Date, y => y.Count );
				}
				catch (Exception Ex)
				{
					Debug.WriteLine( "Exception in GeWeeklyCountsByVersion: " + Ex.ToString() );
				}

				return Results;
			}
		}

		/// <summary>
		/// Return a dictionary of crashes per group grouped by day.
		/// </summary>
		/// <param name="Crashes">A set of crashes to interrogate.</param>
		/// <param name="UserGroupId">The id of the user group to interrogate.</param>
		/// <returns>A dictionary of day vs. crash count.</returns>
		public Dictionary<DateTime, int> GetDailyCountsByGroup( List<FCrashMinimal> Crashes, int UserGroupId )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(UserGroupId=" + UserGroupId + ")" ) )
			{
				Dictionary<DateTime, int> Results = new Dictionary<DateTime, int>();

				var UsersIds = _entities.UserGroups.First(data => data.Id == UserGroupId ).Users.Select(data => data.Id);

				// Trim crashes to user group.
				if( UserGroupId != DashboardController.AllUserGroupId )
				{
					Crashes = Crashes.Where( Crash => UsersIds.Contains( Crash.UserId ) ).ToList();
				}

				try
				{
					Results =
					(
						from CrashDetail in Crashes
						group CrashDetail by CrashDetail.TimeOfCrash.Date into GroupCount
						orderby GroupCount.Key
						select new { Count = GroupCount.Count(), Date = GroupCount.Key }
					).ToDictionary( x => x.Date, y => y.Count );
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetDailyCountsByGroup: " + Ex.ToString() );
				}

				return Results;
			}
		}

		/// <summary>
		/// Return a dictionary of crashes per version grouped by day.
		/// </summary>
		/// <param name="Crashes">A set of crashes to interrogate.</param>
		/// <param name="EngineVersion">Engine version</param>
		/// <param name="AnonymousID">Anonymous id</param>
		/// <returns>A dictionary of day vs. crash count.</returns>
		public Dictionary<DateTime, int> GetDailyCountsByVersion( List<FCrashMinimal> Crashes, string EngineVersion, int AnonymousID )
		{
			using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(EngineVersion=" + EngineVersion + ")" ))
			{
				Dictionary<DateTime, int> Results = new Dictionary<DateTime, int>();

				Crashes = Crashes
					.Where( Crash => Crash.UserId == AnonymousID )
					.Where( Crash => Crash.EngineVersion.Contains( EngineVersion ) )
					.ToList();
				
				try
				{
					Results =
					(
						from CrashDetail in Crashes
						group CrashDetail by CrashDetail.TimeOfCrash.Date into VersionCount
						orderby VersionCount.Key
						select new { Count = VersionCount.Count(), Date = VersionCount.Key }
					).ToDictionary( x => x.Date, y => y.Count );
				}
				catch (Exception Ex)
				{
					Debug.WriteLine( "Exception in GetDailyCountsByVersion: " + Ex.ToString() );
				}

				return Results;
			}
		}

		/// <summary>
		/// The main view of the dash board.
		/// </summary>
		/// <returns>A view showing two charts of crashes over time.</returns>
		public ActionResult Index()
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				DateTime Today = DateTime.UtcNow;
				DateTime AfewMonthsAgo = Today.AddMonths( -6 );

				FAutoScopedLogTimer LogTimerSQL = new FAutoScopedLogTimer( "CrashesFilterByDate", "", "" );

				// Get engine versions from the last 6 months.
				var TempVersions = _Crashes.GetVersions();
				List<string> EngineUE4Versions = new List<string>();
				foreach (var Version in TempVersions)
				{
					if( Version.StartsWith("4.") )
					{
						EngineUE4Versions.Add( Version );
					}
				}

				// Only 4 latest version.
				EngineUE4Versions = EngineUE4Versions.OrderByDescending( item => item ).Take( 5 ).ToList();

			    var endDate = Today.AddDays(1);

				IQueryable<Crash> CrashesInTimeFrame = _Crashes.ListAll()
                    .Where(MyCrash => MyCrash.TimeOfCrash >= AfewMonthsAgo && MyCrash.TimeOfCrash <= endDate);
				//IEnumerable<Crash> Crashes = FRepository.Get().Crashes.FilterByDate( FRepository.Get().Crashes.ListAll(), AfewMonthsAgo, Today );
				var VMinimalCrashes = CrashesInTimeFrame.Select( Crash => new 
				{ 
					TimeOfCrash = Crash.TimeOfCrash.Value, 
					UserID = Crash.UserNameId.Value,
					EngineVersion = Crash.BuildVersion,
				} )
				.ToList();

				List<FCrashMinimal> MinimalCrashes = new List<FCrashMinimal>( VMinimalCrashes.Count );
				foreach( var Anotype in VMinimalCrashes )
				{
					MinimalCrashes.Add( new FCrashMinimal( Anotype.TimeOfCrash, Anotype.UserID, Anotype.EngineVersion ) );
				}
				LogTimerSQL.Dispose();

			    HashSet<int> AnonumousIDs = new HashSet<int>(_entities.UserGroups.First(data => data.Name == "Anonymous").Users.Select(data => data.Id));
				int AnonymousID = AnonumousIDs.First();

			    int GeneralUserGroupId = 1;//FRepository.Get( _Crashes ).FindOrAddGroup( "General" );
			    int CoderUserGroupId = 3;//FRepository.Get( _Crashes ).FindOrAddGroup( "Coder" );
			    int EngineQAUserGroupId = 22;//FRepository.Get( _Crashes ).FindOrAddGroup( "EngineQA" );
			    int GameQAUserGroupId = 21;//FRepository.Get( _Crashes ).FindOrAddGroup( "GameQA" );

				// Weekly
				Dictionary<DateTime, int> WeeklyGeneralResults = GetWeeklyCountsByGroup( MinimalCrashes, GeneralUserGroupId );
				Dictionary<DateTime, int> WeeklyCoderResults = GetWeeklyCountsByGroup( MinimalCrashes, CoderUserGroupId );
				Dictionary<DateTime, int> WeeklyEngineQAResults = GetWeeklyCountsByGroup( MinimalCrashes, EngineQAUserGroupId );
				Dictionary<DateTime, int> WeeklyGameQAResults = GetWeeklyCountsByGroup( MinimalCrashes, GameQAUserGroupId );

				Dictionary<string, Dictionary<DateTime, int>> WeeklyEngineVersionResults = new Dictionary<string, Dictionary<DateTime, int>>();
				foreach (string UE4Version in EngineUE4Versions)
				{
					Dictionary<DateTime, int> Results = GetWeeklyCountsByVersion( MinimalCrashes, UE4Version, AnonymousID );
					WeeklyEngineVersionResults.Add( UE4Version, Results );
				}

				Dictionary<DateTime, int> WeeklyAllResults = GetWeeklyCountsByGroup( MinimalCrashes, AllUserGroupId );
				
				// Daily
				Dictionary<DateTime, int> DailyGeneralResults = GetDailyCountsByGroup( MinimalCrashes, GeneralUserGroupId );
				Dictionary<DateTime, int> DailyCoderResults = GetDailyCountsByGroup( MinimalCrashes, CoderUserGroupId );
				Dictionary<DateTime, int> DailyEngineQAResults = GetDailyCountsByGroup( MinimalCrashes, EngineQAUserGroupId );
				Dictionary<DateTime, int> DailyGameQAResults = GetDailyCountsByGroup( MinimalCrashes, GameQAUserGroupId );


				Dictionary<string, Dictionary<DateTime, int>> DailyEngineVersionResults = new Dictionary<string, Dictionary<DateTime, int>>();
				foreach(string UE4Version in EngineUE4Versions)
				{
					Dictionary<DateTime, int> Results = GetDailyCountsByVersion( MinimalCrashes, UE4Version, AnonymousID );
					DailyEngineVersionResults.Add( UE4Version, Results );
				}

				Dictionary<DateTime, int> DailyAllResults = GetDailyCountsByGroup( MinimalCrashes, AllUserGroupId );

				// Get daily buggs stats.
				List<Bugg> Buggs = _Buggs.ListAll().Where( Bugg => Bugg.TimeOfFirstCrash >= AfewMonthsAgo ).ToList();

				Dictionary<DateTime, int> BuggDailyAllResults  =
				(
					from Bugg in Buggs
					group Bugg by Bugg.TimeOfFirstCrash.Value.Date into GroupCount
					orderby GroupCount.Key
					select new { Count = GroupCount.Count(), Date = GroupCount.Key }
				).ToDictionary( x => x.Date, y => y.Count );
				

				string CrashesByWeek = "";

				foreach( KeyValuePair<DateTime, int> WeeklyResult in WeeklyAllResults )
				{
					int GeneralCrashes = 0;
					WeeklyGeneralResults.TryGetValue( WeeklyResult.Key, out GeneralCrashes );

					int CoderCrashes = 0;
					WeeklyCoderResults.TryGetValue( WeeklyResult.Key, out CoderCrashes );

					int EngineQACrashes = 0;
					WeeklyEngineQAResults.TryGetValue( WeeklyResult.Key, out EngineQACrashes );

					int GameQACrashes = 0;
					WeeklyGameQAResults.TryGetValue( WeeklyResult.Key, out GameQACrashes );

					string AnonymousLine = "";
					foreach (var VersionCrashes in WeeklyEngineVersionResults)
					{
						int EngineVersionCrashes = 0;
						VersionCrashes.Value.TryGetValue( WeeklyResult.Key, out EngineVersionCrashes );

						AnonymousLine += EngineVersionCrashes;
						AnonymousLine += ", ";
					}

					int Year = WeeklyResult.Key.Year;
					int Month = WeeklyResult.Key.AddMonths( -1 ).Month;
					if( WeeklyResult.Key.Month == 13 || WeeklyResult.Key.Month == 1 )
					{
						Month = 0;
					}

					int Day = WeeklyResult.Key.Day + 6;

					string Line = "[new Date(" + Year + ", " + Month + ", " + Day + "), " + GeneralCrashes + ", " + CoderCrashes + ", " + EngineQACrashes + ", " + GameQACrashes + ", " + AnonymousLine + WeeklyResult.Value + "], ";
					CrashesByWeek += Line;
				}

				CrashesByWeek = CrashesByWeek.TrimEnd( ", ".ToCharArray() );

				string CrashesByDay = "";
				foreach( KeyValuePair<DateTime, int> DailyResult in DailyAllResults )
				{
					int GeneralCrashes = 0;
					DailyGeneralResults.TryGetValue( DailyResult.Key, out GeneralCrashes );

					int CoderCrashes = 0;
					DailyCoderResults.TryGetValue( DailyResult.Key, out CoderCrashes );

					int EngineQACrashes = 0;
					DailyEngineQAResults.TryGetValue( DailyResult.Key, out EngineQACrashes );

					int GameQACrashes = 0;
					DailyGameQAResults.TryGetValue( DailyResult.Key, out GameQACrashes );

					string AnonymousLine = "";
					foreach (var VersionCrashes in DailyEngineVersionResults)
					{
						int EngineVersionCrashes = 0;
						VersionCrashes.Value.TryGetValue( DailyResult.Key, out EngineVersionCrashes );

						AnonymousLine += EngineVersionCrashes;
						AnonymousLine +=", ";
					}

					int Year = DailyResult.Key.Year;
					int Month = DailyResult.Key.AddMonths( -1 ).Month;
					if( DailyResult.Key.Month == 13 || DailyResult.Key.Month == 1 )
					{
						Month = 0;
					}

					int Day = DailyResult.Key.Day;

					string Line = "[new Date(" + Year + ", " + Month + ", " + Day + "), " + GeneralCrashes + ", " + CoderCrashes + ", " + EngineQACrashes + ", " + GameQACrashes + ", " + AnonymousLine + DailyResult.Value + "], ";
					CrashesByDay += Line;
				}

				CrashesByDay = CrashesByDay.TrimEnd( ", ".ToCharArray() );

				string BuggsByDay = "";
				foreach( KeyValuePair<DateTime, int> DailyResult in BuggDailyAllResults )
				{
					int Year = DailyResult.Key.Year;
					int Month = DailyResult.Key.AddMonths( -1 ).Month;
					if( DailyResult.Key.Month == 13 || DailyResult.Key.Month == 1 )
					{
						Month = 0;
					}

					int Day = DailyResult.Key.Day;

					string Line = "[new Date(" + Year + ", " + Month + ", " + Day + "), " + DailyResult.Value + "], ";
					BuggsByDay += Line;
				}

				BuggsByDay = BuggsByDay.TrimEnd( ", ".ToCharArray() );

				var ResultDashboard = new DashboardViewModel
				{
					CrashesByWeek = CrashesByWeek,
					CrashesByDay = CrashesByDay,
					BuggsByDay = BuggsByDay,
					EngineVersions = EngineUE4Versions,
				};
				ResultDashboard.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", ResultDashboard );
			}
		}
	}
}