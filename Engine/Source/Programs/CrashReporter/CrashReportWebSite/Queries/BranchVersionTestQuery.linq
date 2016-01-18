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
	var BuildVersions = Crashes
	.Where( c=> c.TimeOfCrash > DateTime.Now.AddMonths(-3))
	.Where( n => n.BuildVersion.StartsWith( "4." ) )
	.Where( n => n.Branch.Contains( "UE4-Release" ) )
	.Select( c => c.BuildVersion )
	.Distinct()
	.ToList();
	
	List<string> DistinctBuildVersions = new List<string>();
	
	foreach (var BuildVersion in BuildVersions)
	{
		var BVParts = BuildVersion.Split( new char[] { '.' }, StringSplitOptions.RemoveEmptyEntries );
		if (BVParts.Length > 2 && BVParts[0] != "0")
		{
			string CleanBV = string.Format( "{0}.{1}.{2}", BVParts[0], BVParts[1], BVParts[2] );
			DistinctBuildVersions.Add(CleanBV);
		}
	}
}

// Define other methods and classes here
