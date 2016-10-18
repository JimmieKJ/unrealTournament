// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Threading;

namespace Tools.CrashReporter.CrashReportCommon
{
	/// <summary>
	/// Thread-safe log creation and writing for crash report processes
	/// </summary>
	public class LogWriter
	{
		/// <summary>
		/// Constructor: opens the log file
		/// </summary>
		/// <param name="AppNameParam">Application name to prepend to log filename</param>
		/// <param name="LogFolderParam">Folder in which to store log file</param>
		public LogWriter(string AppNameParam, string LogFolderParam)
		{
			// Store the arguments for repeated log file creation
			AppName = AppNameParam;
			LogFolder = LogFolderParam;

			// Ensure the log folder exists
			DirectoryInfo DirInfo = new DirectoryInfo(LogFolder);
			if(!DirInfo.Exists)
			{
				DirInfo.Create();
			}

			CreateNewLogFile();
		}

		/// <summary>
		/// Shutdown: flush and close the log file
		/// </summary>
		public void Dispose()
		{
			lock( Sync )
			{
				LogFile.Flush();
				LogFile.Close();
				LogFile = null;
			}
		}

		/// <summary>
		/// Write a line to the log file and to the console
		/// </summary>
		/// <param name="Line">Text to write</param>
		public void Print(string Line)
		{
			if (Line != null)
			{
				string FullLine = DateTime.UtcNow.ToString( "yyyy/MM/dd HH:mm:ss" ) + "UTC [" + Thread.CurrentThread.ManagedThreadId.ToString( "D3" ) + "] : " + Line;
				lock( Sync )
				{
					LogFile.WriteLine( FullLine );
					LogFile.Flush();
				}

				// Relay to the console if it's available
				Console.WriteLine( FullLine );

				if( System.Diagnostics.Debugger.IsAttached )
				{
					Debug.WriteLine( FullLine );
				}
			}
		}

		/// <summary>
		/// Clean out any old log files in the same directory as this log
		/// </summary>
		/// <param name="OlderThanDays">Files older than this many days will be deleted</param>
		public void CleanOutOldLogs(
			int OlderThanDays
			)
		{
			DirectoryInfo DirInfo = new DirectoryInfo(LogFolder);
			foreach(FileInfo LogFileInfo in DirInfo.GetFiles())
			{
				if (LogFileInfo.LastWriteTimeUtc.AddDays(OlderThanDays) < DateTime.UtcNow)
				{
					LogFileInfo.Delete();
				}
			}
		}

		/// <summary>
		/// Create either an initial log file or switch to a new, time-stamped log file
		/// </summary>
		public void CreateNewLogFile()
		{
			lock( Sync )
			{
				var OldLogFile = LogFile;

				var LogFileName = Path.Combine( LogFolder, AppName + "-" + DateTime.UtcNow.ToString( "yyyy-MM-dd" ) + ".txt" );

				// Create a threadsafe text writer
				LogFile = new StreamWriter( LogFileName, true, Encoding.UTF8 );

				Print( AppName + ", Copyright 2014-2016 Epic Games, Inc." );

				// Don't close the previous log file until the new one is up and running
				if( OldLogFile != null )
				{
					OldLogFile.Flush();
					OldLogFile.Close();
				}
			}
		}	

		/// <summary>Open file stream messages are written to</summary>
		volatile TextWriter LogFile;

		/// <summary>Application name to pre-pend to log filenames and put in copyright notice</summary>
		string AppName;

		/// <summary>Folder log has been created in</summary>
		string LogFolder;

		object Sync = new object();
	}
}
