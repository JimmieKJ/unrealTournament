<Query Kind="Program">
  <Connection>
    <ID>e40cf789-b18a-4b65-bae6-a0f5440c1ce2</ID>
    <Persist>true</Persist>
    <Server>db-09</Server>
    <Database>CrashReport</Database>
    <IsProduction>true</IsProduction>
  </Connection>
</Query>

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

void Main()
{
	int UserGroupId = 1;

	var UsersForGroup = Users.Where (User => User.UserGroupId==UserGroupId ).Select (User => User.Id ).ToList();
	
	IQueryable<Crashes> Results = Crashes.Where( X => X.TimeOfCrash.Value > new DateTime(2015,3,3) );
	
	Results =
	(
		from CrashDetail in Results
		where UsersForGroup.Contains( CrashDetail.UserNameId.Value)
		select CrashDetail
	);
		
	var ResultList = Results.ToList();	
	
	foreach (var It in Results)
	{
		Debug.WriteLine( string.Format( "{0}:{1}", It.UserName, It.UserNameId.Value) ); 
	}
}

// Define other methods and classes here