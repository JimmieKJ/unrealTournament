// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
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
	/// Descriptive class for the WER report data
	/// </summary>
	public class WERReportMetadata
	{
		/// <summary>Host operating system version information.</summary>
		public WERReportMetadataOSVersionInformation OSVersionInformation;
		/// <summary>Details about the process that spawned the process that crashed.</summary>
		public WERReportMetadataParentProcessInformation ParentProcessInformation;
		/// <summary>A set of up to ten key value pairs representing the signature of the crash.</summary>
		public WERReportMetadataProblemSignatures ProblemSignatures;
		/// <summary>Additional key value pairs containing information about the crash.</summary>
		public WERReportMetadataDynamicSignatures DynamicSignatures;
		/// <summary>Information about the system the crash occurred on.</summary>
		public WERReportMetadataSystemInformation SystemInformation;

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public WERReportMetadata()
		{
		}
	}

	/// <summary>
	/// The problem signature
	/// </summary>
	public class WERReportMetadataProblemSignatures
	{
		/// <summary>The type of event e.g. crash or hang.</summary>
		public string EventType = "";

		/// <summary>The application name.</summary>
		public string Parameter0 = "";
		/// <summary>The application version.</summary>
		public string Parameter1 = "";
		/// <summary>The application timestamp.</summary>
		public string Parameter2 = "";
		/// <summary>The faulting module name.</summary>
		public string Parameter3 = "";
		/// <summary>The faulting module version.</summary>
		public string Parameter4 = "";
		/// <summary>The faulting module timestamp.</summary>
		public string Parameter5 = "";
		/// <summary>The exception code.</summary>
		public string Parameter6 = "";
		/// <summary>The exception offset.</summary>
		public string Parameter7 = "";
		/// <summary>The command line of the crashed process.</summary>
		public string Parameter8 = "";
		/// <summary>Branch, BaseDir, and engine mode.</summary>
		public string Parameter9 = "";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public WERReportMetadataProblemSignatures()
		{
		}
	}

	/// <summary>
	/// The dynamic signature.
	/// </summary>
	public class WERReportMetadataDynamicSignatures
	{
		/// <summary>The long version of the operating system version.</summary>
		public string Parameter1 = "";
		/// <summary>The LCID of the application.</summary>
		public string Parameter2 = "";
		/// <summary>Generic parameter</summary>
		public string Parameter3 = "";
		/// <summary>Generic parameter</summary>
		public string Parameter4 = "";
		/// <summary>Generic parameter</summary>
		public string Parameter5 = "";
		/// <summary>Generic parameter</summary>
		public string Parameter6 = "";
		/// <summary>Generic parameter</summary>
		public string Parameter7 = "";
		/// <summary>Generic parameter</summary>
		public string Parameter8 = "";
		/// <summary>Generic parameter</summary>
		public string Parameter9 = "";

		/// <summary>Whether the user allowed us to be contacted</summary>
		public bool bAllowToBeContacted = false;

		/// <summary>Deployment name of the crashed app, if any</summary>
		public string DeploymentName = "";

		/// <summary>Was the "crash" a non-fatal event?</summary>
		public bool IsEnsure = false;

		/// <summary>Was the "crash" an assert or check that failed?</summary>
		public bool IsAssert = false;

		/// <summary>Crash type string</summary>
		public string CrashType = "";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public WERReportMetadataDynamicSignatures()
		{
		}
	}

	/// <summary>
	/// Details about the host OS the crash occurred on
	/// </summary>
	public class WERReportMetadataOSVersionInformation
	{
		/// <summary>The Windows version e.g. 6.1.</summary>
		public string WindowsNTVersion = "";
		/// <summary>The build number and service pack. e.g. '7601 Service Pack 1'.</summary>
		public string Build = "";
		/// <summary>The product name.</summary>
		public string Product = "";
		/// <summary>The type of Windows operating system e.g. Professional.</summary>
		public string Edition = "";
		/// <summary>The detailed version.e.g. '7601.17944.amd64fre.win7sp1_gdr.120830-0333'.</summary>
		public string BuildString = "";
		/// <summary>The revision.</summary>
		public string Revision = "";
		/// <summary>The flavor. e.g. 'Multiprocessor Free'.</summary>
		public string Flavor = "";
		/// <summary>The processor architecture e.g. x64.</summary>
		public string Architecture = "";
		/// <summary>The system language code.</summary>
		public string LCID = "";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public WERReportMetadataOSVersionInformation()
		{
		}
	}

	/// <summary>
	/// Details about the process that launched the crashed application.
	/// </summary>
	public class WERReportMetadataParentProcessInformation
	{
		/// <summary>The id of the parent process.</summary>
		public string ParentProcessId = "";
		/// <summary>The full path of the parent process.</summary>
		public string ParentProcessPath = "";
		/// <summary>The command line used to launch the parent process, including the executable name.</summary>
		public string ParentProcessCmdLine = "";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public WERReportMetadataParentProcessInformation()
		{
		}
	}

	/// <summary>
	/// Details about the machine the crash occurred on
	/// </summary>
	public class WERReportMetadataSystemInformation
	{
		/// <summary>A GUID for the machine.</summary>
		public string MID = "";
		/// <summary>The manufacturer of the machine e.g. Hewlett Packard.</summary>
		public string SystemManufacturer = "";
		/// <summary>The system product name.</summary>
		public string SystemProductName = "";
		/// <summary>The current BIOS version.</summary>
		public string BIOSVersion = "";

		/// <summary>A simple default constructor to allow Xml serialisation.</summary>
		public WERReportMetadataSystemInformation()
		{
		}
	}
}