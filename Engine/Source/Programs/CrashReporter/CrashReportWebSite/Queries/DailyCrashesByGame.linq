<Query Kind="Program">
  <Connection>
    <ID>e40cf789-b18a-4b65-bae6-a0f5440c1ce2</ID>
    <Persist>true</Persist>
    <Server>db-09</Server>
    <Database>CrashReport</Database>
    <IsProduction>true</IsProduction>
  </Connection>
  <Output>DataGrids</Output>
</Query>

void Main()
{
	DateTime StartDate = new DateTime( 2015,5,1 );
	DateTime EndDate = new DateTime(2015,5,22);
	TimeSpan TimeSpan = TimeSpan.FromDays(1);
	
	while (StartDate < EndDate)
	{
		DateTime TickEndDate = StartDate+TimeSpan;
		Debug.WriteLine( "StartDate: {0} EndDate: {1}", StartDate, TickEndDate );
		var Filters = Crashes
		.Where (c => c.TimeOfCrash > StartDate && c.TimeOfCrash < TickEndDate)
		.Where (c => c.CrashType == 1 || c.CrashType == 2)
		//.Where (c => c.UserName == "Anonymous")
		//.Select (c => c.GameName)
		.GroupBy (c => c.GameName)
		.OrderByDescending (c => c.Count ());
		
		var Countt = Crashes
		.Where (c => c.TimeOfCrash > StartDate && c.TimeOfCrash < TickEndDate)
		.Where (c => c.CrashType == 1 || c.CrashType == 2)
		//.Where (c => c.UserName == "Anonymous")
		//.Select (c => c.GameName)
		.Count ();
		
		int Total = 0;
		int Top = 0;
		foreach( var Element in Filters )
		{
			Debug.WriteLine( "	GameName: {0} Crashes: {1}", Element.Key, Element.Count () );
			Top++;
			Total += Element.Count ();
			if (Top > 10)
			{
				break;
			}
		}
		Debug.WriteLine( "	Total: {0}/{1}", Total, Countt );
		StartDate += TimeSpan;
	}
}

// Define other methods and classes here