<Query Kind="Expression">
  <Connection>
    <ID>e40cf789-b18a-4b65-bae6-a0f5440c1ce2</ID>
    <Persist>true</Persist>
    <Server>db-09</Server>
    <Database>CrashReport</Database>
    <IsProduction>true</IsProduction>
  </Connection>
  <Output>DataGrids</Output>
</Query>

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//string "GetBestShadowTransform" = "";

Crashes
.Where( X => X.TimeOfCrash > DateTime.UtcNow.AddDays(-7) &&(
X.GameName.Contains( "GetBestShadowTransform" ) ||
							X.ChangeListVersion.Contains( "GetBestShadowTransform" ) ||
							X.PlatformName.Contains( "GetBestShadowTransform" ) ||
							X.EngineMode.Contains( "GetBestShadowTransform" ) ||
							X.Description.Contains( "GetBestShadowTransform" ) ||
							X.RawCallStack.Contains( "GetBestShadowTransform" ) ||
							X.CommandLine.Contains( "GetBestShadowTransform" ) ||
							X.ComputerName.Contains( "GetBestShadowTransform" ) ||
							X.FixedChangeList.Contains( "GetBestShadowTransform" ) ||
							X.Module.Contains( "GetBestShadowTransform" ) ||
							X.BuildVersion.Contains( "GetBestShadowTransform" ) ||
							X.BaseDir.Contains( "GetBestShadowTransform" ) ||
							X.TTPID.Contains( "GetBestShadowTransform" ) ||
							X.Branch.Contains( "GetBestShadowTransform" ) 
							))
.Select (X => new 
{
	X.Summary,
	X.GameName,
	X.ChangeListVersion,
	X.PlatformName,
	X.EngineMode,
	X.Description,
	X.RawCallStack,
	X.CommandLine,
	X.ComputerName,
	X.FixedChangeList,
	X.Module,
	X.BuildVersion,
	X.BaseDir,
	X.TTPID,
	X.Branch
})
/*
	X.GameName.Contains( "GetBestShadowTransform" ) ||
							X.ChangeListVersion.Contains( "GetBestShadowTransform" ) ||
							X.PlatformName.Contains( "GetBestShadowTransform" ) ||
							X.EngineMode.Contains( "GetBestShadowTransform" ) ||
							X.Description.Contains( "GetBestShadowTransform" ) ||
							X.RawCallStack.Contains( "GetBestShadowTransform" ) ||
							X.CommandLine.Contains( "GetBestShadowTransform" ) ||
							X.ComputerName.Contains( "GetBestShadowTransform" ) ||
							X.FixedChangeList.Contains( "GetBestShadowTransform" ) ||
							X.Module.Contains( "GetBestShadowTransform" ) ||
							X.BuildVersion.Contains( "GetBestShadowTransform" ) ||
							X.BaseDir.Contains( "GetBestShadowTransform" ) ||
							X.TTPID.Contains( "GetBestShadowTransform" ) ||
							X.Branch.Contains( "GetBestShadowTransform" ) 
*/