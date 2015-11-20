// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.IO;
using Ionic.Zip;

namespace UnrealBuildTool
{
	class UEDeployMac : UEBuildDeploy
	{
		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			Log.TraceInformation("Deploying now!");
			return true;
		}
	}
}
