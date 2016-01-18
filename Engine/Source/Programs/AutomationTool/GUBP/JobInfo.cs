using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
    /// <summary>
    /// Contains some global attributes of the GUBP job. A GUBP job consists of various nodes that may be executed on many different machines at the same time.
    /// These attributes are constant throughout the job, for any node in the job.
    /// </summary>
    /// <remarks>
    /// This does not contain ALL the job attributes, just the ones that were added during incremental refactoring.
    /// 
    /// This class is immutable to ensure no side-effect code an tamper with it after the job starts.
    /// </remarks>
    public class JobInfo
    {
        /// <summary>
        /// BranchName for this job. P4Env.BuildRootP4 if P4Enabled. Otherwise, uses the -BranchName= parameter. Can be empty string.
        /// </summary>
        public string BranchName { get; private set; }

        /// <summary>
        /// The root name beneath <see cref="CommandUtils.RootBuildStorageDirectory"/> where all temp storage will placed for this job.
        /// </summary>
        public string RootNameForTempStorage { get; private set; }

        /// <summary>
        /// Used as the prefix for temp storage for the job.
        /// 
        /// If P4 is enabled, this is P4Env.BuildRootEscaped, ie the branch name safe to create a file or folder from. If P4 is not enabled, it is NoP4, 
        /// but we never write to the shared temp storage location in those cases.
        /// For testing, arbitrary values can be used.
        /// </summary>
        public string BranchNameForTempStorage { get; private set; }

        /// <summary>
        /// The changelist associated with this job (ie, the code CL that was sync'ed to run this job.
        /// The -CL parameter can be used to override the P4 environment for testing.
        /// If P4 is not enabled, this value will be 0.
        /// </summary>
        public int Changelist { get; private set; }

        /// <summary>
        /// If this is a preflight vuild, this is the changelist that contains the shelve being preflighted. 
        /// Otherwise, will be 0.
        /// </summary>
        public int PreflightShelveChangelist { get; private set; }

        /// <summary>
        /// If this is a preflight build, contains a unique ID to identify the preflight job.
        /// Used to disambiguate multiple preflight runs from the same changelist and shelve, since the
        /// code in the shelve can change from run to run.
        /// </summary>
        public int PreflightUID { get; private set; }

        /// <summary>
        /// Will be true if this job is a preflight run. Shortcut for checking if the shelve is > 0
        /// </summary>
        public bool IsPreflight { get { return PreflightShelveChangelist > 0; } }

        /// <summary>
        /// If this is a preflight run, returns a suffix used to describe the preflight shelve and UID. Used
        /// to uniquely identify a temp storage block for a particular preflight run.
        /// </summary>
        /// <returns></returns>
        public string GetPreflightSuffix()
        {
            return IsPreflight ? string.Format("-PF-{0}-{1}", PreflightShelveChangelist, PreflightUID) : "";
        }

        /// <summary>
        /// Quick method to create a new JobInfo using all the same attributes, but a different CL.
        /// 
        /// Used by some GUBP code to search for and use previous temp storage blocks when checking for node history.
        /// </summary>
        /// <param name="NewCL">The CL to use instead of the current Job's CL.</param>
        /// <returns>A new JobInfo instance with all the same attributes as the current instance, but a different CL.</returns>
        public JobInfo CreateWithNewChangelist(int NewCL)
        {
            return new JobInfo(BranchName, RootNameForTempStorage, BranchNameForTempStorage, NewCL, PreflightShelveChangelist, PreflightUID);
        }

        /// <summary>
        /// Public ctor. This is an immutable class, so all attributes must be set a construction time.
        /// </summary>
        /// <param name="BranchName">See similarly named attribute.</param>
        /// <param name="RootNameForTempStorage">See similarly named attribute.</param>
        /// <param name="BranchNameForTempStorage">See similarly named attribute.</param>
        /// <param name="Changelist">See similarly named attribute.</param>
        /// <param name="PreflightShelveChangelist">See similarly named attribute.</param>
        /// <param name="PreflightUID">See similarly named attribute.</param>
        public JobInfo(string BranchName, string BranchNameForTempStorage, string RootNameForTempStorage, int Changelist, int PreflightShelveChangelist, int PreflightUID)
        {
            this.BranchName = BranchName;
            this.BranchNameForTempStorage = BranchNameForTempStorage;
            this.RootNameForTempStorage = RootNameForTempStorage;
            this.Changelist = Changelist;
            this.PreflightShelveChangelist = PreflightShelveChangelist;
            this.PreflightUID = PreflightUID;
        }
    }
}
