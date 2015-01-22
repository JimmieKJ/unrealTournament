// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Windows.Forms;
using System.IO;
using System.Runtime.InteropServices;

namespace AutoReporter
{
	class OutputLogFile
	{
		private StreamWriter MyStreamWriter;
		private string FileName;

		public OutputLogFile(string filename)
		{
			try
			{
				MyStreamWriter = new StreamWriter(filename);
				this.FileName = filename;
			}
			catch(Exception)
			{
				MyStreamWriter = null;
			}
		}

		public void WriteLine(string line)
		{
			if(MyStreamWriter != null)
			{
				try
				{
					MyStreamWriter.WriteLine(line);
					MyStreamWriter.Flush();
				}
				catch(Exception)
				{

				}
			}
#if DEBUG
			Console.WriteLine(line);
#endif
		}

		public void Close()
		{
			if(MyStreamWriter != null)
			{
				MyStreamWriter.Close();
			}
		}
	};

	static class Program
	{
		private static EpicBalloon NotifyObject = null;
		private const int BalloonTimeInMs = 2000;

		/** 
		 * Code for calling SendMessage.
		 */
		#region Win32 glue

		// import the SendMessage function so we can send windows messages to the UnrealConsole
		[DllImport("user32", CharSet = CharSet.Auto)]
		private static extern int SendMessage(HandleRef hWnd, int msg, int wParam, int lParam);

		[DllImport("user32", CharSet = CharSet.Auto)]
		private static extern int RegisterWindowMessage(string lpString);

		// Constants from the Platform SDK.
		private const int HWND_BROADCAST = 0xffff;

		#endregion

		/**
		 * AutoReporter is a program for sending crash data to a web service.  
		 * First it opens a log file on the local machine.  Then it parses the crash dump.
		 * Next the ReportService is used to create a new report from data extracted from the crash dump.
		 * The log file and ini dump are uploaded to the server.  If all this succeeds, the user is prompted 
		 * to enter a description of the crash, which is then sent to the ReportService.
		 * Finally, if no errors occur the dumps and log are deleted.
		 * 
		 * 3 arguments expected: AutoReport Dump file name, Log file name and Ini dump file name
		 */
		[STAThread]
		static void Main (string[] args)
		{
			Application.EnableVisualStyles ();
			Application.SetCompatibleTextRenderingDefault (false);

			string logFileName;
			if (args.Length >= 2) {
				string logDirectory;
				int endOfLogPath = args [2].LastIndexOf ('/');
				logDirectory = args [2].Substring (0, endOfLogPath + 1);
				logFileName = logDirectory + "AutoReportLog.txt";
			} else {
				logFileName = "AutoReportLog.txt";
			}

			OutputLogFile LogFile = new OutputLogFile (logFileName);
			LogFile.WriteLine ("Log opened: " + logFileName);
			LogFile.WriteLine ("");
			LogFile.WriteLine ("Current Time = " + DateTime.Now.ToString ());
			LogFile.WriteLine ("");
			LogFile.WriteLine ("Arguments:");
			foreach (string arg in args) {
				LogFile.WriteLine (arg);
			}

			if (args.Length < 6) {
				LogFile.WriteLine ("Expected 5 arguments: AutoReport Dump file name, Log file name, Ini dump file name, Mini dump file name, Crash video file name and ProcessID");
				LogFile.Close ();
				return;
			}

			Int32 ProcessId = Convert.ToInt32 (args [0]);

			Thread.Sleep (1000);
			// Check for additional options
			bool bForceUnattended = false;
			bool bShowBalloon = false;
			for (int ExtraArgIndex = 6; ExtraArgIndex < args.Length; ++ExtraArgIndex) {
				string CurArgString = args [ExtraArgIndex];

				// -Unattended : forces unattended mode regardless of command line string
				if (CurArgString.Equals ("-Unattended", StringComparison.OrdinalIgnoreCase)) {
					bForceUnattended = true;
				}
				// -Balloon : displays a system tray notify icon (balloon) and forces unattended mode
				else if (CurArgString.Equals ("-Balloon", StringComparison.OrdinalIgnoreCase)) {
					// balloon not possible on Mac
					if(Environment.OSVersion.Platform != PlatformID.Unix)
						bShowBalloon = true;
					// Unattended mode is implied with -Balloon
					bForceUnattended = true;
				} else {
					LogFile.WriteLine (String.Format ("Unrecognized parameter: {0}", CurArgString));
					LogFile.Close ();
					return;
				}
			}

			ReportFile rFile = new ReportFile ();
			ReportFileData reportData = new ReportFileData ();
			LogFile.WriteLine ("Parsing report file: " + args [1]);
			if (!rFile.ParseReportFile (args [1], reportData, LogFile)) {
				LogFile.WriteLine ("Failed to parse report file: " + args [1]);
				LogFile.Close ();
				return;
			}

			string ProcessName = reportData.GameName;
			Int32 SpaceIndex = ProcessName.IndexOf (' ');
			if (SpaceIndex != -1) {
				// This is likely UE4 UDK... parse off the game name
				ProcessName = ProcessName.Substring (0, SpaceIndex);
			}

			bool bIsUnattended = reportData.CommandLine.Contains ("unattended") || bForceUnattended || Environment.OSVersion.Platform == PlatformID.Unix;
			string CrashDescription;
			string Summary = CrashDescription = bShowBalloon ? "Handled error" : "Unattended mode";

			if (bShowBalloon)
			{
				String MsgText = "An unexpected error has occurred but the application has recovered.  A report will be submitted to the QA database.";

				// If the error occurred in the editor then we'll remind them to save their changes
				if (reportData.EngineMode.Contains("Editor"))
				{
					MsgText += "  Remember to save your work often.";
				}

				// Notify the user of the error by launching the NotifyObject.  
				NotifyObject = new EpicBalloon();
				NotifyObject.Show(MsgText, BalloonTimeInMs);
			}

			if (!bIsUnattended) 
			{
				LogFile.WriteLine ("Attempting to create a new crash description form...");
				Form1 crashDescForm = new Form1 ();
				crashDescForm.summary = Summary;
				crashDescForm.crashDesc = CrashDescription;

				LogFile.WriteLine ("Running attended...");
				crashDescForm.SetCallStack (reportData.CallStack);

				LogFile.WriteLine ("Running the application...");
				Application.Run (crashDescForm);
				Summary = crashDescForm.summary;
				CrashDescription = crashDescForm.crashDesc;
			}

			LogFile.WriteLine ("Crash Summary = " + Summary);
			LogFile.WriteLine ("Crash Description = " + CrashDescription);

			LogFile.WriteLine ("Registering report service...");
			// Create an instance of AutoReportService handler to make a web request to the AutoReportService to create a Crash Report in the database. Once this is complete we'll upload the associated files.
			ReportService.RegisterReport reportService = new ReportService.RegisterReport ();

			Exception serviceException = new Exception ("");
			bool serviceError = false;

			LogFile.WriteLine ("Attempting to create a new crash...");
			int uniqueIndex = -1;
			
			// Make the web request
			try {
				uniqueIndex = reportService.CreateNewCrash (-1, reportData.ComputerName, reportData.UserName, reportData.GameName, reportData.PlatformName,
					reportData.LanguageExt, reportData.TimeOfCrash, reportData.BuildVer, reportData.ChangelistVer, reportData.CommandLine,
					reportData.BaseDir, reportData.CallStack, reportData.EngineMode);
			} catch (Exception e) {
				LogFile.WriteLine ("AutoReporter had an exception in accessing the reportService! --> " + e.ToString ());
				serviceException = e;
				serviceError = true;
				LogFile.WriteLine (e.Message);
			}

			LogFile.WriteLine ("");
			LogFile.WriteLine ("uniqueIndex = " + uniqueIndex.ToString ());
			LogFile.WriteLine ("");

			if (uniqueIndex == -1) {
				LogFile.WriteLine ("The service failed to create a new Crash!");
				serviceError = true;
				serviceException = new Exception ("The service failed to create a new Crash!");
			}

			LogFile.WriteLine ("Attempting to create a new UpdateReportFiles instance...");
			// Create handler to upload the files.
			UploadReportFiles reportUploader = new UploadReportFiles ();
			bool fileUploadSuccess = false;

			if (!serviceError) {
				LogFile.WriteLine ("Attempting to upload files...");
				try {
					fileUploadSuccess = reportUploader.UploadFiles (ProcessName, bShowBalloon, ProcessId, args [2], args [3], args [4], args [5], uniqueIndex, LogFile);
				} catch (NullReferenceException nre) {
					LogFile.WriteLine (nre.Source + " Caused Null Reference Exception");
					// Try again:
					try {
						LogFile.WriteLine ("Process Name: " + ProcessName);
						LogFile.WriteLine ("bShowBalloon: " + bShowBalloon);
						LogFile.WriteLine ("ProcessId: " + ProcessId);
						LogFile.WriteLine ("args[2]: " + args [2]);
						LogFile.WriteLine ("args[3]: " + args [3]);
						LogFile.WriteLine ("args[4]: " + args [4]);
						LogFile.WriteLine ("args[5] ; " + args [5]); 
						LogFile.WriteLine ("uniqueIndex " + uniqueIndex);
						LogFile.WriteLine ("LogFile " + LogFile);

						fileUploadSuccess = reportUploader.UploadFiles (ProcessName, bShowBalloon, ProcessId, args [2], args [3], args [4], args [5], uniqueIndex, LogFile);
					} catch (NullReferenceException nre2) {

						LogFile.WriteLine (nre2.Source + " Caused Null Reference Exception Two Times");
						serviceError = true;
					}
				} catch (Exception e) {
					LogFile.WriteLine ("AutoReporter had an exception uploading files! --> " + e.ToString ());
					serviceException = e;
					serviceError = true;
					

				}

				if (fileUploadSuccess) {
					LogFile.WriteLine ("Successfully uploaded files");
					LogFile.WriteLine ("Saved: " + args [5]);
				} else {
					LogFile.WriteLine ("Failed to upload some files!");
				}
			}

			//Update Crash with Summary and Description Info
			bool updateSuccess = false;
			string logDirectory2;
			int endOfLogPath2 = args [2].LastIndexOf ('/');
			logDirectory2 = args [2].Substring (0, endOfLogPath2 + 1);
			logFileName = logDirectory2 + "AutoReportLog.txt";

			LogFile.WriteLine ("Attempting to add the crash description...");
			try {
				updateSuccess = reportService.AddCrashDescription (uniqueIndex, CrashDescription, Summary);
			} catch (Exception e) {
				LogFile.WriteLine ("AutoReporter had an exception adding crash description! --> " + e.ToString ());
				serviceException = e;
				serviceError = true;
				updateSuccess = false;
				LogFile.WriteLine (e.Message);
			}

			if (uniqueIndex != -1) {
				LogFile.WriteLine ("Attempting to write the crash report URL...");

				#if DEBUG
				string strCrashURL = Properties.Settings.Default.CrashURL_Debug + uniqueIndex;
				#else
				string strCrashURL = Properties.Settings.Default.CrashURL + uniqueIndex;
				#endif
				LogFile.WriteLine ("CrashReport url = " + strCrashURL);
				LogFile.WriteLine ("");

				if (NotifyObject != null) {
					// Set up text to inform the user of the report submission
					String MsgText = "Your crash report was submitted, and the URL was copied to your clipboard.  Click this balloon to display the crash report.";

					// setup crash report URL ready for use by the notification 
					Clipboard.SetText (strCrashURL);
					m_CrashReportUrl = strCrashURL;

					// launch a notification that when clicked will link
					// to the crash report url. 
					NotifyObject.SetEvents(OnBalloonClicked);
					NotifyObject.Show(MsgText, BalloonTimeInMs);
				}

				string str = reportData.PlatformName.ToLower ();
				if (str == "pc" || str == "mac") {
					LogFile.WriteLine ("Attempting to send the crash report Id to the game log file...");
					// On PC, so try just writing to the log.
					string AppLogFileName = Path.Combine (reportData.BaseDir, "..", reportData.GameName + "Game", "Logs", reportData.GameName + ".log");
					if (!File.Exists (AppLogFileName)) {
						// It's also possible the log name is Engine.log (default game name from BaseEngine.ini)
						AppLogFileName = Path.Combine (reportData.BaseDir, "..", reportData.GameName + "Game", "Logs", "Engine.log");
						if (!File.Exists (AppLogFileName))
							// Default to hardcoded UE4.log
							AppLogFileName = Path.Combine (reportData.BaseDir, "..", reportData.GameName + "Game", "Logs", "UE4.log");
					}
					LogFile.WriteLine ("\n");
					LogFile.WriteLine ("Attempting to open log file: " + AppLogFileName);

					try {
						using (StreamWriter AppLogWriter = new StreamWriter(AppLogFileName, true)) {
							AppLogWriter.WriteLine ("");
							AppLogWriter.WriteLine ("CrashReport url = " + strCrashURL);
							AppLogWriter.WriteLine ("");
							AppLogWriter.Flush ();
						}
					} catch (System.Exception e) {
						LogFile.WriteLine ("AutoReporter had an exception creating a stream writer! --> " + e.ToString ());
					}
				}
			}

			if (updateSuccess) {
				LogFile.WriteLine ("Successfully added crash description");
			} else {
				LogFile.WriteLine ("Service failed to add crash description!");
				LogFile.Close ();
				return;
			}

			LogFile.WriteLine ("Closing the AutoReporter log file...");
			LogFile.Close ();

			try {
				//everything was successful, so clean up dump and log files on client
				System.IO.File.Delete (args [1]);
				System.IO.File.Delete (args [3]);
				//todo: need to handle partial failure cases (some files didn't upload, etc) to know if we should delete the log
				//System.IO.File.Delete(logFileName);
			} catch (Exception e) {
				string ExcStr = "AutoReporter had an exception deleting the temp files!\n" + e.ToString ();
				MessageBox.Show (ExcStr, "AutoReporter Status", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
			}

			// Make a copy of the log file w/ the CrashReport # in it...
			// This is to help track down the issue where empty log files are being uploaded to the crash reporter.
			string CopyFileName = logFileName.Replace (".txt", "_" + uniqueIndex + ".txt");
			System.IO.File.Copy (logFileName, CopyFileName);

			if (NotifyObject != null) {
				// spin until the NotifyObject has closed
				while (NotifyObject.IsOpen) {
					// sleep while the notify balloon is fading
					System.Threading.Thread.Sleep (1000);

					// force check the events.
					Application.DoEvents ();
				}
			}
		}

		/// <summary>
		/// Internal static variable used to hold the url of the crash report
		/// once it is generated.
		/// </summary>
		static private String m_CrashReportUrl = "";

		/// <summary>
		/// Event handler which launches the url contained in m_CrashReportURL 
		/// when called. Designed to be called when a balloon is clicked on.
		/// </summary>
		/// <param name="sender">Object which sent event</param>
		/// <param name="e">event arguments</param>
		static void OnBalloonClicked(object sender, EventArgs e)
		{
			// attempts to launch crash report website
			try
			{
				if (m_CrashReportUrl != null)
				{
					System.Diagnostics.Process.Start(m_CrashReportUrl);
				}
			}
			finally
			{
			}
		}

		/// <summary>
		/// EpicBalloon is a subtle yet interactive user notification method. 
		/// It should be used where the user does not need to be interrupted,
		/// but may require confirmation of a task completing.
		/// </summary>
		class EpicBalloon
		{
			/// <summary>
			/// Constructor: initializes values and sets up all required
			/// internal events.
			/// </summary>
			public EpicBalloon()
			{
				// initialize balloon
				balloon = new NotifyIcon();
				balloon.BalloonTipIcon = System.Windows.Forms.ToolTipIcon.Warning;
				
				// create a new icon, (balloons require tray icons to run).
				var iconResource = AutoReporter.Properties.Resources.BalloonTrayIcon;
				balloon.Icon = new System.Drawing.Icon(iconResource, iconResource.Size);

				// set default title and text
				balloon.BalloonTipTitle = "Unreal AutoReporter";
				balloon.Text = "";

				// set on closed to exposed events, these are expected to close the window
				balloon.BalloonTipClicked += new System.EventHandler(OnClosed);
				balloon.BalloonTipClosed += new System.EventHandler(OnClosed);

				// set open flag
				bOpen = false;
			}
			
			/// <summary>
			/// Displays the NotifyIcon, with the desired text for the desired time.
			/// </summary>
			/// <param name="Text">The text to show in the notification</param>
			/// <param name="TimeDisplayed">The time the notification is shown for. NOTE: Opperating system may override this.</param>
			public void Show(String Text, int TimeDisplayed)
			{
				// launch the notification
				balloon.BalloonTipText = Text;
				balloon.Visible = true;
				balloon.ShowBalloonTip(TimeDisplayed);

				// Set internal flag to show the notification is now open. 
				bOpen = true;
			}

			/// <summary>
			/// Allows for custom events to be set, all functions must
			/// for fill the EvtHandler delegate requirements.
			/// </summary>
			/// <param name="OnClick">Called when notification is clicked</param>
			/// <param name="OnFinish">Called when notification is closed (manually or through timeout)</param>
			public void SetEvents(EvtHandler OnClick = null, EvtHandler OnFinish = null)
			{
				if (OnClick != null)
				{
					balloon.BalloonTipClicked += new System.EventHandler(OnClick);
				}

				if (OnFinish != null)
				{
					balloon.BalloonTipClosed += new System.EventHandler(OnFinish);
				}
			}

			/// <summary>
			/// Used for internal event, sets a value allowing the notification to tell
			/// when it has been closed.
			/// </summary>
			/// <param name="sender">Object which sent event</param>
			/// <param name="e">event arguments</param>
			void OnClosed(object sender, EventArgs e)
			{
				bOpen = false;
			}

			/// <summary>
			/// Read Only property: Returns TRUE when the notification is being shown.
			/// </summary>
			public bool IsOpen { get { return bOpen; } }

			/// <summary>
			/// Internal flag, true when notification is open
			/// </summary>
			bool bOpen;

			/// <summary>
			/// .Net Notification icon.
			/// </summary>
			NotifyIcon balloon;

			/// <summary>
			/// Delegate users must for fill to use notification icons events.
			/// </summary>
			/// <param name="sender">Object which sent event</param>
			/// <param name="e">event arguments</param>
			public delegate void EvtHandler(object sender, EventArgs e);
		}
	}
}
