// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	/**
	 *	Base class for platform-specific project generators 
	 */
	public class WindowsProjectGenerator : UEPlatformProjectGenerator
	{
		/**
		 *	Register the platform with the UEPlatformProjectGenerator class
		 */
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for Win32 and Win64
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win32.ToString());
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win64.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Win32, this);
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Win64, this);
		}

		///
		///	VisualStudio project generation functions
		///	
		/**
		 *	Return the VisualStudio platform name for this build platform
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	InConfiguration		The UnrealTargetConfiguration being built
		 *	
		 *	@return	string				The name of the platform that VisualStudio recognizes
		 */
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			if (InPlatform == UnrealTargetPlatform.Win64)
			{
				return "x64";
			}
			return InPlatform.ToString();
		}

		public override bool RequiresVSUserFileGeneration()
		{
			return true;
		}
	}
}
