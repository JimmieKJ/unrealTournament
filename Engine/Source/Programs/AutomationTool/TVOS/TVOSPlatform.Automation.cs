// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;

public class TVOSPlatform : IOSPlatform
{
	public TVOSPlatform()
		:base(UnrealTargetPlatform.TVOS)
	{
		TargetIniPlatformType = UnrealTargetPlatform.IOS;
	}

	public override UnrealBuildTool.UEDeployIOS GetDeployHandler()
	{
		Console.WriteLine("Getting TVOS Deploy()");
		return new UnrealBuildTool.UEDeployTVOS();
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
	{
		return "TVOS";
	}
}
