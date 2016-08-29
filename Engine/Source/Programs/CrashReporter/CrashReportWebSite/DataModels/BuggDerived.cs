using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using Tools.CrashReporter.CrashReportCommon;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels
{
    public partial class Bugg
    {
        public int CrashesInTimeFrameGroup { get; set; }
        public int CrashesInTimeFrameAll { get; set; }
        public int NumberOfUniqueMachines { get; set; }

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

        /// <summary>The first line of the callstack</summary>
        string ToJiraSummary = "";

        /// <summary></summary>
        public string JiraComponentsText { get; set; }

        /// <summary></summary>
        public string JiraResolution { get; set; }

        /// <summary></summary>
        public string JiraFixVersionsText { get; set; }

        /// <summary></summary>
        public string JiraFixCL { get; set; }
        public List<string> GenericFunctionCalls = null;
        public List<string> FunctionCalls = new List<string>();

        List<string> ToJiraDescriptions = new List<string>();

        List<string> ToJiraFunctionCalls = null;

        /// <summary></summary>
        public int LatestCLAffected { get; set; }

        /// <summary></summary>
        public string LatestOSAffected { get; set; }

        /// <summary></summary>
        public string LatestCrashSummary { get; set; }

        /// <summary>Branches in JIRA</summary>
        HashSet<string> ToJiraBranches = null;

        /// <summary>Versions in JIRA</summary>
        List<object> ToJiraVersions = null;

        /// <summary>Platforms in JIRA</summary>
        List<object> ToJiraPlatforms = null;

        /// <summary>Branches in jira summary fortnite</summary>
        List<object> ToBranchName = null;

        /// <summary>The CL of the oldest build when this bugg is occurring</summary>
        int ToJiraFirstCLAffected { get; set; }

        /// <summary></summary>
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

        /// <summary>
        /// Prepares Bugg for JIRA
        /// </summary>
        /// <param name="CrashesForBugg"></param>
        public void PrepareBuggForJira(List<Crash> CrashesForBugg)
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

            foreach (var Crash in CrashesForBugg)
            {
                // Only add machine if the number has 32 characters
                if (Crash.ComputerName != null && Crash.ComputerName.Length == 32)
                {
                    MachineIds.Add(Crash.ComputerName);
                    if (Crash.Description.Length > 4)
                    {
                        HashSetDescriptions.Add(Crash.Description);
                    }
                }

                // @TODO Ignore bad build versions.
                {
                    if (!string.IsNullOrEmpty(Crash.BuildVersion))
                    {
                        this.AffectedVersions.Add(Crash.BuildVersion);
                    }
                    // Depot || Stream
                    if (!string.IsNullOrEmpty(Crash.Branch))
                    {
                        this.BranchesFoundIn.Add(Crash.Branch);
                    }

                    int CrashBuiltFromCL = 0;
                    int.TryParse(Crash.ChangeListVersion, out CrashBuiltFromCL);
                    if (CrashBuiltFromCL > 0)
                    {
                        FirstCLAffected = Math.Min(FirstCLAffected, CrashBuiltFromCL);
                    }

                    if (!string.IsNullOrEmpty(Crash.PlatformName))
                    {
                        // Platform = "Platform [Desc]";
                        var PlatSubs = Crash.PlatformName.Split(" ".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
                        if (PlatSubs.Length >= 1)
                        {
                            this.AffectedPlatforms.Add(PlatSubs[0]);
                        }
                    }
                }
            }

            // CopyToJira 
            foreach (var Line in HashSetDescriptions)
            {
                string ListItem = "- " + HttpUtility.HtmlEncode(Line);
                ToJiraDescriptions.Add(ListItem);
            }

            this.ToJiraFirstCLAffected = FirstCLAffected;

            if (this.AffectedVersions.Count > 0)
            {
                this.BuildVersion = this.AffectedVersions.Last();	// Latest Version Affected
            }

            foreach (var AffectedBuild in this.AffectedVersions)
            {
                var Subs = AffectedBuild.Split(".".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
                if (Subs.Length >= 2)
                {
                    string MajorVersion = Subs[0] + "." + Subs[1];
                    this.AffectedMajorVersions.Add(MajorVersion);
                }
            }

            string BV = this.BuildVersion;
            this.NumberOfUniqueMachines = MachineIds.Count;		// # Affected Users
            string LatestCLAffected = CrashesForBugg.				// CL of the latest build
                Where(Crash => Crash.BuildVersion == BV).
                Max(Crash => Crash.ChangeListVersion);

            int ILatestCLAffected = -1;
            int.TryParse(LatestCLAffected, out ILatestCLAffected);
            this.LatestCLAffected = ILatestCLAffected;			// Latest CL Affected

            string LatestOSAffected = CrashesForBugg.OrderByDescending(Crash => Crash.TimeOfCrash).First().PlatformName;
            this.LatestOSAffected = LatestOSAffected;			// Latest Environment Affected

            // ToJiraSummary
            var FunctionCalls = new CallStackContainer(CrashesForBugg.First()).GetFunctionCallsForJira();
            if (FunctionCalls.Count > 0)
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
            foreach (var Version in this.AffectedMajorVersions)
            {
                bool bValid = JC.GetNameToVersions().ContainsKey(Version);
                if (bValid)
                {
                    this.ToJiraVersions.Add(JC.GetNameToVersions()[Version]);
                }
            }

            // ToJiraBranches
            this.ToJiraBranches = new HashSet<string>();
            foreach (var BranchName in this.BranchesFoundIn)
            {
                if (!string.IsNullOrEmpty(BranchName))
                {
                    // Stream
                    if (BranchName.StartsWith("//"))
                    {
                        this.ToJiraBranches.Add(BranchName);
                    }
                    // Depot
                    else
                    {
                        this.ToJiraBranches.Add(CrashReporterConstants.P4_DEPOT_PREFIX + BranchName);
                    }
                }
            }

            // ToJiraPlatforms
            this.ToJiraPlatforms = new List<object>();
            foreach (var platform in this.AffectedPlatforms)
            {
                bool bValid = JC.GetNameToPlatform().ContainsKey(platform);
                if (bValid)
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

            if (jc.CanBeUsed() && string.IsNullOrEmpty(this.TTPID))
            {
                // Additional Info URL / Link to Crash/Bugg
                string Key = jc.AddJiraTicket(issueFields);
                if (!string.IsNullOrEmpty(Key))
                {
                    TTPID = Key;
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
            fields.Add("project", new Dictionary<string, object> { { "id", 10600 } });
            fields.Add("issuetype", new Dictionary<string, object> { { "id", "1" } }); // Bug
            //branch found in - required = false
            fields.Add("customfield_11201", ToBranchName.ToList());

            //found CL = required = false
            //fields.Add("customfield_11500", th );

            //Platforms - required = false
            fields.Add("customfield_11203", new List<object>() { new Dictionary<string, object>() { { "self", "https://jira.ol.epicgames.net/rest/api/2/customFieldOption/11425" }, { "value", "PC" } } });

            //repro rate required = false
            //fields.Add("customfield_11900", this.Buggs_Crashes.Count);

            // ToJiraVersions	
            fields.Add("assignee", new Dictionary<string, object> { { "name", "fortnite.coretech" } });// assignee

            // Callstack customfield_11807
            string JiraCallstack = "{noformat}" + string.Join("\r\n", ToJiraFunctionCalls) + "{noformat}";
            fields.Add("customfield_11807", JiraCallstack);         // Callstack

            return fields;
        }

        /// <summary> Returns concatenated string of fields from the specified JIRA list. </summary>
        public string GetFieldsFrom(List<object> JiraList, string FieldName)
        {
            string Results = "";

            foreach (var Obj in JiraList)
            {
                Dictionary<string, object> Dict = (Dictionary<string, object>)Obj;
                try
                {
                    Results += (string)Dict[FieldName];
                    Results += " ";
                }
                catch (System.Exception /*E*/ )
                {

                }
            }

            return Results;
        }

        /// <summary> Creates a previews of the bugg, for verifying JIRA's fields. </summary>
        public string ToTooltip()
        {
            string NL = "&#013;";

            string Tooltip = NL;
            Tooltip += "Project: UE" + NL;
            Tooltip += "Summary: " + ToJiraSummary + NL;
            Tooltip += "Description: " + NL + string.Join(NL, ToJiraDescriptions) + NL;
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
            string JiraPlatforms = GetFieldsFrom(ToJiraPlatforms, "value");
            Tooltip += "JiraPlatforms: " + JiraPlatforms + NL;

            string JiraCallstack = "Callstack:" + NL + string.Join(NL, ToJiraFunctionCalls) + NL;
            Tooltip += JiraCallstack;

            return Tooltip;
        }

        /// <summary>
        /// 
        /// </summary>
        public string GetAffectedVersions()
        {
            if (AffectedVersions.Count == 1)
            {
                return AffectedVersions.Max;
            }
            else
            {
                return AffectedVersions.Min + " - " + AffectedVersions.Max;
            }
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
            if (NumMonths > 0)
            {
                return string.Format("{0} month(s) {1} day(s)", NumMonths, NumDays);
            }
            else if (NumDays > 0)
            {
                return string.Format("{0} day(s)", NumDays);
            }
            else
            {
                return "Less than one day";
            }
        }
    }
}