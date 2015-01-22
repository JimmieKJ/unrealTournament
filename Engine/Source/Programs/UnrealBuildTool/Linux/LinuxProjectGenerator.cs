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
    class LinuxProjectGenerator : UEPlatformProjectGenerator
    {
		/**
		 *	Register the platform with the UEPlatformProjectGenerator class
		 */
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for both Linux
			if (BuildConfiguration.bPrintDebugInfo)
			{
                Console.WriteLine("        Registering for {0}", UnrealTargetPlatform.Linux.ToString());
			}
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Linux, this);
		}

		///
		///	VisualStudio project generation functions
		///	
		/**
		 *	Whether this build platform has native support for VisualStudio
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	InConfiguration		The UnrealTargetConfiguration being built
		 *	
		 *	@return	bool				true if native VisualStudio support (or custom VSI) is available
		 */
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}  
    }
}
