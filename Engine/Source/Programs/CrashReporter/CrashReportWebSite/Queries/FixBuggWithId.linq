<Query Kind="Program">
  <Connection>
    <ID>e40cf789-b18a-4b65-bae6-a0f5440c1ce2</ID>
    <Persist>true</Persist>
    <Server>db-09</Server>
    <Database>CrashReport</Database>
    <IsProduction>true</IsProduction>
  </Connection>
</Query>

void Main()
{
	const int BuggId = 43365;
	var BuggCrashes = Buggs_Crashes.Where (bc => bc.BuggId==BuggId);
	Buggs.Where (b => b.Id==BuggId).First ().CrashType=3; // Ensure
	Buggs.Context.SubmitChanges();
	var NumToChange = BuggCrashes.Count();
	int Index = 0;
	foreach( var It in BuggCrashes )
	{
		Index ++;
		if (It.Crash.CrashType != 3)
		{
			It.Crash.CrashType=3; // Ensure
			Debug.WriteLine( "{0} {1,3}/{2,3}", It.CrashId, Index, NumToChange );
			if (Index % 16 == 0)
			{
				Buggs_Crashes.Context.SubmitChanges();
			}
		}
		else
		{
			if (Index % 16 == 0)
			{
				Debug.WriteLine( "{0,3}/{1,3}", Index, NumToChange );
			}
		}
	}
	Buggs_Crashes.Context.SubmitChanges();
}

// Define other methods and classes here