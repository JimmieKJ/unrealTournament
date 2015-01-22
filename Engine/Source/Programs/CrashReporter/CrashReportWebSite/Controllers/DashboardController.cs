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
	/// The controller to handle graphing of crashes per user group over time.
	/// </summary>
	public class DashboardController : Controller
	{
		/// <summary>
		/// The main view of the dash board.
		/// </summary>
		/// <returns>A view showing two charts of crashes over time.</returns>
		public ActionResult Index()
		{
			CrashRepository LocalCrashRepository = new CrashRepository();
			IQueryable<Crash> Crashes = LocalCrashRepository.ListAll();

			int UndefinedUserGroupId = LocalCrashRepository.FindOrAddUserGroup( "Undefined" );
			int GeneralUserGroupId = LocalCrashRepository.FindOrAddUserGroup( "General" );
			int CoderUserGroupId = LocalCrashRepository.FindOrAddUserGroup( "Coder" );
			int EngineQAUserGroupId = LocalCrashRepository.FindOrAddUserGroup( "EngineQA" );
			int GameQAUserGroupId = LocalCrashRepository.FindOrAddUserGroup( "GameQA" );
			int AutomatedUserGroupId = LocalCrashRepository.FindOrAddUserGroup( "Automated" );

			Dictionary<DateTime, int> GeneralResults = LocalCrashRepository.GetWeeklyCountsByGroup( Crashes, GeneralUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> CoderResults = LocalCrashRepository.GetWeeklyCountsByGroup( Crashes, CoderUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> EngineQAResults = LocalCrashRepository.GetWeeklyCountsByGroup( Crashes, EngineQAUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> GameQAResults = LocalCrashRepository.GetWeeklyCountsByGroup( Crashes, GameQAUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> AutomatedResults = LocalCrashRepository.GetWeeklyCountsByGroup( Crashes, AutomatedUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> AllResults = LocalCrashRepository.GetWeeklyCountsByGroup( Crashes, -1, UndefinedUserGroupId );

			Dictionary<DateTime, int> DailyGeneralResults = LocalCrashRepository.GetDailyCountsByGroup( Crashes, GeneralUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> DailyCoderResults = LocalCrashRepository.GetDailyCountsByGroup( Crashes, CoderUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> DailyEngineQAResults = LocalCrashRepository.GetDailyCountsByGroup( Crashes, EngineQAUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> DailyGameQAResults = LocalCrashRepository.GetDailyCountsByGroup( Crashes, GameQAUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> DailyAutomatedResults = LocalCrashRepository.GetDailyCountsByGroup( Crashes, AutomatedUserGroupId, UndefinedUserGroupId );
			Dictionary<DateTime, int> DailyAllResults = LocalCrashRepository.GetDailyCountsByGroup( Crashes, -1, UndefinedUserGroupId );

			string CrashesByWeek = "";

			foreach( KeyValuePair<DateTime, int> Result in AllResults )
			{
				int GeneralCrashes = 0;
				try
				{
					GeneralCrashes = GeneralResults[Result.Key];
				}
				catch
				{
				}

				int CoderCrashes = 0;
				try
				{
					CoderCrashes = CoderResults[Result.Key];
				}
				catch
				{
				}

				int EngineQACrashes = 0;
				try
				{
					EngineQACrashes = EngineQAResults[Result.Key];
				}
				catch
				{
				}

				int GameQACrashes = 0;
				try
				{
					GameQACrashes = GameQAResults[Result.Key];
				}
				catch
				{
				}

				int AutomatedCrashes = 0;
				try
				{
					AutomatedCrashes = AutomatedResults[Result.Key];
				}
				catch
				{
				}

				int Year = Result.Key.Year;
				int Month = Result.Key.AddMonths( -1 ).Month;
				if( Result.Key.Month == 13 || Result.Key.Month == 1 )
				{
					Month = 0;
				}

				int Day = Result.Key.Day + 6;

				string Line = "[new Date(" + Year + ", " + Month + ", " + Day + "), " + GeneralCrashes + ", " + CoderCrashes + ", " + EngineQACrashes + ", " + GameQACrashes + ", " + AutomatedCrashes + ", " + Result.Value + "], ";
				CrashesByWeek = CrashesByWeek + Line;
			}

			CrashesByWeek = CrashesByWeek.TrimEnd( ", ".ToCharArray() );

			string CrashesByDay = "";
			foreach( KeyValuePair<DateTime, int> DailyResult in DailyAllResults )
			{
				int GeneralCrashes = 0;
				try
				{
					GeneralCrashes = DailyGeneralResults[DailyResult.Key];
				}
				catch
				{
				}

				int CoderCrashes = 0;
				try
				{
					CoderCrashes = DailyCoderResults[DailyResult.Key];
				}
				catch
				{
				}

				int EngineQACrashes = 0;
				try
				{
					EngineQACrashes = DailyEngineQAResults[DailyResult.Key];
				}
				catch
				{
				}

				int GameQACrashes = 0;
				try
				{
					GameQACrashes = DailyGameQAResults[DailyResult.Key];
				}
				catch
				{
				}

				int AutomatedCrashes = 0;
				try
				{
					AutomatedCrashes = DailyAutomatedResults[DailyResult.Key];
				}
				catch
				{
				}

				int Year = DailyResult.Key.Year;
				int Month = DailyResult.Key.AddMonths( -1 ).Month;
				if( DailyResult.Key.Month == 13 || DailyResult.Key.Month == 1 )
				{
					Month = 0;
				}

				int Day = DailyResult.Key.Day;

				string Line = "[new Date(" + Year + ", " + Month + ", " + Day + "), " + GeneralCrashes + ", " + CoderCrashes + ", " + EngineQACrashes + ", " + GameQACrashes + ", " + AutomatedCrashes + ", " + DailyResult.Value + "], ";
				CrashesByDay = CrashesByDay + Line;
			}

			CrashesByDay = CrashesByDay.TrimEnd( ", ".ToCharArray() );

			return View( "Index", new DashboardViewModel { CrashesByWeek = CrashesByWeek, CrashesByDay = CrashesByDay } );
		}
	}
}