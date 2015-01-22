// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

using Tools.DotNETCommon.SimpleWebRequest;

namespace Tools.CrashReporter.RegisterPII
{
	/// <summary>
	/// The container for the RegisterPII application.
	/// </summary>
	static class RegisterPIIProgram
	{
		/// <summary>
		/// A simple program to map the anonymous machine guid used in Windows Error Reporting to a real user and machine name.
		/// </summary>
		/// <param name="Arguments">Command line arguments.</param>
		/// <remarks>This is for INTERNAL use only.
		/// The data is passed via a web service call to the Crash Reporter web site.
		/// '/silent' can be passed on the command line to suppress the dialog that pops up.</remarks>
		[STAThread]
		static private void Main( string[] Arguments )
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault( false );

			// Create a new instance of the window (which may not be shown)
			RegisterPII Instance = new RegisterPII();
			
			// Check to see if we should hide the window
			bool bShowWindow = true;
			if( Arguments.Length > 0 )
			{
				if( Arguments[0].ToLower() == "/silent" )
				{
					bShowWindow = false;
				}
			}
			
			// Display the window if it isn't hidden
			if( bShowWindow )
			{
				// Show the dialog
				Instance.Show();
				Application.DoEvents();
			}

			// Send the info to the server
			string WebRequest = "http://" + Properties.Settings.Default.CrashReportWebSite + ":80/Crashes/RegisterPII/" + Environment.UserName + "/" + Environment.MachineName + "/" + Instance.MachineIDString;
			SimpleWebRequest.GetWebServiceResponse( WebRequest, null );

			// Display the window for 5 seconds
			DateTime StartTime = DateTime.UtcNow;
			while( DateTime.UtcNow < StartTime.AddSeconds( 5 ) )
			{
				Application.DoEvents();
				Thread.Sleep( 50 );
			}

			Instance.Close();
		}
	}
}
