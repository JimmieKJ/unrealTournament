// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;

namespace Tools.DotNETCommon.LaunchProcess
{
	/// <summary>
	/// Enum to make the meaning of WaitForExit return code clear
	/// </summary>
	public enum EWaitResult
	{
		/// <summary>The task completed in a timely manner</summary>
		Ok,

		/// <summary>The task was abandoned, since it was taking longer than the specified time-out duration</summary>
		TimedOut
	}

	/// <summary>
	/// A class to handle spawning and monitoring of a child process
	/// </summary>
	public class LaunchProcess : IDisposable
	{
		/// <summary>A callback signature for handling logging.</summary>
		public delegate void CaptureMessageDelegate( string Message );
		/// <summary>The current callback for logging, or null for quiet operation.</summary>
		CaptureMessageDelegate CaptureMessage = null;

		/// <summary>The process that was launched.</summary>
		private Process LaunchedProcess = null;
		/// <summary>Set to true when the launched process finishes.</summary>
		private bool bIsFinished = false;

		/// <summary>
		/// Implementing Dispose.
		/// </summary>
		public void Dispose()
		{
			Dispose( true );
			GC.SuppressFinalize( this );
		}

		/// <summary>
		/// Disposes the resources.
		/// </summary>
		/// <param name="Disposing"></param>
		protected virtual void Dispose( bool Disposing )
		{
			LaunchedProcess.Dispose();
		}

		/// <summary>
		/// The process exit event.
		/// </summary>
		/// <param name="Sender">Unused.</param>
		/// <param name="Args">Unused.</param>
		private void ProcessExit( object Sender, EventArgs Args )
		{
			// Flush and close any pending messages
			if( CaptureMessage != null )
			{
				LaunchedProcess.CancelOutputRead();
				LaunchedProcess.CancelErrorRead();

				// Protect against multiple calls
				CaptureMessage = null;
			}

			LaunchedProcess.EnableRaisingEvents = false;
			bIsFinished = true;
		}

		/// <summary>
		/// Safely invoke the logging callback.
		/// </summary>
		/// <param name="Message">The line of text to pass back to the calling process via the delegate.</param>
		private void PrintLog( string Message )
		{
			if( CaptureMessage != null )
			{
				CaptureMessage( Message );
			}
		}

		/// <summary>
		/// The event called for StdOut an StdErr redirections.
		/// </summary>
		/// <param name="Sender">Unused.</param>
		/// <param name="Args">The container for the line of text to pass through the system.</param>
		private void CaptureMessageCallback( object Sender, DataReceivedEventArgs Args )
		{
			PrintLog( Args.Data );
		}

		/// <summary>
		/// Check to see if the currently running child process has finished.
		/// </summary>
		/// <returns>true if the process was successfully spawned and correctly exited. It also returns true if the process failed to spawn.</returns>
		public bool IsFinished()
		{
			if( LaunchedProcess != null )
			{
				if( bIsFinished )
				{
					return true;
				}

				// Check for timed out
				return false;
			}

			return true;
		}

		/// <summary>
		/// Wait for the launched process to exit, and return its exit code.
		/// </summary>
		/// <param name="Timeout">Number of milliseconds to wait for the process to exit. Default is forever.</param>
		/// <returns>The exit code of the launched process.</returns>
		/// <remarks>false is returned if the process failed to finish before the requested timeout.</remarks>
		public EWaitResult WaitForExit( int Timeout = Int32.MaxValue )
		{
			if( LaunchedProcess != null )
			{
				LaunchedProcess.WaitForExit( Timeout );
				if( !bIsFinished )
				{
					// Calling Kill() here seems to be a race condition on the process terminating after WaitForExit (TTP#315685). Catch anything it throws, and make sure the Kill() finishes.
					try
					{
						LaunchedProcess.Kill();
						LaunchedProcess.WaitForExit();
					}
					catch (Exception)
					{
					}
				}
			}

			return bIsFinished ? EWaitResult.Ok : EWaitResult.TimedOut;
		}

		/// <summary>
		/// Construct a class wrapper that spawns a new child process with StdOut and StdErr optionally redirected and captured.
		/// </summary>
		/// <param name="Executable">The executable to launch.</param>
		/// <param name="WorkingDirectory">The working directory of the process. If this is null, the current directory is used.</param>
		/// <param name="InCaptureMessage">The log callback function. This can be null for no logging.</param>
		/// <param name="Parameters">A string array of parameters passed on the command line, and delimited with spaces.</param>
		/// <remarks>Any errors are passed back through the capture delegate. The existence of the executable and the working directory are checked before spawning is attempted.
		/// An exit code of -1 is returned if there was an exception when spawning the process.</remarks>
		public LaunchProcess( string Executable, string WorkingDirectory, CaptureMessageDelegate InCaptureMessage, params string[] Parameters )
		{
			CaptureMessage = InCaptureMessage;

			// Simple check to ensure the executable exists
			FileInfo Info = new FileInfo( Executable );
			if( !Info.Exists )
			{
				PrintLog( "ERROR: Executable does not exist: " + Executable );
				bIsFinished = true;
				return;
			}

			// Set the default working directory if necessary
			if( WorkingDirectory == null )
			{
				WorkingDirectory = Environment.CurrentDirectory;
			}

			// Simple check to ensure the working directory exists
			DirectoryInfo DirInfo = new DirectoryInfo( WorkingDirectory );
			if( !DirInfo.Exists )
			{
				PrintLog( "ERROR: Working directory does not exist: " + WorkingDirectory );
				bIsFinished = true;
				return;
			}

			// Create a new process to launch
			LaunchedProcess = new Process();

			// Prepare a ProcessStart structure 
			LaunchedProcess.StartInfo.FileName = Info.FullName;
			LaunchedProcess.StartInfo.Arguments = String.Join( " ", Parameters );
			LaunchedProcess.StartInfo.WorkingDirectory = DirInfo.FullName;
			LaunchedProcess.StartInfo.CreateNoWindow = true;

			// Need this for the Exited event as well as the output capturing
			LaunchedProcess.EnableRaisingEvents = true;
			LaunchedProcess.Exited += new EventHandler(ProcessExit);

			// Redirect the output.
			if (CaptureMessage != null)
			{
				LaunchedProcess.StartInfo.UseShellExecute = false;
				LaunchedProcess.StartInfo.RedirectStandardOutput = true;
				LaunchedProcess.StartInfo.RedirectStandardError = true;

				LaunchedProcess.OutputDataReceived += new DataReceivedEventHandler(CaptureMessageCallback);
				LaunchedProcess.ErrorDataReceived += new DataReceivedEventHandler(CaptureMessageCallback);
			}

			// Spawn the process - try to start the process, handling thrown exceptions as a failure.
			try
			{
				PrintLog( "Launching: " + LaunchedProcess.StartInfo.FileName + " " + LaunchedProcess.StartInfo.Arguments + " (CWD: " + LaunchedProcess.StartInfo.WorkingDirectory + ")" );

				LaunchedProcess.Start();

				// Start the output redirection if we have a logging callback
				if( CaptureMessage != null )
				{
					LaunchedProcess.BeginOutputReadLine();
					LaunchedProcess.BeginErrorReadLine();
				}
			}
			catch( Exception Ex )
			{
				// Clean up should there be any exception
				LaunchedProcess = null;
				bIsFinished = true;
				PrintLog( "ERROR: Failed to launch with exception: " + Ex.Message );
			}
		}
	}
}
