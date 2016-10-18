// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;

namespace Tools.CrashReporter.CrashReportCommon
{
	/// <summary> Basic support for native FEngineVersion. </summary>
	public class FEngineVersion
	{
		/// <summary> Initialization constructor. </summary>
		public FEngineVersion(string StringEngineVersion)
		{
			// 4.10.0-0+UE4
			int MinusPos = StringEngineVersion.IndexOf( '-' );
			int PlusPos = StringEngineVersion.IndexOf( '+' );

			if (MinusPos < 0 || PlusPos < 0 || PlusPos < MinusPos)
			{
				VersionNumber = string.Empty;
				Branch = string.Empty;
				return;
			}

			VersionNumber = StringEngineVersion.Substring( 0, MinusPos );
			string ChangelistSring = StringEngineVersion.Substring( MinusPos + 1, PlusPos - MinusPos - 1 );
			uint.TryParse( ChangelistSring, out Changelist );

			Branch = StringEngineVersion.Substring( PlusPos+1 );
		}

		/// <summary>
		/// Returns the branch name corresponding to this version
		/// </summary>
		public string GetCleanedBranch()
		{
			string CleanedBranch = Branch.Replace( '+', '/' );
			CleanedBranch = CleanedBranch.Replace( CrashReporterConstants.P4_DEPOT_PREFIX, "" );
			CleanedBranch = CleanedBranch.Substring( 0, Math.Min( 31, CleanedBranch.Length ) ); // Database limitation.
			return CleanedBranch;
		}

		/// <summary> Version number. </summary>
		public string VersionNumber;

		/// <summary> Change list/ Built from CL. </summary>
		public uint Changelist;

		/// <summary> Branch. </summary>
		public string Branch;

	}

	/// <summary>Helper to read useful information out of the report XML and diagnostics files</summary>
	public class FReportData
	{
		/// <summary>Name of report for logging</summary>
		public readonly string ReportName;

		/// <summary> Ticks for this report data. </summary>
		public readonly Int64 Ticks = DateTime.Now.Ticks;

		/// <summary> Helper method, display this report data as a report name. Debugging purpose. </summary>
		public override string ToString()
		{
			return ReportName;
		}

		/// <returns>Unique name of this report, can be used as name of file.</returns>
		public string GetReportNameAsFilename()
		{
			string EngineVersion = string.Format
			(
				"{0}-{1}+{2}",
				!string.IsNullOrEmpty( BuildVersion ) ? BuildVersion : "NO_BUILD_VERSION",
				!string.IsNullOrEmpty( BuiltFromCL ) ? BuiltFromCL : "NO_BUILT_FROM_CL",
				!string.IsNullOrEmpty( BranchName ) ? BranchName.Replace( '/', '+' ) : "NO_BRANCH"
			);
			string Directory = string.Format( "{0}_{1}__{2}", EngineVersion, !string.IsNullOrEmpty( GameName ) ? GameName : "NO_GAMENAME", Ticks );
			return Directory;
		}

		/// <summary> Returns engine version as a string ie.: 4.7.0-2448202+++depot+UE4-Releases+4.7 </summary>
		public string GetEngineVersion()
		{
			string EngineVersion = string.Format
			(
				"{0}-{1}+{2}",
				!string.IsNullOrEmpty( BuildVersion ) ? BuildVersion : "",
				!string.IsNullOrEmpty( BuiltFromCL ) ? BuiltFromCL : "",
				!string.IsNullOrEmpty( BranchNameWithDepot ) ? BranchNameWithDepot.Replace( '/', '+' ) : ""
			);

			// An invalid version, so return an empty string.
			if( EngineVersion == "-+" )
			{
				return "";
			}

			return EngineVersion;
		}

		/// <summary>
		/// Read the callstack from the diagnostics file
		/// <param name="DiagnosticsPath">Absolute path of the diagnostics file.</param>
		/// <returns>The callstack as an array of entries</returns>
		/// </summary>
		static public string[] GetCallStack(string DiagnosticsPath)
		{
			return ExtractSection(DiagnosticsPath, "<CALLSTACK START>", "<CALLSTACK END>");
		}

		/// <summary>
		/// Read the source context from the diagnostics file
		/// <param name="DiagnosticsPath">Absolute path of the diagnostics file.</param>
		/// <returns>Source code around the error point as an array of lines</returns>
		/// </summary>
		static public string[] GetSourceContext(string DiagnosticsPath)
		{
			return ExtractSection(DiagnosticsPath, "<SOURCE START>", "<SOURCE END>");
		}

		/// <summary>Accessor for the game name from the report metadata</summary>
		public string GameName
		{
			get
			{
				try
				{
					return Metadata.ProblemSignatures.Parameter0;
				}
				catch( System.Exception )
				{
					return "";
				}
			}
		}

		/// <summary>Accessor for the build version string from the report metadata</summary>
		public string BuildVersion
		{
			get
			{
				try
				{
					string FourParms = Metadata.ProblemSignatures.Parameter1;

					var Subs = FourParms.Split( ".".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );

					if( Subs.Length >= 3 )
					{
						return string.Format( "{0}.{1}.{2}", Subs[0], Subs[1], Subs[2] );
					}

					return "";
				}
				catch( System.Exception )
				{
					return "";
				}
			}
		}

		/// <summary>Accessor for the language identifier from the report metadata</summary>
		public string Language
		{
			get
			{
				try
				{
					return Metadata.DynamicSignatures.Parameter2;
				}
				catch( System.Exception )
				{
					return "";
				}
			}
		}

		/// <summary>Accessor for the command line from the report metadata</summary>
		public string CommandLine
		{
			get
			{
				string Result = "";
				try
				{
					if( Metadata.ProblemSignatures != null && Metadata.ProblemSignatures.Parameter8 != null )
					{
						string[] Components = Metadata.ProblemSignatures.Parameter8.Split( new[] { '!' } );
						if( Components.Length > 0 )
						{
							Result = Components[1];
						}
					}
				}
				finally
				{}
				return Result;
			}
		}

		/// <summary>Accessor for the system language from the report metadata</summary>
		public string SystemLanguage
		{
			get
			{
				try
				{
					return Metadata.OSVersionInformation.LCID;
				}
				catch( System.Exception )
				{
					return "";
				}	
			}
		}

		/// <summary>Extract the branch name from the report metadata</summary>
		public string BranchName
		{
			get
			{
				string BranchName = ExtractFromBaseDirString(0).Replace('+', '/');
				if (BranchName.StartsWith(CrashReporterConstants.P4_DEPOT_PREFIX))
				{
					BranchName = BranchName.Substring( CrashReporterConstants.P4_DEPOT_PREFIX.Length );
				}

				BranchName = BranchName.Substring( 0, Math.Min( 31, BranchName.Length ) ); // Database limitation.

				return BranchName;
			}
		}

		/// <summary>Extract the branch name from the report metadata</summary>
		public string BranchNameWithDepot
		{
			get
			{
				string BranchName = ExtractFromBaseDirString( 0 ).Replace( '+', '/' );
				return BranchName;
			}
		}

		/// <summary>Extract the base directory from the report metadata</summary>
		public string BaseDir
		{
			get
			{
				return ExtractFromBaseDirString(1);
			}
		}

		/// <summary>Extract the platform name from the report metadata</summary>
		public string Platform
		{
			get
			{
				// Extract the platform name encoded in parameter9 and add the Windows product info, e.g. Windows 7 Professional
				string PlatformName = "";
				string BranchPath = ExtractFromBaseDirString(1);
				if (BranchPath != "")
				{
					string[] FolderComponents = BranchPath.Split(new[] {'/'}, StringSplitOptions.RemoveEmptyEntries);
					if (FolderComponents.Length != 0)
					{
						PlatformName = FolderComponents[FolderComponents.Length - 1];
					}
				}

				if (!string.IsNullOrEmpty( Metadata.OSVersionInformation.Product ) && !string.IsNullOrEmpty( Metadata.OSVersionInformation.Architecture ))
				{
					PlatformName = string.Format( "{0} [{2} {1}]", PlatformName, Metadata.OSVersionInformation.Architecture == "X64" ? "64b" : "32b", Metadata.OSVersionInformation.Product );
				}

				return PlatformName;
			}
		}

		/// <summary> This should be ErrorMessage=, but leave it as the same for backward compatibility. </summary>
		public const string ErrorMessageMarker = "AssertLog=\"";
		
		const string ExceptionMarker = "Exception was \"";

		const string MachineIdMarker = "MachineId:";
		const string EpicAccountIdMarker = "EpicAccountId:";
		const string UserNameMarker = "Name:";


		/// <summary>
		/// The error message, can be assertion message, ensure message or message from the fatal error.
		/// @see GErrorMessage.
		/// </summary>
		public string[] ErrorMessage
		{
			get
			{
				var EmptyArray = new string[] { };
				try
				{
					// This may fail if we encounter a very long error message.
					int StartIndex = Metadata.ProblemSignatures.Parameter8.IndexOf( ErrorMessageMarker );
					if( StartIndex > 0 )
					{
						string ErrorMessageFlat = Metadata.ProblemSignatures.Parameter8.Substring( StartIndex );

						ErrorMessageFlat = ErrorMessageFlat.Replace( ErrorMessageMarker, "" ).Replace( "\"", "" );
						ErrorMessageFlat = ErrorMessageFlat.Substring( 0, Math.Min( 511, ErrorMessageFlat.Length ) ); // Database limitation.

						EmptyArray = ErrorMessageFlat.Split( new[] { '#' }, StringSplitOptions.RemoveEmptyEntries );
					}

					return EmptyArray;
				}
				catch( System.Exception )
				{
					return EmptyArray;
				}
			}
		}

		/// <summary>
		/// Exception description, used if the ErrorMessage is empty
		/// </summary>
		static public string[] GetExceptionDescription(string DiagnosticsPath)
		{
			var ResultArray = new string[] { "" };
			try
			{
				string[] Lines = File.ReadAllLines( DiagnosticsPath );
				foreach( string Line in Lines )
				{
					if( Line.Contains( ExceptionMarker ) )
					{
						int StartIndex = Line.IndexOf( ExceptionMarker );
						int EndIndex = Line.LastIndexOf( "\"" );
						ResultArray = new string[] { Line.Substring( StartIndex, EndIndex - StartIndex ).Replace( ExceptionMarker, "" ) };
						break;
					}
				}

				return ResultArray;
			}
			catch( System.Exception )
			{
				return ResultArray;
			}
		}

		/// <summary>Extract the engine mode from the report metadata</summary>
		public string EngineMode
		{
			get
			{
				return ExtractFromBaseDirString(2);
			}
		}

		/// <summary>Extract the CL# from the report metadata</summary>
		public string BuiltFromCL
		{
			get
			{
				return ExtractFromBaseDirString(3);
			}
		}

		/// <summary>User-provided description from the report</summary>
		public string[] UserDescription
		{
			get
			{
				var EmptyArray = new string[] { };
				if( Metadata.DynamicSignatures != null && Metadata.DynamicSignatures.Parameter3 != null )
				{
					// Description in the DB has only 512 characters.
					string ExtractedDescription = Metadata.DynamicSignatures.Parameter3;
					return ExtractedDescription.Substring( 0, Math.Min( 480, ExtractedDescription.Length ) ).Split( new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries );
				}
				return EmptyArray;
			}
		}

		/// <summary>The unique ID used to identify the machine the crash occurred on.</summary>
		public string MachineId
		{
			get
			{
				return ExtractMarkedString( MachineIdMarker );
			}
		}

		/// <summary>The Epic account ID for the user who last used the Launcher.</summary>
		public string EpicAccountId
		{
			get
			{
				return ExtractMarkedString( EpicAccountIdMarker );
			}
		}

		/// <summary> The name of the user that caused this crash. </summary>
		public string UserName
		{
			get
			{
				return ExtractMarkedString( UserNameMarker );
			}
		}

		/// <summary> Whether the user allowed us to be contacted. </summary>
		public bool AllowToBeContacted
		{
			get
			{
				return Metadata.DynamicSignatures != null ? Metadata.DynamicSignatures.bAllowToBeContacted : false;
			}
		}

		/// <summary> Game-specific deployment string e.g. "DevPlaytest", "Live" </summary>
		public string DeploymentName
		{
			get
			{
				return Metadata.DynamicSignatures.DeploymentName;
			}
		}

		/// <summary> Is the report an non-fatal error? </summary>
		public bool IsEnsure
		{
			get
			{
				return Metadata.DynamicSignatures.IsEnsure;
			}
		}

		/// <summary> Is the report a crash from a deliberate check like an assert? </summary>
		public bool IsAssert
		{
			get
			{
				return Metadata.DynamicSignatures.IsAssert;
			}
		}

		/// <summary> Crash type string e.g. "Crash, "Assert", "Ensure" </summary>
		public string CrashType
		{
			get
			{
				return Metadata.DynamicSignatures.CrashType;
			}
		}

		/// <summary> Extracts a string marked with the specified string value. </summary>
		private string ExtractMarkedString( string NameMarker )
		{
			if( Metadata.DynamicSignatures != null && Metadata.DynamicSignatures.Parameter4 != null )
			{
				string[] Components = Metadata.DynamicSignatures.Parameter4.Split( new[] { '!' }, StringSplitOptions.RemoveEmptyEntries );
				foreach( string Component in Components )
				{
					if( Component.StartsWith( NameMarker ) )
					{
						string Result = Component.Replace( NameMarker, "" );
						return Result;
					}
				}
			}
			return "";
		}

		/// <summary>
		/// Constructor: store the metadata and extract the callstack from diagnostics file
		/// </summary>
		/// <param name="Data">Report meta-data object, parsed from XML</param>
		/// <param name="InReportNamePath">Name of report for logging</param>
		public FReportData(	WERReportMetadata Data, string InReportNamePath	)
		{
			Metadata = Data;
			ReportName = InReportNamePath;
		}

		/// <summary>
		/// Split the combined base directory string into individual components
		/// <param name="Index">Index of component to retrieve</param>
		/// </summary>
		string ExtractFromBaseDirString(int Index)
		{
			try
			{
				string[] Components = Metadata.ProblemSignatures.Parameter9.Split( new[] { '!' } );
				return Components.Length > Index ? Components[Index] : "";
			}
			catch( System.Exception )
			{
				return "";
			}	
		}

		/// <summary>
		/// Extract a delimited section from the diagnostics file.
		/// </summary>
		/// <param name="Filepath">File name of the diagnostics file to interrogate.</param>
		/// <param name="StartKey">A string key to mark the start of the selection.</param>
		/// <param name="EndKey">A string key to mark the end of the selection.</param>
		/// <returns>A string array of the extracted section.</returns>
		static string[] ExtractSection( string Filepath, string StartKey, string EndKey )
		{
			List<string> Section = new List<string>();

			string[] Lines = File.ReadAllLines( Filepath );

			bool bCollecting = false;
			foreach( string Line in Lines )
			{
				if( Line.StartsWith( StartKey ) )
				{
					bCollecting = true;
				}
				else if( Line.StartsWith( EndKey ) )
				{
					bCollecting = false;
					break;
				}
				else if( bCollecting )
				{
					Section.Add( Line.Trim( Environment.NewLine.ToCharArray() ) );
				}
			}

			return Section.ToArray();
		}

		/// <summary>Metadata read from xml file</summary>
		WERReportMetadata Metadata;
	}
}