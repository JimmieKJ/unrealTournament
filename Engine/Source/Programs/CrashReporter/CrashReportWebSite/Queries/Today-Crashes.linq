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

Crashes
.Where (c => c.TimeOfCrash > DateTime.UtcNow.AddDays(-1))
.OrderBy (c => c.Id)
.Select (c => new 
{
c.Id,
//c.Title,
c.Summary,
c.GameName,
//c.Status,
c.TimeOfCrash,
c.ChangeListVersion,
c.PlatformName,
//c.EngineMode,
//c.Description,
c.RawCallStack,
c.Pattern,
//c.CommandLine,
c.ComputerName,
//c.Selected,
//c.FixedChangeList,
//c.LanguageExt,
c.Module,
c.BuildVersion,
c.BaseDir,
//c.Version,
//c.UserName,
//c.TTPID,
//c.AutoReporterID,
//c.Processed,
c.Branch,
c.CrashType,
//c.UserNameId,
//c.SourceContext,
c.EpicAccountId,
//c.EngineVersion
})

/*
	X.GameName.Contains( Term ) ||
							X.ChangeListVersion.Contains( Term ) ||
							X.PlatformName.Contains( Term ) ||
							X.EngineMode.Contains( Term ) ||
							X.Description.Contains( Term ) ||
							X.RawCallStack.Contains( Term ) ||
							X.CommandLine.Contains( Term ) ||
							X.ComputerName.Contains( Term ) ||
							X.FixedChangeList.Contains( Term ) ||
							X.Module.Contains( Term ) ||
							X.BuildVersion.Contains( Term ) ||
							X.BaseDir.Contains( Term ) ||
							X.TTPID.Contains( Term ) ||
							X.Branch.Contains( Term ) 
*/