// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
		/// Unique name used to identify the build within the branch. Useful for creating unique folders for build artifacts.
		/// </summary>
		public string BuildName { get; private set; }

        /// <summary>
        /// Will be true if this job is a preflight run.
        /// </summary>
        public bool IsPreflight { get; private set; }

        /// <summary>
        /// Public ctor. This is an immutable class, so all attributes must be set a construction time.
        /// </summary>
        /// <param name="BranchName">See similarly named attribute.</param>
        /// <param name="RootNameForTempStorage">See similarly named attribute.</param>
        /// <param name="BranchNameForTempStorage">See similarly named attribute.</param>
        /// <param name="Changelist">See similarly named attribute.</param>
        /// <param name="PreflightShelveChangelist">See similarly named attribute.</param>
        /// <param name="PreflightUID">See similarly named attribute.</param>
        public JobInfo(string BuildName, bool IsPreflight)
        {
			this.BuildName = BuildName;
            this.IsPreflight = IsPreflight;
        }
    }
}
