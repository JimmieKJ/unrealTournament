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

Buggs
.Where (c => c.TimeOfFirstCrash > DateTime.UtcNow.AddDays(-1))
.OrderByDescending (c => c.NumberOfCrashes)
.Select (c => new 
{
c.Id,
c.TTPID,
//c.Title,
//c.Summary,
//c.Priority,
//c.Type,
c.Pattern,//.ToString().Split(new char[]{'+'},StringSplitOptions.RemoveEmptyEntries).Length,
c.NumberOfCrashes,
c.TimeOfFirstCrash,
c.TimeOfLastCrash,
//c.Status,
c.FixedChangeList,
//c.Description,
//c.ReproSteps,
//c.Game,
c.BuildVersion,
c.CrashType,
//c.SummaryV2
})