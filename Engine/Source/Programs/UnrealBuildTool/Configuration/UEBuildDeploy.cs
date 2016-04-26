// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;

namespace UnrealBuildTool
{
	/// <summary>
	///  Base class to handle deploy of a target for a given platform
	/// </summary>
	public abstract class UEBuildDeploy
	{
		/// <summary>
		/// Prepare the target for deployment
		/// </summary>
		/// <param name="InTarget"> The target for deployment</param>
		/// <returns>bool   true if successful, false if not</returns>
		public virtual bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			return true;
		}

		/// <summary>
		/// Prepare the target for deployment
		/// </summary>
		public virtual bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, string ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
			return true;
		}
	}
}
