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
	class MacProjectGenerator : UEPlatformProjectGenerator
	{
		/**
		 *	Register the platform with the UEPlatformProjectGenerator class
		 */
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for Mac
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Mac.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Mac, this);
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
			// Mac is not supported in VisualStudio
			return false;
		}
	}
}
