using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Web;
using Tools.CrashReporter.CrashReportCommon;
using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels
{
    /// <summary>
    /// 
    /// </summary>
    public partial class Crash
    {
        public CallStackContainer CallStackContainer { get; set; }

        /// <summary>Crash context for this crash.</summary>
        FGenericCrashContext _crashContext = null;

        bool _bUseFullMinidumpPath = false;

        private string SitePath = Properties.Settings.Default.SitePath;

        /// <summary>If available, will read CrashContext.runtime-xml.</summary>
        public void ReadCrashContextIfAvailable()
        {
            try
            {
                //\\epicgames.net\Root\Projects\Paragon\QA_CrashReports
                bool bHasCrashContext = HasCrashContextFile();
                if (bHasCrashContext)
                {
                    _crashContext = FGenericCrashContext.FromFile(SitePath + GetCrashContextUrl());
                    bool bTest = _crashContext != null && !string.IsNullOrEmpty(_crashContext.PrimaryCrashProperties.FullCrashDumpLocation);
                    if (bTest)
                    {
                        _bUseFullMinidumpPath = true;

                        //Some temporary code to redirect to the new file location for fulldumps for paragon.
                        //Consider removing this once fulldumps stop appearing in the old location.
                        if (_crashContext.PrimaryCrashProperties.FullCrashDumpLocation.ToLower()
                                .Contains("\\\\epicgames.net\\root\\dept\\gameqa\\paragon\\paragon_launchercrashdumps"))
                        {
                            //Files from old versions of the client may end up in the old location. Check for files there first.
                            if (!System.IO.Directory.Exists(_crashContext.PrimaryCrashProperties.FullCrashDumpLocation))
                            {
                                var suffix =
                                _crashContext.PrimaryCrashProperties.FullCrashDumpLocation.Substring("\\\\epicgames.net\\root\\dept\\gameqa\\paragon\\paragon_launchercrashdumps".Length);
                                _crashContext.PrimaryCrashProperties.FullCrashDumpLocation = String.Format("\\\\epicgames.net\\Root\\Projects\\Paragon\\QA_CrashReports{0}", suffix);

                                //If the file doesn't exist in the new location either then don't use the full minidump path.
                                _bUseFullMinidumpPath =
                                    System.IO.Directory.Exists(_crashContext.PrimaryCrashProperties.FullCrashDumpLocation);
                            }
                        }

                        //End of temporary code.

                        FLogger.Global.WriteEvent("ReadCrashContextIfAvailable " + _crashContext.PrimaryCrashProperties.FullCrashDumpLocation + " is " + _bUseFullMinidumpPath);
                    }
                }
            }
            catch (Exception Ex)
            {
                Debug.WriteLine("Exception in ReadCrashContextIfAvailable: " + Ex.ToString());
            }
        }

        /// <summary>Return true, if there is a crash context file associated with the crash</summary>
        public bool HasCrashContextFile()
        {
            var Path = SitePath + GetCrashContextUrl();
            return System.IO.File.Exists(Path);
        }

        /// <summary>Whether to use the full minidump.</summary>
        public bool UseFullMinidumpPath()
        {
            return _bUseFullMinidumpPath;
        }

        /// <summary>Return true, if there is a metadata file associated with the crash</summary>
        public bool HasMetaDataFile()
        {
            var Path = SitePath + GetMetaDataUrl();
            return System.IO.File.Exists(Path);
        }

        /// <summary>
        /// Retrieve the parsed callstack for the crash.
        /// </summary>
        /// <param name="CrashInstance">The crash we require the callstack for.</param>
        /// <returns>A class containing the parsed callstack.</returns>
        public CallStackContainer GetCallStack()
        {
            return new CallStackContainer(this);
        }

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
            var Path = UseFullMinidumpPath() ? _crashContext.PrimaryCrashProperties.FullCrashDumpLocation : WebPath;
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

        /// <summary>
        /// Return a display friendly version of the time of crash.
        /// </summary>
        /// <returns>A pair of strings representing the date and time of the crash.</returns>
        public string[] GetTimeOfCrash()
        {
            string[] Results = new string[2];
            if (TimeOfCrash.HasValue)
            {
                DateTime LocalTimeOfCrash = TimeOfCrash.Value.ToLocalTime();
                Results[0] = LocalTimeOfCrash.ToShortDateString();
                Results[1] = LocalTimeOfCrash.ToShortTimeString();
            }

            return Results;
        }

        /// <summary>
        /// Return lines of processed callstack entries.
        /// </summary>
        /// <param name="StartIndex">The starting entry of the callstack.</param>
        /// <param name="Count">The number of lines to return.</param>
        /// <returns>Lines of processed callstack entries.</returns>
        public List<CallStackEntry> GetCallStackEntries(int StartIndex, int Count)
        {
            using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer(this.GetType().ToString() + "(Count=" + Count + ")"))
            {
                IEnumerable<CallStackEntry> Results = new List<CallStackEntry>() { new CallStackEntry() };

                if (CallStackContainer != null && CallStackContainer.CallStackEntries != null)
                {
                    int MaxCount = Math.Min(Count, CallStackContainer.CallStackEntries.Count);
                    Results = CallStackContainer.CallStackEntries.Take(MaxCount);
                }

                return Results.ToList();
            }
        }

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

        /// <summary>Return true, if there is a minidump file associated with the crash</summary>
        public bool GetHasMiniDumpFile()
        {
            var Path = SitePath + GetMiniDumpUrl();
            return UseFullMinidumpPath() || System.IO.File.Exists(Path);
        }

        /// <summary>Return true, if there is a video file associated with the crash</summary>
        public bool GetHasVideoFile()
        {
            var Path = SitePath + GetVideoUrl();
            return System.IO.File.Exists(Path);
        }

        /// <summary>Return true, if there is a log file associated with the crash</summary>
        public bool GetHasLogFile()
        {
            var Path = SitePath + GetLogUrl();
            return System.IO.File.Exists(Path);
        }

        /// <summary>Return true, if there is a diagnostics file associated with the crash</summary>
        public bool GetHasDiagnosticsFile()
        {
            var Path = SitePath + GetDiagnosticsUrl();
            return System.IO.File.Exists(Path);
        }
    }
}