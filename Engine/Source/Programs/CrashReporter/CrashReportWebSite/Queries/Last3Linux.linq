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
.Where (c =>c.TimeOfCrash > DateTime.UtcNow.AddDays(-7) )
.Where (c =>c.PlatformName.Contains("Linux") )
.OrderByDescending(c => c.TimeOfCrash)
.Take(3)
.Select (c => new {
c.RawCallStack,
c.TimeOfCrash
})