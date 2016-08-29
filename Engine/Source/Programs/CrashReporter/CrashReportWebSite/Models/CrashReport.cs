// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System;
using System.Data.Linq;
using System.Diagnostics;
using System.Web.UI.WebControls;
using Tools.CrashReporter.CrashReportWebSite.DataModels;
using Tools.DotNETCommon;
using System.Web;

using Tools.CrashReporter.CrashReportCommon;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary> Function call. </summary>
	public partial class FunctionCall
	{
		/// <summary> Helper method, display this Bugg as a human readable string. Debugging purpose. </summary>
		public override string ToString()
		{
			return string.Format( "{0}:{1}", Id, Call );
		}
	}

	/// <summary> Users mapping. </summary>
	public partial class UsersMapping
	{
		/// <summary> Helper method, display this UsersMapping as a human readable string. Debugging purpose. </summary>
		public override string ToString()
		{
			return string.Format( "{0} [{1}]", UserName, UserEmail );
		}
	}

	/// <summary> User. </summary>
	public partial class User
	{
		/// <summary> Helper method, display this User as a human readable string. Debugging purpose. </summary>
		public override string ToString()
		{
			return string.Format( "{0} [{1}/{2}]", UserName, Id, UserGroupId );
		}
	}

	/// <summary> UserGroup. </summary>
	public partial class UserGroup
	{
		/// <summary> Helper method, display this UserGroup as a human readable string. Debugging purpose. </summary>
		public override string ToString()
		{
			return string.Format( "{0} [{1}]", Name, Id );
		}
	}

	/// <summary></summary>
	public partial class Buggs_Crash
	{
		/// <summary></summary>
		public override string ToString()
		{
			return string.Format( "{0} <-> {1}", BuggId, CrashId );
		}
	}

	/// <summary>
	/// Derived information to the default Bugg information from the database.
	/// </summary>
	public partial class Bugg
	{
		/// <summary>
		/// Marked used in the form to identify that we will add this bugg to JIRA.
		/// </summary>
		public static string JiraSubmitName = "CopyToJira-";

		/// <summary></summary>
		public int CrashesInTimeFrameGroup { get; set; }

		/// <summary></summary>
		public int CrashesInTimeFrameAll { get; set; }

		/// <summary></summary>
		public int NumberOfUniqueMachines { get; set; }

		/// <summary></summary>
		public int LatestCLAffected { get; set; }

		/// <summary></summary>
		public string LatestOSAffected { get; set; }

		/// <summary>  </summary>
		public SortedSet<string> AffectedPlatforms { get; set; }

		/// <summary></summary>
		public SortedSet<string> AffectedVersions { get; set; }

		/// <summary> 4.4, 4.5 and so. </summary>
		public SortedSet<string> AffectedMajorVersions { get; set; }

		/// <summary>  </summary>
		public SortedSet<string> BranchesFoundIn { get; set; }
		
		/// <summary></summary>
		public string JiraSummary { get; set; }

		/// <summary></summary>
		public string JiraComponentsText { get; set; }

		/// <summary></summary>
		public string JiraResolution { get; set; }

		/// <summary></summary>
		public string JiraFixVersionsText { get; set; }

		/// <summary></summary>
		public string JiraFixCL { get; set; }

		/// <summary></summary>
		public string LatestCrashSummary { get; set; }

		/// <summary>The CL of the oldest build when this bugg is occurring</summary>
		int ToJiraFirstCLAffected { get; set; }

		/// <summary>The first line of the callstack</summary>
		string ToJiraSummary = "";

		/// <summary>Branches in JIRA</summary>
		HashSet<string> ToJiraBranches = null;

		/// <summary>Versions in JIRA</summary>
		List<object> ToJiraVersions = null;

		/// <summary>Platforms in JIRA</summary>
		List<object> ToJiraPlatforms = null;

        /// <summary>Branches in jira summary fortnite</summary>
        List<object> ToBranchName = null;

		List<string> ToJiraDescriptions = new List<string>();

		List<string> ToJiraFunctionCalls = null;
		List<string> GenericFunctionCalls = null;

		/// <summary> Helper method, display this Bugg as a human readable string. Debugging purpose. </summary>
		public override string ToString()
		{
			return string.Format( "Id={0} NOC={1} TLC={2} BV={3} NOM={4} Pattern={5}", Id, NumberOfCrashes, TimeOfLastCrash, BuildVersion, NumberOfUniqueMachines, Pattern != null ? Pattern.Length : 0 );
		}

		/// <summary>
		/// Prepares Bugg for JIRA
		/// </summary>
		/// <param name="CrashesForBugg"></param>
		public void PrepareBuggForJira( List<Crash> CrashesForBugg )
		{
			var JC = JiraConnection.Get();

			this.AffectedVersions = new SortedSet<string>();
			this.AffectedMajorVersions = new SortedSet<string>(); // 4.4, 4.5 and so
			this.BranchesFoundIn = new SortedSet<string>();
			this.AffectedPlatforms = new SortedSet<string>();
			this.CrashesInTimeFrameAll = CrashesForBugg.Count;
			this.CrashesInTimeFrameGroup = CrashesForBugg.Count;
			var HashSetDescriptions = new HashSet<string>();

			HashSet<string> MachineIds = new HashSet<string>();
			int FirstCLAffected = int.MaxValue;

			foreach( var Crash in CrashesForBugg )
			{
				// Only add machine if the number has 32 characters
				if (Crash.MachineId != null && Crash.MachineId.Length == 32)
				{
					MachineIds.Add( Crash.MachineId );
					if (Crash.Description.Length > 4)
					{
						HashSetDescriptions.Add( Crash.Description );
					}										
				}

				// @TODO Ignore bad build versions.
				{
					if( !string.IsNullOrEmpty( Crash.BuildVersion ) )
					{
						this.AffectedVersions.Add( Crash.BuildVersion );
					}
					// Depot || Stream
					if (!string.IsNullOrEmpty( Crash.Branch ))
					{
						this.BranchesFoundIn.Add( Crash.Branch );
					}

					int CrashBuiltFromCL = 0;
					int.TryParse( Crash.BuiltFromCL, out CrashBuiltFromCL );
					if( CrashBuiltFromCL > 0 )
					{
						FirstCLAffected = Math.Min( FirstCLAffected, CrashBuiltFromCL );
					}

					if( !string.IsNullOrEmpty( Crash.Platform ) )
					{
						// Platform = "Platform [Desc]";
						var PlatSubs = Crash.Platform.Split( " ".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
						if( PlatSubs.Length >= 1 )
						{
							this.AffectedPlatforms.Add( PlatSubs[0] );
						}
					}
				}
			}

			// CopyToJira 
			foreach( var Line in HashSetDescriptions )
			{
				string ListItem = "- " + HttpUtility.HtmlEncode( Line );
				ToJiraDescriptions.Add( ListItem );
			}
			
			this.ToJiraFirstCLAffected = FirstCLAffected;

			if( this.AffectedVersions.Count > 0 )
			{
				this.BuildVersion = this.AffectedVersions.Last();	// Latest Version Affected
			}
            
			foreach( var AffectedBuild in this.AffectedVersions )
			{
				var Subs = AffectedBuild.Split( ".".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
				if( Subs.Length >= 2 )
				{
					string MajorVersion = Subs[0] + "." + Subs[1];
					this.AffectedMajorVersions.Add( MajorVersion );
				}
			}

			string BV = this.BuildVersion;
			this.NumberOfUniqueMachines = MachineIds.Count;		// # Affected Users
			string LatestCLAffected = CrashesForBugg.				// CL of the latest build
				Where( Crash => Crash.BuildVersion == BV ).
				Max( Crash => Crash.BuiltFromCL );

			int ILatestCLAffected = -1;
			int.TryParse( LatestCLAffected, out ILatestCLAffected );
			this.LatestCLAffected = ILatestCLAffected;			// Latest CL Affected

			string LatestOSAffected = CrashesForBugg.OrderByDescending( Crash => Crash.TimeOfCrash ).First().Platform;
			this.LatestOSAffected = LatestOSAffected;			// Latest Environment Affected

			// ToJiraSummary
			var FunctionCalls = new CallStackContainer(CrashesForBugg.First()).GetFunctionCallsForJira();
			if( FunctionCalls.Count > 0 )
			{
				this.ToJiraSummary = FunctionCalls[0];
				this.ToJiraFunctionCalls = FunctionCalls;
			}
			else
			{
				this.ToJiraSummary = "No valid callstack found";
				this.ToJiraFunctionCalls = new List<string>();
			}

			// ToJiraVersions
			this.ToJiraVersions = new List<object>();
			foreach( var Version in this.AffectedMajorVersions )
			{
				bool bValid = JC.GetNameToVersions().ContainsKey( Version );
				if( bValid )
				{
					this.ToJiraVersions.Add( JC.GetNameToVersions()[Version] );
				}
			}

			// ToJiraBranches
			this.ToJiraBranches = new HashSet<string>();
			foreach( var BranchName in this.BranchesFoundIn )
			{
				if( !string.IsNullOrEmpty(BranchName) )
				{
					// Stream
					if (BranchName.StartsWith( "//" ))
					{
						this.ToJiraBranches.Add( BranchName );
					}
					// Depot
					else
					{
						this.ToJiraBranches.Add( CrashReporterConstants.P4_DEPOT_PREFIX + BranchName );
					}
				}
			}

			// ToJiraPlatforms
			this.ToJiraPlatforms = new List<object>();
			foreach( var platform in this.AffectedPlatforms )
			{
                bool bValid = JC.GetNameToPlatform().ContainsKey(platform);
				if( bValid )
				{
                    this.ToJiraPlatforms.Add(JC.GetNameToPlatform()[platform]);
				}
			}

            // ToJiraPlatforms
            this.ToBranchName = new List<object>();
            foreach (var BranchName in this.BranchesFoundIn)
            {
                bool bValid = JC.GetNameToBranch().ContainsKey(BranchName);
                if (bValid)
                {
                    this.ToBranchName.Add(JC.GetNameToBranch()[BranchName]);
                }
            }   
		}

		/// <summary>
		/// Adds the bugg as a new JIRA ticket
		/// </summary>
		public void CopyToJira()
		{
            var jc = JiraConnection.Get();
		    Dictionary<string, object> issueFields;

            if (BranchesFoundIn.Any(data => data.ToLower().Contains("//fort")))
            {
                issueFields = CreateFortniteIssue(jc);
            }
            //else if (BranchesFoundIn.Any(data => data.ToLower().Contains("//orion")))
            //{
            //    issueFields = CreateOrionIssue();
            //}
            else
		    {
                issueFields = CreateGeneralIssue(jc);
		    }
            
			if(jc.CanBeUsed() && string.IsNullOrEmpty( this.Jira ))
			{
				// Additional Info URL / Link to Crash/Bugg
                string Key = jc.AddJiraTicket(issueFields);
				if( !string.IsNullOrEmpty( Key ) )
				{
					Jira = Key;
					var buggs = new BuggRepository();
					buggs.SetJIRAForBuggAndCrashes( Key, Id );
				}
			}
		}

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public Dictionary<string, object> CreateGeneralIssue(JiraConnection jc)
	    {
            var fields = new Dictionary<string, object>();

            fields.Add("project", new Dictionary<string, object> { { "id", 11205 } });	    // UE
            
            fields.Add("summary", "[CrashReport] " + ToJiraSummary);						// Call Stack, Line 1
            fields.Add("description", string.Join("\r\n", ToJiraDescriptions));			    // Description
            fields.Add("issuetype", new Dictionary<string, object> { { "id", "1" } });	    // Bug
            fields.Add("labels", new string[] { "crash", "liveissue" });					// <label>crash, live issue</label>
            fields.Add("customfield_11500", ToJiraFirstCLAffected);						    // Changelist # / Found Changelist
            fields.Add("environment", LatestOSAffected);		    						// Platform

            // Components
            var SupportComponent = jc.GetNameToComponents()["Support"];
            fields.Add("components", new object[] { SupportComponent });

            // ToJiraVersions			
            fields.Add("versions", ToJiraVersions);

            // ToJiraBranches
            fields.Add("customfield_12402", ToJiraBranches.ToList());						// NewBranchFoundIn

            // ToJiraPlatforms
            fields.Add("customfield_11203", ToJiraPlatforms);

            // Callstack customfield_11807
            string JiraCallstack = "{noformat}" + string.Join("\r\n", ToJiraFunctionCalls) + "{noformat}";
            fields.Add("customfield_11807", JiraCallstack);								// Callstack

            string BuggLink = "http://crashreporter/Buggs/Show/" + Id;
            fields.Add("customfield_11205", BuggLink);
            return fields;
	    }

	    /// <summary>
	    /// 
	    /// </summary>
	    /// <param name="jc"></param>
	    /// <returns></returns>
	    public Dictionary<string, object> CreateFortniteIssue(JiraConnection jc)
	    {
	        var fields = new Dictionary<string, object>();

	        fields.Add("summary", "[CrashReport] " + ToJiraSummary); // Call Stack, Line 1
	        fields.Add("description", string.Join("\r\n", ToJiraDescriptions)); // Description
	        fields.Add("project", new Dictionary<string, object> {{"id", 10600}});
	        fields.Add("issuetype", new Dictionary<string, object> {{"id", "1"}}); // Bug
	        //branch found in - required = false
	        fields.Add("customfield_11201", ToBranchName.ToList());

	        //found CL = required = false
	        //fields.Add("customfield_11500", th );

	        //Platforms - required = false
	        fields.Add("customfield_11203", new List<object>(){new Dictionary<string, object>() {{"self", "https://jira.ol.epicgames.net/rest/api/2/customFieldOption/11425"},{"value","PC"}}});
            
            //repro rate required = false
            //fields.Add("customfield_11900", this.Buggs_Crashes.Count);

            // ToJiraVersions	
            fields.Add("assignee", new Dictionary<string, object>{{"name", "fortnite.coretech"}});// assignee

            // Callstack customfield_11807
            string JiraCallstack = "{noformat}" + string.Join("\r\n", ToJiraFunctionCalls) + "{noformat}";
            fields.Add("customfield_11807", JiraCallstack);         // Callstack
            
	        return fields;
	    } 


		/// <summary> Creates a previews of the bugg, for verifying JIRA's fields. </summary>
		public string ToTooltip()
		{
			string NL = "&#013;";

			string Tooltip = NL;
			Tooltip += "Project: UE" + NL;
			Tooltip += "Summary: " + ToJiraSummary + NL;
			Tooltip += "Description: " + NL + string.Join( NL, ToJiraDescriptions ) + NL;
			Tooltip += "Issuetype: " + "1 (bug)" + NL;
			Tooltip += "Labels: " + "crash" + NL;
			Tooltip += "Changelist # / Found Changelist: " + ToJiraFirstCLAffected + NL;
			Tooltip += "LatestOSAffected: " + LatestOSAffected + NL;

			// "name"
			string JiraVersions = GetFieldsFrom(ToJiraVersions, "name");
			Tooltip += "JiraVersions: " + JiraVersions + NL;

			// "value"
			string JiraBranches = "";
			foreach (var Branch in ToJiraBranches)
			{
				JiraBranches += Branch + ", ";
			}
			Tooltip += "JiraBranches: " + JiraBranches + NL;

			// "value"
			string JiraPlatforms = GetFieldsFrom( ToJiraPlatforms, "value" );
			Tooltip += "JiraPlatforms: " + JiraPlatforms + NL;

			string JiraCallstack = "Callstack:" + NL + string.Join( NL, ToJiraFunctionCalls ) + NL;
			Tooltip += JiraCallstack;

			return Tooltip;
		}

		/// <summary> Returns concatenated string of fields from the specified JIRA list. </summary>
		public string GetFieldsFrom( List<object> JiraList, string FieldName )
		{
			string Results = "";

			foreach( var Obj in JiraList )
			{
				Dictionary<string, object> Dict = (Dictionary<string, object>)Obj;
				try
				{
					Results += (string)Dict[FieldName];
					Results += " ";
				}
				catch( System.Exception /*E*/ )
				{
					
				}
			}

			return Results;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public List<Crash> GetCrashes()
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				CrashRepository CrashRepo = new CrashRepository();
				var CrashList =
				(
						from BuggCrash in CrashRepo.Context.Buggs_Crashes
                        where BuggCrash.BuggId == Id && BuggCrash.Crash.TimeOfCrash > DateTime.UtcNow.AddMonths(-6)
						select BuggCrash.Crash
				).OrderByDescending( c => c.TimeOfCrash ).ToList();
				return CrashList;
			}
		}

		/// <summary></summary>
		public string GetCrashTypeAsString()
		{
			if( CrashType == 1 )
			{
				return "Crash";
			}
			else if( CrashType == 2 )
			{
				return "Assert";
			}
			else if( CrashType == 3 )
			{
				return "Ensure";
			}
			return "Unknown";
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public string GetLifeSpan()
		{
			TimeSpan LifeSpan = TimeOfLastCrash.Value - TimeOfFirstCrash.Value;
			// Only to visualize the life span, accuracy not so important here.
			int NumMonths = LifeSpan.Days / 30;
			int NumDays = LifeSpan.Days % 30;
			if( NumMonths > 0 )
			{
				return string.Format( "{0} month(s) {1} day(s)", NumMonths, NumDays );
			}
			else if( NumDays > 0 )
			{
				return string.Format( "{0} day(s)", NumDays );

			}
			else
			{
				return "Less than one day";
			}
		}

		/// <summary>
		/// 
		/// </summary>
		public string GetAffectedVersions()
		{
			if( AffectedVersions.Count == 1 )
			{
				return AffectedVersions.Max;
			}
			else
			{
				return AffectedVersions.Min + " - " + AffectedVersions.Max;
			}
		}

		/// <summary>
		/// Return the top lines of a callstack.
		/// </summary>
		/// <returns>A list of callstack entries.</returns>
		public List<string> GetFunctionCalls()
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Id=" + this.Id + ")" ) )
			{
				if( GenericFunctionCalls == null )
				{
					CrashRepository CrashRepo = new CrashRepository();

					var Crash =
					(
						from BuggCrash in CrashRepo.Context.Buggs_Crashes
						where BuggCrash.BuggId == Id
						select BuggCrash.Crash
					).FirstOrDefault();

					if( Crash != null )
					{
						GenericFunctionCalls = Crash.GetCallStack().GetFunctionCalls();
					}
					else
					{
						GenericFunctionCalls = new List<string>();
					}
				}
				return GenericFunctionCalls;
			}
		}
	}

	/// <summary>
	/// Derived information in addition to what is retrieved from the database.
	/// </summary>
	public partial class Crash
	{
		/// <summary>Hard coded site path.</summary>
		const string SitePath = @"\\devweb-02\Sites";

		/// <summary>Crash context for this crash.</summary>
		FGenericCrashContext CrashContext = null;

		bool bUseFullMinidumpPath = false;

		/// <summary>If available, will read CrashContext.runtime-xml.</summary>
		public void ReadCrashContextIfAvailable()
		{
			try
			{
                //\\epicgames.net\Root\Projects\Paragon\QA_CrashReports
				bool bHasCrashContext = HasCrashContextFile();
				if (bHasCrashContext)
				{
					CrashContext = FGenericCrashContext.FromFile( SitePath + GetCrashContextUrl() );
					bool bTest = CrashContext != null && !string.IsNullOrEmpty( CrashContext.PrimaryCrashProperties.FullCrashDumpLocation );
					if(bTest)
					{
						bUseFullMinidumpPath = true;

                        //Some temporary code to redirect to the new file location for fulldumps for paragon.
                        //Consider removing this once fulldumps stop appearing in the old location.
                        if (CrashContext.PrimaryCrashProperties.FullCrashDumpLocation.ToLower()
                                .Contains("\\\\epicgames.net\\root\\dept\\gameqa\\paragon\\paragon_launchercrashdumps"))
                        {
                            //Files from old versions of the client may end up in the old location. Check for files there first.
                            if(!System.IO.Directory.Exists( CrashContext.PrimaryCrashProperties.FullCrashDumpLocation ))
                            {
                                var suffix =
                                CrashContext.PrimaryCrashProperties.FullCrashDumpLocation.Substring("\\\\epicgames.net\\root\\dept\\gameqa\\paragon\\paragon_launchercrashdumps".Length);
                                CrashContext.PrimaryCrashProperties.FullCrashDumpLocation = String.Format("\\\\epicgames.net\\Root\\Projects\\Paragon\\QA_CrashReports{0}", suffix);

                                //If the file doesn't exist in the new location either then don't use the full minidump path.
                                bUseFullMinidumpPath =
                                    System.IO.Directory.Exists(CrashContext.PrimaryCrashProperties.FullCrashDumpLocation);
                            }
                        }

                        //End of temporary code.

						FLogger.Global.WriteEvent( "ReadCrashContextIfAvailable " + CrashContext.PrimaryCrashProperties.FullCrashDumpLocation + " is " + bUseFullMinidumpPath );
					}
				}
			}
			catch (Exception Ex)
			{
				Debug.WriteLine( "Exception in ReadCrashContextIfAvailable: " + Ex.ToString() );
			}
		}

		/// <summary> Helper method, display this Bugg as a human readable string. Debugging purpose. </summary>
		public override string ToString()
		{
			return string.Format( "Id={0} TOC={1} BV={2}-{3}+{4} G={5} U={6}", Id, TimeOfCrash, this.BuildVersion, this.BuiltFromCL, this.Branch, this.GameName, this.UserName );
		}

		/// <summary>A formatted callstack.</summary>
		public CallStackContainer CallStackContainer { get; set; }

		/// <summary>Resolve the User, supplied by name in older crashes but by ID in newer ones</summary>
		public User User
		{
			get
			{
				return UserById;
			}
		}

		/// <summary>The user group of the user.</summary>
		public string UserGroupName = "";

		/// <summary>
		/// Return the Url of the log.
		/// </summary>
		public string GetLogUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_Launch.log";
		}

		/// <summary>
		/// Return the Url of the minidump.
		/// </summary>
		public string GetMiniDumpUrl()
		{
			var WebPath = Properties.Settings.Default.CrashReporterFiles + Id + "_MiniDump.dmp";
			var Path = UseFullMinidumpPath() ? CrashContext.PrimaryCrashProperties.FullCrashDumpLocation : WebPath;
			return Path;
		}

		/// <summary>
		/// Tooltip for the dump.
		/// </summary>
		public string GetDumpTitle()
		{
			var Title = UseFullMinidumpPath() ? 
                "Copy the link and open in the explorer" : "";
			return Title;
		}

		/// <summary>
		/// Return the short name of the dump.
		/// </summary>
		public string GetDumpName()
		{
			return UseFullMinidumpPath() ? "Fulldump" : "Minidump";
		}

		/// <summary>
		/// Return the Url of the diagnostics file.
		/// </summary>
		public string GetDiagnosticsUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_Diagnostics.txt";
		}

		/// <summary>
		/// Return the Url of the crash context file.
		/// </summary>
		public string GetCrashContextUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_CrashContext.runtime-xml";
		}

		/// <summary>
		/// Return the Url of the Windows Error Report meta data file.
		/// </summary>
		public string GetMetaDataUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_WERMeta.xml";
		}

		/// <summary>
		/// Return the Url of the report video file.
		/// </summary>
		/// <returns>The Url of the crash report video file.</returns>
		public string GetVideoUrl()
		{
			return Properties.Settings.Default.CrashReporterVideos + Id + "_CrashVideo.avi";
		}

		/// <summary> Whether the user allowed us to be contacted. </summary>
		public bool AllowToBeContacted
		{
			get
			{
				return bAllowToBeContacted.GetValueOrDefault( false );
			}
		}

		/// <summary>
		/// Return lines of processed callstack entries.
		/// </summary>
		/// <param name="StartIndex">The starting entry of the callstack.</param>
		/// <param name="Count">The number of lines to return.</param>
		/// <returns>Lines of processed callstack entries.</returns>
		public List<CallStackEntry> GetCallStackEntries( int StartIndex, int Count )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Count=" + Count + ")" ) )
			{
				IEnumerable<CallStackEntry> Results = new List<CallStackEntry>() { new CallStackEntry() };

				if( CallStackContainer != null && CallStackContainer.CallStackEntries != null )
				{
					int MaxCount = Math.Min( Count, CallStackContainer.CallStackEntries.Count );
					Results = CallStackContainer.CallStackEntries.Take( MaxCount );
				}

				return Results.ToList();
			}
		}

		/// <summary>
		/// Return a display friendly version of the time of crash.
		/// </summary>
		/// <returns>A pair of strings representing the date and time of the crash.</returns>
		public string[] GetTimeOfCrash()
		{
			string[] Results = new string[2];
			if( TimeOfCrash.HasValue )
			{
				DateTime LocalTimeOfCrash = TimeOfCrash.Value.ToLocalTime();
				Results[0] = LocalTimeOfCrash.ToShortDateString();
				Results[1] = LocalTimeOfCrash.ToShortTimeString();
			}

			return Results;
		}

		/// <summary></summary>
		public string GetCrashTypeAsString()
		{
			if( CrashType == 1 )
			{
				return "Crash";
			}
			else if( CrashType == 2 )
			{
				return "Assert";
			}
			else if( CrashType == 3 )
			{
				return "Ensure";
			}
			return "Unknown";
		}

		/// <summary>
		/// Gets a formatted callstack for the crash.
		/// </summary>
		/// <returns>A formatted callstack.</returns>
		public CallStackContainer GetCallStack()
		{
			CrashRepository Crashes = new CrashRepository();
			return Crashes.GetCallStack( this );
		}

		/// <summary>
		/// Build a callstack pattern for a crash to ease bucketing of crashes into Buggs.
		/// </summary>
		public void BuildPattern( CrashReportDataContext Context )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Crash=" + Id + ")" ) )
			{
				List<string> PatternList = new List<string>();
				var FunctionCalls = Context.FunctionCalls;

				// Get an array of call stack items
				CallStackContainer CallStack = GetCallStack();

				if( Pattern == null )
				{
					// Set the module based on the modules in the call stack
					Module = CallStack.GetModuleName();
					try
					{
						foreach (CallStackEntry Entry in CallStack.CallStackEntries.Take( CallStackContainer.MaxLinesToParse ))
						{
						    FunctionCall currentFunctionCall;

							if( FunctionCalls.Any( f => f.Call == Entry.FunctionName ) )
							{
								currentFunctionCall = FunctionCalls.First( f => f.Call == Entry.FunctionName );
							}
							else
							{
								currentFunctionCall = new FunctionCall();
								currentFunctionCall.Call = Entry.FunctionName;
								FunctionCalls.InsertOnSubmit( currentFunctionCall );
							}

                            Context.SubmitChanges();
							PatternList.Add( currentFunctionCall.Id.ToString() );
						}

						Pattern = string.Join( "+", PatternList );
                        
						Context.SubmitChanges();
					}
					catch( Exception Ex )
					{
						FLogger.Global.WriteException( "BuildPattern exception: " + Ex.Message.ToString() );
					}
				}
			}
		}

		/// <summary>
		/// http://www.codeproject.com/KB/linq/linq-to-sql-many-to-many.aspx
		/// </summary>
		private EntitySet<Bugg> BuggsCache;

		/// <summary>
		/// 
		/// </summary>
		public EntitySet<Bugg> Buggs
		{
			get
			{
				if( BuggsCache == null )
				{
					BuggsCache = new EntitySet<Bugg>( OnBuggsAdd, OnBuggsRemove );
					BuggsCache.SetSource( Buggs_Crashes.Select( c => c.Bugg ) );
				}

				return BuggsCache;
			}
			set
			{
				BuggsCache.Assign( value );
			}
		}

		private void OnBuggsAdd( Bugg BuggInstance )
		{
			Buggs_Crashes.Add( new Buggs_Crash { Crash = this, Bugg = BuggInstance } );
			SendPropertyChanged( null );
		}

		private void OnBuggsRemove( Bugg BuggInstance )
		{
			var BuggCrash = Buggs_Crashes.FirstOrDefault(
				c => c.CrashId == Id
				&& c.BuggId == BuggInstance.Id );
			Buggs_Crashes.Remove( BuggCrash );

			SendPropertyChanged( null );
		}

		// Extensions.

		/// <summary>
		/// 
		/// </summary>
		public int ID
		{
			get
			{
				return this.Id;
			}
			set
			{
				this.Id = value;
			}
		}

		/// <summary>Return true, if there is a diagnostics file associated with the crash</summary>
		public bool HasDiagnosticsFile()
		{
			var Path = SitePath + GetDiagnosticsUrl();
			return System.IO.File.Exists( Path );
		}


		/// <summary>Return true, if there is a crash context file associated with the crash</summary>
		public bool HasCrashContextFile()
		{
			var Path = SitePath + GetCrashContextUrl();
			return System.IO.File.Exists( Path );
		}

		/// <summary>Return true, if there is a minidump file associated with the crash</summary>
		public bool HasMiniDumpFile()
		{
			var Path = SitePath + GetMiniDumpUrl();
			return UseFullMinidumpPath() ? true : System.IO.File.Exists( Path );
		}

		/// <summary>Whether to use the full minidump.</summary>
		public bool UseFullMinidumpPath()
		{
			return bUseFullMinidumpPath;
		}

		/// <summary>Return true, if there is a log file associated with the crash</summary>
		public bool HasLogFile()
		{
			var Path = SitePath + GetLogUrl();
			return System.IO.File.Exists( Path );
		}

		/// <summary>Return true, if there is a video file associated with the crash</summary>
		public bool HasVideoFile()
		{
			var Path = SitePath + GetVideoUrl();
			return System.IO.File.Exists( Path );
		}

		/// <summary>Return true, if there is a metadata file associated with the crash</summary>
		public bool HasMetaDataFile()
		{
			var Path = SitePath + GetMetaDataUrl();
			return System.IO.File.Exists( Path );
		}

		/// <summary>
		/// 
		/// </summary>
		public string Platform
		{
			get
			{
				return this.PlatformName;
			}
			set
			{
				this.PlatformName = value;
			}
		}

		/// <summary>
		/// 
		/// </summary>
		public string FixCL
		{
			get
			{
				return this.FixedChangeList;
			}
			set
			{
				this.FixedChangeList = value;
			}
		}
	}
}
