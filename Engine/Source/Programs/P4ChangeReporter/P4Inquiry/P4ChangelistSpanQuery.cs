/*
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using Perforce.P4;

namespace P4Inquiry
{
	/// <summary>
	/// Simple class designed to serve as a parameter for a query to the Perforce server that spans over a specified
	/// time frame; additionally allows for the depot to be filtered
	/// </summary>
	public class P4ChangelistSpanQuery : P4DepotFilterQuery
	{
		#region Member Variables

        /// <summary>
        /// The filter to use against the P4 repository
        /// </summary>
        private FileSpec mFileFilter;
		#endregion

		#region Properties
        /// <summary>
        /// The filter to use against the P4 repository
        /// </summary>
        public FileSpec FileFilter
        {
            get { return mFileFilter; }
        }
		#endregion

		#region Constructors
		/// <summary>
        /// Construct a P4ChangelistSpanQuery from the changelist numbers specified as parameters; the numbers are not required to be in order
        /// </summary>
		/// <param name="InQueryChangelistStart">The start or end changelist number for the query</param>
		/// <param name="InQueryChangelistEnd">The start or end changelist number for the query, whichever wasn't specified by the first parameter</param>
		/// <param name="InDepotFilter">Path to filter the depot by</param>
		/// <param name="InUserFilter">User to filter the depot by, can be empty string</param>
        public P4ChangelistSpanQuery(int InQueryChangelistStart, int InQueryChangelistEnd, String InDepotFilter, String InUserFilter="")
			: base(InDepotFilter, InUserFilter)
		{
			// Reorder the changelists increasing
            ReOrder(ref InQueryChangelistStart, ref InQueryChangelistEnd);

            ChangelistIdVersion StartVersionId = new ChangelistIdVersion(InQueryChangelistStart);
            ChangelistIdVersion EndVersionId = new ChangelistIdVersion(InQueryChangelistEnd);

            VersionRange Versions = new VersionRange(StartVersionId, EndVersionId);

            mFileFilter = new FileSpec(new DepotPath(InDepotFilter), null, null, Versions);
        }

        /// <summary>
        /// Construct a P4ChangelistSpanQuery from the dates specified as parameters; the dates are not required to be in order
        /// </summary>
        /// <param name="InQueryDateTimeStart">The start or end date for the query</param>
        /// <param name="InQueryDateTimeEnd">The start or end date for the query, whichever wasn't specified by the first parameter</param>
        /// <param name="InDepotFilter">Path to filter the depot by</param>
        /// <param name="InUserFilter">User to filter the depot by, can be empty string</param>
        public P4ChangelistSpanQuery(DateTime InQueryDateTimeStart, DateTime InQueryDateTimeEnd, String InDepotFilter, String InUserFilter = "")
            : base(InDepotFilter, InUserFilter)
        {
            // Reorder the changelists increasing
            ReOrder(ref InQueryDateTimeStart, ref InQueryDateTimeEnd);

            DateTimeVersion StartDateTime = new DateTimeVersion(InQueryDateTimeStart);
            DateTimeVersion EndDateTime = new DateTimeVersion(InQueryDateTimeEnd);

            VersionRange Versions = new VersionRange(StartDateTime, EndDateTime);

            mFileFilter = new FileSpec(new DepotPath(InDepotFilter), null, null, Versions);
        }

        /// <summary>
        /// Construct a P4ChangelistSpanQuery from the labels specified as parameters; the labels are not required to be in order
        /// </summary>
        /// <param name="InQueryLabelStart">The start or end date for the query</param>
        /// <param name="InQueryLabelEnd">The start or end date for the query, whichever wasn't specified by the first parameter</param>
        /// <param name="InDepotFilter">Path to filter the depot by</param>
        /// <param name="InUserFilter">User to filter the depot by, can be empty string</param>
        public P4ChangelistSpanQuery(Label InQueryLabelStart, Label InQueryLabelEnd, String InDepotFilter, String InUserFilter = "")
            : base(InDepotFilter, InUserFilter)
        {
            // Reorder the changelists increasing
            ReOrder(ref InQueryLabelStart, ref InQueryLabelEnd);

            LabelNameVersion StartLabel = new LabelNameVersion(InQueryLabelStart.Id);
            LabelNameVersion EndLabel = new LabelNameVersion(InQueryLabelEnd.Id);

            VersionRange Versions = new VersionRange(StartLabel, EndLabel);

            mFileFilter = new FileSpec(new DepotPath(InDepotFilter), null, null, Versions);
        }
        
        
        #endregion

		#region Helper Methods
        /// <summary>
        /// Reorder the passed in Changelist numbers, updating them to be in ascending order
        /// </summary>
        /// <param name="InQueryChangelist1">The start or end changelist number for the query</param>
        /// <param name="InQueryChangelist2">The start or end changelist number for the query, whichever wasn't specified by the first parameter</param>
        private void ReOrder(ref int InQueryChangelist1, ref int InQueryChangelist2)
        {
            if (InQueryChangelist2 < InQueryChangelist1)
            {
                InQueryChangelist1 = InQueryChangelist2 - InQueryChangelist1; //A=B-A, B=B
                InQueryChangelist2 -= InQueryChangelist1; //A=B-A, B=A
                InQueryChangelist1 += InQueryChangelist2; //A=B, B=A
            }
        }

        /// <summary>
        /// Reorder the passed in DateTime, updating them to be in ascending order
        /// </summary>
        /// <param name="InQueryDateTime1">The start or end DateTime for the query</param>
        /// <param name="InQueryDateTime2">The start or end DateTime for the query, whichever wasn't specified by the first parameter</param>
        private void ReOrder(ref DateTime InQueryDateTime1, ref DateTime InQueryDateTime2)
        {
            if (InQueryDateTime2 < InQueryDateTime1)
            {
                DateTime SavedDateTime = InQueryDateTime1;
                InQueryDateTime1 = InQueryDateTime2;
                InQueryDateTime2 = SavedDateTime;
            }
        }

        /// <summary>
        /// Reorder the passed in Labels, updating them to be in created ascending order
        /// </summary>
        /// <param name="InLabel1">The start or end Label for the query</param>
        /// <param name="InLabel2">The start or end Label for the query, whichever wasn't specified by the first parameter</param>
        private void ReOrder(ref Label InLabel1, ref Label InLabel2)
        {
            if (InLabel2.Update < InLabel1.Update)
            {
                Label SavedLabel = InLabel1;
                InLabel1 = InLabel2;
                InLabel2 = SavedLabel;
            }
        }
  
        #endregion
    }
}


