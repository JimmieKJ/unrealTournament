// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookOnTheFlyServer.h : handles polite cook requests via network ;)
=============================================================================*/

#pragma once

#include "IPlatformFileSandboxWrapper.h"
#include "CookOnTheFlyServer.generated.h"


class FChunkManifestGenerator;

enum class ECookInitializationFlags
{
	None =										0x0000, // No flags
	Compressed =								0x0001, // will save compressed packages
	Iterative =									0x0002, // use iterative cooking (previous cooks will not be cleaned unless detected out of date, experimental)
	SkipEditorContent =							0x0004, // do not cook any content in the content\editor directory
	Unversioned =								0x0008, // save the cooked packages without a version number
	AutoTick =									0x0010, // enable ticking (only works in the editor)
	AsyncSave =									0x0020, // save packages async
	IncludeServerMaps =							0x0080, // should we include the server maps when cooking
	UseSerializationForPackageDependencies =	0x0100, // should we use the serialization code path for generating package dependencies (old method will be deprecated)
	BuildDDCInBackground =						0x0200, // build ddc content in background while the editor is running (only valid for modes which are in editor IsCookingInEditor())
	GeneratedAssetRegistry =					0x0400, // have we generated asset registry yet
	OutputVerboseCookerWarnings =				0x0800, // output additional cooker warnings about content issues
	MarkupInUsePackages =						0x1000, // mark up with an object flag objects which are in packages which we are about to use or in the middle of using, this means we can gc more often but only gc stuff which we have finished with
	TestCook =									0x2000, // test the cooker garbage collection process and cooking (cooker will never end just keep testing).
};
ENUM_CLASS_FLAGS(ECookInitializationFlags);

enum class ECookByTheBookOptions
{
	None =								0x00000000, // no flags
	CookAll	=							0x00000001, // cook all maps and content in the content directory
	MapsOnly =							0x00000002, // cook only maps
	NoDevContent =						0x00000004, // don't include dev content
	LeakTest =							0x00000008, // test for uobject leaks after each level load
	ForceDisableCompressed =			0x00000010, // force compression to be disabled even if the cooker was initialized with it enabled
	ForceEnableCompressed =				0x00000020, // force compression to be on even if the cooker was initialized with it disabled
	ForceDisableSaveGlobalShaders =		0x00000040, // force global shaders to not be saved (used if cooking multiple times for the same platform and we know we are up todate)
	NoGameAlwaysCookPackages =			0x00000080, // don't include the packages specified by the game in the cook (this cook will probably be missing content unless you know what you are doing)
	NoAlwaysCookMaps =					0x00000100, // don't include always cook maps (this cook will probably be missing content unless you know what you are doing)
	NoDefaultMaps =						0x00000200, // don't include default cook maps (this cook will probably be missing content unless you know what you are doing)
	NoSlatePackages =					0x00000400, // don't include slate content (this cook will probably be missing content unless you know what you are doing)
	NoInputPackages =					0x00000800, // don't include slate content (this cook will probably be missing content unless you know what you are doing)
	DisableUnsolicitedPackages =		0x00001000, // don't cook any packages which aren't in the files to cook list (this is really dangerious as if you request a file it will not cook all it's dependencies automatically)
};
ENUM_CLASS_FLAGS(ECookByTheBookOptions);


UENUM()
namespace ECookMode
{
	enum Type
	{
		/** Default mode, handles requests from network. */
		CookOnTheFly,
		/** Cook on the side. */
		CookOnTheFlyFromTheEditor,
		/** Precook all resources while in the editor. */
		CookByTheBookFromTheEditor,
		/** Cooking by the book (not in the editor). */
		CookByTheBook,
	};
}

DECLARE_STATS_GROUP(TEXT("Cooking"), STATGROUP_Cooking, STATCAT_Advanced);

UCLASS()
class UNREALED_API UCookOnTheFlyServer : public UObject, public FTickableEditorObject, public FExec
{
	GENERATED_BODY()

	UCookOnTheFlyServer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:

	/** Array which has been made thread safe :) */
	template<typename Type, typename SynchronizationObjectType, typename ScopeLockType>
	struct FUnsynchronizedQueue
	{
	private:
		mutable SynchronizationObjectType	SynchronizationObject; // made this mutable so this class can have const functions and still be thread safe
		TArray<Type>		Items;
	public:
		void Enqueue(const Type& Item)
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			Items.Add(Item);
		}
		void EnqueueUnique( const Type& Item )
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			Items.AddUnique(Item);
		}
		bool Dequeue(Type* Result)
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			if (Items.Num())
			{
				*Result = Items[0];
				Items.RemoveAt(0);
				return true;
			}
			return false;
		}
		void DequeueAll(TArray<Type>& Results)
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			Results += Items;
			Items.Empty();
		}

		bool HasItems() const
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			return Items.Num() > 0;
		}

		void Remove( const Type& Item ) 
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			Items.Remove( Item );
		}

		void CopyItems( TArray<Type> &InItems ) const
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			InItems = Items;
		}

		int Num() const 
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			return Items.Num();
		}

		void Empty()
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			Items.Empty();
		}
	};



	struct FDummyCriticalSection
	{
	public:
		FORCEINLINE void Lock() { }
		FORCEINLINE void Unlock() { }
	};

	struct FDummyScopeLock
	{
	public:
		FDummyScopeLock( FDummyCriticalSection * ) { }
	};

public:


	template<typename Type>
	struct FThreadSafeQueue : public FUnsynchronizedQueue<Type, FCriticalSection, FScopeLock>
	{
		/**
		 * Don't add any functions here, this is just a overqualified typedef
		 * Add functions / functionality to the FUnsynchronizedQueue
		 */
	};
	
	template<typename Type>
	struct FQueue : public FUnsynchronizedQueue<Type, FDummyCriticalSection, FDummyScopeLock>
	{
		/**
		 * Don't add any functions here, this is just a overqualified typedef
		 * Add functions / functionality to the FUnsynchronizedQueue
		 */
	};

public:
	/** cooked file requests which includes platform which file is requested for */
	struct FFilePlatformRequest
	{
	protected:
		FName Filename;
		TArray<FName> Platformnames;
	public:
		
		// yes we have some friends
		friend uint32 GetTypeHash(const UCookOnTheFlyServer::FFilePlatformRequest& Key);

		FFilePlatformRequest() { }


		FFilePlatformRequest( const FName& InFileName, const FName& InPlatformName ) : Filename( InFileName )
		{ Platformnames.Add( InPlatformName ); }

		FFilePlatformRequest( const FName& InFilename, const TArray<FName>& InPlatformname ) : Filename( InFilename )
		{ Platformnames = InPlatformname; }

		FFilePlatformRequest( const FName& InFilename, TArray<FName>&& InPlatformname ) : Filename( InFilename ), Platformnames(MoveTemp(InPlatformname)) { }
		FFilePlatformRequest( const FFilePlatformRequest& InFilePlatformRequest ) : Filename( InFilePlatformRequest.Filename ), Platformnames( InFilePlatformRequest.Platformnames ) { }
		FFilePlatformRequest( FFilePlatformRequest&& InFilePlatformRequest ) : Filename( MoveTemp(InFilePlatformRequest.Filename) ), Platformnames( MoveTemp(InFilePlatformRequest.Platformnames) ) { }
		
		void SetFilename( const FString &InFilename ) 
		{
			Filename = FName(*InFilename);
		}

		const FName &GetFilename() const
		{
			return Filename;
		}

		const TArray<FName> &GetPlatformnames() const
		{
			return Platformnames;
		}

		void RemovePlatform( const FName &Platform )
		{
			Platformnames.Remove(Platform);
		}

		void AddPlatform( const FName &Platform )
		{
			check( Platform != NAME_None );
			Platformnames.Add(Platform );
		}

		bool HasPlatform( const FName &Platform ) const
		{
			return Platformnames.Find(Platform) != INDEX_NONE;
		}

		bool IsValid()  const
		{
			return Filename != NAME_None;
		}

		void Clear()
		{
			Filename = TEXT("");
			Platformnames.Empty();
		}

		FFilePlatformRequest &operator=( FFilePlatformRequest &&InFileRequest )
		{
			Filename = MoveTemp( InFileRequest.Filename );
			Platformnames = MoveTemp( InFileRequest.Platformnames );
			return *this;
		}

		bool operator ==( const FFilePlatformRequest &InFileRequest ) const
		{
			if ( InFileRequest.Filename == Filename )
			{
				if ( InFileRequest.Platformnames == Platformnames )
				{
					return true;
				}
			}
			return false;
		}

		FORCEINLINE FString &&ToString() const
		{
			FString Result = FString::Printf(TEXT("%s;"), *Filename.ToString());

			for ( const auto &Platform : Platformnames )
			{
				Result += FString::Printf(TEXT("%s,"), *Platform.ToString() );
			}
			return MoveTemp(Result);
		}

	};


private:

	struct FFilePlatformCookedPackage : public FFilePlatformRequest
	{
	private:
		bool bSucceededSavePackage;
	public:
		FFilePlatformCookedPackage(const FFilePlatformRequest& InFilePlatformRequest, bool bInSuccededSavePackage) : FFilePlatformRequest(InFilePlatformRequest.GetFilename(), InFilePlatformRequest.GetPlatformnames()), bSucceededSavePackage(bInSuccededSavePackage) { }
		FFilePlatformCookedPackage(const FName& InFilename, const TArray<FName>& InPlatformname, bool bInSuccededSavePackage) : FFilePlatformRequest(InFilename, InPlatformname), bSucceededSavePackage(bInSuccededSavePackage) { }
		FFilePlatformCookedPackage(const FName& InFilename, TArray<FName>&& InPlatformname, bool bInSuccededSavePackage) : FFilePlatformRequest(InFilename, MoveTemp(InPlatformname)), bSucceededSavePackage(bInSuccededSavePackage) { }
		FFilePlatformCookedPackage(const FFilePlatformCookedPackage& InFilePlatformRequest) : FFilePlatformRequest(InFilePlatformRequest.Filename, InFilePlatformRequest.Platformnames), bSucceededSavePackage(InFilePlatformRequest.bSucceededSavePackage) { }
		FFilePlatformCookedPackage(FFilePlatformCookedPackage&& InFilePlatformRequest) : FFilePlatformRequest(MoveTemp(InFilePlatformRequest.Filename), MoveTemp(InFilePlatformRequest.Platformnames)), bSucceededSavePackage(InFilePlatformRequest.bSucceededSavePackage) { }

		inline const bool HasSucceededSavePackage() const { return bSucceededSavePackage; }
	};

	/** Helper list of all files which have been cooked */
	struct FThreadSafeFilenameSet
	{
	private:
		mutable FCriticalSection	SynchronizationObject;
		TMap<FName, FFilePlatformCookedPackage> FilesProcessed;
	public:

		void Lock()
		{
			SynchronizationObject.Lock();
		}
		void Unlock()
		{
			SynchronizationObject.Unlock();
		}

		void Add(const FFilePlatformCookedPackage& Request)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			check(Request.IsValid());

			// see if it's already in the requests list
			FFilePlatformRequest *ExistingRequest = FilesProcessed.Find(Request.GetFilename() );
			
			if ( ExistingRequest )
			{
				check( ExistingRequest->GetFilename() == Request.GetFilename() );
				for ( const auto &Platform : Request.GetPlatformnames() )
				{
					ExistingRequest->AddPlatform(Platform);
				}
			}
			else
				FilesProcessed.Add(Request.GetFilename(), Request);
		}
		bool Exists(const FFilePlatformRequest & Request) const
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			const FFilePlatformCookedPackage* OurRequest = FilesProcessed.Find(Request.GetFilename());
			
			if (!OurRequest)
			{
				return false;
			}

			// make sure all the platforms are completed
			for ( const auto& Platform : Request.GetPlatformnames() )
			{
				if ( !OurRequest->GetPlatformnames().Contains( Platform ) )
				{
					return false;
				}
			}

			return true;
			// return FilesProcessed.Contains(Filename);
		}
		// two versions of this function so I don't have to create temporary FFIleplatformRequest in some cases to call the exists function
		bool Exists( const FName& Filename, const TArray<FName>& PlatformNames ) const
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			const FFilePlatformRequest* OurRequest = FilesProcessed.Find( Filename );

			if (!OurRequest)
			{
				return false;
			}

			// make sure all the platforms are completed
			for ( const auto& Platform : PlatformNames )
			{
				if ( !OurRequest->GetPlatformnames().Contains( Platform ) )
				{
					return false;
				}
			}

			return true;
		}

		void RemoveAllFilesForPlatform( const FName& PlatformName )
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			for ( auto& Request : FilesProcessed )
			{	
				Request.Value.RemovePlatform( PlatformName );
			}
		}

		bool GetCookedPlatforms( const FName& Filename, TArray<FName>& PlatformList ) const
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			const FFilePlatformCookedPackage* Request = FilesProcessed.Find(Filename);
			if ( Request )
			{
				PlatformList = Request->GetPlatformnames();
				return true;
			}
			return false;
		}
		int RemoveFile( const FName& Filename )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			return FilesProcessed.Remove( Filename );
		}

		bool RemoveFileForPlatform( const FName& Filename, const FName& PlatformName )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			FFilePlatformCookedPackage* ProcessedFile = FilesProcessed.Find(Filename);
			if( ProcessedFile )
			{
				ProcessedFile->RemovePlatform(PlatformName);
				return true; 
			}
			return false;
		}

		void GetCookedFilesForPlatform(const FName& PlatformName, TArray<FName>& CookedFiles, bool bGetFailedCookedPackages = true, bool bGetSuccessfulCookedPackages = true)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			for (const auto& CookedFile : FilesProcessed)
			{
				if (CookedFile.Value.HasPlatform(PlatformName))
				{
					bool bHasSucceededSavePackage = CookedFile.Value.HasSucceededSavePackage();
					if ((bHasSucceededSavePackage && bGetSuccessfulCookedPackages) ||
						((bHasSucceededSavePackage == false) && bGetFailedCookedPackages) )
					CookedFiles.Add(CookedFile.Value.GetFilename());
				}
			}
		}
		void Empty(int32 ExpectedNumElements = 0)
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			FilesProcessed.Empty(ExpectedNumElements);
		}
	};


	struct FFilenameQueue
	{
	private:
		TArray<FName>			Queue;
		TMap<FName, TArray<FName>> PlatformList;
		mutable FCriticalSection SynchronizationObject;
	public:

		template <class PREDICATE_CLASS>
		void Sort(const PREDICATE_CLASS& Predicate)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			Queue.Sort(Predicate);
		}

		const TArray<FName> &GetQueue() const { return Queue;  }
		void EnqueueUnique(const FFilePlatformRequest& Request, bool ForceEnqueFront = false)
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			TArray<FName>* Platforms = PlatformList.Find(Request.GetFilename());
			if ( Platforms == NULL )
			{
				PlatformList.Add(Request.GetFilename(), Request.GetPlatformnames());
				Queue.Add(Request.GetFilename());
			}
			else
			{
				// add the requested platforms to the platform list
				for ( const auto& Platform : Request.GetPlatformnames() )
				{
					Platforms->AddUnique(Platform);
				}
			}

			if ( ForceEnqueFront )
			{
				int32 Index = Queue.Find(Request.GetFilename());
				check( Index != INDEX_NONE );
				if ( Index != 0 )
				{
					Queue.Swap(0, Index);
				}
			}
		}

		bool Dequeue(FFilePlatformRequest* Result)
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			if (Queue.Num())
			{
				FName Filename = Queue[0];
				Queue.RemoveAt(0);
				TArray<FName> Platforms = PlatformList.FindChecked(Filename);
				PlatformList.Remove(Filename);
				*Result = MoveTemp(FFilePlatformRequest(MoveTemp(Filename), MoveTemp(Platforms)));
				return true;
			}
			return false;
		}

		void DequeueAllRequests( TArray<FFilePlatformRequest>& RequestArray )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			if ( Queue.Num() )
			{
				for ( const auto& Request : PlatformList )
				{
					RequestArray.Add( FFilePlatformRequest( Request.Key, Request.Value ) );
				}
				PlatformList.Empty();
				Queue.Empty();
			}
		}

		bool Exists( const FName& Filename, const TArray<FName>& PlatformNames ) const 
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			const TArray<FName>* Platforms = PlatformList.Find( Filename );
			if ( Platforms == NULL )
				return false;

			for ( const auto& PlatformName : PlatformNames )
			{
				if ( !Platforms->Contains( PlatformName ) )
					return false;
			}
			return true;
		}

		bool Exists(const FName& Filename)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			const TArray<FName>* Platforms = PlatformList.Find(Filename);
			if (Platforms == NULL)
				return false;
			return true;
		}
		
		bool HasItems() const
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			return Queue.Num() > 0;
		}

		int Num() const 
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			return Queue.Num();
		}


		void Empty()  
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			Queue.Empty();
			PlatformList.Empty();
		}
	};

	struct FThreadSafeUnsolicitedPackagesList
	{
	private:
		FCriticalSection SyncObject;
		TArray<FFilePlatformRequest> CookedPackages;
	public:
		void AddCookedPackage( const FFilePlatformRequest& PlatformRequest )
		{
			FScopeLock S( &SyncObject );
			CookedPackages.Add( PlatformRequest );
		}
		void GetPackagesForPlatformAndRemove( const FName& Platform, TArray<FName> PackageNames )
		{
			FScopeLock S( &SyncObject );
			
			for ( int I = CookedPackages.Num()-1; I >= 0; --I )
			{
				FFilePlatformRequest &Request = CookedPackages[I];

				if ( Request.GetPlatformnames().Contains( Platform ) )
				{
					// remove the platform
					Request.RemovePlatform( Platform );

					if ( Request.GetPlatformnames().Num() == 0 )
					{
						CookedPackages.RemoveAt(I);
					}
				}
			}
		}
		void Empty()
		{
			FScopeLock S( &SyncObject );
			CookedPackages.Empty();
		}
	};


	struct FCachedPackageFilename
	{
	public:
		FCachedPackageFilename(FString &&InPackageFilename, FString &&InStandardFilename, FName InStandardFileFName ) :
			PackageFilename( MoveTemp( InPackageFilename )),
			StandardFilename(MoveTemp(InStandardFilename)),
			StandardFileFName( InStandardFileFName )
		{
		}

		FCachedPackageFilename( const FCachedPackageFilename &In )
		{
			PackageFilename = In.PackageFilename;
			StandardFilename = In.StandardFilename;
			StandardFileFName = In.StandardFileFName;
		}

		FCachedPackageFilename( FCachedPackageFilename &&In )
		{
			PackageFilename = MoveTemp(In.PackageFilename);
			StandardFilename = MoveTemp(In.StandardFilename);
			StandardFileFName = In.StandardFileFName;
		}

		FString PackageFilename; // this is also full path
		FString StandardFilename;
		FName StandardFileFName;
	};

	/** Simple thread safe proxy for TSet<FName> */
	class FThreadSafeNameSet
	{
		TSet<FName> Names;
		FCriticalSection NamesCritical;
	public:
		void Add(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			Names.Add(InName);
		}
		bool AddUnique(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			if (!Names.Contains(InName))
			{
				Names.Add(InName);
				return true;
			}
			return false;
		}
		bool Contains(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			return Names.Contains(InName);
		}
		void Remove(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			Names.Remove(InName);
		}
		void Empty()
		{
			FScopeLock NamesLock(&NamesCritical);
			Names.Empty();
		}
		void GetNames(TSet<FName>& OutNames)
		{
			FScopeLock NamesLock(&NamesCritical);
			OutNames.Append(Names);
		}
	};
private:
	/** Current cook mode the cook on the fly server is running in */
	ECookMode::Type CurrentCookMode;
	/** Directory to output to instead of the default should be empty in the case of DLC cooking */ 
	FString OutputDirectoryOverride;
	//////////////////////////////////////////////////////////////////////////
	// Cook by the book options
public:
	struct FChildCooker
	{
		FChildCooker() : ReadPipe(nullptr), ReturnCode(-1), bFinished(false),  Thread(nullptr) { }

		FProcHandle ProcessHandle;
		FString ResponseFileName;
		FString BaseResponseFileName;
		void* ReadPipe;
		int32 ReturnCode;
		bool bFinished;
		FRunnableThread* Thread;
	};
private:
	struct FCookByTheBookOptions
	{
	public:
		FCookByTheBookOptions() : bLeakTest(false),
			bGenerateStreamingInstallManifests(false),
			bGenerateDependenciesForMaps(false),
			bRunning(false),
			CookTime( 0.0 ),
			CookStartTime( 0.0 ), 
			bErrorOnEngineContentUse(false),
			bForceEnableCompressedPackages(false),
			bForceDisableCompressedPackages(false),
			bIsChildCooker(false),
			bDisableUnsolicitedPackages(false),
			ChildCookIdentifier(-1)
		{ }

		/** Should we test for UObject leaks */
		bool bLeakTest;
		/** Should we generate streaming install manifests (only valid option in cook by the book) */
		bool bGenerateStreamingInstallManifests;
		/** Should we generate a seperate manifest for map dependencies */
		bool bGenerateDependenciesForMaps;
		/** Is cook by the book currently running */
		bool bRunning;
		/** Cancel has been queued will be processed next tick */
		bool bCancel;
		/** DlcName setup if we are cooking dlc will be used as the directory to save cooked files to */
		FString DlcName;
		/** Create a release from this manifest and store it in the releases directory for this cgame */
		FString CreateReleaseVersion;
		/** Leak test: last gc items (only valid when running from commandlet requires gc between each cooked package) */
		TSet<FWeakObjectPtr> LastGCItems;
		/** Map of platform name to manifest generator, manifest is only used in cook by the book however it needs to be maintained across multiple cook by the books. */
		TMap<FName, FChunkManifestGenerator*> ManifestGenerators;
		/** Dependency graph of maps as root objects. */
		TMap<FName, TMap< FName, TSet <FName> > > MapDependencyGraphs; 
		/** If a cook is cancelled next cook will need to resume cooking */ 
		TArray<FFilePlatformRequest> PreviousCookRequests; 
		/** If we are based on a release version of the game this is the set of packages which were cooked in that release */
		TMap<FName,TArray<FName> > BasedOnReleaseCookedPackages;
		/** Timing information about cook by the book */
		double CookTime;
		double CookStartTime;
		/** error when detecting engine content being used in this cook */
		bool bErrorOnEngineContentUse;
		/** force this cook by the book to enable \ disable package compression */
		bool bForceEnableCompressedPackages;
		bool bForceDisableCompressedPackages;
		bool bIsChildCooker;
		bool bDisableUnsolicitedPackages;
		int32 ChildCookIdentifier;
		FString ChildCookFilename;
		TSet<FName> ChildUnsolicitedPackages;
		TArray<FChildCooker> ChildCookers;
		TArray<FName> TargetPlatformNames;
		
	};
	FCookByTheBookOptions* CookByTheBookOptions;
	

	//////////////////////////////////////////////////////////////////////////
	// Cook on the fly options
	/** Cook on the fly server uses the NetworkFileServer */
	TArray<class INetworkFileServer*> NetworkFileServers;

	//////////////////////////////////////////////////////////////////////////
	// General cook options
	TArray<UClass*> FullGCAssetClasses;
	/** Number of packages to load before performing a garbage collect. Set to 0 to never GC based on number of loaded packages */
	uint32 PackagesPerGC;
	/** Amount of time that is allowed to be idle before forcing a garbage collect. Set to 0 to never force GC due to idle time */
	double IdleTimeToGC;
	/** Max memory the cooker should use before forcing a gc */
	uint64 MaxMemoryAllowance;
	/** Min memory before the cooker should partial gc */
	uint64 MinMemoryBeforeGC;
	/** If we have less then this much memory free then finish current task and kick off gc */
	uint64 MinFreeMemory;
	/** Max number of packages to save before we partial gc */
	int32 MaxNumPackagesBeforePartialGC;
	/** Num packages saved since last partial gc */
	int32 NumPackagesSavedSinceLastPartialGC;

	ECookInitializationFlags CookFlags;
	TAutoPtr<class FSandboxPlatformFile> SandboxFile;
	bool bIsInitializingSandbox; // stop recursion into callbacks when we are initializing sandbox
	bool bIsSavingPackage; // used to stop recursive mark package dirty functions

	//////////////////////////////////////////////////////////////////////////
	// cook in editor specific
	TArray<FWeakObjectPtr> CachedMaterialsToCacheArray;
	TArray<FWeakObjectPtr> CachedTexturesToCacheArray;
	int32 LastUpdateTick;
	int32 MaxPrecacheShaderJobs;
	void TickPrecacheObjectsForPlatforms(const float TimeSlice, const TArray<const ITargetPlatform*>& TargetPlatform);

	//////////////////////////////////////////////////////////////////////////
	
	// data about the current packages being processed
	// stores temporal state like finished cache as an optimization so we don't need to 
	struct FReentryData
	{
		FName FileName;
		bool bBeginCacheFinished;
		int BeginCacheCount;
		bool bFinishedCacheFinished;
		TArray<UObject*> CachedObjectsInOuter;

		FReentryData() : FileName(NAME_None), bBeginCacheFinished(false), BeginCacheCount(0), bFinishedCacheFinished(false)
		{ }

		void Reset( const FName& InFilename )
		{
			FileName = InFilename;
			bBeginCacheFinished = false;
			BeginCacheCount = 0;
		}
	};

	TMap<FName, FReentryData> PackageReentryData;

	FReentryData& GetReentryData(const UPackage* Package);

	FThreadSafeQueue<struct FRecompileRequest*> RecompileRequests;
	FFilenameQueue CookRequests; // list of requested files
	FThreadSafeUnsolicitedPackagesList UnsolicitedCookedPackages;
	FThreadSafeFilenameSet CookedPackages; // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
	FThreadSafeNameSet NeverCookPackageList;
	FThreadSafeNameSet UncookedEditorOnlyPackages; // set of packages that have been rejected due to being referenced by editor-only properties


	FString GetCachedPackageFilename( const FName& PackageName ) const;
	FString GetCachedStandardPackageFilename( const FName& PackageName ) const;
	FName GetCachedStandardPackageFileFName( const FName& PackageName ) const;
	FString GetCachedPackageFilename( const UPackage* Package ) const;
	FString GetCachedStandardPackageFilename( const UPackage* Package ) const;
	FName GetCachedStandardPackageFileFName( const UPackage* Package ) const;
	const FString& GetCachedSandboxFilename( const UPackage* Package, TAutoPtr<class FSandboxPlatformFile>& SandboxFile ) const;
	const FName* GetCachedPackageFilenameToPackageFName(const FName& StandardPackageFilename) const;
	const FCachedPackageFilename& Cache(const FName& PackageName) const;
	void ClearPackageFilenameCache() const;
	bool ClearPackageFilenameCacheForPackage( const UPackage* Package ) const;
	bool ClearPackageFilenameCacheForPackage( const FName& PackageName ) const;

	FString ConvertCookedPathToUncookedPath(const FString& CookedPackageName) const;

	// get dependencies for this package 
	const TArray<FName>& GetFullPackageDependencies(const FName& PackageName) const;
	mutable TMap<FName, TArray<FName>> CachedFullPackageDependencies;

	// declared mutable as it's used to cache package filename strings and I don't want to declare all functions using it as non const
	// used by GetCached * Filename functions
	mutable TMap<FName, FCachedPackageFilename> PackageFilenameCache; // filename cache (only process the string operations once)
	mutable TMap<FName, FName> PackageFilenameToPackageFNameCache;

	// declared mutable as it's used purely as a cache and don't want to have to declare all the functions as non const just because of this cache
	// used by IniSettingsOutOfDate and GetCurrentIniStrings 
	mutable TMap<FName, TArray<FString>> CachedIniVersionStringsMap;

public:

	enum ECookOnTheSideResult
	{
		COSR_CookedMap				= 0x00000001,
		COSR_CookedPackage			= 0x00000002,
		COSR_ErrorLoadingPackage	= 0x00000004,
		COSR_RequiresGC				= 0x00000008,
		COSR_WaitingOnCache			= 0x00000010,
		COSR_WaitingOnChildCookers	= 0x00000020,
		COSR_MarkedUpKeepPackages	= 0x00000040
	};


	virtual ~UCookOnTheFlyServer();

	/**
	 * FTickableEditorObject interface used by cook on the side
	 */
	TStatId GetStatId() const override;
	void Tick(float DeltaTime) override;
	bool IsTickable() const override;
	ECookMode::Type GetCookMode() const { return CurrentCookMode; }


	/**
	 * FExec interface used in the editor
	 */
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Initialize the CookServer so that either CookOnTheFly can be called or Cook on the side can be started and ticked
	 */
	void Initialize( ECookMode::Type DesiredCookMode, ECookInitializationFlags InCookInitializationFlags, const FString &OutputDirectoryOverride = FString(TEXT("")) );

	/**
	 * Cook on the side, cooks while also running the editor...
	 *
	 * @param  BindAnyPort					Whether to bind on any port or the default port.
	 *
	 * @return true on success, false otherwise.
	 */
	bool StartNetworkFileServer( bool BindAnyPort );

	/**
	 * Broadcast our the fileserver presence on the network
	 */
	bool BroadcastFileserverPresence( const FGuid &InstanceId );
	/** 
	 * Stop the network file server
	 *
	 */
	void EndNetworkFileServer();


	struct FCookByTheBookStartupOptions
	{
	public:
		TArray<ITargetPlatform*> TargetPlatforms;
		TArray<FString> CookMaps;
		TArray<FString> CookDirectories;
		TArray<FString> NeverCookDirectories;
		TArray<FString> CookCultures; 
		TArray<FString> IniMapSections;
		TArray<FString> CookPackages; // list of packages we should cook, used to specify specific packages to cook
		ECookByTheBookOptions CookOptions;
		FString DLCName;
		FString CreateReleaseVersion;
		FString BasedOnReleaseVersion;
		FString ChildCookFileName; // if we are the child cooker 
		int32 ChildCookIdentifier; // again, only if you are the child cooker
		bool bGenerateStreamingInstallManifests; 
		bool bGenerateDependenciesForMaps; 
		bool bErrorOnEngineContentUse; // this is a flag for dlc, will cause the cooker to error if the dlc references engine content
		int32 NumProcesses;
		FCookByTheBookStartupOptions() :
			CookOptions(ECookByTheBookOptions::None),
			DLCName(FString()),
			ChildCookIdentifier(-1),
			bGenerateStreamingInstallManifests(false),
			bGenerateDependenciesForMaps(false),
			bErrorOnEngineContentUse(false),
			NumProcesses(0)
		{ }
	};

	/**
	 * Start a cook by the book session
	 * Cook on the fly can't run at the same time as cook by the book
	 */
	void StartCookByTheBook( const FCookByTheBookStartupOptions& CookByTheBookStartupOptions );

	/**
	 * ValidateCookByTheBookSettings
	 * look at the cookByTheBookOptions and ensure there isn't any conflicting settings
	 */
	void ValidateCookByTheBookSettings() const;

	/**
	 * Queue a cook by the book cancel (you might want to do this instead of calling cancel directly so that you don't have to be in the game thread when canceling
	 */
	void QueueCancelCookByTheBook();

	/**
	 * Cancel the currently running cook by the book (needs to be called from the game thread)
	 */
	void CancelCookByTheBook();

	bool IsCookByTheBookRunning() const;

	/**
	 * Get any packages which are in memory, these were probably required to be loaded because of the current package we are cooking, so we should probably cook them also
	 */
	void GetUnsolicitedPackages(TArray<UPackage*>& PackagesToSave, bool &ContainsFullGCAssetClasses, const TArray<FName>& TargetPlatformNames) const;

	/**
	 * Handles cook package requests until there are no more requests, then returns
	 *
	 * @param  CookFlags output of the things that might have happened in the cook on the side
	 * 
	 * @return returns ECookOnTheSideResult
	 */
	uint32 TickCookOnTheSide( const float TimeSlice, uint32 &CookedPackagesCount );

	/**
	 * Editor Tick, special tick which is called only when used from the editor (IsCookingInEditor)
	 *
	 * @param Timeslice, duration this function is allowed to run in
	 * @param RequestedTargetPlatform, keep this target platform up to date this is the platform we are likely to launch on next
	 */
	void EditorTick( const float Timeslice, const TArray<const ITargetPlatform*>& RequestedTargetPlatform);

	/**
	 * Clear all the previously cooked data all cook requests from now on will be considered recook requests
	 */
	void ClearAllCookedData();

	/**
	 * Clear all the previously cooked data for the platform passed in 
	 * 
	 * @param name of the platform to clear the cooked packages for
	 */
	void ClearPlatformCookedData( const FName& PlatformName );


	/**
	 * Recompile any global shader changes 
	 * if any are detected then clear the cooked platform data so that they can be rebuilt
	 * 
	 * @return return true if shaders were recompiled
	 */
	bool RecompileChangedShaders(const TArray<FName>& TargetPlatforms);


	/**
	 * Force stop whatever pending cook requests are going on and clear all the cooked data
	 * Note cook on the side / cook on the fly clients may not be able to recover from this if they are waiting on a cook request to complete
	 */
	void StopAndClearCookedData();

	/** 
	 * Process any shader recompile requests
	 */
	void TickRecompileShaderRequests();

	bool HasCookRequests() const { return CookRequests.HasItems(); }

	bool HasRecompileShaderRequests() const { return RecompileRequests.HasItems(); }

	uint32 NumConnections() const;

	/**
	 * Is this cooker running in the editor
	 *
	 * @return true if we are running in the editor
	 */
	bool IsCookingInEditor() const;

	/**
	 * Is this cooker running in real time mode (where it needs to respect the timeslice) 
	 * 
	 * @return returns true if this cooker is running in realtime mode like in the editor
	 */
	bool IsRealtimeMode() const;

	/**
	 * Helper function returns if we are in any cook by the book mode
	 *
	 * @return if the cook mode is a cook by the book mode
	 */
	bool IsCookByTheBookMode() const;

	/**
	 * Helper function returns if we are in any cook on the fly mode
	 *
	 * @return if the cook mode is a cook on the fly mode
	 */
	bool IsCookOnTheFlyMode() const;
	

	virtual void BeginDestroy() override;

	/**
	 * SetFullGCAssetClasses FullGCAssetClasses is used to determine when TickCookOnTheSide returns RequiresGC
	 *   When one of these classes is saved it will return COSR_RequiresGC
	 */
	void SetFullGCAssetClasses( const TArray<UClass*>& InFullGCAssetClasses );

	/** Returns the configured number of packages to process before GC */
	uint32 GetPackagesPerGC() const;

	/** Returns the configured amount of idle time before forcing a GC */
	double GetIdleTimeToGC() const;

	/** Returns the configured amount of memory allowed before forcing a GC */
	uint64 GetMaxMemoryAllowance() const;

	/** Mark package as keep around for the cooker (don't GC) */
	void MarkGCPackagesToKeepForCooker();

	bool HasExceededMaxMemory() const;

	/**
	* RequestPackage to be cooked
	*
	* @param StandardPackageFName name of the package in standard format as returned by FPaths::MakeStandardFilename
	* @param TargetPlatforms name of the targetplatforms we want this package cooked for
	* @param bForceFrontOfQueue should we put this package in the front of the cook queue (next to be processed) or at the end
	*/
	bool RequestPackage(const FName& StandardPackageFName, const TArray<FName>& TargetPlatforms, const bool bForceFrontOfQueue);

	/**
	* RequestPackage to be cooked
	* This function can only be called while the cooker is in cook by the book mode
	*
	* @param StandardPackageFName name of the package in standard format as returned by FPaths::MakeStandardFilename
	* @param bForceFrontOfQueue should we put this package in the front of the cook queue (next to be processed) or at the end
	*/
	bool RequestPackage(const FName& StandardPackageFName, const bool bForceFrontOfQueue);


	/**
	 * Callbacks from editor 
	 */

	void OnObjectModified( UObject *ObjectMoving );
	void OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);
	void OnObjectUpdated( UObject *Object );
	void OnObjectSaved( UObject *ObjectSaved );

	/**
	 * Marks a package as dirty for cook
	 * causes package to be recooked on next request (and all dependent packages which are currently cooked)
	 */
	void MarkPackageDirtyForCooker( UPackage *Package );

	/**
	 * MaybeMarkPackageAsAlreadyLoaded
	 * Mark the package as already loaded if we have already cooked the package for all requested target platforms
	 * this hints to the objects on load that we don't need to load all our bulk data
	 * 
	 * @param Package to mark as not requiring reload
	 */
	void MaybeMarkPackageAsAlreadyLoaded(UPackage* Package);

	/**
	 * Callbacks from UObject globals
	 */
	void PreGarbageCollect();
	void OnStringAssetReferenceLoadedPackage(const FName& PackageName);


private:
	
	//////////////////////////////////////////////////////////////////////////
	// cook by the book specific functions
	/**
	 * Collect all the files which need to be cooked for a cook by the book session
	 */
	void CollectFilesToCook(TArray<FName>& FilesInPath, 
		const TArray<FString>& CookMaps, const TArray<FString>& CookDirectories, const TArray<FString>& CookCultures, 
		const TArray<FString>& IniMapSections, ECookByTheBookOptions FilesToCookFlags);

	/**
	 * AddFileToCook add file to cook list 
	 */
	void AddFileToCook( TArray<FName>& InOutFilesToCook, const FString &InFilename ) const;

	/**
	 * Call back from the TickCookOnTheSide when a cook by the book finishes (when started form StartCookByTheBook)
	 */
	void CookByTheBookFinished();

	/**
	 * StartChildCookers to help out with cooking
	 * only valid in cook by the book (not from the editor)
	 * 
	 * @param NumChildCookersToSpawn, number of child cookers we want to use
	 */
	void StartChildCookers(int32 NumChildCookersToSpawn, const TArray<FName>& TargetPlatformNames, const FString& ExtraCmdParams = FString());

	/**
	 * TickChildCookers
	 * output the information form the child cookers to the main cooker output
	 * 
	 * @return return true if all child cookers are finished
	 */
	bool TickChildCookers();

	/**
	 * CleanUPChildCookers
	 * can only be called after TickChildCookers returns true
	 */
	void CleanUpChildCookers();

	/**
	 * Get all the packages which are listed in asset registry passed in.  
	 *
	 * @param AssetRegistryPath path of the assetregistry.bin file to read
	 * @param OutPackageNames out list of packages which were contained in the asset registry file
	 * @return true if successfully read false otherwise
	 */
	bool GetAllPackagesFromAssetRegistry( const FString& AssetRegistryPath, TArray<FName>& OutPackageNames ) const;

	/**
	 * BuildMapDependencyGraph
	 * builds a map of dependencies from maps
	 * 
	 * @param PlatformName name of the platform we want to build a map for
	 */
	void BuildMapDependencyGraph(const FName& PlatformName);

	/**
	* WriteMapDependencyGraph
	* write a previously built map dependency graph out to the sandbox directory for a platform
	*
	* @param PlatformName name of the platform we want to save out the dependency graph for
	*/
	void WriteMapDependencyGraph(const FName& PlatformName);

	/**
	 * IsChildCooker, 
	 * returns if this cooker is a sue chef for some other master chef.
	 */
	bool IsChildCooker() const
	{
		if (IsCookByTheBookMode())
		{
			return !(CookByTheBookOptions->ChildCookFilename.IsEmpty());
		}
		return false;
	}



	//////////////////////////////////////////////////////////////////////////
	// cook on the fly specific functions
	/**
	 * Cook requests for a package from network
	 *  blocks until cook is complete
	 * 
	 * @param  Filename	requested file to cook
	 * @param  Platformname platform to cook for
	 * @param  out UnsolicitedFiles return a list of files which were cooked as a result of cooking the requested package
	 */
	void HandleNetworkFileServerFileRequest( const FString& Filename, const FString& Platformname, TArray<FString>& UnsolicitedFiles );

	/**
	 * Shader recompile request from network
	 *  blocks until shader recompile complete
	 *
	 * @param  RecompileData input params for shader compile and compiled shader output
	 */
	void HandleNetworkFileServerRecompileShaders(const struct FShaderRecompileData& RecompileData);

	//////////////////////////////////////////////////////////////////////////
	// general functions

	/**
	 * Determines if a package should be cooked
	 * 
	 * @param InFileName		package file name
	 * @param InPlatformName	desired platform to cook for
	 * @return If the package should be cooked
	 */
	bool ShouldCook(const FString& InFileName, const FName& InPlatformName);

	/**
	 * Initialize the sandbox 
	 */
	void InitializeSandbox();

	/**
	 * Clean up the sandbox
	 */
	void TermSandbox();

	/**
	 * GetDependencies
	 * 
	 * @param Packages List of packages to use as the root set for dependency checking
	 * @param Found return value, all objects which package is dependent on
	 */
	void GetDependencies( const TSet<UPackage*>& Packages, TSet<UObject*>& Found);


	/**
	 * GetDependencies
	 * get package dependencies according to the asset registry
	 * 
	 * @param Packages List of packages to use as the root set for dependency checking
	 * @param Found return value, all objects which package is dependent on
	 */
	void GetDependentPackages( const TSet<UPackage*>& Packages, TSet<FName>& Found);

	/**
	 * GetDependencies
	 * get package dependencies according to the asset registry
	 *
	 * @param Root set of packages to use when looking for dependencies
	 * @param FoundPackages list of packages which were found
	 */
	void GetDependentPackages(const TSet<FName>& RootPackages, TSet<FName>& FoundPackages);
	/**
	 * GenerateManifestInfo
	 * generate the manifest information for a given package
	 *
	 * @param Package package to generate manifest information for
	 */
	void GenerateManifestInfo( UPackage* Package, const TArray<FName>& TargetPlatformNames );


	/**
	 * GenerateManifestInfo
	 * generate manfiest information for the given package and add it to the manifest
	 * this version is teh same as GenerateManifestInfo which takes in a UPackageObject except that doesn't require the package to be loaded
	 *
	 * @param Package name of the package to generate the manifest information for
	 * @param TargetPlatformNames to add the manifest information to
	 */
	void GenerateManifestInfo(const FName& Package, const TArray<FName>& TargetPlatformNames);

	/**
	 * ContainsWorld
	 * use the asset registry to determine if a Package contains a UWorld or ULevel object
	 * 
	 * @param PackageName to return if it contains the a UWorld object or a ULevel
	 * @return true if the Package contains a UWorld or ULevel false otherwise
	 */
	bool ContainsMap(const FName& PackageName) const;

	/**
	 * GetCurrentIniVersionStrings gets the current ini version strings for compare against previous cook
	 * 
	 * @param IniVersionStrings return list of the important current ini version strings
	 * @return false if function fails (should assume all platforms are out of date)
	 */
	bool GetCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, TArray<FString> &IniVersionStrings ) const;

	/**
	 * GetCookedIniVersionStrings gets the ini version strings used in previous cook for specified target platform
	 * 
	 * @param IniVersionStrings return list of the previous cooks ini version strings
	 * @return false if function fails to find the ini version strings
	 */
	bool GetCookedIniVersionStrings( const ITargetPlatform* TargetPlatform, TArray<FString>& IniVersionStrings ) const;


	/**
	 * Convert a path to a full sandbox path
	 * is effected by the cooking dlc settings
	 * This function should be used instead of calling the FSandbox Sandbox->ConvertToSandboxPath functions
	 */
	FString ConvertToFullSandboxPath( const FString &FileName, bool bForWrite = false ) const;
	FString ConvertToFullSandboxPath( const FString &FileName, bool bForWrite, const FString& PlatformName ) const;

	/**
	 * GetSandboxAssetRegistryFilename
	 * 
	 * return full path of the asset registry in the sandbox
	 */
	const FString GetSandboxAssetRegistryFilename();

	const FString GetCookedAssetRegistryFilename(const FString& PlatformName);

	/**
	 * Get the sandbox root directory for that platform
	 * is effected by the CookingDlc settings
	 * This should be used instead of calling the Sandbox function
	 */
	FString GetSandboxDirectory( const FString& PlatformName ) const;

	inline bool IsCookingDLC() const
	{
		// can only cook dlc in cook by the book
		// we are cooking dlc when the dlc name is setup
		if ( CookByTheBookOptions )
		{
			return  !CookByTheBookOptions->DlcName.IsEmpty();
		}
		return false;
	}

	/**
	 * GetDLCContentPath
	 * 
	 * @return return the path to the source dlc content
	 */
	FString GetDLCContentPath();
	
	inline bool IsCreatingReleaseVersion()
	{
		if ( CookByTheBookOptions )
		{
			return !CookByTheBookOptions->CreateReleaseVersion.IsEmpty();
		}
		return false;
	}

	/**
	 * Loads the cooked ini version settings maps into the Ini settings cache
	 * 
	 * @param TargetPlatforms to look for ini settings for
	 */
	bool CacheIniVersionStringsMap( const ITargetPlatform* TargetPlatform ) const;

	/**
	 * Checks if important ini settings have changed since last cook for each target platform 
	 * 
	 * @param TargetPlatforms to check if out of date
	 * @param OutOfDateTargetPlatforms return list of out of date target platforms which should be cleaned
	 */
	bool IniSettingsOutOfDate( const ITargetPlatform* TargetPlatform ) const;

	/**
	 * Saves ini settings which are in the memory cache to the hard drive in ini files
	 *
	 * @param TargetPlatforms to save
	 */
	bool SaveCurrentIniSettings( const ITargetPlatform* TargetPlatform ) const;

	/**
	 * IsCookFlagSet
	 * 
	 * @param InCookFlag used to check against the current cook flags
	 * @return true if the cook flag is set false otherwise
	 */
	bool IsCookFlagSet( const ECookInitializationFlags& InCookFlags ) const 
	{
		return (CookFlags & InCookFlags) != ECookInitializationFlags::None;
	}

	/**
	 *	Get the given packages 'cooked' timestamp (i.e. account for dependencies)
	 *
	 *	@param	InFilename			The filename of the package
	 *	@param	OutDateTime			The timestamp the cooked file should have
	 *
	 *	@return	bool				true if the package timestamp was found, false if not
	 */
	bool GetPackageTimestamp( const FString& InFilename, FDateTime& OutDateTime );

	/** If true, the maximum file length of a package being saved will be reduced by 32 to compensate for compressed package intermediate files */
	bool ShouldConsiderCompressedPackageFileLengthRequirements() const;

	/**
	 *	Cook (save) the given package
	 *
	 *	@param	Package				The package to cook/save
	 *	@param	SaveFlags			The flags to pass to the SavePackage function
	 *	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	 *
	 *	@return	ESavePackageResult::Success if packages was cooked
	 */
	ESavePackageResult SaveCookedPackage(UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate);
	/**
	 *	Cook (save) the given package
	 *
	 *	@param	Package				The package to cook/save
	 *	@param	SaveFlags			The flags to pass to the SavePackage function
	 *	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	 *	@param  TargetPlatformNames Only cook for target platforms which are included in this array (if empty cook for all target platforms specified on commandline options)
	 *									TargetPlatformNames is in and out value returns the platforms which the SaveCookedPackage function saved for
	 *
	 *	@return	ESavePackageResult::Success if packages was cooked
	 */
	ESavePackageResult SaveCookedPackage(UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate, TArray<FName> &TargetPlatformNames);


	/**
	 *  Save the global shader map
	 *  
	 *  @param	Platforms		List of platforms to make global shader maps for
	 */
	void SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms);


	/** Create sandbox file in directory using current settings supplied */
	void CreateSandboxFile();
	/** Gets the output directory respecting any command line overrides */
	FString GetOutputDirectoryOverride() const;

	/** Cleans sandbox folders for all target platforms */
	void CleanSandbox( const bool bIterative );

	/** Populate the cooked packages list from the on disk content using time stamps and dependencies to figure out if they are ok */
	void PopulateCookedPackagesFromDisk( const TArray<ITargetPlatform*>& Platforms );

	/** Generates asset registry */
	void GenerateAssetRegistry();

	/** Generates long package names for all files to be cooked */
	void GenerateLongPackageNames(TArray<FName>& FilesInPath);


	void GetDependencies( UPackage* Package, TArray<UPackage*> Dependencies );

};

FORCEINLINE uint32 GetTypeHash(const UCookOnTheFlyServer::FFilePlatformRequest &Key)
{
	uint32 Hash = GetTypeHash( Key.Filename );
	for ( const auto &PlatformName : Key.Platformnames )
	{
		Hash += Hash << 2 ^ GetTypeHash( PlatformName );
	}
	return Hash;
}

