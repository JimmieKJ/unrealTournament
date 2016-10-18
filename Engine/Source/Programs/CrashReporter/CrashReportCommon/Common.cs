// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Runtime.Serialization;
using System.Xml.Serialization;
using System.Collections;
using System.Collections.Generic;
using Tools.DotNETCommon.XmlHandler;
using System.Xml;
using System.Linq;
using System.Security;

namespace Tools.CrashReporter.CrashReportCommon
{
	/// <summary>
	/// A class to return the result of the CheckReport web request.
	/// </summary>
	public class CrashReporterResult
	{
		/// <summary>A bool representing success or failure of the CheckReport web request.</summary>
		[XmlAttribute]
		public bool bSuccess = false;

		/// <summary>An optional integer value typically representing a unique ID.</summary>
		[XmlAttribute]
		public int ID = 0;

		/// <summary>A optional message to explain any return values.</summary>
		[XmlAttribute]
		public string Message = "";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public CrashReporterResult()
		{
		}
	}

	/// <summary>
	/// A class to send to the web service to check to see if the report has already been uploaded.
	/// </summary>
	public class CheckReportRequest
	{
		/// <summary>A unique ID, actually the directory name of the WER.</summary>
		[XmlAttribute]
		public string ReportId = "";

		/// <summary>LEGACY SUPPORT for crashes reported from clients before CrashReportUploader had been rebuilt</summary>
		[XmlAttribute]
		public string DirectoryName = "";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public CheckReportRequest()
		{
		}
	}

	/// <summary>
	/// A class to send to the CrashReport website to add a new crash to the database.
	/// </summary>
	public class CrashDescription
	{
        /// <summary>The type of the crash e.g. crash, assert or ensure. </summary>
	    [XmlElement] 
        public string CrashType = "";

		/// <summary>The name of the branch this game was built out of.</summary>
		[XmlElement]
		public string BranchName = "";

		/// <summary>The name of the game that crashed.</summary>
		[XmlElement]
		public string GameName = "";

		/// <summary>The platform that crashed e.g. Win64.</summary>
		[XmlElement]
		public string Platform = "";

		/// <summary>The mode the game was in e.g. editor.</summary>
		[XmlElement]
		public string EngineMode = "";

		/// <summary>The four component version of the app e.g. 4.4.1.0</summary>
		[XmlElement]
		public string BuildVersion = "";

		/// <summary> Changelist number. </summary>
		[XmlElement]
		public int BuiltFromCL = 0;

		/// <summary>The command line of the application that crashed.</summary>
		[XmlElement]
		public string CommandLine = "";

		/// <summary>The base directory where the app was running.</summary>
		[XmlElement]
		public string BaseDir = "";

		/// <summary>The language code the app was running in e.g. 1033.</summary>
		[XmlElement]
		public string Language = "";

		/// <summary>The language code of the system.</summary>
		[XmlElement]
		public string SystemLanguage = "";

		/// <summary> 
		/// The name of the user that caused this crash.
		/// @UserName varchar(64)  
		/// </summary>
		[XmlElement]
		public string UserName = "";

		/// <summary>
		/// The GUID of the machine the crash occurred on.
		/// @ComputerName varchar(64) 
		/// </summary>
		[XmlElement]
		public string MachineGuid = "";

		/// <summary> 
		/// The Epic account ID for the user who last used the Launcher.
		/// @EpicAccountId	varchar(64)	
		/// </summary>
		[XmlElement]
		public string EpicAccountId = "";

		/// <summary>An array of strings representing the callstack of the crash.</summary>
		[XmlElement]
		public string[] CallStack = null;

		/// <summary>An array of strings showing the source code around the crash.</summary>
		[XmlElement]
		public string[] SourceContext = null;

		/// <summary>An array of strings representing the user description of the crash.</summary>
		[XmlElement]
		public string[] UserDescription = null;

		/// <summary>A string representing the user activity hint text from the application.</summary>
		[XmlElement]
		public string UserActivityHint = "";

		/// <summary>The error message, can be assertion message, ensure message or message from the fatal error.</summary>
		[XmlElement]
		public string[] ErrorMessage = null;

		/// <summary>Crash GUID from the crash context.</summary>
		[XmlElement]
		public string CrashGUID = "";

		/// <summary>The UTC time the crash occurred.</summary>
		[XmlElement]
		public DateTime TimeofCrash;

		/// <summary>Whether this crash has a minidump.</summary>
		[XmlElement]
		public bool bHasMiniDump = false;

		/// <summary>Whether this crash has a log file.</summary>
		[XmlElement]
		public bool bHasLog = false;

		/// <summary>Obsolete.</summary>
		[XmlElement]
		public bool bHasDiags = false;

		/// <summary>Obsolete.</summary>
		[XmlElement]
		public bool bHasWERData = false;

		/// <summary>Whether this crash has a video.</summary>
		[XmlElement]
		public bool bHasVideo = false;

		/// <summary> Whether the user allowed us to be contacted. </summary>
		[XmlElement]
		public bool bAllowToBeContacted = false;

		/// <summary>A simple default constructor to allow Xml serialization.</summary>
		public CrashDescription()
		{
		}
	}

	

	/// <summary> Details about received compressed crash. </summary>
	public class FCompressedCrashInformation
	{
		/// <summary> Size of the compressed data. </summary>
		public string CompressedSize = "";

		/// <summary> Size of the data after decompression. </summary>
		public string UncompressedSize = "";

		/// <summary> Number of files stored in the compressed data. </summary>
		public string NumberOfFiles = "";

		/// <summary>A simple default constructor to allow Xml serialization.</summary>
		public FCompressedCrashInformation()
		{}

		/// <summary>Initialization constructor.</summary>
		public FCompressedCrashInformation( string InCompressedSize, string InUncompressedSize, string InNumberOfFiles )
		{
			CompressedSize = InCompressedSize;
			UncompressedSize = InUncompressedSize;
			NumberOfFiles = InNumberOfFiles;
		}

		/// <summary> CompressedSize as int. </summary>
		public int GetCompressedSize()
		{
			int Result = 0;
			int.TryParse( CompressedSize, out Result );
			return Result;
		}

		/// <summary> UncompressedSize as int. </summary>
		public int GetUncompressedSize()
		{
			int Result = 0;
			int.TryParse( UncompressedSize, out Result );
			return Result;
		}

		/// <summary> NumberOfFiles as int. </summary>
		public int GetNumberOfFiles()
		{
			int Result = 0;
			int.TryParse( NumberOfFiles, out Result );
			return Result;
		}
	}

	/// <summary> Helper class for reading binary data. </summary>
	public class FBinaryReaderHelper
	{
		/// <summary> Reads FString serialized with SerializeAsANSICharArray. </summary>
		public static string ReadFixedSizeString( BinaryReader BinaryStream )
		{
			uint Length = BinaryStream.ReadUInt32();
			string Result = new string( BinaryStream.ReadChars( (int)Length ) );
			Result = FixFixedSizeString( Result );
			return Result;
		}

		static String FixFixedSizeString( string String )
		{
			int RealLength = 0;
			while( String[RealLength++] != '\0' )
			{
			}
			string Result = String.Remove( RealLength - 1 );
			return Result;
		}
	}

	/// <summary>
	/// Base class for Crash Reporter system exceptions
	/// </summary>
	[Serializable]
	public class CrashReporterException : Exception
	{
		/// <summary>
		/// Default constructor
		/// </summary>
		public CrashReporterException() {}

		/// <summary>
		/// Construct from an error message
		/// </summary>
		/// <param name="message">The error message</param>
		public CrashReporterException(string message)
			: base(message) {}

		/// <summary>
		/// Construct from an error message and inner exception
		/// </summary>
		/// <param name="message">The error message</param>
		/// <param name="inner">Inner exception</param>
		public CrashReporterException(string message, Exception inner)
			: base(message, inner) {}

		/// <summary>
		/// Protected constructor for serialization
		/// </summary>
		/// <param name="info">Serialization info</param>
		/// <param name="context">Streaming context </param>
		protected CrashReporterException(SerializationInfo info, StreamingContext context)
			: base(info, context) {}
	}
}
