// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Runtime.CompilerServices;
using System.Diagnostics;

namespace AutomationTool
{
	#region LogUtils


	public class LogUtils
	{
		private static Stopwatch Timer;
		private static AutomationFileTraceListener FileLog;

		/// <summary>
		/// Initializes trace logging.
		/// </summary>
		/// <param name="CommandLine">Command line.</param>
		public static void InitLogging(string[] CommandLine)
		{
            // ensure UTF8Output flag is respected, since we are initializing logging early in the program.
            if (CommandLine.Any(Arg => Arg.Equals("-utf8output", StringComparison.InvariantCultureIgnoreCase)))
            {
                Console.OutputEncoding = new System.Text.UTF8Encoding(false, false);
            }

			Timer = (CommandUtils.ParseParam(CommandLine, "-Timestamps"))? Stopwatch.StartNew() : null;
			var VerbosityLevel = CommandUtils.ParseParam(CommandLine, "-Verbose") ? TraceEventType.Verbose : TraceEventType.Information;
			var Filter = new VerbosityFilter(VerbosityLevel);
			Trace.Listeners.Add(new AutomationConsoleTraceListener());
			FileLog = new AutomationFileTraceListener();
			Trace.Listeners.Add(FileLog);
			Trace.Listeners.Add(new AutomationMemoryLogListener());
			foreach (TraceListener Listener in Trace.Listeners)
			{
				Listener.Filter = Filter;
			}
		}

		/// <summary>
		/// Closes logging system
		/// </summary>
		public static void CloseFileLogging()
		{
			if (FileLog != null)
			{
				Trace.Listeners.Remove(FileLog);
				FileLog.Close();
				FileLog.Dispose();
				FileLog = null;
			}
		}

		/// <summary>
		/// Formats message for logging.
		/// </summary>
		/// <param name="Source">Source of the message (usually "Class.Method")</param>
		/// <param name="Verbosity">Message verbosity level</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Format arguments</param>
		/// <returns>Formatted message</returns>
		public static string FormatMessage(string Source, TraceEventType Verbosity, string Format, params object[] Args)
		{
			return FormatMessage(Source, Verbosity, String.Format(Format, Args));
		}

		/// <summary>
		/// Formats message for logging.
		/// </summary>
		/// <param name="Source">Source of the message (usually Class.Method)</param>
		/// <param name="Verbosity">Message verbosity level</param>
		/// <param name="Message">Message text</param>
		/// <returns>Formatted message</returns>
		public static string FormatMessage(string Source, TraceEventType Verbosity, string Message)
		{
			string FormattedMessage = "";
			if(Timer != null)
			{
				FormattedMessage += String.Format("[{0:hh\\:mm\\:ss\\.fff}] ", Timer.Elapsed);
			}
			FormattedMessage += Source + ": ";
			switch (Verbosity)
			{
				case TraceEventType.Error:
					FormattedMessage += "ERROR: ";
					break;
				case TraceEventType.Warning:
					FormattedMessage += "WARNING: ";
					break;
			}
			FormattedMessage += Message;
			return FormattedMessage;
		}

		/// <summary>
		/// Dumps exception info to log.
		/// </summary>
		/// <param name="Verbosity">Verbosity</param>
		/// <param name="Ex">Exception</param>
		public static string FormatException(Exception Ex)
		{
			var Message = String.Format("Exception in {0}: {1}{2}Stacktrace: {3}", Ex.Source, Ex.Message, Environment.NewLine, Ex.StackTrace);
			if (Ex.InnerException != null)
			{
				Message += String.Format("InnerException in {0}: {1}{2}Stacktrace: {3}", Ex.InnerException.Source, Ex.InnerException.Message, Environment.NewLine, Ex.InnerException.StackTrace);
			}
			return Message;
		}

		/// <summary>
		/// Returns a unique logfile name.
		/// </summary>
		/// <param name="Base">Base name for the logfile</param>
		/// <returns>Unique logfile name.</returns>
		public static string GetUniqueLogName(string Base)
		{
			const int MaxAttempts = 1000;

			string LogFilename = string.Empty;

			int Attempt = 0;
			do
			{
				if (Attempt == 0)
				{
					LogFilename = String.Format("{0}.txt", Base);
				}
				else
				{
					LogFilename = String.Format("{0}.{1}.txt", Base, Attempt);
				}
			} while (File.Exists(LogFilename) && ++Attempt < MaxAttempts);

			if (File.Exists(LogFilename))
			{
				throw new AutomationException(String.Format("Failed to create logfile {0}.", LogFilename));
			}
			return LogFilename;
		}

		public static string GetLogTail(string Filename = null, int NumLines = 250)
		{
			List<string> Lines;
			if (Filename == null)
			{
				Lines = new List<string>(AutomationMemoryLogListener.GetAccumulatedLog().Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries));
			}
			else
			{
				Lines = new List<string>(CommandUtils.ReadAllLines(Filename));
			}

			if (Lines.Count > NumLines)
			{
				Lines.RemoveRange(0, Lines.Count - NumLines);
			}
			string Result = "";
			foreach (var Line in Lines)
			{
				Result += Line + Environment.NewLine;
			}
			return Result;
		}


	}

	#endregion


	#region VerbosityFilter

	/// <summary>
	/// Trace verbosity filter.
	/// </summary>
	class VerbosityFilter : TraceFilter
	{
		private TraceEventType VerbosityLevel = TraceEventType.Information;
		public VerbosityFilter(TraceEventType Level)
		{
			VerbosityLevel = Level;
		}

		public override bool ShouldTrace(TraceEventCache Cache, string Source, TraceEventType EventType, int Id, string FormatOrMessage, object[] Args, object Data1, object[] Data)
		{
			return EventType <= VerbosityLevel;
		}
	}

	#endregion

	#region AutomationConsoleTraceListener

	/// <summary>
	/// Trace console listener.
	/// </summary>
	class AutomationConsoleTraceListener : TraceListener
	{
		public AutomationConsoleTraceListener()
		{
			// We use the command line directly here, because GlobalCommandLine isn't initialized early enough
			if (SharedUtils.ParseCommandLine().Any(Arg => Arg.ToLower() == "-utf8output"))
			{
				Console.OutputEncoding = new System.Text.UTF8Encoding(false, false);
			}
		}

		/// <summary>
		/// Writes a formatted line to the console.
		/// </summary>
		/// <param name="Source">Message source</param>
		/// <param name="Verbosity">Message verbosity</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Format arguments</param>
		private void WriteLine(string Source, TraceEventType Verbosity, string Format, params object[] Args)
		{
			string Message = LogUtils.FormatMessage(Source, Verbosity, Format, Args);
			if (Filter == null || Filter.ShouldTrace(null, Source, Verbosity, 0, Format, Args, null, null))
			{
				ConsoleColor DefaultColor = Console.ForegroundColor;
				switch (Verbosity)
				{
					case TraceEventType.Critical:
					case TraceEventType.Error:
						Console.ForegroundColor = ConsoleColor.Red;
						break;
					case TraceEventType.Warning:
						Console.ForegroundColor = ConsoleColor.Yellow;
						break;
				}
				Console.WriteLine(Message);
				Console.ForegroundColor = DefaultColor;
			}
		}

		#region TraceListener Interface

		public override void Write(string message)
		{
			WriteLine(String.Empty, TraceEventType.Information, message);
		}

		public override void WriteLine(string message)
		{
			WriteLine(String.Empty, TraceEventType.Information, message);
		}

		public override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string message)
		{
			WriteLine(source, eventType, "{0}", message);
		}

		public override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string format, params object[] args)
		{
			WriteLine(source, eventType, format, args);
		}

		#endregion
	}

	#endregion

	#region AutomationMemoryLogListener

	/// <summary>
	/// Trace console listener.
	/// </summary>
	class AutomationMemoryLogListener : TraceListener
	{
		private static StringBuilder AccumulatedLog = new StringBuilder(1024 * 1024 * 20);
		private static object SyncObject = new object();

		/// <summary>
		/// Writes a formatted line to the console.
		/// </summary>
		/// <param name="Source">Message source</param>
		/// <param name="Verbosity">Message verbosity</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Format arguments</param>
		private void WriteLine(string Source, TraceEventType Verbosity, string Format, params object[] Args)
		{
			lock (SyncObject)
			{
				string Message = LogUtils.FormatMessage(Source, Verbosity, Format, Args);
				if (Filter == null || Filter.ShouldTrace(null, Source, Verbosity, 0, Format, Args, null, null))
				{
					AccumulatedLog.AppendLine(Message);
				}
			}
		}

		public static string GetAccumulatedLog()
		{
			lock (SyncObject)
			{
				return AccumulatedLog.ToString();
			}
		}

		#region TraceListener Interface

		public override void Write(string message)
		{
			WriteLine(String.Empty, TraceEventType.Information, message);
		}

		public override void WriteLine(string message)
		{
			WriteLine(String.Empty, TraceEventType.Information, message);
		}

		public override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string message)
		{
			WriteLine(source, eventType, "{0}", message);
		}

		public override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string format, params object[] args)
		{
			WriteLine(source, eventType, format, args);
		}

		#endregion
	}

	#endregion


	#region AutomationFileTraceListener

	/// <summary>
	/// Log file trace listener.
	/// </summary>
	class AutomationFileTraceListener : TraceListener
	{
		private string LogFilename;
		private StreamWriter LogFile;
		private static object SyncObject = new object();

		public AutomationFileTraceListener()
		{
			const int MaxAttempts = 10;
			int Attempt = 0;
			var TempLogFolder = Path.GetTempPath();
			do
			{
				if (Attempt == 0)
				{
					LogFilename = CommandUtils.CombinePaths(TempLogFolder, "Log.txt");
				}
				else
				{
					LogFilename = CommandUtils.CombinePaths(TempLogFolder, String.Format("Log_{0}.txt", Attempt));
				}
				try
				{
					FileStream File = new FileStream(LogFilename, FileMode.Create, FileAccess.ReadWrite, FileShare.Read);
					LogFile = new StreamWriter(File);
					LogFile.AutoFlush = true;
				}
				catch (Exception Ex)
				{
					if (Attempt == (MaxAttempts - 1))
					{
						UnrealBuildTool.Log.TraceWarning("Unable to create log file: {0}", LogFilename);
						UnrealBuildTool.Log.TraceWarning(LogUtils.FormatException(Ex));
					}
				}
			} while (LogFile == null && ++Attempt < MaxAttempts);
		}

		private void WriteToFile(string Message)
		{
			lock (SyncObject)
			{
				if (LogFile != null)
				{
					LogFile.WriteLine(Message);
				}
			}
		}

		/// <summary>
		/// Writes a formatted line to the log.
		/// </summary>
		/// <param name="SkipStackFrames"></param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public void WriteLine(string Source, TraceEventType Verbosity, string Format, params object[] Args)
		{
			var Message = LogUtils.FormatMessage(Source, Verbosity, Format, Args);
			WriteToFile(Message);
		}

		/// <summary>
		/// Writes a formatted line to the log.
		/// </summary>
		/// <param name="SkipStackFrames"></param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public void WriteLine(string Source, TraceEventType Verbosity, string Message)
		{
			Message = LogUtils.FormatMessage(Source, Verbosity, Message);
			WriteToFile(Message);
		}

		#region TraceListener Interface

		public override void Flush()
		{
			lock (SyncObject)
			{
				if (LogFile != null)
				{
					LogFile.Flush();
				}
			}
			base.Flush();
		}

		public override void Close()
		{
			lock (SyncObject)
			{
				if (LogFile != null)
				{
					LogFile.Flush();
					LogFile.Close();
					LogFile.Dispose();
					LogFile = null;
				}
			}
			try
			{
				// Try to copy the log file to the log folder. The reason why it's done here is that
				// at the time the log file is being initialized the env var may not yet be set (this 
				// applies to local log folder in particular)
				var LogFolder = Environment.GetEnvironmentVariable(EnvVarNames.LogFolder);
				if (!String.IsNullOrEmpty(LogFolder) && Directory.Exists(LogFolder) &&
						!String.IsNullOrEmpty(LogFilename) && File.Exists(LogFilename))
				{
					var DestFilename = CommandUtils.CombinePaths(LogFolder, "UAT_" + Path.GetFileName(LogFilename));
					SafeCopyLogFile(LogFilename, DestFilename);
				}
			}
			catch (Exception)
			{
				// Silently ignore, logging is pointless because eveything is shut down at this point
			}
			base.Close();
		}

		/// <summary>
		/// Copies log file to the final log folder, does multiple attempts if the destination file could not be created.
		/// </summary>
		/// <param name="SourceFilename"></param>
		/// <param name="DestFilename"></param>
		private void SafeCopyLogFile(string SourceFilename, string DestFilename)
		{
			const int MaxAttempts = 10;
			int AttemptNo = 0;
			var DestLogFilename = DestFilename;
			bool Result = false;
			do
			{				
				try
				{
					File.Copy(SourceFilename, DestLogFilename, true);
					Result = true;
				}
				catch (Exception)
				{
					var ModifiedFilename = String.Format("{0}_{1}{2}", Path.GetFileNameWithoutExtension(DestFilename), AttemptNo, Path.GetExtension(DestLogFilename));
					DestLogFilename = CommandUtils.CombinePaths(Path.GetDirectoryName(DestFilename), ModifiedFilename);
					AttemptNo++;
				}
			}
			while (Result == false && AttemptNo <= MaxAttempts);
		}

		public override void Write(string message)
		{
			WriteLine(String.Empty, TraceEventType.Information, message);
		}

		public override void WriteLine(string message)
		{
			WriteLine(String.Empty, TraceEventType.Information, message);
		}

		public override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string message)
		{
			WriteLine(source, TraceEventType.Verbose, message);
		}

		public override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string format, params object[] args)
		{
			WriteLine(source, TraceEventType.Verbose, format, args);
		}

		#endregion
	}

	#endregion
}
