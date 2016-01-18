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
	var Platforms = Crashes
	.Where(c => c.TimeOfCrash > DateTime.Now.AddMonths(-3))
	.Select(c => c.PlatformName)
	.Distinct()
	.ToList();

	HashSet<string> DistinctPlatforms = new HashSet<string>();

	foreach (var Platform in Platforms)
	{
		var Parts = Platform.Split(new char[] { '[',']' }, StringSplitOptions.RemoveEmptyEntries);
		if (Parts.Length > 1)
		{
			DistinctPlatforms.Add(Parts[0]);
		}
	}
	
	foreach (var P in DistinctPlatforms)
	{
		Console.WriteLine( "Platform: {0}", P );
	}
}

// Define other methods and classes here