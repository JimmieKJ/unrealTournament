// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Linq.Expressions;
using System.Net;
using System.Text;
using System.Web.Mvc;
using System.Xml;
using Tools.CrashReporter.CrashReportWebSite.Models;
using Tools.CrashReporter.CrashReportWebSite.Properties;
using Tools.DotNETCommon;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	// Each time the controller is run, create a new CSV file in the specified folder and add the link and the top of the site,
	// so it could be easily downloaded
	// Removed files older that 7 days

	/*
	GameName
	TimeOfCrash
	BuiltFromCL
	PlatformName
	EngineMode
	MachineId
	Module
	BuildVersion
	Jira
	Branch
	CrashType
	EpicId
	BuggId
	*/


	/// <summary>CSV data row.</summary>
	public class FCSVRow
	{
		/// <summary>GameName.</summary>
		public string GameName;
		/// <summary>TimeOfCrash.</summary>
		public DateTime TimeOfCrash;
		/// <summary>BuiltFromCL.</summary>
		public string BuiltFromCL;
		/// <summary>PlatformName.</summary>
		public string PlatformName;
		/// <summary>EngineMode.</summary>
		public string EngineMode;
		/// <summary>MachineId.</summary>
		public string MachineId;
		/// <summary>Module.</summary>
		public string Module;
		/// <summary>BuildVersion.</summary>
		public string BuildVersion;
		/// <summary>Jira.</summary>
		public string Jira;
		/// <summary>Branch.</summary>
		public string Branch;
		/// <summary>CrashType.</summary>
		public short CrashType;
		/// <summary>EpicId.</summary>
		public string EpicId;
		/// <summary>BuggId.</summary>
		public int BuggId;

		/// <summary>Returns crash type as a string.</summary>
		public string GetCrashTypeAsString()
		{
			if (CrashType == 1)
			{
				return "Crash";
			}
			else if (CrashType == 2)
			{
				return "Assert";
			}
			else if (CrashType == 3)
			{
				return "Ensure";
			}
			return "Unknown";
		}
	}

	/// <summary>
	/// The controller to handle graphing of crashes per user group over time.
	/// </summary>
	public class CSVController : Controller
	{
		/// <summary>Fake id for all user groups</summary>
		public static readonly int AllUserGroupId = -1;

		//static readonly int MinNumberOfCrashes = 2;

		/// <summary>
		/// Retrieve all Buggs matching the search criteria.
		/// </summary>
		/// <param name="FormData">The incoming form of search criteria from the client.</param>
		/// <returns>A view to display the filtered Buggs.</returns>
		public CSV_ViewModel GetResults( FormHelper FormData )
		{
			BuggRepository BuggsRepo = new BuggRepository();
			CrashRepository CrashRepo = new CrashRepository();

			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				string AnonymousGroup = "Anonymous";
				//List<String> Users = FRepository.Get().GetUserNamesFromGroupName( AnonumousGroup );
				int AnonymousGroupID = FRepository.Get( BuggsRepo ).FindOrAddGroup( AnonymousGroup );
				HashSet<int> AnonumousIDs = FRepository.Get( BuggsRepo ).GetUserIdsFromUserGroup( AnonymousGroup );
				int AnonymousID = AnonumousIDs.First();
				HashSet<string> UserNamesForUserGroup = FRepository.Get( BuggsRepo ).GetUserNamesFromGroupName( AnonymousGroup );

				// Enable to narrow results and improve debugging performance.
				//FormData.DateFrom = FormData.DateTo.AddDays( -1 );
				FormData.DateTo = FormData.DateTo.AddDays( 1 );

				var FilteringQueryJoin = CrashRepo
					.ListAll()
					.Where( c => c.EpicAccountId != "" )
					// Only crashes and asserts
					.Where( c => c.CrashType == 1 || c.CrashType == 2 )
					// Only anonymous user
					.Where( c => c.UserNameId == AnonymousID )
					// Filter be date
					.Where( c => c.TimeOfCrash > FormData.DateFrom && c.TimeOfCrash < FormData.DateTo )
					.Join
					(
						CrashRepo.Context.Buggs_Crashes,
						c => c,
						bc => bc.Crash,
						( c, bc ) => new
						{
							GameName = c.GameName,
							TimeOfCrash = c.TimeOfCrash.Value,
							BuiltFromCL = c.BuiltFromCL,
							PlatformName = c.PlatformName,
							EngineMode = c.EngineMode,
							MachineId = c.MachineId,
							Module = c.Module,
							BuildVersion = c.BuildVersion,
							Jira = c.Jira,
							Branch = c.Branch,
							CrashType = c.CrashType.Value,
							EpicId = c.EpicAccountId,
							BuggId = bc.BuggId,
						}
					);

				var FilteringQueryCrashes = CrashRepo
					.ListAll()
					.Where( c => c.EpicAccountId != "" )
					// Only crashes and asserts
					.Where( c => c.CrashType == 1 || c.CrashType == 2 )
					// Only anonymous user
					.Where( c => c.UserNameId == AnonymousID );

				int TotalCrashes = CrashRepo
					.ListAll()
					.Count();

				int TotalCrashesYearToDate = CrashRepo
					.ListAll()
					// Year to date
					.Where( c => c.TimeOfCrash > new DateTime( DateTime.UtcNow.Year, 1, 1 ) )
					.Count();

				var CrashesFilteredWithDateQuery = FilteringQueryCrashes
					// Filter be date
					.Where( c => c.TimeOfCrash > FormData.DateFrom && c.TimeOfCrash < FormData.DateTo );

				int CrashesFilteredWithDate = CrashesFilteredWithDateQuery
					.Count();

				int CrashesYearToDateFiltered = FilteringQueryCrashes
					// Year to date
					.Where( c => c.TimeOfCrash > new DateTime( DateTime.UtcNow.Year, 1, 1 ) )
					.Count();

				int AffectedUsersFiltered = FilteringQueryCrashes
					.Select( c => c.EpicAccountId )
					.Distinct()
					.Count();

				int UniqueCrashesFiltered = FilteringQueryCrashes
					.Select( c => c.Pattern )
					.Distinct()
					.Count();


				int NumCrashes = FilteringQueryJoin.Count();

				// Get all users
				// SLOW
				//var Users = FRepository.Get( BuggsRepo ).GetAnalyticsUsers().ToDictionary( X => X.EpicAccountId, Y => Y );

				// Export data to the file.
				string CSVPathname = Path.Combine( Settings.Default.CrashReporterCSV, DateTime.UtcNow.ToString( "yyyy-MM-dd.HH-mm-ss" ) );
				CSVPathname += string
					.Format( "__[{0}---{1}]__{2}", 
					FormData.DateFrom.ToString( "yyyy-MM-dd" ), 
					FormData.DateTo.ToString( "yyyy-MM-dd" ), 
					NumCrashes ) 
					+ ".csv";

				string ServerPath = Server.MapPath( CSVPathname );
				var CSVFile = new StreamWriter( ServerPath, true, Encoding.UTF8 );

				using (FAutoScopedLogTimer ExportToCSVTimer = new FAutoScopedLogTimer( "ExportToCSV" ))
				{
					var RowType = FilteringQueryJoin.FirstOrDefault().GetType();

					string AllProperties = "";
					foreach( var Property in RowType.GetProperties() )
					{
						AllProperties += Property.Name;
						AllProperties += "; ";
					}

					// Write header
					CSVFile.WriteLine( AllProperties );

					foreach (var Row in FilteringQueryJoin)
					{
						var BVParts = Row.BuildVersion.Split( new char[] { '.' }, StringSplitOptions.RemoveEmptyEntries );
						if (BVParts.Length > 2 && BVParts[0] != "0")
						{
							string CleanEngineVersion = string.Format( "{0}.{1}.{2}", BVParts[0], BVParts[1], BVParts[2] );

							string[] RowProperties = new string[]
							{
								Row.GameName,
								Row.TimeOfCrash.ToString(),
								Row.BuiltFromCL,
								Row.PlatformName,
								Row.EngineMode,
								Row.MachineId,
								Row.Module,
								CleanEngineVersion,
								Row.Jira,
								Row.Branch,
								Row.CrashType == 1 ? "Crash" : "Assert",
								Row.EpicId,
								Row.BuggId.ToString()
							};

							string JoinedLine = string.Join( "; ", RowProperties );
							JoinedLine += "; ";

							CSVFile.WriteLine( JoinedLine );
						}
					}

					CSVFile.Flush();
					CSVFile.Close();
					CSVFile = null;
				}

				List<FCSVRow> CSVRows = FilteringQueryJoin
					.OrderByDescending( X => X.TimeOfCrash )
					.Take( 32 )
					.Select( c => new FCSVRow 
					{
						GameName = c.GameName,
						TimeOfCrash = c.TimeOfCrash,
						BuiltFromCL = c.BuiltFromCL,
						PlatformName = c.PlatformName,
						EngineMode = c.EngineMode,
						MachineId = c.MachineId,
						Module = c.Module,
						BuildVersion = c.BuildVersion,
						Jira = c.Jira,
						Branch = c.Branch,
						CrashType = c.CrashType,
						EpicId = c.EpicId,
						BuggId = c.BuggId,
					} )
					.ToList();
				
				return new CSV_ViewModel
				{
					CSVRows = CSVRows,
					CSVPathname = CSVPathname,
					DateFrom = (long)( FormData.DateFrom - CrashesViewModel.Epoch ).TotalMilliseconds,
					DateTo = (long)( FormData.DateTo - CrashesViewModel.Epoch ).TotalMilliseconds,
					DateTimeFrom = FormData.DateFrom,
					DateTimeTo = FormData.DateTo,
					AffectedUsersFiltered = AffectedUsersFiltered,
					UniqueCrashesFiltered = UniqueCrashesFiltered,
					CrashesFilteredWithDate = CrashesFilteredWithDate,
					TotalCrashes = TotalCrashes,
					TotalCrashesYearToDate = TotalCrashesYearToDate,
				};
			}
		}


		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public ActionResult Index( FormCollection Form )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				FormHelper FormData = new FormHelper( Request, Form, "JustReport" );
				CSV_ViewModel Results = GetResults( FormData );
				Results.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", Results );
			}
		}
	}
}