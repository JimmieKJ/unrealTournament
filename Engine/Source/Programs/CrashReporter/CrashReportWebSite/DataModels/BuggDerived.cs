using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using Newtonsoft.Json;
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

        public string JiraProject { get; set; }

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
        /// <param name="crashesForBugg"></param>
        public void PrepareBuggForJira(List<Crash> crashesForBugg)
        {
            var jiraConnection = JiraConnection.Get();

            this.AffectedVersions = new SortedSet<string>();
            this.AffectedMajorVersions = new SortedSet<string>(); // 4.4, 4.5 and so
            this.BranchesFoundIn = new SortedSet<string>();
            this.AffectedPlatforms = new SortedSet<string>();
            var hashSetDescriptions = new HashSet<string>();

            var machineIds = new HashSet<string>();
            var firstClAffected = int.MaxValue;

            foreach (var crash in crashesForBugg)
            {
                // Only add machine if the number has 32 characters
                if (crash.ComputerName != null && crash.ComputerName.Length == 32)
                {
                    machineIds.Add(crash.ComputerName);
                    if (crash.Description.Length > 4)
                    {
                        hashSetDescriptions.Add(crash.Description);
                    }
                }

                if (!string.IsNullOrEmpty(crash.BuildVersion))
                {
                    this.AffectedVersions.Add(crash.BuildVersion);
                }
                // Depot || Stream
                if (!string.IsNullOrEmpty(crash.Branch))
                {
                    this.BranchesFoundIn.Add(crash.Branch);
                }

                var crashBuiltFromCl = 0;
                int.TryParse(crash.ChangeListVersion, out crashBuiltFromCl);
                if (crashBuiltFromCl > 0)
                {
                    firstClAffected = Math.Min(firstClAffected, crashBuiltFromCl);
                }

                if (!string.IsNullOrEmpty(crash.PlatformName))
                {
                    // Platform = "Platform [Desc]";
                    var platSubs = crash.PlatformName.Split(" ".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
                    if (platSubs.Length >= 1)
                    {
                        this.AffectedPlatforms.Add(platSubs[0]);
                    }
                }
            }

            // CopyToJira 
            foreach (var line in hashSetDescriptions)
            {
                var listItem = "- " + HttpUtility.HtmlEncode(line);
                ToJiraDescriptions.Add(listItem);
            }

            this.ToJiraFirstCLAffected = firstClAffected;
            if (this.AffectedVersions.Count > 0)
            {
                this.BuildVersion = this.AffectedVersions.Last();	// Latest Version Affected
            }

            foreach (var affectedBuild in this.AffectedVersions)
            {
                var subs = affectedBuild.Split(".".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
                if (subs.Length >= 2)
                {
                    var majorVersion = subs[0] + "." + subs[1];
                    this.AffectedMajorVersions.Add(majorVersion);
                }
            }

            var bv = this.BuildVersion;
            this.NumberOfUniqueMachines = machineIds.Distinct().Count();// # Affected Users
            var latestClAffected = crashesForBugg.				// CL of the latest build
                Where(crash => crash.BuildVersion == bv).
                Max(crash => crash.ChangeListVersion);

            var ILatestCLAffected = -1;
            int.TryParse(latestClAffected, out ILatestCLAffected);
            this.LatestCLAffected = ILatestCLAffected;			// Latest CL Affected

            var latestOsAffected = crashesForBugg.OrderByDescending(crash => crash.TimeOfCrash).First().PlatformName;
            this.LatestOSAffected = latestOsAffected;			// Latest Environment Affected

            // ToJiraSummary
            var functionCalls = new CallStackContainer(crashesForBugg.First()).GetFunctionCallsForJira();
            if (functionCalls.Count > 0)
            {
                this.ToJiraSummary = functionCalls[0];
                this.ToJiraFunctionCalls = functionCalls;
            }
            else
            {
                this.ToJiraSummary = "No valid callstack found";
                this.ToJiraFunctionCalls = new List<string>();
            }

            // ToJiraVersions
            this.ToJiraVersions = new List<object>();
            foreach (var version in this.AffectedMajorVersions)
            {
                var bValid = jiraConnection.GetNameToVersions().ContainsKey(version);
                if (bValid)
                {
                    this.ToJiraVersions.Add(jiraConnection.GetNameToVersions()[version]);
                }
            }

            // ToJiraBranches
            this.ToJiraBranches = new HashSet<string>();
            foreach (var branchName in this.BranchesFoundIn)
            {
                if (!string.IsNullOrEmpty(branchName))
                {
                    // Stream
                    if (branchName.StartsWith("//"))
                    {
                        this.ToJiraBranches.Add(branchName);
                    }
                    // Depot
                    else
                    {
                        this.ToJiraBranches.Add(CrashReporterConstants.P4_DEPOT_PREFIX + branchName);
                    }
                }
            }

            // ToJiraPlatforms
            this.ToJiraPlatforms = new List<object>();
            foreach (var platform in this.AffectedPlatforms)
            {
                bool bValid = jiraConnection.GetNameToPlatform().ContainsKey(platform);
                if (bValid)
                {
                    this.ToJiraPlatforms.Add(jiraConnection.GetNameToPlatform()[platform]);
                }
            }

            // ToJiraPlatforms
            this.ToBranchName = new List<object>();
            foreach (var BranchName in this.BranchesFoundIn)
            {
                bool bValid = jiraConnection.GetNameToBranch().ContainsKey(BranchName);
                if (bValid)
                {
                    this.ToBranchName.Add(jiraConnection.GetNameToBranch()[BranchName]);
                }
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public void CopyToJira()
        {
            var jc = JiraConnection.Get();
            Dictionary<string, object> issueFields;

            switch (this.JiraProject)
            {
                case "UE":
                    issueFields = CreateGeneralIssue(jc);
                    break;
                case "FORT":
                    issueFields = CreateFortniteIssue(jc);
                    break;
                case "ORION":
                    issueFields = CreateOrionIssue(jc);
                    break;
                default:
                    issueFields = CreateGeneralIssue(jc);
                    break;
            }

            if (jc.CanBeUsed() && string.IsNullOrEmpty(this.TTPID))
            {
                // Additional Info URL / Link to Crash/Bugg
                var key = jc.AddJiraTicket(issueFields);
                if (!string.IsNullOrEmpty(key))
                {
                    TTPID = key;
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
            fields.Add("customfield_11203", new List<object>() { new Dictionary<string, object>() { { "self", "https://jira.it.epicgames.net/rest/api/2/customFieldOption/11425" }, { "value", "PC" } } });

            //repro rate required = false
            //fields.Add("customfield_11900", this.Buggs_Crashes.Count);

            // Callstack customfield_11807
            string JiraCallstack = "{noformat}" + string.Join("\r\n", ToJiraFunctionCalls) + "{noformat}";
            fields.Add("customfield_11807", JiraCallstack);         // Callstack

            return fields;
        }

        public Dictionary<string, object> CreateOrionIssue(JiraConnection jc)
        {
            var fields = new Dictionary<string, object>();

            fields.Add("summary", "[CrashReport] " + ToJiraSummary); // Call Stack, Line 1
            fields.Add("description", string.Join("\r\n", ToJiraDescriptions)); // Description
            fields.Add("project", new Dictionary<string, object> { { "id", 10700 } });
            fields.Add("issuetype", new Dictionary<string, object> { { "id", "1" } }); // Bug
            fields.Add("components", new object[]{new Dictionary<string, object>{{"id", "14604"}}});

            return fields;
        } 

        /// <summary> Returns concatenated string of fields from the specified JIRA list. </summary>
        public string GetFieldsFrom(List<object> jiraList, string fieldName)
        {
            var results = "";

            foreach (var obj in jiraList)
            {
                var dict = (Dictionary<string, object>)obj;
                try
                {
                    results += (string)dict[fieldName];
                    results += " ";
                }
                catch (System.Exception /*E*/ )
                {

                }
            }

            return results;
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