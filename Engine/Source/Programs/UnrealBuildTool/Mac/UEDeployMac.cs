// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

		public static bool GeneratePList(string ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, string InEngineDir, string ExeName)
		{
			string IntermediateDirectory = (bIsUE4Game ? InEngineDir : ProjectDirectory) + "/Intermediate/Mac";
			string DestPListFile = IntermediateDirectory + "/" + ExeName + "-Info.plist";
			string SrcPListFile = (bIsUE4Game ? (InEngineDir + "Source/Programs/") : (ProjectDirectory + "/Source/")) + GameName + "/Resources/Mac/Info.plist";
			if (!File.Exists(SrcPListFile))
			{
				SrcPListFile = InEngineDir + "/Source/Runtime/Launch/Resources/Mac/Info.plist";
			}

			string PListData = null;
			if (File.Exists(SrcPListFile))
			{
				PListData = File.ReadAllText(SrcPListFile);
			}
			else
			{
				return false;
			}

			string BundleVersion = MacToolChain.LoadEngineDisplayVersion();
			PListData = PListData.Replace("${EXECUTABLE_NAME}", ExeName).Replace("${APP_NAME}", GameName).Replace("${ICON_NAME}", GameName).Replace("${MACOSX_DEPLOYMENT_TARGET}", MacToolChain.MinMacOSVersion).Replace("${BUNDLE_VERSION}", BundleVersion);

			if (!Directory.Exists(IntermediateDirectory))
			{
				Directory.CreateDirectory(IntermediateDirectory);
			}
			File.WriteAllText(DestPListFile, PListData);

			return true;
		}
	}
}
