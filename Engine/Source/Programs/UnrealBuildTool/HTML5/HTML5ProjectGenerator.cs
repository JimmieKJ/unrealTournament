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
	class HTML5ProjectGenerator : UEPlatformProjectGenerator
	{
		/**
		 *	Register the platform with the UEPlatformProjectGenerator class
		 */
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for HTML5
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.HTML5.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.HTML5, this);
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
			// HTML5 is not supported in VisualStudio
			return false;
		}
	}
}
