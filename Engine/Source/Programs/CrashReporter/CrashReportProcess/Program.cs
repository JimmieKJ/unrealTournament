// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.ServiceProcess;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// The container for the Crash Report Processor application.
	/// </summary>
	static class CrashReportProcessProgram
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		static void Main()
		{
			CrashReporterProcessServicer CrashReporterProcess = new CrashReporterProcessServicer();
#if !DEBUG
			if( !Environment.UserInteractive )
			{
				// Launch the service as normal if we aren't in the debugger
				ServiceBase.Run( CrashReporterProcess );
			}
			else
#endif
			{
				// Call OnStart, wait for a console key press, call OnStop
				CrashReporterProcess.DebugRun();
			}
		}
	}
}
