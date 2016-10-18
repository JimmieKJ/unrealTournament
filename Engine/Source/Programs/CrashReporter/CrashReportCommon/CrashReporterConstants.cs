// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Xml.Serialization;

namespace Tools.CrashReporter.CrashReportCommon
{
	/// <summary>
	/// Any constant variables that need sharing between client and server.
	/// </summary>
	public class CrashReporterConstants
	{
		/// <summary>Completely arbitrary maximum file size to upload (basic exploit check). Currently 100MB.</summary>
		public const long MaxFileSizeToUpload = 100 * 1024 * 1024;

		/// <summary>Size of data chunks expected. Currently 16kB.</summary>
		public const int StreamChunkSize = 16384;

		/// <summary> Minimum depth of a callstack that is considered to be a valid one. </summary>
		public const int MinCallstackDepth = 2;

		/// <summary>The name of the generated report. Currently 'Diagnostics.txt'.</summary>
		public const string DiagnosticsFileName = "Diagnostics.txt";

		/// <summary>The name of the generated video of the editor session. Currently 'CrashVideo.avi'</summary>
		public const string VideoFileName = "CrashVideo.avi";

		/// <summary> Licensee branch name, we ignore crashes from this branch. </summary>
		public const string LicenseeBranchName = "UE4-QA";

		/// <summary>String to chop off the start of branch paths</summary>
		public const string P4_DEPOT_PREFIX = "//depot/";

		/// <summary>UE4 minidump name</summary>
		public const string UE4MinidumpName = "UE4Minidump.dmp";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public CrashReporterConstants()
		{
		}
	}
}