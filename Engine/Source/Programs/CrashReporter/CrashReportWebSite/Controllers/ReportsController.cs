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
using Tools.DotNETCommon;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{

	//How hard would it be for you to create a CSV of:
	// Epic ID, 
	// buggs ID, 
	// engine version, 
	// 
	// # of crashes with that buggs ID for 
	//	this engine version and 
	//	user

	/// <summary>
	/// The controller to handle graphing of crashes per user group over time.
	/// </summary>
	public class ReportsController : Controller
	{
		/// <summary>Fake id for all user groups</summary>
		public static readonly int AllUserGroupId = -1;

		/// <summary>
		/// Retrieve all Buggs matching the search criteria.
		/// </summary>
		/// <param name="FormData">The incoming form of search criteria from the client.</param>
		/// <param name="BuggIDToBeAddedToJira">ID of the bugg that will be added to JIRA</param>
		/// <returns>A view to display the filtered Buggs.</returns>
		public ReportsViewModel GetResults( FormHelper FormData, int BuggIDToBeAddedToJira )
		{
			BuggRepository BuggsRepo = new BuggRepository();
			CrashRepository CrashRepo = new CrashRepository();

			// @TODO yrx 2015-02-17 BuggIDToBeAddedToJira replace with List<int> based on check box and Submit?
			// It would be great to have a CSV export of this as well with buggs ID being the key I can then use to join them :)
			// 
			// Enumerate JIRA projects if needed.
			// https://jira.ol.epicgames.net//rest/api/2/project
			var JC = JiraConnection.Get();
			var JiraComponents = JC.GetNameToComponents();
			var JiraVersions = JC.GetNameToVersions();

			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				string AnonumousGroup = "Anonymous";
				//List<String> Users = FRepository.Get().GetUserNamesFromGroupName( AnonumousGroup );
				int AnonymousGroupID = FRepository.Get( BuggsRepo ).FindOrAddGroup( AnonumousGroup );
				HashSet<int> AnonumousIDs = FRepository.Get( BuggsRepo ).GetUserIdsFromUserGroup( AnonumousGroup );
				int AnonymousID = AnonumousIDs.First();

				var Crashes = CrashRepo
					.FilterByDate( CrashRepo.ListAll(), FormData.DateFrom, FormData.DateTo )
					// Only crashes and asserts
					.Where( Crash => Crash.CrashType == 1 || Crash.CrashType == 2 )
					// Only anonymous user
					.Where( Crash => Crash.UserNameId.Value == AnonymousID )
					.Select( Crash => new
					{
						ID = Crash.Id,
						TimeOfCrash = Crash.TimeOfCrash.Value,
						//UserID = Crash.UserNameId.Value, 
						BuildVersion = Crash.BuildVersion,
						JIRA = Crash.Jira,
						Platform = Crash.PlatformName,
						FixCL = Crash.FixedChangeList,
						BuiltFromCL = Crash.BuiltFromCL,
						Pattern = Crash.Pattern,
						MachineID = Crash.MachineId,
						Branch = Crash.Branch,
						Description = Crash.Description,
						RawCallStack = Crash.RawCallStack,
					} )
					.ToList();
				int NumCrashes = Crashes.Count;

				/*
				// Build patterns for crashes where patters is null.
				var CrashesWithoutPattern = FRepository.Get().Crashes
					.FilterByDate( FRepository.Get().Crashes.ListAll(), FormData.DateFrom, FormData.DateTo.AddDays( 1 ) )
					// Only crashes and asserts
					.Where( Crash => Crash.Pattern == null || Crash.Pattern == "" )
					.Select( Crash => Crash )
					.ToList();

				foreach( var Crash in CrashesWithoutPattern )
				{
					Crash.BuildPattern();
				}
				*/

				// Total # of ALL (Anonymous) crashes in timeframe
				int TotalAnonymousCrashes = NumCrashes;

				// Total # of UNIQUE (Anonymous) crashes in timeframe
				HashSet<string> UniquePatterns = new HashSet<string>();
				HashSet<string> UniqueMachines = new HashSet<string>();
				Dictionary<string, int> PatternToCount = new Dictionary<string, int>();

				//List<int> CrashesWithoutPattern = new List<int>();
				//List<DateTime> CrashesWithoutPatternDT = new List<DateTime>();

				foreach( var Crash in Crashes )
				{
					if( string.IsNullOrEmpty( Crash.Pattern ) )
					{
						//CrashesWithoutPattern.Add( Crash.ID );
						//CrashesWithoutPatternDT.Add( Crash.TimeOfCrash );
						continue;
					}

					UniquePatterns.Add( Crash.Pattern );
					UniqueMachines.Add( Crash.MachineID );

					bool bAdd = !PatternToCount.ContainsKey( Crash.Pattern );
					if( bAdd )
					{
						PatternToCount.Add( Crash.Pattern, 1 );
					}
					else
					{
						PatternToCount[Crash.Pattern]++;
					}
				}
				var PatternToCountOrdered = PatternToCount.OrderByDescending( X => X.Value ).ToList();
				const int NumTopRecords = 200;
				var PatternAndCount = PatternToCountOrdered.Take( NumTopRecords ).ToDictionary( X => X.Key, Y => Y.Value );

				int TotalUniqueAnonymousCrashes = UniquePatterns.Count;

				// Total # of AFFECTED USERS (Anonymous) in timeframe
				int TotalAffectedUsers = UniqueMachines.Count;

				var RealBuggs = BuggsRepo.Context.Buggs.Where( Bugg => PatternAndCount.Keys.Contains( Bugg.Pattern ) ).ToList();

				// Build search string.
				HashSet<string> FoundJiras = new HashSet<string>();
				Dictionary<string, List<Bugg>> JiraIDtoBugg = new Dictionary<string, List<Bugg>>();

				List<Bugg> Buggs = new List<Bugg>( NumTopRecords );
				foreach( var Top in PatternAndCount )
				{
					Bugg NewBugg = RealBuggs.Where( X => X.Pattern == Top.Key ).FirstOrDefault();
					if( NewBugg != null )
					{
						using( FAutoScopedLogTimer TopTimer = new FAutoScopedLogTimer( string.Format( "{0}:{1}", Buggs.Count + 1, NewBugg.Id ) ) )
						{
							var CrashesForBugg = Crashes.Where( Crash => Crash.Pattern == Top.Key ).ToList();

							// Convert anonymous to full type;
							var FullCrashesForBugg = new List<Crash>( CrashesForBugg.Count );
							foreach( var Anon in CrashesForBugg )
							{
								FullCrashesForBugg.Add( new Crash()
								{
									ID = Anon.ID,
									TimeOfCrash = Anon.TimeOfCrash,
									BuildVersion = Anon.BuildVersion,
									Jira = Anon.JIRA,
									Platform = Anon.Platform,
									FixCL = Anon.FixCL,
									BuiltFromCL = Anon.BuiltFromCL,
									Pattern = Anon.Pattern,
									MachineId = Anon.MachineID,
									Branch = Anon.Branch,
									Description = Anon.Description,
									RawCallStack = Anon.RawCallStack,
								} );
							}

							NewBugg.PrepareBuggForJira( FullCrashesForBugg );

							// Verify valid JiraID, this may be still a TTP 
							if( !string.IsNullOrEmpty( NewBugg.Jira ) )
							{
								int TTPID = 0;
								int.TryParse( NewBugg.Jira, out TTPID );

								if( TTPID == 0 )
								{
									AddBuggJiraMapping(NewBugg, ref FoundJiras, ref JiraIDtoBugg);						
								}
							}

							Buggs.Add( NewBugg );
						}
					}
					else
					{
						FLogger.Global.WriteEvent( "Bugg for pattern " + Top.Key + " not found" );
					}
				}

				if( BuggIDToBeAddedToJira > 0 )
				{
					var Bugg = Buggs.Where( X => X.Id == BuggIDToBeAddedToJira ).FirstOrDefault();
					if( Bugg != null )
					{
						Bugg.CopyToJira();
						AddBuggJiraMapping( Bugg, ref FoundJiras, ref JiraIDtoBugg );
					}
				}

				if( JC.CanBeUsed() )
				{
					var BuggsCopy = new List<Bugg>( Buggs );

					HashSet<string> InvalidJiras = new HashSet<string>();

					// Simple verification of JIRA
					foreach (var Value in FoundJiras)
					{
						if( Value.Length < 3 || !Value.Contains('-') )
						{
							InvalidJiras.Add(Value);
						}
					}

					foreach (var InvalidJira in InvalidJiras)
					{
						FoundJiras.Remove( InvalidJira );
					}

					// Grab the data form JIRA.
					string JiraSearchQuery = string.Join( " OR ", FoundJiras );

					using( FAutoScopedLogTimer JiraResultsTimer = new FAutoScopedLogTimer( "JiraResults" ) )
					{
						bool bInvalid = false;
						var JiraResults = new Dictionary<string, Dictionary<string, object>>();
						try
						{
							JiraResults = JC.SearchJiraTickets(
							JiraSearchQuery,
							new string[] 
							{ 
								"key",				// string
								"summary",			// string
								"components",		// System.Collections.ArrayList, Dictionary<string,object>, name
								"resolution",		// System.Collections.Generic.Dictionary`2[System.String,System.Object], name
								"fixVersions",		// System.Collections.ArrayList, Dictionary<string,object>, name
								"customfield_11200" // string
							} );
						}
						catch (System.Exception)
						{
							bInvalid = true;
						}

						// Invalid records have been found, find the broken using the slow path.
						if( bInvalid )
						{
							foreach (var Query in FoundJiras)
							{
								try
								{
									var TempResult = JC.SearchJiraTickets(
									Query,
									new string[] 
									{ 
										"key",				// string
										"summary",			// string
										"components",		// System.Collections.ArrayList, Dictionary<string,object>, name
										"resolution",		// System.Collections.Generic.Dictionary`2[System.String,System.Object], name
										"fixVersions",		// System.Collections.ArrayList, Dictionary<string,object>, name
										"customfield_11200" // string
									} );

									foreach(var Temp in TempResult)
									{
										JiraResults.Add( Temp.Key, Temp.Value );
									}
								}
								catch (System.Exception)
								{

								}
							}
						}

						// Jira Key, Summary, Components, Resolution, Fix version, Fix changelist
						foreach( var Jira in JiraResults )
						{
							string JiraID = Jira.Key;

							string Summary = (string)Jira.Value["summary"];

							string ComponentsText = "";
							System.Collections.ArrayList Components = (System.Collections.ArrayList)Jira.Value["components"];
							foreach( Dictionary<string, object> Component in Components )
							{
								ComponentsText += (string)Component["name"];
								ComponentsText += " ";
							}

							Dictionary<string, object> ResolutionFields = (Dictionary<string, object>)Jira.Value["resolution"];
							string Resolution = ResolutionFields != null ? (string)ResolutionFields["name"] : "";

							string FixVersionsText = "";
							System.Collections.ArrayList FixVersions = (System.Collections.ArrayList)Jira.Value["fixVersions"];
							foreach( Dictionary<string, object> FixVersion in FixVersions )
							{
								FixVersionsText += (string)FixVersion["name"];
								FixVersionsText += " ";
							}

							int FixCL = Jira.Value["customfield_11200"] != null ? (int)(decimal)Jira.Value["customfield_11200"] : 0;

							List<Bugg> BuggsForJira;
							JiraIDtoBugg.TryGetValue( JiraID, out BuggsForJira );

							//var BuggsForJira = JiraIDtoBugg[JiraID];

							if( BuggsForJira != null )
							{
								foreach( Bugg Bugg in BuggsForJira )
								{
									Bugg.JiraSummary = Summary;
									Bugg.JiraComponentsText = ComponentsText;
									Bugg.JiraResolution = Resolution;
									Bugg.JiraFixVersionsText = FixVersionsText;
									if( FixCL != 0 )
									{
										Bugg.JiraFixCL = FixCL.ToString();
									}

									BuggsCopy.Remove( Bugg );
								}
							}
						}
					}

					// If there are buggs, we need to update the summary to indicate an error.
					// Usually caused when bugg's project has changed.
					foreach( var Bugg in BuggsCopy.Where( b => !string.IsNullOrEmpty( b.Jira ) ) )
					{
						Bugg.JiraSummary = "JIRA MISMATCH";
						Bugg.JiraComponentsText = "JIRA MISMATCH";
						Bugg.JiraResolution = "JIRA MISMATCH";
						Bugg.JiraFixVersionsText = "JIRA MISMATCH";
						Bugg.JiraFixCL = "JIRA MISMATCH";
					}
				}

				Buggs = Buggs.OrderByDescending( b => b.CrashesInTimeFrameGroup ).ToList();

				return new ReportsViewModel
				{
					Buggs = Buggs,
					/*Crashes = Crashes,*/
					DateFrom = (long)( FormData.DateFrom - CrashesViewModel.Epoch ).TotalMilliseconds,
					DateTo = (long)( FormData.DateTo - CrashesViewModel.Epoch ).TotalMilliseconds,
					TotalAffectedUsers = TotalAffectedUsers,
					TotalAnonymousCrashes = TotalAnonymousCrashes,
					TotalUniqueAnonymousCrashes = TotalUniqueAnonymousCrashes
				};
			}
		}

		private void AddBuggJiraMapping( Bugg NewBugg, ref HashSet<string> FoundJiras, ref Dictionary<string, List<Bugg>> JiraIDtoBugg )
		{
			string JiraID = NewBugg.Jira;
			FoundJiras.Add( "key = " + JiraID );

			bool bAdd = !JiraIDtoBugg.ContainsKey( JiraID );
			if( bAdd )
			{
				JiraIDtoBugg.Add( JiraID, new List<Bugg>() );
			}

			JiraIDtoBugg[JiraID].Add( NewBugg );
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ReportsForm"></param>
		/// <returns></returns>
		public ActionResult Index( FormCollection ReportsForm )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				FormHelper FormData = new FormHelper( Request, ReportsForm, "JustReport" );

				// Handle 'CopyToJira' button
				int BuggIDToBeAddedToJira = 0;
				foreach( var Entry in ReportsForm )
				{
					if( Entry.ToString().Contains( Bugg.JiraSubmitName ) )
					{
						int.TryParse( Entry.ToString().Substring( Bugg.JiraSubmitName.Length ), out BuggIDToBeAddedToJira );
						break;
					}
				}

				ReportsViewModel Results = GetResults( FormData, BuggIDToBeAddedToJira );
				Results.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", Results );
			}
		}
	}
}