/*
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */

using System;
using Perforce.P4;
using System.Xml.Serialization;

namespace P4ChangeParser
{
    /// <summary>
    /// Class providing a logical representation of the data contained within a changelist on Perforce
    /// </summary>
    public class SerializableChangelist
    {
		#region Member Variables
		/// <summary>
		/// Changelist number
		/// </summary>
		private int mId = int.MinValue;

		/// <summary>
		/// Changelist submission date, stored in UTC time
		/// </summary>
		private DateTime mModifiedDate = DateTime.MinValue;

		/// <summary>
		/// Client which submitted the changelist
		/// </summary>
		private String mClientId = String.Empty;

		/// <summary>
		/// User who submitted the changelist
		/// </summary>
		private String mOwnerName = String.Empty;

		/// <summary>
		/// Changelist description
		/// </summary>
		private String mDescription = String.Empty;
		#endregion

		#region Properties
		/// <summary>
		/// Changelist number
		/// </summary>
		public int Id
		{
			get { return mId; }
			set { mId = value; }
		}

		/// <summary>
		/// Changelist submission date, stored in UTC time
		/// </summary>
		public DateTime ModifiedDate
		{
			get { return mModifiedDate; }
			set { mModifiedDate = value; }
		}

		/// <summary>
		/// Client which submitted the changelist
		/// </summary>
		public String ClientId
		{
			get { return mClientId; }
			set { mClientId = value; }
		}

		/// <summary>
		/// User who submitted the changelist
		/// </summary>
		public String OwnerName
		{
			get { return mOwnerName; }
			set { mOwnerName = value; }
		}

		/// <summary>
		/// Changelist description; Ignored during Xml serialization
		/// </summary>
		[XmlIgnore()]
		public String Description
		{
			get { return mDescription; }
			set { mDescription = value; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a SerializableChangelist from a Changelist
		/// </summary>
		/// <param name="InChangelistRecord">P4Record containing changelist data, retrived from the P4.NET API</param>
		public SerializableChangelist(Changelist InChangelistRecord)
		{
            mId = InChangelistRecord.Id;
            mModifiedDate = InChangelistRecord.ModifiedDate;
            mClientId = InChangelistRecord.ClientId;
            mOwnerName = InChangelistRecord.OwnerName;
            mDescription = InChangelistRecord.Description;
		}

		/// <summary>
		/// Default constructor; necessary for Xml serializer to work, however shouldn't be used by others, so made private
		/// </summary>
        private SerializableChangelist()
		{
		}
		#endregion
	}
}