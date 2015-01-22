/*
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using Perforce.P4;

namespace P4Inquiry
{
	/// <summary>
	/// Class providing a light wrapper around the P4.NET API; designed to simplify querying for changelist
	/// and label information
	/// </summary>
	public class P4Inquirer
	{
		#region Enumerations
		/// <summary>
		/// Enumeration signifying connection status to the Perforce server
		/// </summary>
		public enum EP4ConnectionStatus
		{
			/// <summary>
			/// Connected properly to the server
			/// </summary>
			P4CS_Connected,

			/// <summary>
			/// Disconnected from the server
			/// </summary>
			P4CS_Disconnected,

			/// <summary>
			/// Encountered a log-in error during the last connection attempt
			/// </summary>
			P4CS_UserLoginError,

            /// <summary>
            /// Encountered a log-in error during the last connection attempt
            /// </summary>
            P4CS_UserPasswordLoginError,

            /// <summary>
            /// Encountered a log-in error during the last connection attempt
            /// </summary>
            P4CS_ServerLoginError,

            /// <summary>
			/// Encountered some other error during the last connection attempt
			/// </summary>
			P4CS_GeneralError
		};
		#endregion

		#region Member Variables
		/// <summary>
		/// P4Net.api connection to the Perforce server
		/// </summary>
        private Repository mP4Repository = null;

        /// <summary>
        /// P4 server name
        /// </summary>
        private string mServerName;

        /// <summary>
        /// P4 user name
        /// </summary>
        private string mUserName;

        /// <summary>
        /// P4 user password
        /// </summary>
        private string mUserPassword;

		/// <summary>
		/// Current status of the connection to the Perforce server
		/// </summary>
		private EP4ConnectionStatus ConnectionStatus = EP4ConnectionStatus.P4CS_Disconnected;
		#endregion

        #region Properties
        /// <summary>
        /// The server to login to
        /// </summary>
        public String ServerName
        {
            get { return mServerName; }
			// Only attempt to change the server if we're not already connected
            set { if (ConnectionStatus != EP4ConnectionStatus.P4CS_Connected) { mServerName = value; } }
        }

        /// <summary>
        /// User to login to server with
        /// </summary>
        public String UserName
        {
            get { return mUserName; }

            // Only attempt to change the user if we're not already connected
            set { if (ConnectionStatus != EP4ConnectionStatus.P4CS_Connected) { mUserName = value; } }
       }

        /// <summary>
        /// Password for the user to login to server with
        /// </summary>
        public String UserPassword
        {
            private get { return mUserPassword; }
			// Only attempt to change the password if we're not already connected
            set { if (ConnectionStatus != EP4ConnectionStatus.P4CS_Connected) { mUserPassword = value; } }
        }        
        #endregion


		#region Constructors
		/// <summary>
		/// Construct a P4Inquirer object
		/// </summary>
		/// <param name="InServer">Server name to connect to</param>
		/// <param name="InUser">User name to connect with</param>
		public P4Inquirer(String InServer, String InUser)
		{
            ServerName = InServer;
			UserName = InUser;
		}

		/// <summary>
		/// Construct a P4Inquirer object
		/// </summary>
		/// <param name="InServer">Server name to connect to</param>
		/// <param name="InUser">User name to connect with</param>
		/// <param name="InPassword">User password to connect with</param>
		public P4Inquirer(String InServer, String InUser, String InPassword)
			: this(InServer, InUser)
		{
			UserPassword = InPassword;
		}

		/// <summary>
		/// Construct a P4Inquirer object
		/// </summary>
		public P4Inquirer() {}
		#endregion

		#region Finalizer
		/// <summary>
		/// Finalizer; ensure the Perforce connection is terminated
		/// </summary>
		~P4Inquirer()
		{
			Disconnect();
		}
		#endregion

		#region Connection-Related Methods
		/// <summary>
		/// Attempt to connect to the Perforce server
		/// </summary>
		/// <returns>Connection status after the connection attempt</returns>
		public EP4ConnectionStatus Connect()
		{
			// Only attempt to connect if not already successfully connected
			if (ConnectionStatus != EP4ConnectionStatus.P4CS_Connected)
			{
				// Assume disconnected state
				ConnectionStatus = EP4ConnectionStatus.P4CS_Disconnected;

				// Attempt to connect to the server
				try
				{
                    var P4Server = new Server(new ServerAddress(mServerName));

                    mP4Repository = new Repository(P4Server);

                    Connection P4Connection = mP4Repository.Connection;


                    P4Connection.UserName = mUserName;

                    P4Connection.Client = new Client();

                    P4Connection.Connect(null);

                    //Check user connection error, invalid login
                    if (P4Connection.ErrorList!=null)
                    {
                        if (P4Connection.ErrorList[0].ErrorCode == 822483067)
                        {
                            Disconnect();
                            ConnectionStatus = EP4ConnectionStatus.P4CS_UserLoginError;
                        }
                        else
                        {
                            Disconnect();
                            ConnectionStatus = EP4ConnectionStatus.P4CS_GeneralError;
                        }
                    }
                    else
                    {
                        Credential LoginCredential = P4Connection.Login(mUserPassword, null, null);
                        if (P4Connection.ErrorList != null)
                        {
                            if (P4Connection.ErrorList[0].ErrorCode == 839195695)
                            {
                                Disconnect();
                                ConnectionStatus = EP4ConnectionStatus.P4CS_UserPasswordLoginError;
                            }
                            else
                            {
                                Disconnect();
                                ConnectionStatus = EP4ConnectionStatus.P4CS_GeneralError;
                            }
                        }
                        else
                        {
                            P4Connection.Credential = LoginCredential;
                            ConnectionStatus = EP4ConnectionStatus.P4CS_Connected;
                        }
                    }
                }
				// Disconnect and return an error status if an exception was thrown
                catch (P4Exception ex)
				{
                    if (ex.ErrorCode == 841354277)
                    {
                        //Could not connect to the server
                        Disconnect();
                        ConnectionStatus = EP4ConnectionStatus.P4CS_ServerLoginError;
                    }
                    else
                    {
                        //unknown error
                        Disconnect();
                        ConnectionStatus = EP4ConnectionStatus.P4CS_GeneralError;
                    }
                }
			}
			return ConnectionStatus;
		}

		/// <summary>
		/// Disconnect from the Perforce server
		/// </summary>
		public void Disconnect()
		{
			try
			{
                ConnectionStatus = EP4ConnectionStatus.P4CS_Disconnected;
                mP4Repository.Connection.Disconnect();
            }
			catch (NullReferenceException)
			{
				//Ignore null reference connection may not have been initialized when we are disposing.
			}
		}
		#endregion

		#region Queries
		/// <summary>
		/// Query the server for a single changelist specified by the query parameter
		/// </summary>
        /// <param name="ChangelistNumber">The changelist number to query for</param>
		/// <returns>P4Changelist object representing the queried changelist, if it exists; otherwise, P4Changelist.InvalidChangelist</returns>
        public Changelist QueryChangelist(int ChangelistNumber)
		{
            Changelist ChangelistRecordSet = null;

			// Can only query if currently connected to the server!
			if (ConnectionStatus == EP4ConnectionStatus.P4CS_Connected)
			{
				try
				{
                    //do not include file diffs in this operation.
                    Options options = new Options();
                    options.Add("-s","");

					// Attempt to run a describe query on the changelist specified by the InQuery
                    ChangelistRecordSet = mP4Repository.GetChangelist(ChangelistNumber, options);
    			}
                catch (P4Exception E)
				{
					Console.WriteLine("Error running Perforce command!\n{0}", E.Message);
				}
			}

			return ChangelistRecordSet;
		}

		/// <summary>
		/// Query the server for multiple changelists in a range specified by the query parameter
		/// </summary>
		/// <param name="InQuery">Changelist query representing a start and end changelist number to query the Perforce server for changelists between</param>
		/// <return>Returns a list of changelists</return>
        public IList<Changelist> QueryChangelists(P4ChangelistSpanQuery InQuery)
		{
            IList<Changelist> ChangelistRecordSet = null;

			// Only attempt to query if actually connected
			if (ConnectionStatus == EP4ConnectionStatus.P4CS_Connected)
			{
				try
				{
                    Options options = new Options();
                    
                    //Extended descriptions
                    options.Add("-l",null);

                    //Only submitted changelists
                    options.Add("-s", "submitted");

                    //Filter by user
                    if (!string.IsNullOrWhiteSpace(InQuery.FilterUser))
                    {
                        options.Add("-u", InQuery.FilterUser);
                    }

                    ChangelistRecordSet = mP4Repository.GetChangelists(options, InQuery.FileFilter);
				}
				catch (P4Exception E)
				{
					Console.WriteLine("Error running Perforce command!\n{0}", E.Message);
				}
			}
            return ChangelistRecordSet;
		}

		/// <summary>
		/// Query the server for a single label, as specified by the query parameter
		/// </summary>
		/// <param name="InQuery">String query containing the name of the relevant label to query for</param>
		/// <returns>Label object representing the queried label, if it exists; otherwise, null</returns>
		public Label QueryLabel(string InQuery)
		{
			// Assume an invalid label
			Label QueriedLabel = null;

			// Can only perform the query if already connected to the server
			if (ConnectionStatus == EP4ConnectionStatus.P4CS_Connected)
			{
				try
				{
					// Run a "labels" command to retrieve the first label that matches the query string. The "labels" command
					// returns a result based on pattern matching, so also have to do a strcmp to ensure the correct label is returned.
                    QueriedLabel = mP4Repository.GetLabel(InQuery);
				}
                catch (P4Exception E)
				{
					Console.WriteLine("Error running Perforce command!\n{0}", E.Message);
				}
			}
			return QueriedLabel;
		}
		#endregion
	}
}
