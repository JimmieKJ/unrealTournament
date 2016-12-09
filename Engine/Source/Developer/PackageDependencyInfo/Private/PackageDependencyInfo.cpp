// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PackageDependencyInfo.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Modules/ModuleManager.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectResource.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UObjectIterator.h"
#include "PackageDependencyInfoPrivate.h"
#include "SecureHash.h"
#include "QueuedThreadPool.h"

IMPLEMENT_MODULE( FPackageDependencyInfoModule, PackageDependencyInfo );

DEFINE_LOG_CATEGORY_STATIC(LogPackageDependencyInfo, Log, All);

/**
 * Visitor to gather local files with their timestamps
 */
class FPackageDependencyTimestampVisitor : public IPlatformFile::FDirectoryVisitor
{
private:
	/** The file interface to use for any file operations */
	IPlatformFile& FileInterface;

	/** true if we want directories in this list */
	bool bCacheDirectories;
	
	/** true if we want to generate hases for the files we find */
	bool bGenerateHashes;

	/** 
	 *	A list of directories wildcards that must be in the path to process the folder
	 *	If empty, it will process all directories found
	 */
	TArray<FString> DirectoriesWildcards;

	/** A list of directories that we should not traverse into */
	TArray<FString> DirectoriesToIgnore;

	/** A list of directories that we should only go one level into */
	TArray<FString> DirectoriesToNotRecurse;

	/**
	 * A list of file name wildcards which much match before processing the file
	 */
	TArray<FString> FileWildcards;

public:
	/** Relative paths to local files and their timestamps */
	struct FDependencyInfo
	{
		FDateTime TimeStamp;
		FMD5Hash Hash;

		FDependencyInfo() : TimeStamp(0) { }
	};
	TMap<FString, FDependencyInfo> FileTimes;
	
	FPackageDependencyTimestampVisitor(IPlatformFile& InFileInterface, const TArray<FString>& InDirectoriesWildcards, 
		const TArray<FString>& InDirectoriesToIgnore, const TArray<FString>& InDirectoriesToNotRecurse, const TArray<FString>& InFileWildcards, bool bInCacheDirectories=false, bool bInGenerateHashes=false)
		: FileInterface(InFileInterface)
		, bCacheDirectories(bInCacheDirectories)
		, bGenerateHashes(bInGenerateHashes)
		, FileWildcards( InFileWildcards )
	{
		for (int32 DirIndex = 0; DirIndex < InDirectoriesWildcards.Num(); DirIndex++)
		{
			FString Dir = InDirectoriesWildcards[DirIndex];
			Dir.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
			DirectoriesWildcards.Add(Dir);
		}

		// make sure the paths are standardized, since the Visitor will assume they are standard
		for (int32 DirIndex = 0; DirIndex < InDirectoriesToIgnore.Num(); DirIndex++)
		{
			FString DirToIgnore = InDirectoriesToIgnore[DirIndex];
			FPaths::MakeStandardFilename(DirToIgnore);
			DirectoriesToIgnore.Add(DirToIgnore);
		}

		for (int32 DirIndex = 0; DirIndex < InDirectoriesToNotRecurse.Num(); DirIndex++)
		{
			FString DirToNotRecurse = InDirectoriesToNotRecurse[DirIndex];
			FPaths::MakeStandardFilename(DirToNotRecurse);
			DirectoriesToNotRecurse.Add(DirToNotRecurse);
		}
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		// make sure all paths are "standardized" so the other end can match up with it's own standardized paths
		FString RelativeFilename = FilenameOrDirectory;
		FPaths::MakeStandardFilename(RelativeFilename);

		// cache files and optionally directories
		if (!bIsDirectory)
		{
			bool bShouldProcess = true;
			if (DirectoriesWildcards.Num() > 0)
			{
				bShouldProcess = false;
				// If it is a file, and it does not contain one of the directory wildcards, skip it
				for (int32 DirIndex = 0; DirIndex < DirectoriesWildcards.Num(); DirIndex++)
				{
					if ( RelativeFilename.Contains(DirectoriesWildcards[DirIndex]) )
					{
						bShouldProcess = true;
						break;
					}
				}
			}

			if ( FileWildcards.Num() && bShouldProcess)
			{
				bShouldProcess = false;
				for ( const FString& FileWildcard : FileWildcards )
				{
					if ( RelativeFilename.Contains( FileWildcard ) )
					{
						bShouldProcess = true;
						break;
					}
				}
			}

			if ( bShouldProcess )
			{
				FDependencyInfo DependencyInfo;
				DependencyInfo.TimeStamp = FileInterface.GetTimeStamp(FilenameOrDirectory);
				if (bGenerateHashes)
				{
					IFileHandle* FileHandle = FileInterface.OpenRead(FilenameOrDirectory, false);
					TArray<uint8> LocalScratch;
					LocalScratch.SetNumUninitialized(1024 * 64);
					FMD5 MD5;

					const int64 Size = FileHandle->Size();
					int64 Position = 0;

					// Read in BufferSize chunks
					while (Position < Size)
					{
						const int64 ReadNum = FMath::Min(Size - Position, (int64)LocalScratch.Num());
						FileHandle->Read(LocalScratch.GetData(), ReadNum);
						MD5.Update(LocalScratch.GetData(), ReadNum);

						Position += ReadNum;
					}
					DependencyInfo.Hash.Set(MD5);
				}
				FileTimes.Add(RelativeFilename, DependencyInfo);
			}
		}
		else if (bCacheDirectories)
		{
			// we use a timestamp of 0 to indicate a directory
			FileTimes.Add(RelativeFilename, FDependencyInfo());
		}

		// iterate over directories we care about
		if (bIsDirectory)
		{
			bool bShouldRecurse = true;
			// look in all the ignore directories looking for a match
			for (int32 DirIndex = 0; DirIndex < DirectoriesToIgnore.Num() && bShouldRecurse; DirIndex++)
			{
				if (RelativeFilename.StartsWith(DirectoriesToIgnore[DirIndex]))
				{
					bShouldRecurse = false;
				}
			}

			if (bShouldRecurse == true)
			{
				// If it is a directory that we should not recurse (ie we don't want to process subdirectories of it)
				// handle that case as well...
				for (int32 DirIndex = 0; DirIndex < DirectoriesToNotRecurse.Num() && bShouldRecurse; DirIndex++)
				{
					if (RelativeFilename.StartsWith(DirectoriesToNotRecurse[DirIndex]))
					{
						// Are we more than level deep in that directory?
						FString CheckFilename = RelativeFilename.Right(RelativeFilename.Len() - DirectoriesToNotRecurse[DirIndex].Len());
						if (CheckFilename.Len() > 1)
						{
							bShouldRecurse = false;
						}
					}
				}
			}

			// recurse if we should
			if (bShouldRecurse)
			{
				FileInterface.IterateDirectory(FilenameOrDirectory, *this);
			}
		}

		return true;
	}
};




class FFindModuleSourcePaths : public IPlatformFile::FDirectoryVisitor
{
private:
	/** The file interface to use for any file operations */
	IPlatformFile& FileInterface;
public:
	TMap<FString, FString> SourcePaths;

	FFindModuleSourcePaths(IPlatformFile& InFileInterface)
		: FileInterface(InFileInterface)
	{
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if ( !bIsDirectory )
		{
			FString Filename = FilenameOrDirectory;
			if ( Filename.EndsWith(TEXT(".build.cs")) )
			{
				const FString SourcePath = FPaths::GetPath(Filename);
				FString ModuleName = FPaths::GetCleanFilename(Filename);
				ModuleName.RemoveFromEnd(TEXT(".build.cs"));

				SourcePaths.Add( ModuleName, SourcePath );
			}
		}
		if ( bIsDirectory )
		{
			FileInterface.IterateDirectory(FilenameOrDirectory, *this);
		}

		return true;
	}
};


struct FComparePackageDependencyName
{
	FORCEINLINE bool operator()(const FPackageDependencyTrackingInfo& A, const FPackageDependencyTrackingInfo& B) const
	{
		return A.PackageName < B.PackageName;
	}
};

////
FString FPackageDependencyInfo::ScriptSourcePkgName = TEXT("*** SCRIPTSOURCE ***");
FString FPackageDependencyInfo::ShaderSourcePkgName = TEXT("*** SHADERSOURCE ***");


FPackageDependencyInfo::~FPackageDependencyInfo()
{
	for ( auto& PkgInfo : PackageInformation )
	{
		delete PkgInfo.Value;
	}

	PackageInformation.Empty();
}


void FPackageDependencyInfo::Initialize(bool bInPreProcessAllFiles)
{
	// Prep everything
	DetermineShaderSourceTimeStamp();
	DetermineScriptSourceTimeStamp();
	PrepContentPackageTimeStamps();

	// If requested, go ahead and determine dependency timestamps for all files
	if (bInPreProcessAllFiles == true)
	{
		DetermineAllDependentTimeStamps();
	}
}

bool FPackageDependencyInfo::InitializeDependencyInfo( const TCHAR* InPackageName, FPackageDependencyTrackingInfo*& OutPkgInfo )
{

	FPackageDependencyTrackingInfo** pPkgInfo = PackageInformation.Find(InPackageName);
	if ((pPkgInfo == NULL) || (*pPkgInfo == NULL))
	{
		// We don't have this package?
		UE_LOG(LogPackageDependencyInfo, Display, TEXT("\tPackage Info not found for %s!"), InPackageName);
		return false;
	}

	OutPkgInfo = *pPkgInfo;

	FPackageDependencyTrackingInfo* PkgInfo = *pPkgInfo;
	// Don't process if it was already processed
	if (PkgInfo->bInitializedHashes)
	{
		return true;
	}

	TSet<FPackageDependencyTrackingInfo*> PackagesToDetermineDependencies;
	PackagesToDetermineDependencies.Add( PkgInfo );
	TSet<FPackageDependencyTrackingInfo*> AllPackages;
	AllPackages.Add(PkgInfo);

	bool bNeedIteration = true;
	while ( bNeedIteration )
	{
		bNeedIteration = false;

		TSet<FPackageDependencyTrackingInfo*> NewPackages;
		for ( FPackageDependencyTrackingInfo* CurrentPackage : PackagesToDetermineDependencies )
		{
			DeterminePackageDependencies( CurrentPackage, NewPackages);
		}

		PackagesToDetermineDependencies.Empty(NewPackages.Num());
		for ( FPackageDependencyTrackingInfo* NewPackage : NewPackages )
		{
			bool bProcessed = false;
			AllPackages.Add(NewPackage, &bProcessed);
			if ( bProcessed == false )
			{
				PackagesToDetermineDependencies.Add( NewPackage );
				bNeedIteration = true;
			}
		}
	}

	for (FPackageDependencyTrackingInfo* DepPkgInfo : AllPackages)
	{
		check(DepPkgInfo->bBeingProcessed == false);
		check(DepPkgInfo->bInitializedDependencies == true);
	}

	for (FPackageDependencyTrackingInfo* DepPkgInfo : AllPackages)
	{
		if ( DepPkgInfo->bInitializedHashes || (DepPkgInfo->bValid == false) )
		{
			continue;
		}

		FMD5 Hash;
		FDateTime EarliestTime = DepPkgInfo->TimeStamp;
		RecursiveResolveDependentHash(DepPkgInfo, Hash, EarliestTime);
		DepPkgInfo->DependentHash.Set(Hash);
		DepPkgInfo->DependentTimeStamp = EarliestTime;
		DepPkgInfo->bInitializedHashes = true;
	}

	for (FPackageDependencyTrackingInfo* DepPkgInfo : AllPackages)
	{
		check(DepPkgInfo->bBeingProcessed == false);
		check(DepPkgInfo->bInitializedHashes == true);
	}

	return true;
}

bool FPackageDependencyInfo::DetermineFullPackageHash(const TCHAR* InPackageName, FMD5Hash& OutFullPackageHash)
{
	FPackageDependencyTrackingInfo* PkgInfo = nullptr;
	bool bSuccessful = InitializeDependencyInfo(InPackageName, PkgInfo);

	if (bSuccessful)
	{
		check(PkgInfo != nullptr);
		OutFullPackageHash = PkgInfo->FullPackageHash;
	}
	return bSuccessful;
}

bool FPackageDependencyInfo::GetPackageDependencies(const TCHAR* PackageName, TArray<FString>& OutDependencies)
{
	FPackageDependencyTrackingInfo* PkgInfo = nullptr;
	bool bSuccessful = InitializeDependencyInfo(PackageName, PkgInfo);

	if (bSuccessful)
	{
		check(PkgInfo != nullptr);
		check(PkgInfo->bInitializedHashes == true);


		TArray<FPackageDependencyTrackingInfo*> SortedDependentPackages;
		PkgInfo->DependentPackages.GenerateValueArray(SortedDependentPackages);

		SortedDependentPackages.StableSort(FComparePackageDependencyName());

		for (const FPackageDependencyTrackingInfo* DependentPackage : SortedDependentPackages)
		{
			OutDependencies.Add(DependentPackage->PackageName);
		}

	}
	return bSuccessful;
}


bool FPackageDependencyInfo::DeterminePackageDependentHash(const TCHAR* InPackageName, FMD5Hash& OutDependentHash)
{
	FPackageDependencyTrackingInfo* PkgInfo = nullptr;
	bool bSuccessful = InitializeDependencyInfo(InPackageName, PkgInfo);

	if (bSuccessful)
	{
		check(PkgInfo->bInitializedHashes == true);
		check(PkgInfo != nullptr);
		OutDependentHash = PkgInfo->DependentHash;
	}
	return bSuccessful;
}


bool FPackageDependencyInfo::DeterminePackageDependentTimeStamp(const TCHAR* InPackageName, FDateTime& OutNewestTime)
{
	FPackageDependencyTrackingInfo* PkgInfo = nullptr;
	bool bSuccessful = InitializeDependencyInfo(InPackageName, PkgInfo);

	if ( bSuccessful )
	{
		check(PkgInfo != nullptr);
		OutNewestTime = PkgInfo->DependentTimeStamp;
	}
	return bSuccessful;
}

void FPackageDependencyInfo::DetermineDependentTimeStamps(const TArray<FString>& InPackageList)
{
	FDateTime TempTimeStamp;
	for (int32 PkgIdx = 0; PkgIdx < InPackageList.Num(); PkgIdx++)
	{
		DeterminePackageDependentTimeStamp(*(InPackageList[PkgIdx]), TempTimeStamp);
	}
}

bool FPackageDependencyInfo::RecursiveGetDependentPackageHashes(const TCHAR* InPackageName, TMap<FString, FMD5Hash>& Dependencies)
{
	FPackageDependencyTrackingInfo* PkgInfo = nullptr;
	bool bSuccessful = InitializeDependencyInfo(InPackageName, PkgInfo);

	if ( bSuccessful )
	{
		TArray<FPackageDependencyTrackingInfo*> AllDependentPackages;
		AllDependentPackages.Add(PkgInfo);
		for (int32 I = 0; I < AllDependentPackages.Num(); ++I )
		{
			FPackageDependencyTrackingInfo* Current = AllDependentPackages[I];

			check(Current->bBeingProcessed == false);
			check(Current->bInitializedHashes == true);

			Dependencies.Add( Current->PackageName, Current->FullPackageHash);

			for ( auto& Dependency : Current->DependentPackages )
			{
				AllDependentPackages.AddUnique( Dependency.Value );
			}
		}
	}

	return bSuccessful;
}

void FPackageDependencyInfo::DetermineAllDependentTimeStamps()
{
	FDateTime TempTimeStamp;
	for (TMap<FString, FPackageDependencyTrackingInfo*>::TIterator PkgIt(PackageInformation); PkgIt; ++PkgIt)
	{
		FString& PkgName = PkgIt.Key();
		FPackageDependencyTrackingInfo*& PkgInfo = PkgIt.Value();

		if ( PkgInfo->bInitializedHashes == false )
		{
			FPackageDependencyTrackingInfo* PkgDependencyInfo = nullptr;
			InitializeDependencyInfo(*PkgName, PkgDependencyInfo);
		}
	}

	// GC to ensure packages aren't hanging around
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
}

/** Simple thread safe proxy for TSet<FName> */
template<class T>
class TThreadSafeSet
{
	TSet<T> ArraySet;
	FCriticalSection CriticalSection;
public:
	void Add(T InName)
	{
		FScopeLock Lock(&CriticalSection);
		ArraySet.Add(InName);
	}
	bool Contains(T InName)
	{
		FScopeLock Lock(&CriticalSection);
		return ArraySet.Contains(InName);
	}
	void Remove(T InName)
	{
		FScopeLock Lock(&CriticalSection);
		ArraySet.Remove(InName);
	}
	void Empty()
	{
		FScopeLock Lock(&CriticalSection);
		ArraySet.Empty();
	}
	void GetNames(TSet<T>& OutNames)
	{
		FScopeLock Lock(&CriticalSection);
		OutNames.Append(ArraySet);
	}
};

template<class T>
class TThreadSafeWorkList
{
private:
	TArray<T> InternalArray;
	TSet<T> AllEntries; // all entries that have ever been added to this array, don't allow two
	FCriticalSection CriticalSection;
public:
	TThreadSafeWorkList(const TArray<T>& NewArray)
	{
		FScopeLock Lock(&CriticalSection);
		for (const auto& Item : NewArray)
		{
			Add(Item);
		}
	}

	TThreadSafeWorkList(const TSet<T>& NewArray)
	{
		FScopeLock Lock(&CriticalSection);
		AllEntries = NewArray;
		InternalArray = AllEntries.Array();
	}

	void Add(T Item)
	{
		FScopeLock Lock(&CriticalSection);
		bool bAlreadyInSet;
		AllEntries.Add(Item, &bAlreadyInSet);
		if (!bAlreadyInSet)
		{
			InternalArray.Add(Item);
		}
	}
	bool Pop(T& OutResult)
	{
		FScopeLock Lock(&CriticalSection);
		if ( InternalArray.Num() )
		{
			OutResult = InternalArray[0];
			InternalArray.RemoveAtSwap(0);
			return true;
		}
		return false;
	}
	const TSet<T>& GetAllEntries() const
	{
		return AllEntries;
	}
};



void FPackageDependencyInfo::AsyncDetermineAllDependentPackageInfo(const TArray<FString>& PackageNames, int32 NumAsyncDependencyTasks)
{
	TSet<FPackageDependencyTrackingInfo*> PackageInfoSet;

	if ( PackageNames.Num() == 0 )
	{
		for ( const auto& PackageInfoIt : PackageInformation )
		{
			PackageInfoSet.Add(PackageInfoIt.Value);
		}
	}
	else
	{
		for ( const auto& PackageName : PackageNames )
		{
			FPackageDependencyTrackingInfo* PackageInfo = PackageInformation.FindRef(PackageName);
			if ( PackageInfo == nullptr )
			{
				UE_LOG(LogPackageDependencyInfo, Display, TEXT("Unable to resolve package name %s"), *PackageName);
				continue;
			}
			PackageInfoSet.Add(PackageInfo);
		}
	}
	TThreadSafeWorkList<FPackageDependencyTrackingInfo*> ThreadSafeQueue(PackageInfoSet);


	// worker task
	class FResolvePackageDependencies : public FNonAbandonableTask
	{
	private:
		TThreadSafeWorkList<FPackageDependencyTrackingInfo*>& ThreadSafeQueue;
		FPackageDependencyInfo* PackageDependencyInfo;
	public:
		
		/** Constructor */
		FResolvePackageDependencies(TThreadSafeWorkList<FPackageDependencyTrackingInfo*>& InThreadSafeQueue, FPackageDependencyInfo* InPackageDependencyInfo ) :
			ThreadSafeQueue(InThreadSafeQueue),
			PackageDependencyInfo(InPackageDependencyInfo)
		{
		}

		/** Performs work on thread */
		void DoWork()
		{
			FPackageDependencyTrackingInfo* CurrentPackageInfo = nullptr;
			while (ThreadSafeQueue.Pop(CurrentPackageInfo))
			{
				TSet<FPackageDependencyTrackingInfo*> AllPackages;
				PackageDependencyInfo->DeterminePackageDependencies( CurrentPackageInfo, AllPackages );
				for ( auto& DependencyPackageInfo : AllPackages )
				{
					ThreadSafeQueue.Add(DependencyPackageInfo);
				}
			}
		}
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FResolvePackageDependencies, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

	typedef FAsyncTask<FResolvePackageDependencies> ResolvePackageDependenciesTask;
	TArray<ResolvePackageDependenciesTask *> CurrentTasks;
	
	for ( int I = 0; I < NumAsyncDependencyTasks; ++I )
	{
		auto Task = new ResolvePackageDependenciesTask(ThreadSafeQueue, this);
		CurrentTasks.Add(Task);
		Task->StartBackgroundTask(
#if WITH_EDITOR
			// use the large thread pool when it exists (editor)
			GLargeThreadPool
#endif
		);
	}

	while ( true )
	{
		bool bIsDone = true;
		for ( auto& Task : CurrentTasks )
		{
			if ( !Task->IsDone() )
			{
				bIsDone = false;
				break;
			}
		}

		if ( bIsDone )
			break;

		FPlatformProcess::Sleep( 0.01f );
	}
	for (auto& Task : CurrentTasks)
	{
		check(Task->IsDone());
		delete Task;
	}

	TSet<FPackageDependencyTrackingInfo*> AllPackages = ThreadSafeQueue.GetAllEntries();


	for (auto& DepPkgInfo : AllPackages)
	{
		check(DepPkgInfo->bBeingProcessed == false);
		check(DepPkgInfo->bInitializedDependencies == true || DepPkgInfo->bValid == false);
	}

	for (auto& DepPkgInfo : AllPackages)
	{
		if (DepPkgInfo->bInitializedHashes || (DepPkgInfo->bValid == false))
		{
			continue;
		}

		FMD5 Hash;
		FDateTime EarliestTime = DepPkgInfo->TimeStamp;
		RecursiveResolveDependentHash(DepPkgInfo, Hash, EarliestTime);
		DepPkgInfo->DependentHash.Set(Hash);
		DepPkgInfo->DependentTimeStamp = EarliestTime;
		DepPkgInfo->bInitializedHashes = true;
	}

	for (auto& DepPkgInfo : AllPackages)
	{
		check(DepPkgInfo->bBeingProcessed == false);
		check((DepPkgInfo->bInitializedHashes == true) || (DepPkgInfo->bValid == false));
	}
	// need to clean up linkers created in DeterminePackageDependencies as they were created with NoVerify and can mess up cooker operations
	CollectGarbage(RF_NoFlags, true);
}


void FPackageDependencyInfo::DetermineShaderSourceTimeStamp()
{
	ShaderSourceTimeStamp = FDateTime::MinValue();

	// Get all the shader source files
	FString ShaderSourceDirectory = FPlatformProcess::ShaderDir();

	// use the timestamp grabbing visitor (include directories)
	TArray<FString> DirectoriesWildcards;
	TArray<FString> DirectoriesToIgnore;
	TArray<FString> DirectoriesToNotRecurse;
	TArray<FString> FileWildcards;
	FileWildcards.Add(TEXT(".usf"));

	FPackageDependencyTimestampVisitor TimeStampVisitor(FPlatformFileManager::Get().GetPlatformFile(), DirectoriesWildcards, DirectoriesToIgnore, DirectoriesToNotRecurse, FileWildcards, false, true);
	TimeStampVisitor.Visit(*ShaderSourceDirectory, true);

	FMD5 AllShaderHash;
	TimeStampVisitor.FileTimes.KeySort(TLess<FString>());

	for (TMap<FString, FPackageDependencyTimestampVisitor::FDependencyInfo>::TIterator It(TimeStampVisitor.FileTimes); It; ++It)
	{
		FString ShaderFilename = It.Key();
		check(FPaths::GetExtension(ShaderFilename) == TEXT("usf"));

		AllShaderHash.Update(It.Value().Hash.GetBytes(), It.Value().Hash.GetSize() );

		// It's a shader file
		FDateTime ShaderTimestamp = It.Value().TimeStamp;
		if (ShaderTimestamp > ShaderSourceTimeStamp)
		{
			NewestShaderSource = ShaderFilename;
			ShaderSourceTimeStamp = ShaderTimestamp;
		}
	}

	// Add a 'fake' package tracking info for shader source
	ShaderSourcePkgInfo = new FPackageDependencyTrackingInfo(ShaderSourcePkgName, ShaderSourceTimeStamp);
	ShaderSourcePkgInfo->bInitializedHashes = ShaderSourcePkgInfo->bInitializedDependencies = true;
	ShaderSourcePkgInfo->DependentTimeStamp = ShaderSourceTimeStamp;
	ShaderSourcePkgInfo->FullPackageHash.Set(AllShaderHash);
	ShaderSourcePkgInfo->DependentHash = ShaderSourcePkgInfo->FullPackageHash;
	PackageInformation.Add(ShaderSourcePkgName, ShaderSourcePkgInfo);
}

/** Determine the newest 'script' time stamp */
void FPackageDependencyInfo::DetermineScriptSourceTimeStamp()
{
	DetermineEngineScriptSourceTimeStamp();
	DetermineGameScriptSourceTimeStamp();

	ScriptSourceTimeStamp = EngineScriptSourceTimeStamp;
	if (ScriptSourceTimeStamp < GameScriptSourceTimeStamp)
	{
		ScriptSourceTimeStamp = GameScriptSourceTimeStamp;
	}

	{
		// TODO: don't need this fake script package as the below code can generate package hashes for each script package
		// Add a 'fake' package tracking info for script source
		ScriptSourcePkgInfo = new FPackageDependencyTrackingInfo(ScriptSourcePkgName, ScriptSourceTimeStamp);
		ScriptSourcePkgInfo->DependentTimeStamp = ScriptSourceTimeStamp;
		ScriptSourcePkgInfo->bInitializedHashes = ScriptSourcePkgInfo->bInitializedDependencies = true;
		FMD5 Empty;
		ScriptSourcePkgInfo->FullPackageHash.Set(Empty);
		PackageInformation.Add(ScriptSourcePkgName, ScriptSourcePkgInfo);
	}

#if 1

	for ( TObjectIterator<UPackage> It; It; ++It )
	{
		const UPackage* Package = *It;
		if ( FPackageName::IsScriptPackage(Package->GetName()) )
		{
			const FString ScriptPackageName = Package->GetName();

			// Add package tracking info for script source
			FPackageDependencyTrackingInfo* ScriptPackageInfo = new FPackageDependencyTrackingInfo(ScriptPackageName, ScriptSourceTimeStamp);
			ScriptPackageInfo->bInitializedHashes = ScriptPackageInfo->bInitializedDependencies = true;
			ScriptPackageInfo->DependentTimeStamp = ScriptSourceTimeStamp;
			FMD5 PackageSourceHash;
			FGuid PackageGuid = Package->GetGuid();
			PackageSourceHash.Update( (uint8*)(&PackageGuid), sizeof(FGuid) );
			ScriptPackageInfo->FullPackageHash.Set(PackageSourceHash);
			ScriptPackageInfo->DependentHash = ScriptPackageInfo->FullPackageHash; // set to the same thing so we can treat normal packages and script packages the same way
			PackageInformation.Add(ScriptPackageName, ScriptPackageInfo);
		}
	}

#else
	FFindModuleSourcePaths SourcePaths(FPlatformFileManager::Get().GetPlatformFile());

	SourcePaths.Visit(*FPaths::GameSourceDir(), true);
	SourcePaths.Visit(*FPaths::EngineSourceDir(), true);
	SourcePaths.Visit(*FPaths::EnginePluginsDir(), true);
	SourcePaths.Visit(*FPaths::GamePluginsDir(), true);


	TArray<FName> AllModuleNames;
	FModuleManager::Get().FindModules(TEXT("*"), AllModuleNames);
	for (TArray<FName>::TConstIterator ModuleNameIt(AllModuleNames); ModuleNameIt; ++ModuleNameIt)
	{
		// ScriptPackageNames.Add(*ModuleNameIt, *ConvertToLongScriptPackageName(*ModuleNameIt->ToString()));
		const FString* ModuleSourcePath = SourcePaths.SourcePaths.Find(ModuleNameIt->ToString());
		if (ModuleSourcePath)
		{
			DetermineScriptSourceTimeStampForModule(ModuleNameIt->ToString(), *ModuleSourcePath);
		}
	}
#endif
}

void FPackageDependencyInfo::DetermineScriptSourceTimeStamp(FString PathName)
{
	FDateTime OutNewestTime = FDateTime::MinValue();


	FString ScriptLongPackageName = FString::Printf(TEXT("/Script%s"), *PathName);

	FString ScriptSourceDirectory;
	if ( FPackageName::ConvertRootPathToContentPath(PathName, ScriptSourceDirectory) == false)
	{
		UE_LOG(LogPackageDependencyInfo, Display, TEXT("Unable to convert %s to source directory unable to register script source hashes"), *ScriptLongPackageName);
		return;
	}
	ScriptSourceDirectory = ScriptSourceDirectory.Replace( TEXT("Content"), TEXT("Source"));

	if ( ScriptSourceDirectory.Contains(TEXT("Source")) == false )
	{
		ScriptSourceDirectory /= TEXT("Source");
	}

	if ( FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*ScriptSourceDirectory) )
	{
		UE_LOG(LogPackageDependencyInfo, Display, TEXT("Unable to find %s directory unable to register script source hashes for %s"), *ScriptSourceDirectory, *ScriptLongPackageName);
		return;
	}

	// use the timestamp grabbing visitor (include directories)
	TArray<FString> DirectoriesWildcards;
	DirectoriesWildcards.Add(TEXT("Classes/"));	// @todo uht: Now scanning Public and Private folders, as well as Classes folder
	DirectoriesWildcards.Add(TEXT("Public/"));
	DirectoriesWildcards.Add(TEXT("Private/"));
	TArray<FString> DirectoriesToIgnore;
	TArray<FString> DirectoriesToNotRecurse;
	TArray<FString> FileWildcards;
	FileWildcards.Add(TEXT(".h"));

	FPackageDependencyTimestampVisitor TimeStampVisitor(FPlatformFileManager::Get().GetPlatformFile(), DirectoriesWildcards, DirectoriesToIgnore, DirectoriesToNotRecurse, FileWildcards, false, true);

	TimeStampVisitor.Visit(*ScriptSourceDirectory, true);

	if (TimeStampVisitor.FileTimes.Num() == 0)
	{
		return;
	}

	TimeStampVisitor.FileTimes.KeySort(TLess<FString>());
	FMD5 ScriptMD5;
	for (TMap<FString, FPackageDependencyTimestampVisitor::FDependencyInfo>::TIterator It(TimeStampVisitor.FileTimes); It; ++It)
	{
		FString ScriptFilename = It.Key();
		// It's a 'script' file
		FDateTime ScriptTimestamp = It.Value().TimeStamp;
		if (ScriptTimestamp > OutNewestTime)
		{
			OutNewestTime = ScriptTimestamp;
		}

		FMD5Hash Hash = It.Value().Hash;

		ScriptMD5.Update(Hash.GetBytes(), Hash.GetSize());
	}

	FMD5Hash ScriptHash;
	ScriptHash.Set(ScriptMD5);
	FPackageDependencyTrackingInfo* ScriptSourceInfo = new FPackageDependencyTrackingInfo( ScriptLongPackageName, OutNewestTime );
	ScriptSourceInfo->DependentTimeStamp = OutNewestTime;
	ScriptSourceInfo->FullPackageHash = ScriptHash;

	PackageInformation.Add(ScriptLongPackageName, ScriptSourceInfo);
}

void FPackageDependencyInfo::DetermineScriptSourceTimeStampForModule(const FString& ModuleName, const FString& ModuleSourcePath )
{
#if 0
	
	FString ScriptLongPackageName = FPackageName::ConvertToLongScriptPackageName(*ModuleName);

	UPackage* Package = FindPackage(ANY_PACKAGE, ScriptLongPackageName);



	FMD5Hash ScriptHash;
	ScriptHash.Set(ScriptMD5);
	FPackageDependencyTrackingInfo* ScriptSourceInfo = new FPackageDependencyTrackingInfo(ScriptLongPackageName, OutNewestTime);
	ScriptSourceInfo->DependentTimeStamp = OutNewestTime;
	ScriptSourceInfo->FullPackageHash = ScriptHash;

	PackageInformation.Add(ScriptLongPackageName, ScriptSourceInfo);
#else
	// use the timestamp grabbing visitor (include directories)
	TArray<FString> DirectoriesWildcards;
	DirectoriesWildcards.Add(TEXT("Classes/"));	// @todo uht: Now scanning Public and Private folders, as well as Classes folder
	DirectoriesWildcards.Add(TEXT("Public/"));
	DirectoriesWildcards.Add(TEXT("Private/"));
	TArray<FString> DirectoriesToIgnore;
	TArray<FString> DirectoriesToNotRecurse;
	TArray<FString> FileWildcards;
	FileWildcards.Add(TEXT(".h"));
	FDateTime OutNewestTime = FDateTime::MinValue();

	bool hasHeaderFiles = false;

	FPackageDependencyTimestampVisitor TimeStampVisitor(FPlatformFileManager::Get().GetPlatformFile(), DirectoriesWildcards, DirectoriesToIgnore, DirectoriesToNotRecurse, FileWildcards, false, true);
	TimeStampVisitor.Visit(*ModuleSourcePath, true);


	if ( TimeStampVisitor.FileTimes.Num() == 0 )
	{
		// no dependency here
		return;
	}


	TimeStampVisitor.FileTimes.KeySort(TLess<FString>());
	FMD5 ScriptMD5;

	for (TMap<FString, FPackageDependencyTimestampVisitor::FDependencyInfo>::TIterator It(TimeStampVisitor.FileTimes); It; ++It)
	{
		FString ScriptFilename = It.Key();
		check( FPaths::GetExtension(ScriptFilename) == TEXT("h"));
		// It's a 'script' file
		FDateTime ScriptTimestamp = It.Value().TimeStamp;
		if (ScriptTimestamp > OutNewestTime)
		{
			OutNewestTime = ScriptTimestamp;
		}
		FMD5Hash Hash = It.Value().Hash;
		ScriptMD5.Update(Hash.GetBytes(), Hash.GetSize());
	}


	FString ScriptLongPackageName = FPackageName::ConvertToLongScriptPackageName(*ModuleName);

	FMD5Hash ScriptHash;
	ScriptHash.Set(ScriptMD5);
	FPackageDependencyTrackingInfo* ScriptSourceInfo = new FPackageDependencyTrackingInfo(ScriptLongPackageName, OutNewestTime);
	ScriptSourceInfo->DependentTimeStamp = OutNewestTime;
	ScriptSourceInfo->FullPackageHash = ScriptHash;

	PackageInformation.Add(ScriptLongPackageName, ScriptSourceInfo);
#endif
}

void FPackageDependencyInfo::DetermineEngineScriptSourceTimeStamp()
{
	DetermineScriptSourceTimeStamp(true, EngineScriptSourceTimeStamp);
}

void FPackageDependencyInfo::DetermineGameScriptSourceTimeStamp()
{
	DetermineScriptSourceTimeStamp(false, GameScriptSourceTimeStamp);
}

void FPackageDependencyInfo::DetermineScriptSourceTimeStamp(bool bInGame, FDateTime& OutNewestTime)
{
	OutNewestTime = FDateTime::MinValue();

	// @todo uht: This code ignores Plugins and Developer code!
	FString ScriptSourceDirectory = bInGame ? (FPaths::GameDir() / TEXT("Source")) : (FPaths::EngineDir() / TEXT("Source") / TEXT("Runtime"));

	// use the timestamp grabbing visitor (include directories)
	TArray<FString> DirectoriesWildcards;
	DirectoriesWildcards.Add(TEXT("Classes/"));	// @todo uht: Now scanning Public and Private folders, as well as Classes folder
	DirectoriesWildcards.Add(TEXT("Public/"));	
	DirectoriesWildcards.Add(TEXT("Private/"));
	TArray<FString> DirectoriesToIgnore;
	TArray<FString> DirectoriesToNotRecurse;
	TArray<FString> FileWildcards;
	FileWildcards.Add(TEXT(".h"));

	FPackageDependencyTimestampVisitor TimeStampVisitor(FPlatformFileManager::Get().GetPlatformFile(), DirectoriesWildcards, DirectoriesToIgnore, DirectoriesToNotRecurse, FileWildcards, false);
	TimeStampVisitor.Visit(*ScriptSourceDirectory, true);
	for (TMap<FString, FPackageDependencyTimestampVisitor::FDependencyInfo>::TIterator It(TimeStampVisitor.FileTimes); It; ++It)
	{
		FString ScriptFilename = It.Key();
		if (FPaths::GetExtension(ScriptFilename) == TEXT("h"))
		{
			// It's a 'script' file
			FDateTime ScriptTimestamp = It.Value().TimeStamp;
			if (ScriptTimestamp > OutNewestTime)
			{
				OutNewestTime = ScriptTimestamp;
			}
		}
	}
}

void FPackageDependencyInfo::PrepContentPackageTimeStamps()
{
	TArray<FString> ContentDirectories;
	{
		TArray<FString> RootContentPaths;
		FPackageName::QueryRootContentPaths( RootContentPaths );
		for( TArray<FString>::TConstIterator RootPathIt( RootContentPaths ); RootPathIt; ++RootPathIt )
		{
			const FString& RootPath = *RootPathIt;
			const FString& ContentFolder = FPackageName::LongPackageNameToFilename( RootPath );
			ContentDirectories.Add( ContentFolder );
		}
	}

	// use the timestamp grabbing visitor (include directories)
	TArray<FString> DirectoriesWildcards;
	TArray<FString> DirectoriesToIgnore;
	TArray<FString> DirectoriesToNotRecurse;
	TArray<FString> FileWildcards;

	for (int DirIdx = 0; DirIdx < ContentDirectories.Num(); DirIdx++)
	{
		FPackageDependencyTimestampVisitor TimeStampVisitor(FPlatformFileManager::Get().GetPlatformFile(), DirectoriesWildcards, DirectoriesToIgnore, DirectoriesToNotRecurse, FileWildcards, false);
		TimeStampVisitor.Visit(*ContentDirectories[DirIdx], true);
		for (TMap<FString, FPackageDependencyTimestampVisitor::FDependencyInfo>::TIterator It(TimeStampVisitor.FileTimes); It; ++It)
		{
			FString ContentFilename = It.Key();
			if ( FPackageName::IsPackageExtension(*FPaths::GetExtension(ContentFilename, true)) )
			{
				FDateTime ContentTimestamp = It.Value().TimeStamp;
				//ContentFilename = FPaths::GetBaseFilename(ContentFilename, false);
				// Add it to the pkg info mapping
				FPackageDependencyTrackingInfo* NewInfo = new FPackageDependencyTrackingInfo(ContentFilename, ContentTimestamp);
				PackageInformation.Add(ContentFilename, NewInfo);
			}
		}
	}
}


bool FPackageDependencyInfo::DeterminePackageDependencies(FPackageDependencyTrackingInfo* PkgInfo, TSet<FPackageDependencyTrackingInfo*>& AllPackages)
{
	check(PkgInfo->bBeingProcessed == false);


	if ( PkgInfo->bInitializedDependencies )
	{
		return true;
	}

	checkf((PkgInfo->bInitializedDependencies == false), TEXT("RecursiveDeterminePackageDependentTimeStamp: Package initialized: %s"), *PkgInfo->PackageName);

	// We have the package info, so process the actual package.
	BeginLoad();
	auto Linker = GetPackageLinker(NULL, *PkgInfo->PackageName, LOAD_NoVerify, NULL, NULL);
	EndLoad();
	if (Linker != NULL)
	{
		PkgInfo->bBeingProcessed = true;
		{
			// generate hash for file
			FArchive* Loader = Linker->Loader;
			int64 OriginalPos = Loader->Tell();
			Loader->Seek(0);
			PkgInfo->FullPackageHash = FMD5Hash::HashFileFromArchive(Loader);
			Loader->Seek(OriginalPos);
		}
		// Start off with setting the dependent time to the package itself
		PkgInfo->TimeStamp = PkgInfo->TimeStamp;

		// Map? Code (ie blueprint)?
		PkgInfo->bContainsMap = Linker->ContainsMap();
		PkgInfo->bContainsBlueprints = Linker->ContainsCode();

		static const FName CheckMaterial(TEXT("Material"));
		static const FName CheckMIC(TEXT("MaterialInstanceConstant"));
		static const FName CheckMID(TEXT("MaterialInstanceDynamic"));
		static const FName CheckLMIC(TEXT("LandscapeMaterialInstanceConstant"));
		static const FName CheckWorld(TEXT("World"));
		static const FName CheckBlueprint(TEXT("Blueprint"));
		static const FName CheckAnimBlueprint(TEXT("AnimBlueprint"));

		// Check the export map for material interfaces
		for (int32 ExpIdx = 0; ExpIdx < Linker->ExportMap.Num(); ExpIdx++)
		{
			FObjectExport& ObjExp = Linker->ExportMap[ExpIdx];
			FName ExpClassName = Linker->GetExportClassName(ExpIdx);
			if ((ExpClassName == CheckMaterial) ||
				(ExpClassName == CheckMIC) ||
				(ExpClassName == CheckMID) ||
				(ExpClassName == CheckLMIC))
			{
				PkgInfo->bContainsShaders = true;
				PkgInfo->DependentPackages.Add(ShaderSourcePkgName, ShaderSourcePkgInfo);
				AllPackages.Add(ShaderSourcePkgInfo);
			}
			else if (ExpClassName == CheckWorld)
			{
				PkgInfo->bContainsMap = true;
			}
			else if ((ExpClassName == CheckBlueprint) ||
				(ExpClassName == CheckAnimBlueprint))
			{
				PkgInfo->bContainsBlueprints = true;
				PkgInfo->DependentPackages.Add(ScriptSourcePkgName, ScriptSourcePkgInfo);
				AllPackages.Add(ScriptSourcePkgInfo);
			}
		}

		// Check the dependencies
		//@todo. Make this a function of the linker? Almost the exact same code is used in PkgInfo commandlet...
		FName LinkerName = Linker->LinkerRoot->GetFName();
		TArray<FName> DependentPackages;
		for (int32 ImpIdx = 0; ImpIdx < Linker->ImportMap.Num(); ImpIdx++)
		{
			FObjectImport& ObjImp = Linker->ImportMap[ImpIdx];

			FName PackageName = NAME_None;
			FName OuterName = NAME_None;
			if (!ObjImp.OuterIndex.IsNull())
			{
				// Find the package which contains this import.  import.SourceLinker is cleared in EndLoad, so we'll need to do this manually now.
				FPackageIndex OutermostLinkerIndex = ObjImp.OuterIndex;
				for (FPackageIndex LinkerIndex = ObjImp.OuterIndex; !LinkerIndex.IsNull();)
				{
					OutermostLinkerIndex = LinkerIndex;
					LinkerIndex = Linker->ImpExp(LinkerIndex).OuterIndex;
				}
				PackageName = Linker->ImpExp(OutermostLinkerIndex).ObjectName;
			}

			if (PackageName == NAME_None && ObjImp.ClassName == NAME_Package)
			{
				PackageName = ObjImp.ObjectName;
			}

			if ((PackageName != NAME_None) && (PackageName != LinkerName))
			{
				DependentPackages.AddUnique(PackageName);
			}

			if ((ObjImp.ClassPackage != NAME_None) && (ObjImp.ClassPackage != LinkerName))
			{
				DependentPackages.AddUnique(ObjImp.ClassPackage);
			}
		}

		for (int32 DependentIdx = 0; DependentIdx < DependentPackages.Num(); DependentIdx++)
		{
			FString PkgName = DependentPackages[DependentIdx].ToString();
			FText Reason;
			if (!FPackageName::IsValidLongPackageName(PkgName, true, &Reason))
			{
				//UE_LOG(LogPackageDependencyInfo, Display, TEXT("%s --> %s"), *PkgName, *Reason.ToString());
				continue;
			}

			FString LongName = PkgName;
			if (!FPackageName::IsScriptPackage(LongName))
			{
				LongName = FPackageName::LongPackageNameToFilename(PkgName);
			}

			FPackageDependencyTrackingInfo** pDepPkgInfo = PackageInformation.Find(LongName);
			if ((pDepPkgInfo == NULL) || (*pDepPkgInfo == NULL))
			{
				continue;
			}

			FPackageDependencyTrackingInfo* DepPkgInfo = *pDepPkgInfo;
			PkgInfo->DependentPackages.Add(LongName, DepPkgInfo);
			AllPackages.Add(DepPkgInfo);
		}

		DependentPackages.StableSort();
		

		PkgInfo->bInitializedDependencies = true;
		PkgInfo->bBeingProcessed = false;

		if ( Linker->LinkerRoot->HasAnyInternalFlags(EInternalObjectFlags::Async) )
		{
			Linker->LinkerRoot->ClearInternalFlags(EInternalObjectFlags::Async);
		}
	}
	else
	{
		UE_LOG(LogPackageDependencyInfo, Warning, TEXT("RecursiveDeterminePackageDependentTimeStamp: Failed to find linker for %s"), *PkgInfo->PackageName);
		PkgInfo->bValid = false;
		return false;
	}
	return true;
}

void FPackageDependencyInfo::RecursiveResolveDependentHash(FPackageDependencyTrackingInfo* PkgInfo, FMD5 &OutHash, FDateTime& EarliestTime)
{
	if (PkgInfo != NULL)
	{
		if ( PkgInfo->bBeingProcessed )
		{
			// circular reference
			return;
		}
		
		PkgInfo->bBeingProcessed = true;


		check(PkgInfo->bInitializedDependencies);

		if ( PkgInfo->TimeStamp < EarliestTime )
		{
			check( PkgInfo->TimeStamp != FDateTime::MinValue() );
			EarliestTime = PkgInfo->TimeStamp;
		}

		check( PkgInfo->FullPackageHash.IsValid() );

		OutHash.Update( PkgInfo->FullPackageHash.GetBytes(), PkgInfo->FullPackageHash.GetSize() );

		TArray<FPackageDependencyTrackingInfo*> SortedDependentPackages;
		PkgInfo->DependentPackages.GenerateValueArray(SortedDependentPackages);


		SortedDependentPackages.StableSort(FComparePackageDependencyName());
		

		for (FPackageDependencyTrackingInfo* DependentPackage : SortedDependentPackages)
		{
			RecursiveResolveDependentHash(DependentPackage, OutHash, EarliestTime );
		}

		PkgInfo->bBeingProcessed = false;
	}
}

////
FPackageDependencyInfo* FPackageDependencyInfoModule::PackageDependencyInfo = NULL;

void FPackageDependencyInfoModule::StartupModule()
{
	PackageDependencyInfo = new FPackageDependencyInfo();
	checkf(PackageDependencyInfo, TEXT("PackageDependencyInfo module failed to create instance!"));
	PackageDependencyInfo->Initialize(false);
}

void FPackageDependencyInfoModule::ShutdownModule()
{
	delete PackageDependencyInfo;
	PackageDependencyInfo = NULL;
}

bool FPackageDependencyInfoModule::DeterminePackageDependentTimeStamp(const TCHAR* InPackageName, FDateTime& OutNewestTime)
{
	check(PackageDependencyInfo);
	return PackageDependencyInfo->DeterminePackageDependentTimeStamp(InPackageName, OutNewestTime);
}

bool FPackageDependencyInfoModule::DeterminePackageDependentHash(const TCHAR* InPackageName, FMD5Hash& OutDependentHash)
{
	check(PackageDependencyInfo);
	return PackageDependencyInfo->DeterminePackageDependentHash(InPackageName, OutDependentHash);
}

bool FPackageDependencyInfoModule::DetermineFullPackageHash(const TCHAR* InPackageName, FMD5Hash& OutFullPackageHash)
{
	check(PackageDependencyInfo);
	return PackageDependencyInfo->DetermineFullPackageHash(InPackageName, OutFullPackageHash);
}

void FPackageDependencyInfoModule::DetermineDependentTimeStamps(const TArray<FString>& InPackageList)
{
	check(PackageDependencyInfo);
	return PackageDependencyInfo->DetermineDependentTimeStamps(InPackageList);
}

void FPackageDependencyInfoModule::DetermineAllDependentTimeStamps()
{
	check(PackageDependencyInfo);
	return PackageDependencyInfo->DetermineAllDependentTimeStamps();
}

void FPackageDependencyInfoModule::AsyncDetermineAllDependentPackageInfo(const TArray<FString>& PackageNames, int32 NumThreads)
{
	check(PackageDependencyInfo);
	return PackageDependencyInfo->AsyncDetermineAllDependentPackageInfo(PackageNames, NumThreads);
}

bool FPackageDependencyInfoModule::RecursiveGetDependentPackageHashes(const TCHAR* InPackageName, TMap<FString, FMD5Hash>& Dependencies)
{
	check(PackageDependencyInfo);
	return PackageDependencyInfo->RecursiveGetDependentPackageHashes(InPackageName, Dependencies);
}

void FPackageDependencyInfoModule::GetAllPackageDependentInfo(TMap<FString, FPackageDependencyTrackingInfo*>& OutPkgDependencyInfo)
{
	check(PackageDependencyInfo);
	OutPkgDependencyInfo = PackageDependencyInfo->PackageInformation;
}

bool FPackageDependencyInfoModule::GetPackageDependencies( const TCHAR* PackageName, TArray<FString>& OutPackageNames )
{
	check( PackageDependencyInfo);
	return PackageDependencyInfo->GetPackageDependencies(PackageName, OutPackageNames);
}