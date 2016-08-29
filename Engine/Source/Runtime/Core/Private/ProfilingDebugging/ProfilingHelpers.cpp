// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
 * code everywhere.  And we can have consistent naming for all our files.
 *
 */

// Core includes.
#include "CorePrivatePCH.h"
#include "EngineVersion.h"


// find where these are really defined
const static int32 MaxFilenameLen = 100;
DEFINE_LOG_CATEGORY(LogHistograms);

#if WITH_ENGINE
FGetMapNameDelegate GGetMapNameDelegate;
#endif

void FHistogram::InitLinear(double MinTime, double MaxTime, double BinSize)
{
	double CurrentBinMin = MinTime;
	while (CurrentBinMin < MaxTime)
	{
		Bins.Add(FBin(CurrentBinMin, CurrentBinMin + BinSize));
		CurrentBinMin += BinSize;
	}
	Bins.Add(FBin(CurrentBinMin));	// catch-all-above bin
}

void FHistogram::InitHitchTracking()
{
	Bins.Empty();

	Bins.Add(FBin(0.0, 9.0));		// >= 120 fps
	Bins.Add(FBin(9.0, 17.0));		// 60 - 120 fps
	Bins.Add(FBin(17.0, 34.0));		// 30 - 60 fps
	Bins.Add(FBin(34.0, 50.0));		// 20 - 30 fps
	Bins.Add(FBin(50.0, 67.0));		// 15 - 20 fps
	Bins.Add(FBin(67.0, 100.0));	// 10 - 15 fps
	Bins.Add(FBin(100.0, 200.0));	// 5 - 10 fps
	Bins.Add(FBin(200.0, 300.0));	// < 5 fps
	Bins.Add(FBin(300.0, 500.0));
	Bins.Add(FBin(500.0, 750.0));
	Bins.Add(FBin(750.0, 1000.0));
	Bins.Add(FBin(1000.0, 1500.0));
	Bins.Add(FBin(1500.0, 2000.0));
	Bins.Add(FBin(2000.0, 2500.0));
	Bins.Add(FBin(2500.0, 5000.0));
	Bins.Add(FBin(5000.0));
}

void FHistogram::Reset()
{
	for (FBin& Bin : Bins)
	{
		Bin.Count = 0;
		Bin.Sum = 0.0;
	}
}

void FHistogram::AddMeasurement(double Value)
{
	if (LIKELY(Bins.Num()))
	{
		FBin& FirstBin = Bins[0];
		if (UNLIKELY(Value < FirstBin.MinValue))
		{
			return;
		}

		for (int BinIdx = 0, LastBinIdx = Bins.Num() - 1; BinIdx < LastBinIdx; ++BinIdx)
		{
			FBin& Bin = Bins[BinIdx];
			if (Bin.UpperBound > Value)
			{
				++Bin.Count;
				Bin.Sum += Value;
				return;
			}
		}

		// if we got here, Value did not fit in any of the previous bins
		FBin& LastBin = Bins.Last();
		++LastBin.Count;
		LastBin.Sum += Value;
	}
}

void FHistogram::DumpToAnalytics(const FString& ParamNamePrefix, TArray<TPair<FString, double>>& OutParamArray)
{
	double TotalSum = 0;
	double TotalObservations = 0;
	double AverageObservation = 0;

	if (LIKELY(Bins.Num()))
	{
		for (int BinIdx = 0, LastBinIdx = Bins.Num() - 1; BinIdx < LastBinIdx; ++BinIdx)
		{
			FBin& Bin = Bins[BinIdx];
			FString ParamName = FString::Printf(TEXT("_%.0f_%.0f"), Bin.MinValue, Bin.UpperBound);
			OutParamArray.Add(TPairInitializer<FString, double>(ParamNamePrefix + ParamName + TEXT("_Count"), Bin.Count));
			OutParamArray.Add(TPairInitializer<FString, double>(ParamNamePrefix + ParamName + TEXT("_Sum"), Bin.Sum));

			TotalObservations += Bin.Count;
			TotalSum += Bin.Sum;
		}

		FBin& LastBin = Bins.Last();
		FString ParamName = FString::Printf(TEXT("_%.0f_AndAbove"), LastBin.MinValue);
		OutParamArray.Add(TPairInitializer<FString, double>(ParamNamePrefix + ParamName + TEXT("_Count"), LastBin.Count));
		OutParamArray.Add(TPairInitializer<FString, double>(ParamNamePrefix + ParamName + TEXT("_Sum"), LastBin.Sum));

		TotalObservations += LastBin.Count;
		TotalSum += LastBin.Sum;
	}

	if (TotalObservations > 0)
	{
		AverageObservation = TotalSum / TotalObservations;
	}

	// add an average value for ease of monitoring/analyzing
	OutParamArray.Add(TPairInitializer<FString, double>(ParamNamePrefix + TEXT("_Average"), AverageObservation));
}

void FHistogram::SetBinCountByIndex(int32 IdxBin, int32 Count)
{
	if (Bins.IsValidIndex(IdxBin))
	{
		Bins[IdxBin].Count = Count;
	}
}

void FHistogram::SetBinSumByIndex(int IdxBin, double InSum)
{
	if (Bins.IsValidIndex(IdxBin))
	{
		Bins[IdxBin].Sum = InSum;
	}
}

void FHistogram::DumpToLog(const FString& HistogramName)
{
	UE_LOG(LogHistograms, Log, TEXT("Histogram '%s': %d bins"), *HistogramName, Bins.Num());

	if (LIKELY(Bins.Num()))
	{
		double TotalSum = 0;
		double TotalObservations = 0;

		for (int BinIdx = 0, LastBinIdx = Bins.Num() - 1; BinIdx < LastBinIdx; ++BinIdx)
		{
			FBin& Bin = Bins[BinIdx];
			UE_LOG(LogHistograms, Log, TEXT("Bin %4.0f - %4.0f: %5d observation(s) which sum up to %f"), Bin.MinValue, Bin.UpperBound, Bin.Count, Bin.Sum);

			TotalObservations += Bin.Count;
			TotalSum += Bin.Sum;
		}

		FBin& LastBin = Bins.Last();
		UE_LOG(LogHistograms, Log, TEXT("Bin %4.0f +     : %5d observation(s) which sum up to %f"), LastBin.MinValue, LastBin.Count, LastBin.Sum);
		TotalObservations += LastBin.Count;
		TotalSum += LastBin.Sum;

		if (TotalObservations > 0)
		{
			UE_LOG(LogHistograms, Log, TEXT("Average value for observation: %f"), TotalSum / TotalObservations);
		}
	}
}


/**
 * This will get the changelist that should be used with the Automated Performance Testing
 * If one is passed in we use that otherwise we use the FEngineVersion::Current().GetChangelist().  This allows
 * us to have build machine built .exe and test them. 
 *
 * NOTE: had to use AutomatedBenchmarking as the parsing code is flawed and doesn't match
 *       on whole words.  so automatedperftestingChangelist was failing :-( 
 **/
int32 GetChangeListNumberForPerfTesting()
{
	int32 Retval = FEngineVersion::Current().GetChangelist();

	// if we have passed in the changelist to use then use it
	int32 FromCommandLine = 0;
	FParse::Value( FCommandLine::Get(), TEXT("-gABC="), FromCommandLine );

	// we check for 0 here as the CIS always appends -AutomatedPerfChangelist but if it
	// is from the "built" builds when it will be a 0
	if( FromCommandLine != 0 )
	{
		Retval = FromCommandLine;
	}

	return Retval;
}

// can be optimized or moved to a more central spot
bool IsValidCPPIdentifier(const FString& In)
{
	bool bFirstChar = true;

	for (auto& Char : In)
	{
		if (!bFirstChar && Char >= TCHAR('0') && Char <= TCHAR('9'))
		{
			// 0..9 is allowed unless on first character
			continue;
		}

		bFirstChar = false;

		if (Char == TCHAR('_')
			|| (Char >= TCHAR('a') && Char <= TCHAR('z'))
			|| (Char >= TCHAR('A') && Char <= TCHAR('Z')))
		{
			continue;
		}

		return false;
	}

	return true;
}

FString GetBuildNameForPerfTesting()
{
	FString BuildName;

	if (FParse::Value(FCommandLine::Get(), TEXT("-BuildName="), BuildName))
	{
		// we take the specified name
		if (!IsValidCPPIdentifier(BuildName))
		{
			UE_LOG(LogInit, Error, TEXT("The name specified by -BuildName=<name> is not valid (needs to follow C++ identifier rules)"));
			BuildName.Empty();
		}
	}
	
	if(BuildName.IsEmpty())
	{
		// we generate a build name from the change list number
		int32 ChangeListNumber = GetChangeListNumberForPerfTesting();

		BuildName = FString::Printf(TEXT("CL%d"), ChangeListNumber);
	}

	return BuildName;
}


/**
 * This makes it so UnrealConsole will open up the memory profiler for us
 *
 * @param NotifyType has the <namespace>:<type> (e.g. UE_PROFILER!UE3STATS:)
 * @param FullFileName the File name to copy from the console
 **/
void SendDataToPCViaUnrealConsole( const FString& NotifyType, const FString& FullFileName )
{
	const FString AbsoluteFilename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead( *FullFileName );

	UE_LOG( LogProfilingDebugging, Warning, TEXT( "SendDataToPCViaUnrealConsole %s%s" ), *NotifyType, *AbsoluteFilename );

	const FString NotifyString = NotifyType + AbsoluteFilename + LINE_TERMINATOR;
	
	// send it across via UnrealConsole
	FMsg::SendNotificationString( *NotifyString );
}


FString CreateProfileFilename(const FString& InFileExtension, bool bIncludeDateForDirectoryName)
{
	return CreateProfileFilename(TEXT(""), InFileExtension, bIncludeDateForDirectoryName);
}

/** 
 * This will generate the profiling file name that will work with limited filename sizes on consoles.
 * We want a uniform naming convention so we will all just call this function.
 *
 * @param ProfilingType this is the type of profiling file this is
 * 
 **/
FString CreateProfileFilename( const FString& InFilename, const FString& InFileExtension, bool bIncludeDateForDirectoryName )
{
	FString Retval;

	// set up all of the parts we will use
	FString MapNameStr;

#if WITH_ENGINE
	if(GGetMapNameDelegate.IsBound())
	{
		MapNameStr = GGetMapNameDelegate.Execute();
	}
	else
	{
		MapNameStr = TEXT( "LoadTimeFile" );
	}
#endif		// WITH_ENGINE

	const FString PlatformStr(FPlatformProperties::PlatformName());

	/** This is meant to hold the name of the "sessions" that is occurring **/
	static bool bSetProfilingSessionFolderName = false;
	static FString ProfilingSessionFolderName = TEXT(""); 

	// here we want to have just the same profiling session name so all of the files will go into that folder over the course of the run so you don't just have a ton of folders
	FString FolderName;
	if( bSetProfilingSessionFolderName == false )
	{
		// now create the string
		FolderName = FString::Printf(TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *FDateTime::Now().ToString(TEXT("%m.%d-%H.%M.%S")));
		FolderName = FolderName.Right(MaxFilenameLen);

		ProfilingSessionFolderName = FolderName;
		bSetProfilingSessionFolderName = true;
	}
	else
	{
		FolderName = ProfilingSessionFolderName;
	}

	// now create the string
	// NOTE: due to the changelist this is implicitly using the same directory
	FString FolderNameOfProfileNoDate = FString::Printf( TEXT("%s-%s-%i"), *MapNameStr, *PlatformStr, GetChangeListNumberForPerfTesting() );
	FolderNameOfProfileNoDate = FolderNameOfProfileNoDate.Right(MaxFilenameLen);


	FString NameOfProfile;
	if (InFilename.IsEmpty())
	{
		NameOfProfile = FString::Printf(TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *FDateTime::Now().ToString(TEXT("%d-%H.%M.%S")));
	}
	else
	{
		NameOfProfile = InFilename;
	}
	NameOfProfile = NameOfProfile.Right(MaxFilenameLen);

	FString FileNameWithExtension = FString::Printf( TEXT("%s%s"), *NameOfProfile, *InFileExtension );
	FileNameWithExtension = FileNameWithExtension.Right(MaxFilenameLen);

	FString Filename;
	if( bIncludeDateForDirectoryName == true )
	{
		Filename = FolderName / FileNameWithExtension;
	}
	else
	{
		Filename = FolderNameOfProfileNoDate / FileNameWithExtension;
	}


	Retval = Filename;

	return Retval;
}



FString CreateProfileDirectoryAndFilename( const FString& InSubDirectoryName, const FString& InFileExtension )
{
	FString MapNameStr;
#if WITH_ENGINE
	check(GGetMapNameDelegate.IsBound());
	MapNameStr = GGetMapNameDelegate.Execute();
#endif		// WITH_ENGINE
	const FString PlatformStr(FPlatformProperties::PlatformName());


	// create Profiling dir and sub dir
	const FString PathName = (FPaths::ProfilingDir() + InSubDirectoryName + TEXT("/"));
	IFileManager::Get().MakeDirectory( *PathName );
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *PathName );

	// create the directory name of this profile
	FString NameOfProfile = FString::Printf(TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *FDateTime::Now().ToString(TEXT("%m.%d-%H.%M")));	
	NameOfProfile = NameOfProfile.Right(MaxFilenameLen);

	IFileManager::Get().MakeDirectory( *(PathName+NameOfProfile) );
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *(PathName+NameOfProfile) );


	// create the actual file name
	FString FileNameWithExtension = FString::Printf( TEXT("%s%s"), *NameOfProfile, *InFileExtension );
	FileNameWithExtension = FileNameWithExtension.Left(MaxFilenameLen);
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *FileNameWithExtension );


	FString Filename = PathName / NameOfProfile / FileNameWithExtension;
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *Filename );

	return Filename;
}
