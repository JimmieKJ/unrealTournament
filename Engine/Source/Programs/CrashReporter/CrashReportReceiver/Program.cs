// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.ServiceProcess;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportReceiver
{
	/// <summary>
	/// The container for the Crash Report Receiver application.
	/// </summary>
	static class CrashReportReceiverProgram
	{
		/// <summary>
		/// The main entry point for crash report receiver application.
		/// </summary>
		static void Main()
		{
			CrashReporterReceiverServicer CrashReporterReceiver = new CrashReporterReceiverServicer();
			if( !Environment.UserInteractive )
			{
				// Launch the service as normal if we aren't in the debugger, or aren't in an interactive environment
				ServiceBase.Run( CrashReporterReceiver );
			}
			else
			{
				// Call OnStart, wait for a console key press, call OnStop
				CrashReporterReceiver.DebugRun();
			}
		}
	}
}
