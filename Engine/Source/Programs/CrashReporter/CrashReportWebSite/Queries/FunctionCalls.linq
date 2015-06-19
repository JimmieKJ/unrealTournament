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

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*IdList.Remove( 63039 ); // 63039	KERNELBASE!RaiseException()
IdList.Remove( 63138 ); // 63138	UE4Editor_Core!FDebug::EnsureFailed()
IdList.Remove( 63137 ); // 63137	UE4Editor_Core!NewReportEnsure()
IdList.Remove( 63149 ); // 63149	UE4Editor_Core!FOutputDeviceWindowsError::Serialize()
IdList.Remove( 63151 ); // 63151	UE4Editor_Core!FDebug::AssertFailed()
IdList.Remove( 63445 ); // 63445	UE4Editor_Core!FDebug::EnsureNotFalseFormatted()
IdList.Remove( 64334 ); // 64334	UE4Editor_Core!FOutputDevice::Logf__VA()*/

class CustomFuncComparer : IEqualityComparer<string>
{
    public bool Equals(string x, string y)
    {
        return y.IndexOf( x, StringComparison.InvariantCultureIgnoreCase ) != -1;
    }

    public int GetHashCode(string obj)
    {
        return obj.GetHashCode();
    }
}

string CrashTypeToString( Crashes MyCrash )
{
	if(MyCrash.CrashType==1)
	{
		return "Crash";
	}
	else if(MyCrash.CrashType==2)
	{
		return "Assert";
	}
	else if(MyCrash.CrashType==3)
	{
		return "Ensure";
	}
	return "NULL";
}

void Main()
{
	var LatestCrashes = Crashes
		.Where (c => c.TimeOfCrash > DateTime.UtcNow.AddHours(-1))
		.OrderBy (c => c.Id);
	
	Dictionary<int, string> IdToCallstack = new Dictionary<int,string>();
	Dictionary<int, string> IdToFunc = new Dictionary<int,string>();
	
	int Num = LatestCrashes.Count();
	
	int Cx=0;
	foreach (var Crash in LatestCrashes)
	{
		Cx++;
		string[] Ids = Crash.Pattern.Split( "+".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
		List<int> IdList = new List<int>();
		foreach( string id in Ids )
		{
			int i;
			if( int.TryParse( id, out i ) )
			{
				IdList.Add( i );
			}
		}
		var CallstackEntries = FunctionCalls.Where (fc => IdList.Contains(fc.Id)).ToList();
		
		List<string> CallstackFuncs = new List<string>();
		// Order by Ids
		foreach( int Id in IdList )
		{
			var Found = CallstackEntries.Find( FC => FC.Id == Id );
			CallstackFuncs.Add( Found.Call );
		}
		
		Debug.WriteLine( string.Format( "Crash: {0,4}/{1,4}:{2}", Cx, Num, Crash.Id ));
		Debug.WriteLine( string.Format( " Module: {0} [{1}]", Crash.Module, CrashTypeToString(Crash))  );
		int Nx=0;
		foreach (var Entry in CallstackFuncs)
		{
			Nx++;
			if (Nx > 3)
			{
				break;
			}
			Debug.WriteLine( string.Format( "    {0,2}: {1}", Nx, Entry ) );
		}
		Debug.WriteLine( "" );
	}	
}

// Define other methods and classes here

// Define other methods and classes here