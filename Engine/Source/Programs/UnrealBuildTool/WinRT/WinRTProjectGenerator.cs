// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class WinRTProjectGenerator : UEPlatformProjectGenerator
	{
		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for WinRT
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.WinRT.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.WinRT, this);

			// For now only register WinRT_ARM is truly a Windows 8 machine.
			// This will prevent people who do all platform builds from running into the compiler issue.
			if (WinRTPlatform.IsWindows8() == true)
			{
				Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.WinRT_ARM.ToString());
				UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.WinRT_ARM, this);
			}
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Return the VisualStudio platform name for this build platform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The name of the platform that VisualStudio recognizes</returns>
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			if (InPlatform == UnrealTargetPlatform.WinRT)
			{
				return "WinRT";
			}
			return InPlatform.ToString();
		}

		/// <summary>
		/// Return the platform toolset string to write into the project configuration
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The custom configuration section for the project file; Empty string if it doesn't require one</returns>
		public override string GetVisualStudioPlatformToolsetString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFile InProjectFile)
		{
			return "		<PlatformToolset>v110</PlatformToolset>" + ProjectFileGenerator.NewLine;
		}

		/// <summary>
		/// Get whether this platform deploys
		/// </summary>
		/// <returns>bool  true if the 'Deploy' option should be enabled</returns>
		public override bool GetVisualStudioDeploymentEnabled(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}
	}
}
