// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObj.cpp: Unreal object global data and functions
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "Misc/SecureHash.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectAnnotation.h"
#include "UObject/TlsObjectInitializers.h"

DEFINE_LOG_CATEGORY(LogUObjectGlobals);


#if USE_MALLOC_PROFILER
#include "MallocProfiler.h"
#endif

bool						GIsSavingPackage = false;
int32							GImportCount							= 0;
/** Forced exports for EndLoad optimization.											*/
int32							GForcedExportCount						= 0;
TArray<UObject*>			GObjLoaded;

/** List of linkers that we want to close the loaders for (to free file handles) - needs to be delayed until EndLoad is called with GObjBeginLoadCount of 0 */
TArray<ULinkerLoad*>		GDelayedLinkerClosePackages;

/** Count for BeginLoad multiple loads.									*/
static int32					GObjBeginLoadCount						= 0;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Used to verify that the Super::Serialize chain is intact.			*/
TArray<UObject*,TInlineAllocator<16> >		DebugSerialize;
#endif

/** Object annotation used by the engine to keep track of which objects are selected */
FUObjectAnnotationSparseBool GSelectedAnnotation;

DEFINE_STAT(STAT_InitProperties);
DEFINE_STAT(STAT_ConstructObject);
DEFINE_STAT(STAT_AllocateObject);
DEFINE_STAT(STAT_PostConstructInitializeProperties);
DEFINE_STAT(STAT_LoadConfig);
DEFINE_STAT(STAT_LoadObject);
DEFINE_STAT(STAT_FindObject);
DEFINE_STAT(STAT_FindObjectFast);
DEFINE_STAT(STAT_NameTableEntries);
DEFINE_STAT(STAT_NameTableAnsiEntries);
DEFINE_STAT(STAT_NameTableWideEntries);
DEFINE_STAT(STAT_NameTableMemorySize);
DEFINE_STAT(STAT_DestroyObject);

/** CoreUObject delegates */
FCoreUObjectDelegates::FReplaceHotReloadClassDelegate FCoreUObjectDelegates::ReplaceHotReloadClassDelegate;
// Delegates used by SavePackage()
FCoreUObjectDelegates::FIsPackageOKToSaveDelegate FCoreUObjectDelegates::IsPackageOKToSaveDelegate;
FCoreUObjectDelegates::FAutoPackageBackupDelegate FCoreUObjectDelegates::AutoPackageBackupDelegate;

FCoreUObjectDelegates::FOnPreObjectPropertyChanged FCoreUObjectDelegates::OnPreObjectPropertyChanged;
FCoreUObjectDelegates::FOnObjectPropertyChanged FCoreUObjectDelegates::OnObjectPropertyChanged;

#if WITH_EDITOR
// Set of objects modified this frame
TSet<UObject*> FCoreUObjectDelegates::ObjectsModifiedThisFrame;
FCoreUObjectDelegates::FOnObjectModified FCoreUObjectDelegates::OnObjectModified;
FCoreUObjectDelegates::FOnAssetLoaded FCoreUObjectDelegates::OnAssetLoaded;
FCoreUObjectDelegates::FOnObjectSaved FCoreUObjectDelegates::OnObjectSaved;
#endif // WITH_EDITOR

FCoreUObjectDelegates::FOnRedirectorFollowed FCoreUObjectDelegates::RedirectorFollowed;

FSimpleMulticastDelegate FCoreUObjectDelegates::PreGarbageCollect;
FSimpleMulticastDelegate FCoreUObjectDelegates::PostGarbageCollect;
FSimpleMulticastDelegate FCoreUObjectDelegates::PreLoadMap;
FSimpleMulticastDelegate FCoreUObjectDelegates::PostLoadMap;
FSimpleMulticastDelegate FCoreUObjectDelegates::PostDemoPlay;
FCoreUObjectDelegates::FOnLoadObjectsOnTop FCoreUObjectDelegates::ShouldLoadOnTop;

FCoreUObjectDelegates::FStringAssetReferenceLoaded FCoreUObjectDelegates::StringAssetReferenceLoaded;
FCoreUObjectDelegates::FStringAssetReferenceSaving FCoreUObjectDelegates::StringAssetReferenceSaving;
FCoreUObjectDelegates::FPackageCreatedForLoad FCoreUObjectDelegates::PackageCreatedForLoad;

/** Check wehether we should report progress or not */
bool ShouldReportProgress()
{
	return GIsEditor && !IsRunningCommandlet() && !GIsAsyncLoading;
}

/**
 * Fast version of StaticFindObject that relies on the passed in FName being the object name
 * without any group/ package qualifiers.
 *
 * @param	ObjectClass		The to be found object's class
 * @param	ObjectPackage	The to be found object's outer
 * @param	ObjectName		The to be found object's class
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	AnyPackage		Whether to look in any package
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* StaticFindObjectFast( UClass* ObjectClass, UObject* ObjectPackage, FName ObjectName, bool ExactClass, bool AnyPackage, EObjectFlags ExclusiveFlags )
{
	if (GIsSavingPackage || GIsGarbageCollecting)
	{
		UE_LOG(LogUObjectGlobals, Fatal,TEXT("Illegal call to StaticFindObjectFast() while serializing object data or garbage collecting!"));
	}

	// We don't want to return any objects that are currently being background loaded unless we're using FindObject during async loading.
	ExclusiveFlags |= GIsAsyncLoading ? RF_NoFlags : RF_AsyncLoading;
	return StaticFindObjectFastInternal( ObjectClass, ObjectPackage, ObjectName, ExactClass, AnyPackage, ExclusiveFlags );
}

//
// Find an optional object.
//
UObject* StaticFindObject( UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, bool ExactClass )
{
	INC_DWORD_STAT(STAT_FindObject);

	if (GIsSavingPackage)
	{
		UE_LOG(LogUObjectGlobals, Fatal,TEXT("Illegal call to StaticFindObject() while serializing object data!"));
	}

	if (GIsGarbageCollecting)
	{
		UE_LOG(LogUObjectGlobals, Fatal,TEXT("Illegal call to StaticFindObject() while collecting garbage!"));
	}

	// Resolve the object and package name.
	const bool bAnyPackage = InObjectPackage==ANY_PACKAGE;
	UObject* ObjectPackage = bAnyPackage ? NULL : InObjectPackage;
	FString InName = OrigInName;

	UObject* MatchingObject = NULL;

#if WITH_EDITOR
	// If the editor is running, and T3D is being imported, ensure any packages referenced are fully loaded.
	if ((GIsEditor == true) && (GIsImportingT3D == true))// && (ObjectPackage != ANY_PACKAGE) && (ObjectPackage != NULL))
	{
		static bool s_bCurrentlyLoading = false;

		if (s_bCurrentlyLoading == false)
		{
			FString NameCheck = OrigInName;
			if (NameCheck.Contains(TEXT(".")) && 
				!NameCheck.Contains(TEXT("'")) && 
				!NameCheck.Contains(TEXT(":")) )
			{
				s_bCurrentlyLoading = true;
				MatchingObject = StaticLoadObject(ObjectClass, NULL, OrigInName, NULL,  LOAD_NoWarn, NULL);
				s_bCurrentlyLoading = false;
				if (MatchingObject != NULL)
				{
					return MatchingObject;
				}
			}
		}
	}
#endif	//#if !WITH_EDITOR

	// Don't resolve the name if we're searching in any package
	if( !bAnyPackage && !ResolveName( ObjectPackage, InName, false, false ) )
	{
		return NULL;
	}

	FName ObjectName(*InName, FNAME_Add, true);
	MatchingObject = StaticFindObjectFast( ObjectClass, ObjectPackage, ObjectName, ExactClass, bAnyPackage );

	return MatchingObject;
}

//
// Find an object; can't fail.
//
UObject* StaticFindObjectChecked( UClass* ObjectClass, UObject* ObjectParent, const TCHAR* InName, bool ExactClass )
{
	UObject* Result = StaticFindObject( ObjectClass, ObjectParent, InName, ExactClass );
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if( !Result )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Failed to find object '%s %s.%s'"), *ObjectClass->GetName(), ObjectParent==ANY_PACKAGE ? TEXT("Any") : ObjectParent ? *ObjectParent->GetName() : TEXT("None"), InName));
	}
#endif
	return Result;
}

//
// Find an object; won't assert on GIsSavingPackage or GIsGarbageCollecting
//
UObject* StaticFindObjectSafe( UClass* ObjectClass, UObject* ObjectParent, const TCHAR* InName, bool ExactClass )
{
	if ( !GIsSavingPackage && !GIsGarbageCollecting )
	{
		return StaticFindObject( ObjectClass, ObjectParent, InName, ExactClass );
	}
	else
	{
		return NULL;
	}
}


//
// Global property setting.
//
void GlobalSetProperty( const TCHAR* Value, UClass* Class, UProperty* Property, bool bNotifyObjectOfChange )
{
	if ( Property != NULL && Class != NULL )
	{
		// Apply to existing objects of the class.
		for( FObjectIterator It; It; ++It )
		{	
			UObject* Object = *It;
			if( Object->IsA(Class) && !Object->IsPendingKill() )
			{
				// If we're in a PIE session then only allow set commands to affect PlayInEditor objects.
				if( !GIsPlayInEditorWorld || ( Object->GetOutermost()->PackageFlags & PKG_PlayInEditor ) != 0 )
				{
#if WITH_EDITOR
					if( !Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange )
					{
						Object->PreEditChange(Property);
					}
#endif // WITH_EDITOR
					Property->ImportText( Value, Property->ContainerPtrToValuePtr<uint8>(Object), 0, Object );
#if WITH_EDITOR
					if( !Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange )
					{
						FPropertyChangedEvent PropertyEvent(Property);
						Object->PostEditChangeProperty(PropertyEvent);
					}
#endif // WITH_EDITOR
				}
			}
		}

		if (FPlatformProperties::HasEditorOnlyData())
		{
			// Apply to defaults.
			UObject* DefaultObject = Class->GetDefaultObject();
			check(DefaultObject != NULL);
			DefaultObject->SaveConfig();
		}
	}
}


/**
 * Executes a delegate by calling the named function on the object bound to the delegate.  You should
 * always first verify that the delegate is safe to execute by calling IsBound() before calling this function.
 *
 * @param	Params		Parameter structure
 */
//void FScriptDelegate::ProcessDelegate( void* Parameters ) const
//{
//	checkf( Object.IsValid() != false, TEXT( "ProcessDelegate() called with no object bound to delegate!" ) );
//	checkf( FunctionName != NAME_None, TEXT( "ProcessDelegate() called with no function name set!" ) );
//
//	// Object was pending kill, so we cannot execute the delegate.  Note that it's important to assert
//	// here and not simply continue execution, as memory may be left uninitialized if the delegate is
//	// not able to execute, resulting in much harder-to-detect code errors.  Users should always make
//	// sure IsBound() returns true before calling ProcessDelegate()!
//	UObject* ObjectPtr = static_cast< UObject* >( Object.Get() );	// Down-cast
//	checkSlow( !ObjectPtr->IsPendingKill() );
//
//	// Object *must* implement the specified function
//	UFunction* Function = ObjectPtr->FindFunctionChecked( FunctionName );
//
//	// Execute the delegate!
//	ObjectPtr->ProcessEvent(Function, Parameters);
//}


/**
 * Executes a multi-cast delegate by calling all functions on objects bound to the delegate.  Always
 * safe to call, even if when no objects are bound, or if objects have expired.
 *
 * @param	Params				Parameter structure
 */
//void FMulticastScriptDelegate::ProcessMulticastDelegate(void* Parameters) const
//{
//	if( InvocationList.Num() > 0 )
//	{
//		// Create a copy of the invocation list, just in case the list is modified by one of the callbacks during the broadcast
//		typedef TArray< FScriptDelegate, TInlineAllocator< 4 > > FInlineInvocationList;
//		FInlineInvocationList InvocationListCopy = FInlineInvocationList(InvocationList);
//	
//		// Invoke each bound function
//		for( FInlineInvocationList::TConstIterator FunctionIt( InvocationListCopy ); FunctionIt; ++FunctionIt )
//		{
//			if( FunctionIt->IsBound() )
//			{
//				// Invoke this delegate!
//				FunctionIt->ProcessDelegate(Parameters);
//			}
//			else if ( FunctionIt->IsCompactable() )
//			{
//				// Function couldn't be executed, so remove it.  Note that because the original list could have been modified by one of the callbacks, we have to search for the function to remove here.
//				RemoveInternal( *FunctionIt );
//			}
//		}
//	}
//}


/*-----------------------------------------------------------------------------
	UObject Tick.
-----------------------------------------------------------------------------*/

/**
 * Static UObject tick function, used to verify certain key assumptions and to tick the async loading code.
 *
 * @warning: The streaming stats rely on this function not doing any work besides calling ProcessAsyncLoading.
 * @todo: Move stats code into core?
 *
 * @param DeltaTime	Time in seconds since last call
 * @param bUseFullTimeLimit	If true, use the entire time limit even if blocked on I/O
 * @param AsyncLoadingTime Time in seconds to use for async loading limit
 */
void StaticTick( float DeltaTime, bool bUseFullTimeLimit, float AsyncLoadingTime )
{
	check(!IsLoading());

	// Spend a bit of time (pre)loading packages - currently 5 ms.
	ProcessAsyncLoading(true, bUseFullTimeLimit, AsyncLoadingTime);

	// Check natives.
	extern int32 GNativeDuplicate;
	if( GNativeDuplicate )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Duplicate native registered: %i"), GNativeDuplicate );
	}
	// Check for duplicates.
	extern int32 GCastDuplicate;
	if( GCastDuplicate )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Duplicate cast registered: %i"), GCastDuplicate );
	}

#if STATS
	// Set name table stats.
	int32 NameTableEntries = FName::GetMaxNames();
	int32 NameTableAnsiEntries = FName::GetNumAnsiNames();
	int32 NameTableWideEntries = FName::GetNumWideNames();
	int32 NameTableMemorySize = FName::GetNameTableMemorySize();
	SET_DWORD_STAT( STAT_NameTableEntries, NameTableEntries );
	SET_DWORD_STAT( STAT_NameTableAnsiEntries, NameTableAnsiEntries );
	SET_DWORD_STAT( STAT_NameTableWideEntries, NameTableWideEntries);
	SET_DWORD_STAT( STAT_NameTableMemorySize, NameTableMemorySize );

#if 0 // can't read stats with the new stats system
	// Set async I/O bandwidth stats.
	static uint32 PreviousReadSize	= 0;
	static uint32 PrevioudReadCount	= 0;
	static float PreviousReadTime	= 0;
	float ReadTime	= GStatManager.GetStatValueFLOAT( STAT_AsyncIO_PlatformReadTime );
	uint32 ReadSize	= GStatManager.GetStatValueDWORD( STAT_AsyncIO_FulfilledReadSize );
	uint32 ReadCount	= GStatManager.GetStatValueDWORD( STAT_AsyncIO_FulfilledReadCount );

	// It is possible that the stats are update in between us reading the values so we simply defer till
	// next frame if that is the case. This also handles partial updates. An individual value might be 
	// slightly wrong but we have enough small requests to smooth it out over a few frames.
	if( (ReadTime  - PreviousReadTime ) > 0.f 
	&&	(ReadSize  - PreviousReadSize ) > 0 
	&&	(ReadCount - PrevioudReadCount) > 0 )
	{
		float Bandwidth = (ReadSize - PreviousReadSize) / (ReadTime - PreviousReadTime) / 1048576.f;
		SET_FLOAT_STAT( STAT_AsyncIO_Bandwidth, Bandwidth );
		PreviousReadTime	= ReadTime;
		PreviousReadSize	= ReadSize;
		PrevioudReadCount	= ReadCount;
	}
	else
	{
		SET_FLOAT_STAT( STAT_AsyncIO_Bandwidth, 0.f );
	}
#endif
#endif
}



/*-----------------------------------------------------------------------------
   File loading.
-----------------------------------------------------------------------------*/

//
// Safe load error-handling.
//
void SafeLoadError( UObject* Outer, uint32 LoadFlags, const TCHAR* Error, const TCHAR* Fmt, ... )
{
	// Variable arguments setup.
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );

	if( FParse::Param( FCommandLine::Get(), TEXT("TREATLOADWARNINGSASERRORS") ) == true )
	{
		UE_LOG(LogUObjectGlobals, Error, TEXT("%s"), TempStr ); 
	}
	else
	{
		// Don't warn here if either quiet or no warn are set
		if( (LoadFlags & LOAD_Quiet) == 0 && (LoadFlags & LOAD_NoWarn) == 0)
		{ 
			UE_LOG(LogUObjectGlobals, Warning, TEXT("%s"), TempStr ); 
		}
	}
}

/**
 * Find an existing package by name
 * @param InOuter		The Outer object to search inside
 * @param PackageName	The name of the package to find
 *
 * @return The package if it exists
 */
UPackage* FindPackage( UObject* InOuter, const TCHAR* PackageName )
{
	FString InName;
	if( PackageName )
	{
		InName = PackageName;
	}
	else
	{
		InName = MakeUniqueObjectName( InOuter, UPackage::StaticClass() ).ToString();
	}
	ResolveName( InOuter, InName, 1, 0 );

	UPackage* Result = NULL;
	if ( InName != TEXT("None") )
	{
		Result = FindObject<UPackage>( InOuter, *InName );
	}
	else
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package named 'None'") );
	}
	return Result;
}

UPackage* CreatePackage( UObject* InOuter, const TCHAR* PackageName )
{
	FString InName;

	if( PackageName )
	{
		InName = PackageName;
	}

	if (InName.Contains(TEXT("//")))
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Attempted to create a package with name containing double slashes. PackageName: %s"), PackageName);
	}

	if( InName.EndsWith( TEXT( "." ) ) )
	{
		FString InName2 = InName.Left( InName.Len() - 1 );
		UE_LOG(LogUObjectGlobals, Log,  TEXT( "Invalid Package Name entered - '%s' renamed to '%s'" ), *InName, *InName2 );
		InName = InName2;
	}

	if(InName.Len() == 0)
	{
		InName = MakeUniqueObjectName( InOuter, UPackage::StaticClass() ).ToString();
	}

	ResolveName( InOuter, InName, 1, 0 );

	UPackage* Result = NULL;
	if ( InName.Len() == 0 )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package with an empty package name.") );
	}

	if ( InName != TEXT("None") )
	{
		Result = FindObject<UPackage>( InOuter, *InName );
		if( Result == NULL )
		{
			FName NewPackageName(*InName, FNAME_Add, true);
			if (FPackageName::IsShortPackageName(NewPackageName))
			{
				UE_LOG(LogUObjectGlobals, Warning, TEXT("Attempted to create a package with a short package name: %s Outer: %s"), PackageName, InOuter ? *InOuter->GetFullName() : TEXT("NullOuter"));
			}
			else
			{
				Result = new( InOuter, NewPackageName, RF_Public )UPackage(FObjectInitializer());
			}
		}
	}
	else
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package named 'None'") );
	}

	return Result;
}

//
// Resolve a package and name.
//
bool ResolveName( UObject*& InPackage, FString& InOutName, bool Create, bool Throw )
{
	FString* IniFilename = NULL;

	// See if the name is specified in the .ini file.
	if( FCString::Strnicmp( *InOutName, TEXT("engine-ini:"), FCString::Strlen(TEXT("engine-ini:")) )==0 )
	{
		IniFilename = &GEngineIni;
	}
	else if( FCString::Strnicmp( *InOutName, TEXT("game-ini:"), FCString::Strlen(TEXT("game-ini:")) )==0 )
	{
		IniFilename = &GGameIni;
	}
	else if( FCString::Strnicmp( *InOutName, TEXT("input-ini:"), FCString::Strlen(TEXT("input-ini:")) )==0 )
	{
		IniFilename = &GInputIni;
	}
	else if( FCString::Strnicmp( *InOutName, TEXT("editor-ini:"), FCString::Strlen(TEXT("editor-ini:")) )==0 )
	{
		IniFilename = &GEditorIni;
	}


	if( IniFilename && InOutName.Contains(TEXT(".")) )
	{
		// Get .ini key and section.
		FString Section = InOutName.Mid(1+InOutName.Find(TEXT(":"), ESearchCase::CaseSensitive));
		int32 i = Section.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		FString Key;
		if( i != -1)
		{
			Key = Section.Mid(i+1);
			Section = Section.Left(i);
		}

		// Look up name.
		FString Result;
		if( !GConfig->GetString( *Section, *Key, Result, *IniFilename ) )
		{
			if( Throw == true )
			{
				UE_LOG(LogUObjectGlobals, Error, TEXT( " %s %s " ), *FString::Printf( TEXT("Can't find '%s' in configuration file section=%s key=%s"), *InOutName, *Section, *Key), **IniFilename );
			}
			return false;
		}
		InOutName = Result;
	}

	// Strip off the object class.
	ConstructorHelpers::StripObjectClass( InOutName );

	// Handle specified packages.
	int32 i;

	// if you're attempting to find an object in any package using a dotted name that isn't fully
	// qualified (such as ObjectName.SubobjectName - notice no package name there), you normally call
	// StaticFindObject and pass in ANY_PACKAGE as the value for InPackage.  When StaticFindObject calls ResolveName,
	// it passes NULL as the value for InPackage, rather than ANY_PACKAGE.  As a result, unless the first chunk of the
	// dotted name (i.e. ObjectName from the above example) is a UPackage, the object will not be found.  So here we attempt
	// to detect when this has happened - if we aren't attempting to create a package, and a UPackage with the specified
	// name couldn't be found, pass in ANY_PACKAGE as the value for InPackage to the call to FindObject<UObject>().
	bool bSubobjectPath = false;

	// to make parsing the name easier, replace the subobject delimiter with an extra dot
	InOutName.ReplaceInline(SUBOBJECT_DELIMITER, TEXT(".."));
	while( (i=InOutName.Find(TEXT("."))) != -1 )
	{
		FString PartialName = InOutName.Left(i);

		// if the next part of InOutName ends in two dots, it indicates that the next object in the path name
		// is not a top-level object (i.e. it's a subobject).  e.g. SomePackage.SomeGroup.SomeObject..Subobject
		if ( InOutName.Mid(i+1,1) == TEXT(".") )
		{
			InOutName      = PartialName + InOutName.Mid(i+1);
			bSubobjectPath = true;
			Create         = false;
		}

		// In case this is a short script package name, convert to long name before passing to CreatePackage/FindObject.
		FName* ScriptPackageName = FPackageName::FindScriptPackageName(*PartialName);
		if (ScriptPackageName)
		{
			PartialName = ScriptPackageName->ToString();
		}
		// Only long package names are allowed so don't even attempt to create one because whatever the name represents
		// it's not a valid package name anyway.
		
		if (!Create)
		{
			UObject* NewPackage = FindObject<UPackage>( InPackage, *PartialName );
			if( !NewPackage )
			{
				NewPackage = FindObject<UObject>( InPackage == NULL ? ANY_PACKAGE : InPackage, *PartialName );
				if( !NewPackage )
				{
					return bSubobjectPath;
				}
			}
			InPackage = NewPackage;
		}
		else if (!FPackageName::IsShortPackageName(PartialName))
		{
			if (!ScriptPackageName)
			{
				InPackage = LoadPackage(dynamic_cast<UPackage*>(InPackage), *PartialName, 0);
			}
			if (!InPackage)
			{
				InPackage = CreatePackage(InPackage, *PartialName);
			}

			check(InPackage);
		}
		InOutName = InOutName.Mid(i+1);
	}

	return true;
}

/**
 * Parse an object from a text representation
 *
 * @param Stream			String containing text to parse
 * @param Match				Tag to search for object representation within string
 * @param Class				The class of the object to be loaded.
 * @param DestRes			returned uobject pointer
 * @param InParent			Outer to search
 * @param bInvalidObject	[opt] Optional output.  If true, Tag was matched but the specified object wasn't found.
 *
 * @return true if the object parsed successfully
 */
bool ParseObject( const TCHAR* Stream, const TCHAR* Match, UClass* Class, UObject*& DestRes, UObject* InParent, bool* bInvalidObject )
{
	TCHAR TempStr[1024];
	if( !FParse::Value( Stream, Match, TempStr, ARRAY_COUNT(TempStr) ) )
	{
		// Match not found
		return 0;
	}
	else if( FCString::Stricmp(TempStr,TEXT("NONE"))==0 )
	{
		// Match found, object explicit set to be None
		DestRes = NULL;
		return 1;
	}
	else
	{
		// Look this object up.
		UObject* Res = StaticFindObject( Class, InParent, TempStr );
		if( !Res )
		{
			// Match found, object not found
			if (bInvalidObject)
			{
				*bInvalidObject = true;
			}
			return 0;
		}

		// Match found, object found
		DestRes = Res;
		return 1;
	}
}

/**
 * Find or load an object by string name with optional outer and filename specifications.
 * These are optional because the InName can contain all of the necessary information.
 *
 * @param ObjectClass	The class (or a superclass) of the object to be loaded.
 * @param InOuter		An optional object to narrow where to find/load the object from
 * @param InName		String name of the object. If it's not fully qualified, InOuter and/or Filename will be needed
 * @param Filename		An optional file to load from (or find in the file's package object)
 * @param LoadFlags		Flags controlling how to handle loading from disk
 * @param Sandbox		A list of packages to restrict the search for the object
 * @param bAllowObjectReconciliation	Whether to allow the object to be found via FindObject in the case of seek free loading
 *
 * @return The object that was loaded or found. NULL for a failure.
 */
UObject* StaticLoadObject(UClass* ObjectClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, uint32 LoadFlags, UPackageMap* Sandbox, bool bAllowObjectReconciliation )
{
	//UE_CLOG(GIsRoutingPostLoad, LogUObjectGlobals, Fatal, TEXT("Calling StaticLoadObject during PostLoad is forbidden."));

	SCOPE_CYCLE_COUNTER(STAT_LoadObject);
	check(ObjectClass);
	check(InName);

	FString StrName = InName;

	// break up the name into packages, returning the innermost name and its outer
	ResolveName(InOuter, StrName, true, true);
	if (InOuter)
	{
		if( bAllowObjectReconciliation && ((FApp::IsGame() && !GIsEditor && !IsRunningCommandlet()) 
#if WITH_EDITOR
			|| GIsImportingT3D
#endif
			))
		{
			if (UObject* Result = StaticFindObjectFast(ObjectClass, InOuter, *StrName))
			{
				return Result;
			}
		}

		if( (FPlatformProperties::RequiresCookedData() || FPlatformProperties::IsServerOnly()) && GUseSeekFreeLoading )
		{
			if ( (LoadFlags&LOAD_NoWarn) == 0 )
			{
				UE_LOG(LogUObjectGlobals, Warning ,TEXT("StaticLoadObject for %s %s %s couldn't find object in memory!"),
					*ObjectClass->GetName(),
					*InOuter->GetName(),
					*StrName);
			}
		}
		else
		{
			// now that we have one asset per package, we load the entire package whenever a single object is requested
			LoadPackage(NULL, *InOuter->GetOutermost()->GetName(), LoadFlags & ~LOAD_Verify);

			// now, find the object in the package
			if (UObject* Result = Result = StaticFindObjectFast(ObjectClass, InOuter, *StrName))
			{
				return Result;
			}

			// If the object was not found, check for a redirector and follow it if the class matches
			UObjectRedirector* Redirector = FindObjectFast<UObjectRedirector>(InOuter, *StrName);
			if ( Redirector && Redirector->DestinationObject && Redirector->DestinationObject->IsA(ObjectClass) )
			{
				return Redirector->DestinationObject;
			}
		}
	}

	// we haven't created or found the object, error
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("ClassName"), FText::FromString( ObjectClass->GetName() ));
	Arguments.Add(TEXT("OuterName"), InOuter ? FText::FromString( InOuter->GetPathName() ) : NSLOCTEXT( "Core", "None", "None" ));
	Arguments.Add(TEXT("ObjectName"), FText::FromString( StrName ));
	const FString Error = FText::Format( NSLOCTEXT( "Core", "ObjectNotFound", "Failed to find object '{ClassName} {OuterName}.{ObjectName}'" ), Arguments ).ToString();
	SafeLoadError(InOuter, LoadFlags, *Error, *Error);

	return nullptr;
}

//
// Load a class.
//
UClass* StaticLoadClass( UClass* BaseClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, uint32 LoadFlags, UPackageMap* Sandbox )
{
	check(BaseClass);

	UClass* Class = LoadObject<UClass>( InOuter, InName, Filename, LoadFlags, Sandbox );
	if( Class && !Class->IsChildOf(BaseClass) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ClassName"), FText::FromString( Class->GetFullName() ));
		Arguments.Add(TEXT("BaseClassName"), FText::FromString( BaseClass->GetFullName() ));
		const FString Error = FText::Format( NSLOCTEXT( "Core", "LoadClassMismatch", "{ClassName} is not a child class of {BaseClassName}" ), Arguments ).ToString();
		SafeLoadError(InOuter, LoadFlags,*Error, *Error);

		// return NULL class due to error
		Class = NULL;
	}
	return Class;
}

/**
* Loads a package and all contained objects that match context flags.
*
* @param	InOuter		Package to load new package into (usually NULL or ULevel->GetOuter())
* @param	Filename	Long package name to load.
* @param	LoadFlags	Flags controlling loading behavior
* @param	ImportLinker	Linker that requests this package through one of its imports
* @return	Loaded package if successful, NULL otherwise
*/
UPackage* LoadPackageInternal(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags, ULinkerLoad* ImportLinker)
{
	UPackage* Result = NULL;

	FString FileToLoad;

	if( InLongPackageName && FCString::Strlen(InLongPackageName) > 0 )
	{
		FileToLoad = InLongPackageName;
	}
	else if( InOuter )
	{
		FileToLoad = InOuter->GetName();
	}

	// Make sure we're trying to load long package names only.
	if (FPackageName::IsShortPackageName(FileToLoad))
	{
		FString LongPackageName;
		FName* ScriptPackageName = FPackageName::FindScriptPackageName(*FileToLoad);
		if (ScriptPackageName)
		{
			UE_LOG(LogUObjectGlobals, Warning, TEXT("LoadPackage: %s is a short script package name."), InLongPackageName);
			FileToLoad = ScriptPackageName->ToString();
		}
		else if (!FPackageName::SearchForPackageOnDisk(FileToLoad, &FileToLoad))
		{
			UE_LOG(LogUObjectGlobals, Warning, TEXT("LoadPackage can't find package %s."), *FileToLoad);
			return NULL;
		}
	}

#if WITH_EDITOR
	TGuardValue<bool> IsEditorLoadingPackage(GIsEditorLoadingPackage, GIsEditor || GIsEditorLoadingPackage);
#endif

	FScopedSlowTask SlowTask(100, FText::Format(NSLOCTEXT("Core", "LoadingPackage_Scope", "Loading Package '{0}'"), FText::FromString(FileToLoad)), ShouldReportProgress());
	SlowTask.bVisibleOnUI = false;
	
	SlowTask.EnterProgressFrame(10);

	// Try to load.
	BeginLoad();

	SlowTask.EnterProgressFrame(30);
	{
		// Keep track of start time.
		const double StartTime = FPlatformTime::Seconds();

		// Create a new linker object which goes off and tries load the file.
		ULinkerLoad* Linker = GetPackageLinker(InOuter, *FileToLoad, LoadFlags, nullptr, nullptr);
		if (!Linker)
		{
			EndLoad();
			return nullptr;
		}

		if (Linker->LinkerRoot && Linker->LinkerRoot->HasAnyFlags(RF_WasLoaded))
		{
			// The linker is associated with a package that has already been loaded.
			// Loading packages that have already been loaded is unsupported.
			EndLoad();
			return Linker->LinkerRoot;
		}

		Result = Linker->LinkerRoot;

		// If we are loading a package for diff'ing, set the package flag
		if(LoadFlags & LOAD_ForDiff)
		{
			Result->PackageFlags |= PKG_ForDiffing;
		}

		// Save the filename we load from
		CA_SUPPRESS(28182)
		Result->FileName = FName(*FileToLoad);

		// is there a script SHA hash for this package?
		uint8 SavedScriptSHA[20];
		CA_SUPPRESS(6011)
		bool bHasScriptSHAHash = FSHA1::GetFileSHAHash(*Linker->LinkerRoot->GetName(), SavedScriptSHA, false);
		if (bHasScriptSHAHash)
		{
			// if there is, start generating the SHA for any script code in this package
			Linker->StartScriptSHAGeneration();
		}

		SlowTask.EnterProgressFrame(30);

		if( !(LoadFlags & LOAD_Verify) )
		{
			// Make sure we pass the property that's currently being serialized by the linker that owns the import 
			// that triggered this LoadPackage call
			auto OldSerialziedProperty = Linker->GetSerializedProperty();
			if (ImportLinker)
			{
				Linker->SetSerializedProperty(ImportLinker->GetSerializedProperty());
			}

			Linker->LoadAllObjects();

			Linker->SetSerializedProperty(OldSerialziedProperty);
		}

		SlowTask.EnterProgressFrame(30);

		EndLoad();

#if WITH_EDITOR
		GIsEditorLoadingPackage = *IsEditorLoadingPackage;
#endif

		// Set package-requires-localization flags from archive after loading. This reinforces flagging of packages that haven't yet been resaved.
		Result->ThisRequiresLocalizationGather(Linker->RequiresLocalizationGather());

#if WITH_ENGINE
		// Cancel all texture allocations that haven't been claimed yet.
		Linker->Summary.TextureAllocations.CancelRemainingAllocations( true );
#endif		// WITH_ENGINE

		// if we are calculating the script SHA for a package, do the comparison now
		if (bHasScriptSHAHash)
		{
			// now get the actual hash data
			uint8 LoadedScriptSHA[20];
			Linker->GetScriptSHAKey(LoadedScriptSHA);

			// compare SHA hash keys
			if (FMemory::Memcmp(SavedScriptSHA, LoadedScriptSHA, 20) != 0)
			{
				appOnFailSHAVerification(*Linker->Filename, false);
			}
		}

		// Only set time it took to load package if the above EndLoad is the "outermost" EndLoad.
		if( Result && !IsLoading() && !(LoadFlags & LOAD_Verify) )
		{
			Result->SetLoadTime( FPlatformTime::Seconds() - StartTime );
		}

		// @todo: the next two conditions should check the file limit
		if (FPlatformProperties::RequiresCookedData())
		{
			// give a hint to the IO system that we are done with this file for now
			FIOSystem::Get().HintDoneWithFile(*Linker->Filename);
		}

		// with UE4 and single asset per package, we load so many packages that some platforms will run out
		// of file handles. So, this will close the package, but just things like bulk data loading will
		// fail, so we only currently do this when loading on consoles
		if (FPlatformProperties::RequiresCookedData())
		{
			if (GObjBeginLoadCount == 0)
			{
				if (Result && Linker->Loader)
				{
					ResetLoaders(Result);
				}
				delete Linker->Loader;
				Linker->Loader = NULL;
			}
			else
			{
				GDelayedLinkerClosePackages.Add(Linker);
			}
		}
	}

	if( GUseSeekFreeLoading && Result && !(LoadFlags & LOAD_NoSeekFreeLinkerDetatch) )
	{
		// We no longer need the linker. Passing in NULL would reset all loaders so we need to check for that.
		ResetLoaders( Result );
	}
	// Mark package as loaded.
	Result->SetFlags(RF_WasLoaded);

	return Result;
}

UPackage* LoadPackage(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags)
{
	return LoadPackageInternal(InOuter, InLongPackageName, LoadFlags, /*ImportLinker =*/ nullptr);
}

/**
 * Returns whether we are currently loading a package (sync or async)
 *
 * @return true if we are loading a package, false otherwise
 */
bool IsLoading()
{
	check(GObjBeginLoadCount>=0);
	return GObjBeginLoadCount > 0;
}

//
// Begin loading packages.
//warning: Objects may not be destroyed between BeginLoad/EndLoad calls.
//
void BeginLoad()
{
	if( ++GObjBeginLoadCount == 1 && !GIsAsyncLoading )
	{
		// Make sure we're finishing up all pending async loads, and trigger texture streaming next tick if necessary.
		FlushAsyncLoading( NAME_None );

		// Validate clean load state.
		check(GObjLoaded.Num()==0);
	}
}

// Sort objects by linker name and file offset
struct FCompareUObjectByLinkerAndOffset
{
	FORCEINLINE bool operator()( const UObject& A, const UObject &B ) const
	{
		ULinker* LinkerA = A.GetLinker();
		ULinker* LinkerB = B.GetLinker();

		// Both objects have linkers.
		if( LinkerA && LinkerB )
		{
			// Identical linkers, sort by offset in file.
			if( LinkerA == LinkerB )
			{
				FObjectExport& ExportA = LinkerA->ExportMap[ A.GetLinkerIndex() ];
				FObjectExport& ExportB = LinkerB->ExportMap[ B.GetLinkerIndex() ];
				return ExportA.SerialOffset < ExportB.SerialOffset;
			}
			// Sort by linker name.
			else
			{
				return LinkerA->GetFName().GetComparisonIndex() < LinkerB->GetFName().GetComparisonIndex();
			}
		}
		// Neither objects have a linker, don't do anything.
		else if( LinkerA == LinkerB )
		{
			return false;
		}
		// Sort objects with linkers vs. objects without
		else
		{
			return (LinkerA != NULL);
		}
	}
};

//
// End loading packages.
//
void EndLoad()
{
	check(GObjBeginLoadCount>0);
	if (GIsAsyncLoading)
	{
		GObjBeginLoadCount--;
		return;
	}

#if WITH_EDITOR
	FScopedSlowTask SlowTask(0, NSLOCTEXT("Core", "PerformingPostLoad", "Performing post-load..."), ShouldReportProgress());

	int32 NumObjectsLoaded = 0, NumObjectsFound = 0;
#endif

	while( --GObjBeginLoadCount == 0 && (GObjLoaded.Num() || GImportCount || GForcedExportCount) )
	{
		// Make sure we're not recursively calling EndLoad as e.g. loading a config file could cause
		// BeginLoad/EndLoad to be called.
		GObjBeginLoadCount++;

		// Temporary list of loaded objects as GObjLoaded might expand during iteration.
		TArray<UObject*> ObjLoaded;
		TSet<ULinkerLoad*> LoadedLinkers;
		while( GObjLoaded.Num() )
		{
			// Accumulate till GObjLoaded no longer increases.
			ObjLoaded += GObjLoaded;
			GObjLoaded.Empty();

			// Sort by Filename and Offset.
			ObjLoaded.Sort( FCompareUObjectByLinkerAndOffset() );

			// Finish loading everything.
			for( int32 i=0; i<ObjLoaded.Num(); i++ )
			{
				// Preload.
				UObject* Obj = ObjLoaded[i];
				if( Obj->HasAnyFlags(RF_NeedLoad) )
				{
					check(Obj->GetLinker());
					Obj->GetLinker()->Preload( Obj );
				}
			}

			// Start over again as new objects have been loaded that need to have "Preload" called on them before
			// we can safely PostLoad them.
			if(GObjLoaded.Num())
			{
				continue;
			}

#if WITH_EDITOR
			SlowTask.CompletedWork = SlowTask.TotalAmountOfWork;
			SlowTask.TotalAmountOfWork += ObjLoaded.Num();
			SlowTask.CurrentFrameScope = 0;
#endif

			if ( GIsEditor )
			{
				for( int32 i=0; i<ObjLoaded.Num(); i++ )
				{
					UObject* Obj = ObjLoaded[i];
					if ( Obj->GetLinker() )
					{
						LoadedLinkers.Add(Obj->GetLinker());
					}
				}
			}

			{
				// set this so that we can perform certain operations in which are only safe once all objects have been de-serialized.
				TGuardValue<bool> GuardIsRoutingPostLoad(GIsRoutingPostLoad, true);

				// Postload objects.
				for(int32 i = 0; i < ObjLoaded.Num(); i++)
				{
				
					UObject* Obj = ObjLoaded[i];
					check(Obj);
#if WITH_EDITOR
					SlowTask.EnterProgressFrame(1, FText::Format(NSLOCTEXT("Core", "FinalizingUObject", "Finalizing load of {0}"), FText::FromString(Obj->GetName())));
#endif
					Obj->ConditionalPostLoad();
				}
			}

#if WITH_EDITOR
			// Send global notification for each object that was loaded.
			// Useful for updating UI such as ContentBrowser's loaded status.
			{
				for( int32 CurObjIndex=0; CurObjIndex<ObjLoaded.Num(); CurObjIndex++ )
				{
					UObject* Obj = ObjLoaded[CurObjIndex];
					check(Obj);
					if ( Obj->IsAsset() )
					{
						FCoreUObjectDelegates::OnAssetLoaded.Broadcast(Obj);
					}
				}
			}
#endif	// WITH_EDITOR

			// Empty array before next iteration as we finished postloading all objects.
			ObjLoaded.Empty( GObjLoaded.Num() );
		}

		if ( GIsEditor && LoadedLinkers.Num() > 0 )
		{
			for ( TSet<ULinkerLoad*>::TIterator It(LoadedLinkers); It; ++It )
			{
				ULinkerLoad* LoadedLinker = *It;
				check(LoadedLinker);

				if ( LoadedLinker->LinkerRoot != NULL && !LoadedLinker->LinkerRoot->IsFullyLoaded() )
				{
					bool bAllExportsCreated = true;
					for ( int32 ExportIndex = 0; ExportIndex < LoadedLinker->ExportMap.Num(); ExportIndex++ )
					{
						FObjectExport& Export = LoadedLinker->ExportMap[ExportIndex];
						if ( !Export.bForcedExport && Export.Object == NULL )
						{
							bAllExportsCreated = false;
							break;
						}
					}

					if ( bAllExportsCreated )
					{
						LoadedLinker->LinkerRoot->MarkAsFullyLoaded();
					}
				}
			}
		}

		// Dissociate all linker import and forced export object references, since they
		// may be destroyed, causing their pointers to become invalid.
		DissociateImportsAndForcedExports();

		// close any linkers' loaders that were requested to be closed once GObjBeginLoadCount goes to 0
		for (int32 Index = 0; Index < GDelayedLinkerClosePackages.Num(); Index++)
		{
			ULinkerLoad* Linker = GDelayedLinkerClosePackages[Index];
			if (Linker)
			{
				if (Linker->Loader && Linker->LinkerRoot)
				{
					ResetLoaders(Linker->LinkerRoot);
				}
				delete Linker->Loader;
				Linker->Loader = NULL;
			}
		}
		GDelayedLinkerClosePackages.Empty();
	}
}

/*-----------------------------------------------------------------------------
	Object name functions.
-----------------------------------------------------------------------------*/

/**
 * Create a unique name by combining a base name and an arbitrary number string.
 * The object name returned is guaranteed not to exist.
 *
 * @param	Parent		the outer for the object that needs to be named
 * @param	Class		the class for the object
 * @param	BaseName	optional base name to use when generating the unique object name; if not specified, the class's name is used
 *
 * @return	name is the form BaseName_##, where ## is the number of objects of this
 *			type that have been created since the last time the class was garbage collected.
 */
FName MakeUniqueObjectName( UObject* Parent, UClass* Class, FName BaseName/*=NAME_None*/ )
{
	check(Class);
	if ( BaseName == NAME_None )
	{
		BaseName = Class->GetFName();
	}

	// cache the class's name's index for faster name creation later
	FName TestName;
	if (!FPlatformProperties::HasEditorOnlyData() && GFastPathUniqueNameGeneration)
	{
		/*   Fast Path Name Generation
		* A significant fraction of object creation time goes into verifying that the a chosen unique name is really unique.
		* The idea here is to generate unique names using very high numbers and only in situations where collisions are 
		* impossible for other reasons.
		*
		* Rationale for uniqueness as used here.
		* - Consoles do not save objects in general, and certainly not animation trees. So we could never load an object that would later clash.
		* - We assume that we never load or create any object with a "name number" as large as, say, MAX_int32 / 2, other than via 
		*   HACK_FastPathUniqueNameGeneration.
		* - After using one of these large "name numbers", we decrement the static UniqueIndex, this no two names generated this way, during the
		*   same run, could ever clash.
		* - We assume that we could never create anywhere near MAX_int32/2 total objects at runtime, within a single run. 
		* - We require an outer for these items, thus outers must themselves be unique. Therefore items with unique names created on the fast path
		*   could never clash with anything with a different outer. For animation trees, these outers are never saved or loaded, thus clashes are 
		*   impossible.
		*/
		static int32 UniqueIndex = MAX_int32 - 1000;
		TestName = FName(BaseName, --UniqueIndex);
		checkSlow(Parent); 
		checkSlow(Parent!=ANY_PACKAGE); 
		checkSlow(!StaticFindObjectFastInternal( NULL, Parent, TestName ));
	}
	else
	{
		UObject* ExistingObject;

		do
		{
			// create the next name in the sequence for this class
			if (BaseName.GetComparisonIndex() == NAME_Package)
			{
				if ( Parent == NULL )
				{
					//package names should default to "/Temp/Untitled" when their parent is NULL. Otherwise they are a group.
					TestName = FName( *FString::Printf(TEXT("/Temp/%s"), *FName(NAME_Untitled).ToString()), ++Class->ClassUnique);
				}
				else
				{
					//package names should default to "Untitled"
					TestName = FName(NAME_Untitled, ++Class->ClassUnique);
				}
			}
			else
			{
				TestName = FName(BaseName, ++Class->ClassUnique);
			}

			if (Parent == ANY_PACKAGE)
			{
				ExistingObject = StaticFindObject( NULL, ANY_PACKAGE, *TestName.ToString() );
			}
			else
			{
				ExistingObject = StaticFindObjectFastInternal( NULL, Parent, TestName );
			}
		} 
		while( ExistingObject );
	}
	return TestName;
}

/**
 * Given an actor label string, generates an FName that can be used as an object name for that label.
 * The generated name isn't guaranteed to be unique.  If the object's current name is already satisfactory, then
 * that name will be returned.
 *
 * @param	ActorLabel	The label string to convert to an FName
 * @param	CurrentObjectName	The object's current name, or NAME_None if it has no name yet
 *
 * @return	The generated actor object name
 */
FName MakeObjectNameFromActorLabel( const FString& InActorLabel, const FName CurrentObjectName )
{
	FString GeneratedName = InActorLabel;

	// Convert the actor label, which may consist of just about any possible character, into a
	// suitable name for a UObject (remove whitespace, certain symbols, etc.)
	{
		for( int32 BadCharacterIndex = 0; BadCharacterIndex < ARRAY_COUNT( INVALID_OBJECTNAME_CHARACTERS ) - 1; ++BadCharacterIndex )
		{
			const TCHAR TestChar[2] = { INVALID_OBJECTNAME_CHARACTERS[ BadCharacterIndex ], 0 };
			const int32 NumReplacedChars = GeneratedName.ReplaceInline( TestChar, TEXT( "" ) );
		}
	}

	// If the current object name (without a number) already matches our object's name, then use the existing name
	if( CurrentObjectName.GetPlainNameString() == GeneratedName )
	{
		// The object's current name is good enough!  This avoids renaming objects that don't really need to be renamed.
		return CurrentObjectName;
	}

	// If the new name is empty (for example, because it was composed entirely of invalid characters).
	// then we'll use the current name
	if( GeneratedName.IsEmpty() )
	{
		return CurrentObjectName;
	}

	const FName GeneratedFName( *GeneratedName );
	check( GeneratedFName.IsValidXName( INVALID_OBJECTNAME_CHARACTERS ) );

	return GeneratedFName;
}

/*-----------------------------------------------------------------------------
   Duplicating Objects.
-----------------------------------------------------------------------------*/

/**
 * Constructor - zero initializes all members
 */
FObjectDuplicationParameters::FObjectDuplicationParameters( UObject* InSourceObject, UObject* InDestOuter )
: SourceObject(InSourceObject), DestOuter(InDestOuter), DestName(NAME_None)
, FlagMask(RF_AllFlags), ApplyFlags(RF_NoFlags), PortFlags(PPF_None), DestClass(NULL), CreatedObjects(NULL)
{
	checkSlow(SourceObject);
	checkSlow(DestOuter);
	checkSlow(SourceObject->IsValidLowLevel());
	checkSlow(DestOuter->IsValidLowLevel());
	DestClass = SourceObject->GetClass();
}


UObject* StaticDuplicateObject(UObject const* SourceObject, UObject* DestOuter, const TCHAR* DestName, EObjectFlags FlagMask, UClass* DestClass, EDuplicateForPie DuplicateForPIE)
{
	if (!IsAsyncLoading()  && SourceObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		// Detach linker for the outer if it already exists, to avoid problems with PostLoad checking the Linker version
		ResetLoaders(DestOuter);
	}

	// @todo: handle const down the callstack.  for now, let higher level code use it and just cast it off
	FObjectDuplicationParameters Parameters(const_cast<UObject*>(SourceObject), DestOuter);
	if ( DestName && FCString::Strcmp(DestName,TEXT("")) )
	{
		Parameters.DestName = FName(DestName, FNAME_Add, true);
	}

	if ( DestClass == NULL )
	{
		Parameters.DestClass = SourceObject->GetClass();
	}
	else
	{
		Parameters.DestClass = DestClass;
	}
	Parameters.FlagMask = FlagMask;
	if( DuplicateForPIE == SDO_DuplicateForPie)
	{
		Parameters.PortFlags = PPF_DuplicateForPIE;
	}

	return StaticDuplicateObjectEx(Parameters);
}

UObject* StaticDuplicateObjectEx( FObjectDuplicationParameters& Parameters )
{
	// make sure the two classes are the same size, as this hopefully will mean they are serialization
	// compatible. It's not a guarantee, but will help find errors
	checkf( (Parameters.DestClass->GetPropertiesSize() >= Parameters.SourceObject->GetClass()->GetPropertiesSize()),
		TEXT("Source and destination class sizes differ.  Source: %s (%i)   Destination: %s (%i)"),
		*Parameters.SourceObject->GetClass()->GetName(), Parameters.SourceObject->GetClass()->GetPropertiesSize(),
		*Parameters.DestClass->GetName(), Parameters.DestClass->GetPropertiesSize());
	FObjectInstancingGraph InstanceGraph;

	if( !GIsDuplicatingClassForReinstancing )
	{
	// make sure we are not duplicating RF_RootSet as this flag is special
	// also make sure we are not duplicating the RF_ClassDefaultObject flag as this can only be set on the real CDO
	Parameters.FlagMask &= ~(RF_RootSet|RF_ClassDefaultObject);
	}

	// disable object and component instancing while we're duplicating objects, as we're going to instance components manually a little further below
	InstanceGraph.EnableSubobjectInstancing(false);

	// we set this flag so that the component instancing code doesn't think we're creating a new archetype, because when creating a new archetype,
	// the ObjectArchetype for instanced components is set to the ObjectArchetype of the source component, which in the case of duplication (or loading)
	// will be changing the archetype's ObjectArchetype to the wrong object (typically the CDO or something)
	InstanceGraph.SetLoadingObject(true);

	UObject* DupRootObject = Parameters.DuplicationSeed.FindRef(Parameters.SourceObject);
	if ( DupRootObject == NULL )
	{
		DupRootObject = StaticConstructObject(	Parameters.DestClass,
														Parameters.DestOuter,
														Parameters.DestName,
														Parameters.ApplyFlags|Parameters.SourceObject->GetMaskedFlags(Parameters.FlagMask),
														Parameters.SourceObject->GetArchetype()->GetClass() == Parameters.DestClass
																? Parameters.SourceObject->GetArchetype()
																: NULL,
														true,
														&InstanceGraph
														);
	}

	TArray<uint8> ObjectData;

	FUObjectAnnotationSparse<FDuplicatedObject,false>  DuplicatedObjectAnnotation;

	// if seed objects were specified, add those to the DuplicatedObjects map now
	if ( Parameters.DuplicationSeed.Num() > 0 )
	{
		for ( TMap<UObject*,UObject*>::TIterator It(Parameters.DuplicationSeed); It; ++It )
		{
			UObject* Src = It.Key();
			UObject* Dup = It.Value();
			checkSlow(Src);
			checkSlow(Dup);

			// create the DuplicateObjectInfo for this object
			DuplicatedObjectAnnotation.AddAnnotation( Src, FDuplicatedObject( Dup ) );
		}
	}

	// Read from the source object(s)
	FDuplicateDataWriter Writer(
		DuplicatedObjectAnnotation,	// Ref: Object annotation which stores the duplicated object for each source object
		ObjectData,					// Out: Serialized object data
		Parameters.SourceObject,	// Source object to copy
		DupRootObject,				// Destination object to copy into
		Parameters.FlagMask,		// Flags to be copied for duplicated objects
		Parameters.ApplyFlags,		// Flags to always set on duplicated objects
		&InstanceGraph,				// Instancing graph
		Parameters.PortFlags );		// PortFlags

	TArray<UObject*> SerializedObjects;

	
	if (GIsDuplicatingClassForReinstancing)
	{
		FBlueprintSupport::DuplicateAllFields(dynamic_cast<UStruct*>(Parameters.SourceObject), Writer);
	}

	InstanceGraph.SetDestinationRoot( DupRootObject );
	while(Writer.UnserializedObjects.Num())
	{
		UObject*	Object = Writer.UnserializedObjects.Pop();
		Object->Serialize(Writer);
		SerializedObjects.Add(Object);
	};


	FDuplicateDataReader	Reader(DuplicatedObjectAnnotation,ObjectData, Parameters.PortFlags);
	for(int32 ObjectIndex = 0;ObjectIndex < SerializedObjects.Num();ObjectIndex++)
	{
		UObject* SerializedObject = SerializedObjects[ObjectIndex];

		FDuplicatedObject ObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( SerializedObject );
		checkSlow( !ObjectInfo.IsDefault() );

		if ( !SerializedObject->HasAnyFlags(RF_ClassDefaultObject) )
		{
			ObjectInfo.DuplicatedObject->Serialize(Reader);
		}
		else
		{
			// if the source object was a CDO, then transient property values were serialized by the FDuplicateDataWriter
			// and in order to read those properties out correctly, we'll need to enable defaults serialization on the
			// reader as well.
			Reader.StartSerializingDefaults();
			ObjectInfo.DuplicatedObject->Serialize(Reader);
			Reader.StopSerializingDefaults();
		}
	}

	InstanceGraph.EnableSubobjectInstancing(true);

	for( int32 ObjectIndex = 0;ObjectIndex < SerializedObjects.Num(); ObjectIndex++)
	{
		UObject* OrigObject = SerializedObjects[ObjectIndex];

		// don't include any objects which were included in the duplication seed map in the instance graph, as the "duplicate" of these objects
		// may not necessarily be the object that is supposed to be its archetype (the caller can populate the duplication seed map with any objects they wish)
		// and the DuplicationSeed is only used for preserving inter-object references, not for object graphs in SCO and we don't want to call PostDuplicate/PostLoad
		// on them as they weren't actually duplicated
		if ( Parameters.DuplicationSeed.Find(OrigObject) == NULL )
		{
			FDuplicatedObject DupObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( OrigObject );

			UObject* DupObjectArchetype = DupObjectInfo.DuplicatedObject->GetArchetype();

			bool bDuplicateForPIE = (Parameters.PortFlags & PPF_DuplicateForPIE) != 0;

			// Any PIE duplicated object that has the standalone flag is a potential garbage collection issue
			ensure(!(bDuplicateForPIE && DupObjectInfo.DuplicatedObject->HasAnyFlags(RF_Standalone)));

			DupObjectInfo.DuplicatedObject->PostDuplicate(bDuplicateForPIE);
			if ( !DupObjectInfo.DuplicatedObject->IsTemplate() )
			{
				// Don't want to call PostLoad on class duplicated CDOs
				DupObjectInfo.DuplicatedObject->ConditionalPostLoad();
			}
			DupObjectInfo.DuplicatedObject->CheckDefaultSubobjects();
		}
	}

	// if the caller wanted to know which objects were created, do that now
	if ( Parameters.CreatedObjects != NULL )
	{
		// note that we do not clear the map first - this is to allow callers to incrementally build a collection
		// of duplicated objects through multiple calls to StaticDuplicateObject

		// now add each pair of duplicated objects;
		// NOTE: we don't check whether the entry was added from the DuplicationSeed map, so this map
		// will contain those objects as well.
		for(int32 ObjectIndex = 0;ObjectIndex < SerializedObjects.Num();ObjectIndex++)
		{
			UObject* OrigObject = SerializedObjects[ObjectIndex];

			// don't include any objects which were in the DuplicationSeed map, as CreatedObjects should only contain the list
			// of objects actually created during this call to SDO
			if ( Parameters.DuplicationSeed.Find(OrigObject) == NULL )
			{
				FDuplicatedObject DupObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( OrigObject );
				Parameters.CreatedObjects->Add(OrigObject, DupObjectInfo.DuplicatedObject);
			}
		}
	}

	//return DupRoot;
	return DupRootObject;
}


/**
 * Save a copy of this object into the transaction buffer if we are currently recording into
 * one (undo/redo). If bMarkDirty is true, will also mark the package as needing to be saved.
 *
 * @param	bMarkDirty	If true, marks the package dirty if we are currently recording into a
 *						transaction buffer
 * @param	Object		object to save.
*
 * @return	true if a copy of the object was saved and the package potentially marked dirty; false
 *			if we are not recording into a transaction buffer, the package is a PIE/script package,
 *			or the object is not transactional (implies the package was not marked dirty)
 */
bool SaveToTransactionBuffer(UObject* Object, bool bMarkDirty)
{
	bool bSavedToTransactionBuffer = false;

	// Neither PIE world objects nor script packages should end up in the transaction buffer. Additionally, in order
	// to save a copy of the object, we must have a transactor and the object must be transactional.
	const bool IsTransactional = Object->HasAnyFlags(RF_Transactional);
	const bool IsNotPIEOrContainsScriptObject = ( ( Object->GetOutermost()->PackageFlags & ( PKG_PlayInEditor | PKG_ContainsScript ) ) == 0 );

	if ( GUndo && IsTransactional && IsNotPIEOrContainsScriptObject )
	{
		// Mark the package dirty, if requested
		if ( bMarkDirty )
		{
			Object->MarkPackageDirty();
		}

		// Save a copy of the object to the transactor
		GUndo->SaveObject( Object );
		bSavedToTransactionBuffer = true;
	}

	return bSavedToTransactionBuffer;
}

/**
 * Check for StaticAllocateObject error; only for use with the editor, make or other commandlets.
 * 
 * @param	Class		the class of the object to create
 * @param	InOuter		the object to create this object within (the Outer property for the new object will be set to the value specified here).
 * @param	Name		the name to give the new object. If no value (NAME_None) is specified, the object will be given a unique name in the form of ClassName_#.
 * @param	SetFlags	the ObjectFlags to assign to the new object. some flags can affect the behavior of constructing the object.
 * @return	true if NULL should be returned; there was a problem reported 
 */
bool StaticAllocateObjectErrorTests( UClass* InClass, UObject* InOuter, FName InName, EObjectFlags InFlags)
{
	// Validation checks.
	if( !InClass )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Empty class for object %s"), *InName.ToString() );
		return true;
	}

	// for abstract classes that are being loaded NOT in the editor we want to error.  If they are in the editor we do not want to have an error
	if ( InClass->HasAnyClassFlags(CLASS_Abstract) && (InFlags&RF_ClassDefaultObject) == 0 )
	{
		if ( GIsEditor )
		{
			const FString ErrorMsg = FString::Printf(TEXT("Class which was marked abstract was trying to be loaded.  It will be nulled out on save. %s %s"), *InName.ToString(), *InClass->GetName());
			// if we are trying instantiate an abstract class in the editor we'll warn the user that it will be nulled out on save
			UE_LOG(LogUObjectGlobals, Warning, TEXT("%s"), *ErrorMsg);
			ensureMsg(false, *ErrorMsg);
		}
		else
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Can't create object %s: class %s is abstract"), *InName.ToString(), *InClass->GetName()));
			return true;
		}
	}

	if( InOuter == NULL )
	{
		if ( InClass != UPackage::StaticClass() )
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Object is not packaged: %s %s"), *InClass->GetName(), *InName.ToString()) );
			return true;
		}
		else if ( InName == NAME_None )
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package named 'None'") );
			return true;
		}
	}

	if ( (InFlags & RF_ClassDefaultObject) == 0 )
	{
		if ( InOuter != NULL && !InOuter->IsA(InClass->ClassWithin) )
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Object %s %s created in %s instead of %s"), *InClass->GetName(), *InName.ToString(), *InOuter->GetClass()->GetName(), *InClass->ClassWithin->GetName()) );
			return true;
		}
	}
	return false;
}

/**
* Call back into the async loading code to inform of the creation of a new object
*/
void NotifyConstructedDuringAsyncLoading(UObject* Object, bool bSubObject);

/**
* For object overwrites, the class may want to persist some info over the re-intialize
* this is only used for classes in the script compiler
**/
//@todo UE4 this is clunky
static FRestoreForUObjectOverwrite* ObjectRestoreAfterInitProps = NULL;  

UObject* StaticAllocateObject
(
	UClass*			InClass,
	UObject*		InOuter,
	FName			InName,
	EObjectFlags	InFlags,
	bool bCanRecycleSubobjects,
	bool* bOutRecycledSubobject
)
{
//	SCOPE_CYCLE_COUNTER(STAT_AllocateObject);
	checkSlow(InOuter != INVALID_OBJECT); // not legal
	check(!InClass || (InClass->ClassWithin && InClass->ClassConstructor));
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (StaticAllocateObjectErrorTests(InClass,InOuter,InName,InFlags))
		{
			return NULL;
		}
	}
#endif // WITH_EDITOR
	bool bCreatingCDO = (InFlags & RF_ClassDefaultObject) != 0;

	check(InClass);
	check(GIsEditor || bCreatingCDO || !InClass->HasAnyClassFlags(CLASS_Abstract)); // this is a warning in the editor, otherwise it is illegal to create an abstract class, except the CDO
	check(InOuter || (InClass == UPackage::StaticClass() && InName != NAME_None)); // only packages can not have an outer, and they must be named explicitly
	check(bCreatingCDO || !InOuter || InOuter->IsA(InClass->ClassWithin));


	if (bCreatingCDO)
	{
		check(InClass->GetClass());
		if( !GIsDuplicatingClassForReinstancing )
		{
		InName = InClass->GetDefaultObjectName();
		}
		// never call PostLoad on class default objects
		InFlags &= ~(RF_NeedPostLoad|RF_NeedPostLoadSubobjects);
	}

	UObject* Obj = NULL;
	if(InName == NAME_None)
	{
		InName = MakeUniqueObjectName( InOuter, InClass );
	}
	else
	{
		// See if object already exists.
		Obj = StaticFindObjectFastInternal( /*Class=*/ NULL, InOuter, InName, true );

		// Temporary: If the object we found is of a different class, allow the object to be allocated.
		// This breaks new UObject assumptions and these need to be fixed.
		if (Obj && !Obj->GetClass()->IsChildOf(InClass))
		{
			UE_LOG(LogUObjectGlobals, Fatal,
				TEXT("Objects have the same fully qualified name but different paths.\n")
				TEXT("\tNew Object: %s %s.%s\n")
				TEXT("\tExisting Object: %s"),
				*InClass->GetName(), InOuter ? *InOuter->GetPathName() : TEXT(""), *InName.GetPlainNameString(),
				*Obj->GetFullName());
		}
	}

	ULinkerLoad*	Linker						= NULL;
	int32				LinkerIndex					= INDEX_NONE;
	bool			bWasConstructedOnOldObject	= false;
	// True when the object to be allocated already exists and is a subobject.
	bool bSubObject = false;
	int32 TotalSize = InClass->GetPropertiesSize();
	checkSlow(TotalSize);

	if( Obj == NULL )
	{
		int32 Alignment	= FMath::Max( 4, InClass->GetMinAlignment() );
		Obj = (UObject *)GUObjectAllocator.AllocateUObject(TotalSize,Alignment,GIsInitialLoad);
	}
	else
	{
		// Replace an existing object without affecting the original's address or index.
		check(!Obj->HasAnyFlags(RF_Unreachable));

		check(!ObjectRestoreAfterInitProps); // otherwise recursive construction
		ObjectRestoreAfterInitProps = Obj->GetRestoreForUObjectOverwrite();

		// Remember linker, flags, index, and native class info.
		Linker		= Obj->GetLinker();
		LinkerIndex = Obj->GetLinkerIndex();
		InFlags		|= Obj->GetMaskedFlags(RF_Native | RF_RootSet);

		if ( bCreatingCDO )
		{
			check(Obj->HasAllFlags(RF_ClassDefaultObject));
			Obj->SetFlags(InFlags);
			// never call PostLoad on class default objects
			Obj->ClearFlags(RF_NeedPostLoad|RF_NeedPostLoadSubobjects);
		}
		else if(!InOuter || !InOuter->HasAnyFlags(RF_ClassDefaultObject))
		{
			// Should only get in here if we're NOT creating a subobject of a CDO.  CDO subobjects may still need to be serialized off of disk after being created by the constructor
			// if really necessary there was code to allow replacement of object just needing postload, but lets not go there unless we have to
			checkf(!Obj->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_ClassDefaultObject), *FText::Format( NSLOCTEXT( "Core", "ReplaceNotFullyLoaded_f", "Attempting to replace an object that hasn't been fully loaded: {0}" ), FText::FromString( Obj->GetFullName() ) ).ToString() );
		}
		// Subobjects are always created in the constructor, no need to re-create them here unless their archetype != CDO or they're blueprint generated.	
		if (!bCreatingCDO && (!bCanRecycleSubobjects || !Obj->IsDefaultSubobject()))
		{
			// Destroy the object.
			SCOPE_CYCLE_COUNTER(STAT_DestroyObject);
			// Check that the object hasn't been destroyed yet.
			if(!Obj->HasAnyFlags(RF_FinishDestroyed))
			{
				// Get the name before we start the destroy, as destroy renames it
				FString OldName = Obj->GetFullName();

				// Begin the asynchronous object cleanup.
				Obj->ConditionalBeginDestroy();

				// Wait for the object's asynchronous cleanup to finish.
				while (!Obj->IsReadyForFinishDestroy()) 
				{
					// If we're not in the editor, and aren't doing something specifically destructive like reconstructing blueprints, this is fatal
					if (!GIsEditor && FApp::IsGame() && !GIsReconstructingBlueprintInstances)
					{
						UE_LOG(LogUObjectGlobals, Fatal, TEXT("Gamethread hitch waiting for resource cleanup on a UObject (%s) overwrite. Fix the higher level code so that this does not happen."), *OldName );
					}
					FPlatformProcess::Sleep(0);
				}
				// Finish destroying the object.
				Obj->ConditionalFinishDestroy();
			}
			Obj->~UObject();
			bWasConstructedOnOldObject	= true;
		}
		else
		{
			bSubObject = true;
		}
	}

	// If class is transient, non-archetype objects must be transient.
	bool const bCreatingArchetype = (InFlags & RF_ArchetypeObject) != 0;
	if ( !bCreatingCDO && InClass->HasAnyClassFlags(CLASS_Transient) && !bCreatingArchetype )
	{
		InFlags |= RF_Transient;
	}

	if (!bSubObject)
	{
	FMemory::Memzero((void *)Obj, TotalSize);
	new ((void *)Obj) UObjectBase(InClass,InFlags,InOuter,InName);
	}
	else
	{
		// Propagate flags to subobjects created in the native constructor.
		Obj->SetFlags(InFlags);
	}

	if (bWasConstructedOnOldObject)
	{
		// Reassociate the object with it's linker.
		Obj->SetLinker(Linker,LinkerIndex,false);
		if(Linker)
		{
			check(Linker->ExportMap[LinkerIndex].Object == NULL);
			Linker->ExportMap[LinkerIndex].Object = Obj;
		}
	}

	if (GIsAsyncLoading)
	{
		NotifyConstructedDuringAsyncLoading(Obj, bSubObject);
	}

	// Let the caller know if a subobject has just been recycled.
	if (bOutRecycledSubobject)
	{
		*bOutRecycledSubobject = bSubObject;
	}
	
	return Obj;
}

//@todo UE4 - move this stuff to UnObj.cpp or something
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Stack to ensure that PostInitProperties is routed through Super:: calls. **/
TArray<UObject*> PostInitPropertiesCheck;
#endif


void UObject::PostInitProperties()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	PostInitPropertiesCheck.Push(this);
#endif
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	GetClass()->CreatePersistentUberGraphFrame(this);
#endif
}

UObject::UObject(const FObjectInitializer& ObjectInitializer)
{
	check(!ObjectInitializer.Obj || ObjectInitializer.Obj == this);
	const_cast<FObjectInitializer&>(ObjectInitializer).Obj = this;
	const_cast<FObjectInitializer&>(ObjectInitializer).FinalizeSubobjectClassInitialization();
}

/* Global flag so that FObjectFinders know if they are called from inside the UObject constructors or not. */
int32 GIsInConstructor = 0;
/* Object that is currently being constructed with ObjectInitializer */
static UObject* GConstructedObject = NULL;

FObjectInitializer::FObjectInitializer() :
	Obj(NULL),
	ObjectArchetype(NULL),
	bCopyTransientsFromClassDefaults(false),
	bShouldIntializePropsFromArchetype(true),
	bSubobjectClassInitializationAllowed(true),
	InstanceGraph(NULL),
	LastConstructedObject(GConstructedObject)
{
	// Mark we're in the constructor now.	
	GIsInConstructor++;
	GConstructedObject = Obj;
	FTlsObjectInitializers::Push(this);
}	

FObjectInitializer::FObjectInitializer(UObject* InObj, UObject* InObjectArchetype, bool bInCopyTransientsFromClassDefaults, bool bInShouldIntializeProps, struct FObjectInstancingGraph* InInstanceGraph) :
	Obj(InObj),
	ObjectArchetype(InObjectArchetype),
	// if the SubobjectRoot NULL, then we want to copy the transients from the template, otherwise we are doing a duplicate and we want to copy the transients from the class defaults
	bCopyTransientsFromClassDefaults(bInCopyTransientsFromClassDefaults),
	bShouldIntializePropsFromArchetype(bInShouldIntializeProps),
	bSubobjectClassInitializationAllowed(true),
	InstanceGraph(InInstanceGraph),
	LastConstructedObject(GConstructedObject)
{
	// Mark we're in the constructor now.
	GIsInConstructor++;
	GConstructedObject = Obj;
	FTlsObjectInitializers::Push(this);
}

/**
 * Destructor for internal class to finalize UObject creation (initialize properties) after the real C++ constructor is called.
 **/
FObjectInitializer::~FObjectInitializer()
{
	FTlsObjectInitializers::Pop();
	// Let the FObjectFinders know we left the constructor.
	GIsInConstructor--;
	check(GIsInConstructor >= 0);
	GConstructedObject = LastConstructedObject;

//	SCOPE_CYCLE_COUNTER(STAT_PostConstructInitializeProperties);
	check(Obj);
	const bool bIsCDO = Obj->HasAnyFlags(RF_ClassDefaultObject);
	UClass* Class = Obj->GetClass();

	if ( Class != UObject::StaticClass() )
	{
		// InClass->GetClass() == NULL when InClass hasn't been fully initialized yet (during static registration)
		if ( !ObjectArchetype  && Class->GetClass() )
		{
			ObjectArchetype = Class->GetDefaultObject();
		}
	}
	else if (bIsCDO)
	{
		// for the Object CDO, make sure that we do not use an archetype
		check(!ObjectArchetype);
	}
	if (bShouldIntializePropsFromArchetype)
	{
		UClass* BaseClass = (bIsCDO && !GIsDuplicatingClassForReinstancing) ? Obj->GetClass()->GetSuperClass() : Class;
		if (BaseClass == NULL)
		{
			check(Class==UObject::StaticClass());
			BaseClass = Class;
		}
	
		UObject* Defaults = ObjectArchetype ? ObjectArchetype : BaseClass->GetDefaultObject(false); // we don't create the CDO here if it doesn't already exist
		InitProperties(Obj, BaseClass, Defaults, bCopyTransientsFromClassDefaults);
	}

	bool bAllowInstancing = IsInstancingAllowed();
	bool bNeedSubobjectInstancing = InitSubobjectProperties(bAllowInstancing);

	// Restore class information if replacing native class.
	if (ObjectRestoreAfterInitProps != NULL)
	{
		ObjectRestoreAfterInitProps->Restore();
		delete ObjectRestoreAfterInitProps;
		ObjectRestoreAfterInitProps = NULL;
	}

	bool bNeedInstancing = false;
	// if HasAnyFlags(RF_NeedLoad), we do these steps later
	if (!Obj->HasAnyFlags(RF_NeedLoad))
	{
		if (bIsCDO || Class->HasAnyClassFlags(CLASS_PerObjectConfig))
		{
			Obj->LoadConfig(NULL, NULL, bIsCDO ? UE4::LCPF_ReadParentSections : UE4::LCPF_None);
		}
		if (bAllowInstancing)
		{
			// Instance subobject templates for non-cdo blueprint classes or when using non-CDO template.
			const bool bInitPropsWithArchetype = Class->GetDefaultObject(false) == NULL || Class->GetDefaultObject(false) != ObjectArchetype || Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			if ((!bIsCDO || bShouldIntializePropsFromArchetype) && Class->HasAnyClassFlags(CLASS_HasInstancedReference) && bInitPropsWithArchetype)
			{
				// Only blueprint generated CDOs can have their subobjects instanced.
				check(!bIsCDO || !Class->HasAnyClassFlags(CLASS_Intrinsic|CLASS_Native));

				bNeedInstancing = true;
			}
		}
	}
	if (bNeedInstancing || bNeedSubobjectInstancing)
	{
		InstanceSubobjects(Class, bNeedInstancing, bNeedSubobjectInstancing);
	}

	Obj->PostInitProperties();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!PostInitPropertiesCheck.Num() || (PostInitPropertiesCheck.Pop() != Obj))
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s failed to route PostInitProperties"), *Obj->GetClass()->GetName() );
	}
	// Check if all TSubobjectPtr properties have been initialized.
	if (!Obj->HasAnyFlags(RF_NeedLoad))
	{
		for (UProperty* P = Class->RefLink; P; P = P->NextRef)
		{
			if (P->HasAnyPropertyFlags(CPF_SubobjectReference))
			{
				UObjectProperty* ObjProp = CastChecked<UObjectProperty>(P);
				UObject* PropertyValue = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(Obj));
				if (!FSubobjectPtr::IsInitialized(PropertyValue))
				{
					UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s must be initialized in the constructor (at least to NULL) by calling ObjectInitializer.CreateDefaultSubobject"), *ObjProp->GetFullName() );
				}
				else if (PropertyValue && P->HasAnyPropertyFlags(CPF_Transient))
				{
					// Transient subobjects can't be in the list of ComponentInits
					for (int32 Index = 0; Index < ComponentInits.SubobjectInits.Num(); Index++)
					{
						UObject* Subobject = ComponentInits.SubobjectInits[Index].Subobject;
						UE_CLOG(Subobject->GetFName() == PropertyValue->GetFName(), 
							LogUObjectGlobals, Fatal, TEXT("Transient property %s contains a reference to non-transient subobject %s."), *ObjProp->GetFullName(), *PropertyValue->GetName() );
					}
				}
			}
		}
	}
#endif
	if (!Obj->HasAnyFlags(RF_NeedLoad) 
		// if component instancing is not enabled, then we leave the components in an invalid state, which will presumably be fixed by the caller
		&& ((InstanceGraph == NULL) || InstanceGraph->IsSubobjectInstancingEnabled())) 
	{
		Obj->CheckDefaultSubobjects();
	}
}

bool FObjectInitializer::IsInstancingAllowed() const
{
	return (InstanceGraph == NULL) || InstanceGraph->IsSubobjectInstancingEnabled();
}

bool FObjectInitializer::InitSubobjectProperties(bool bAllowInstancing) const
{
	bool bNeedSubobjectInstancing = false;
	// initialize any subobjects, now that the constructors have run
	for (int32 Index = 0; Index < ComponentInits.SubobjectInits.Num(); Index++)
	{
		UObject* Subobject = ComponentInits.SubobjectInits[Index].Subobject;
		UObject* Template = ComponentInits.SubobjectInits[Index].Template;
		InitProperties(Subobject, Template->GetClass(), Template, false);
		if (bAllowInstancing && !Subobject->HasAnyFlags(RF_NeedLoad))
		{
			bNeedSubobjectInstancing = true;
		}
	}

	return bNeedSubobjectInstancing;
}

void FObjectInitializer::InstanceSubobjects(UClass* Class, bool bNeedInstancing, bool bNeedSubobjectInstancing) const
{
	FObjectInstancingGraph TempInstancingGraph;
	FObjectInstancingGraph* UseInstancingGraph = InstanceGraph ? InstanceGraph : &TempInstancingGraph;
	UseInstancingGraph->AddNewObject(Obj);
	// Add any default subobjects
	for (int32 Index = 0; Index < ComponentInits.SubobjectInits.Num(); Index++)
	{
		UseInstancingGraph->AddNewObject(ComponentInits.SubobjectInits[Index].Subobject);
	}
	if (bNeedInstancing)
	{
		UObject* Archetype = Obj->GetArchetype();
		Class->InstanceSubobjectTemplates(Obj, Archetype, Archetype ? Archetype->GetClass() : NULL, Obj, UseInstancingGraph);
	}
	if (bNeedSubobjectInstancing)
	{
		// initialize any subobjects, now that the constructors have run
		for (int32 Index = 0; Index < ComponentInits.SubobjectInits.Num(); Index++)
		{
			UObject* Subobject = ComponentInits.SubobjectInits[Index].Subobject;
			UObject* Template = ComponentInits.SubobjectInits[Index].Template;
			if (!Subobject->HasAnyFlags(RF_NeedLoad))
			{
				Subobject->GetClass()->InstanceSubobjectTemplates(Subobject, Template, Template->GetClass(), Subobject, UseInstancingGraph);
			}
		}
	}
}

UClass* FObjectInitializer::GetClass() const
{
	return Obj->GetClass();
}

void FSubobjectPtr::Set(UObject* InObject)
{
	if (Object != InObject && IsInitialized(Object) && !Object->IsPendingKill())
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Unable to overwrite default subobject %s"), *Object->GetPathName());
	}
	else
	{
		Object = InObject;
	}
}

// Binary initialize object properties to zero or defaults.
void FObjectInitializer::InitProperties(UObject* Obj, UClass* DefaultsClass, UObject* DefaultData, bool bCopyTransientsFromClassDefaults)
{
	SCOPE_CYCLE_COUNTER(STAT_InitProperties);

	check(DefaultsClass && Obj);

	UClass* Class = Obj->GetClass();
	// bool to indicate that we need to initialize any non-native properties (native ones were done when the native constructor was called by the code that created and passed in a FObjectInitializer object)
	bool bNeedInitialize = !Class->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);

	if (Obj->HasAnyFlags(RF_NeedLoad))
	{
		bCopyTransientsFromClassDefaults = false;
	}

	if (!bNeedInitialize && !bCopyTransientsFromClassDefaults && DefaultsClass == Class)
	{
		// This is just a fast path for the below in the common case that we are not doing a duplicate or initializing a CDO and this is all native.
		// We only do it if the DefaultData object is NOT a CDO of the object that's being initialized. CDO data is already initialized in the
		// object's constructor.
		if (DefaultData)
		{
			if (Class->GetDefaultObject(false) != DefaultData)
			{
				for (UProperty* P = Class->PropertyLink; P; P = P->PropertyLinkNext)
				{
					P->CopyCompleteValue_InContainer(Obj, DefaultData);
				}
			}
			else
			{
				// Copy all properties that require additional initialization (CPF_Config, CPF_Localized).
				for (UProperty* P = Class->PostConstructLink; P; P = P->PostConstructLinkNext)
				{
					P->CopyCompleteValue_InContainer(Obj, DefaultData);
				}
			}
		}
	}
	else
	{
		UObject* ClassDefaults = bCopyTransientsFromClassDefaults ? DefaultsClass->GetDefaultObject() : NULL;		
		for (UProperty* P = Class->PropertyLink; P; P = P->PropertyLinkNext)
		{
			if (bNeedInitialize)
			{		
				bNeedInitialize = InitNonNativeProperty(P, Obj);
			}

			if (bCopyTransientsFromClassDefaults && P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient))
			{
				// This is a duplicate. The value for all transient or non-duplicatable properties should be copied
				// from the source class's defaults.
				P->CopyCompleteValue_InContainer(Obj, ClassDefaults);
			}
			else if (P->IsInContainer(DefaultsClass))
			{
				P->CopyCompleteValue_InContainer(Obj, DefaultData);
			}
		}
	}
}

bool FObjectInitializer::IslegalOverride(FName InComponentName, class UClass *DerivedComponentClass, class UClass *BaseComponentClass) const
{
	if (DerivedComponentClass && BaseComponentClass && !DerivedComponentClass->IsChildOf(BaseComponentClass) )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s is not a legal override for component %s because it does not derive from %s."), 
			*DerivedComponentClass->GetFullName(), *InComponentName.ToString(), *BaseComponentClass->GetFullName());
		return false;
	}
	return true;
}

void FObjectInitializer::AssertIfSubobjectSetupIsNotAllowed(const TCHAR* SubobjectName) const
{
	UE_CLOG(!bSubobjectClassInitializationAllowed, LogUObjectGlobals, Fatal,
		TEXT("%s.%s: Subobject class setup is only allowed in base class constructor call (in the initialization list)"), Obj ? *Obj->GetFullName() : TEXT("NULL"), SubobjectName);
}

UObject* StaticConstructObject
(
	UClass*			InClass,
	UObject*		InOuter								/*=GetTransientPackage()*/,
	FName			InName								/*=NAME_None*/,
	EObjectFlags	InFlags								/*=0*/,
	UObject*		InTemplate							/*=NULL*/,
	bool			bCopyTransientsFromClassDefaults	/*=false*/, 
	FObjectInstancingGraph* InInstanceGraph				/*=NULL*/
)
{
	SCOPE_CYCLE_COUNTER(STAT_ConstructObject);
	UObject* Result = NULL;

	checkf(!InTemplate || InTemplate->IsA(InClass) || (InFlags & RF_ClassDefaultObject), TEXT("StaticConstructObject %s is not an instance of class %s and it is not a CDO."), *GetFullNameSafe(InTemplate), *GetFullNameSafe(InClass)); // template must be an instance of the class we are creating, except CDOs

	// Subobjects are always created in the constructor, no need to re-create them unless their archetype != CDO or they're blueprint generated.
	// If the existing subobject is to be re-used it can't have BeginDestroy called on it so we need to pass this information to StaticAllocateObject.	
	const bool bIsNativeFromCDO = InClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic) && 
		(
			!InTemplate || 
			(InName != NAME_None && InTemplate == UObject::GetArchetypeFromRequiredInfo(InClass, InOuter, InName, !!(InFlags & RF_ClassDefaultObject)))
		);
	bool bRecycledSubobject = false;
	Result = StaticAllocateObject(InClass, InOuter, InName, InFlags, bIsNativeFromCDO, &bRecycledSubobject);
	check(Result != NULL);
	// Don't call the constructor on recycled subobjects, they haven't been destroyed.
	if (!bRecycledSubobject)
	{		
		FScopeCycleCounterUObject ConstructorScope(Result);
		(*InClass->ClassConstructor)( FObjectInitializer(Result, InTemplate, bCopyTransientsFromClassDefaults, true, InInstanceGraph) );
	}
	
	if( GIsEditor && GUndo && (InFlags & RF_Transactional) && !(InFlags & RF_NeedLoad) && !InClass->IsChildOf(UField::StaticClass()) )
	{
		// Set RF_PendingKill and update the undo buffer so an undo operation will set RF_PendingKill on the newly constructed object.
		Result->SetFlags(RF_PendingKill);
		SaveToTransactionBuffer(Result, false);
		Result->ClearFlags(RF_PendingKill);
	}
	return Result;
}

void FObjectInitializer::AssertIfInConstructor(UObject* Outer, const TCHAR* ErrorMessage)
{
	UE_CLOG(GIsInConstructor && Outer == GConstructedObject, LogUObjectGlobals, Fatal, TEXT("%s"), ErrorMessage);
}

/**
 * Stores the object flags for all objects in the tracking array.
 */
void FScopedObjectFlagMarker::SaveObjectFlags()
{
	StoredObjectFlags.Empty();

	for ( FObjectIterator It; It; ++It )
	{
		StoredObjectFlags.Add(*It, It->GetFlags());
	}
}

/**
 * Restores the object flags for all objects from the tracking array.
 */
void FScopedObjectFlagMarker::RestoreObjectFlags()
{
	for ( TMap<UObject*,EObjectFlags>::TIterator It(StoredObjectFlags); It; ++It )
	{
		UObject* Object = It.Key();
		EObjectFlags PreviousObjectFlags = It.Value();

		// clear all flags
		Object->ClearFlags(RF_AllFlags);

		// then reset the ones that were originally set
		Object->SetFlags(PreviousObjectFlags);
	}
}

void ConstructorHelpers::FailedToFind(const TCHAR* ObjectToFind)
{
	auto CurrentInitializer = FTlsObjectInitializers::Top();
	const FString Message = FString::Printf(TEXT("CDO Constructor (%s): Failed to find %s\n"),
		(CurrentInitializer && CurrentInitializer->GetClass()) ? *CurrentInitializer->GetClass()->GetName() : TEXT("Unknown"),
		ObjectToFind);
	FPlatformMisc::LowLevelOutputDebugString(*Message);
	UClass::GetDefaultPropertiesFeedbackContext().Log(ELogVerbosity::Error, *Message);
}

void ConstructorHelpers::CheckFoundViaRedirect(UObject *Object, const FString& PathName, const TCHAR* ObjectToFind)
{
	UObjectRedirector* Redir = FindObject<UObjectRedirector>(ANY_PACKAGE, *PathName);
	if (Redir && Redir->DestinationObject == Object)
	{
		FString NewString = Object->GetFullName();
		NewString.ReplaceInline(TEXT(" "), TEXT("'"));
		NewString += TEXT("'");

		auto CurrentInitializer = FTlsObjectInitializers::Top();
		const FString Message = FString::Printf(TEXT("CDO Constructor (%s): Followed redirector (%s), change code to new path (%s)\n"),
			(CurrentInitializer && CurrentInitializer->GetClass()) ? *CurrentInitializer->GetClass()->GetName() : TEXT("Unknown"),
			ObjectToFind, *NewString);

		FPlatformMisc::LowLevelOutputDebugString(*Message);
		UClass::GetDefaultPropertiesFeedbackContext().Log(ELogVerbosity::Warning, *Message);
	}
}

void ConstructorHelpers::CheckIfIsInConstructor(const TCHAR* ObjectToFind)
{
	UE_CLOG(!GIsInConstructor, LogUObjectGlobals, Fatal, TEXT("FObjectFinders can't be used outside of constructors to find %s"), ObjectToFind);
}

void ConstructorHelpers::StripObjectClass( FString& PathName, bool bAssertOnBadPath /*= false */ )
{	
	int32 NameStartIndex = INDEX_NONE;
	PathName.FindChar( TCHAR('\''), NameStartIndex );
	if( NameStartIndex != INDEX_NONE )
	{
		int32 NameEndIndex = INDEX_NONE;
		PathName.FindLastChar( TCHAR('\''), NameEndIndex );
		if(NameEndIndex > NameStartIndex)
		{
			PathName = PathName.Mid( NameStartIndex+1, NameEndIndex-NameStartIndex-1 );
		}
		else
		{
			UE_CLOG( bAssertOnBadPath, LogUObjectGlobals, Fatal, TEXT("Bad path name: %s, missing \' or an incorrect format"), *PathName );
		}
	}
}

/**
 * Archive for tagging unreachable objects in a non recursive manner.
 */
class FCollectorTagUsedNonRecursive : public FReferenceCollector
{
public:

	/**
	 * Default constructor.
	 */
	FCollectorTagUsedNonRecursive()
		:	CurrentObject(NULL)
	{
	}

	// FReferenceCollector interface
	virtual bool IsIgnoringArchetypeRef() const override
	{
		return false;
	}
	virtual bool IsIgnoringTransient() const override
	{
		return false;
	}

	/**
	 * Performs reachability analysis. This information is later used by e.g. IncrementalPurgeGarbage or IsReferenced. The 
	 * algorithm is a simple mark and sweep where all objects are marked as unreachable. The root set passed in is 
	 * considered referenced and also objects that have any of the KeepFlags but none of the IgnoreFlags. RF_PendingKill is 
	 * implicitly part of IgnoreFlags and no object in the root set can have this flag set.
	 *
	 * @param KeepFlags		Objects with these flags will be kept regardless of being referenced or not (see line below)
	 * @param SearchFlags	If set, ignore objects with these flags for initial set, and stop recursion when found
	 * @param FoundReferences	If non-NULL, fill in with all objects that point to an object with SearchFlags set
	 * @param 
	 */
	void PerformReachabilityAnalysis( EObjectFlags KeepFlags, EObjectFlags SearchFlags = RF_NoFlags, FReferencerInformationList* FoundReferences = NULL)
	{
		// Reset object count.
		extern int32 GObjectCountDuringLastMarkPhase;
		GObjectCountDuringLastMarkPhase = 0;
		ReferenceSearchFlags = SearchFlags;
		FoundReferencesList = FoundReferences;

		checkSlow(!DebugSerialize.Num()); // should only be filled int he scope of this function
		// Iterate over all objects.
		for( FObjectIterator It; It; ++It )
		{
			UObject* Object	= *It;
			checkSlow(Object->IsValidLowLevel());
			GObjectCountDuringLastMarkPhase++;

			// Special case handling for objects that are part of the root set.
			if( Object->HasAnyFlags( RF_RootSet ) )
			{
				checkSlow( Object->IsValidLowLevel() );
				// We cannot use RF_PendingKill on objects that are part of the root set.
				checkCode( if( Object->HasAnyFlags( RF_PendingKill ) ) { UE_LOG(LogUObjectGlobals, Fatal, TEXT("Object %s is part of root set though has been marked RF_PendingKill!"), *Object->GetFullName() ); } );
				// Add to list of objects to serialize.
				ObjectsToSerialize.Add( Object );
			}
			// Regular objects.
			else
			{
				// Mark objects as unreachable unless they have any of the passed in KeepFlags set and none of the passed in Search.
				if( (Object->HasAnyFlags(KeepFlags) || KeepFlags == RF_NoFlags) && !Object->HasAnyFlags( SearchFlags ) )
				{
					ObjectsToSerialize.Add( Object );
				}
				else
				{
					Object->SetFlags( RF_Unreachable );
				}
			}
		}

		// Keep serializing objects till we reach the end of the growing array at which point
		// we are done.
		int32 CurrentIndex = 0;
		while( CurrentIndex < ObjectsToSerialize.Num() )
		{
			CurrentObject = ObjectsToSerialize[CurrentIndex++];
			CurrentReferenceInfo = NULL;

			// Serialize object.
			FindReferences( CurrentObject );
		}
		checkSlow(!DebugSerialize.Num()); // everyone should have been removed, or we should have errored
	}

private:

	void FindReferences( UObject* Object )
	{
		check(Object != NULL);

		if( !Object->GetClass()->IsChildOf(UClass::StaticClass()) )
		{
			FSimpleObjectReferenceCollectorArchive CollectorArchive( Object, *this );
			Object->SerializeScriptProperties( CollectorArchive );
		}
		Object->CallAddReferencedObjects(*this);
	}

	/**
	 * Adds passed in object to ObjectsToSerialize list and also removed RF_Unreachable
	 * which is used to signify whether an object already is in the list or not.
	 *
	 * @param	Object	object to add
	 */
	void AddToObjectList( const UObject* ReferencingObject, const UObject* ReferencingProperty, UObject* Object )
	{
#if ENABLE_GC_DEBUG_OUTPUT
		// this message is to help track down culprits behind "Object in PIE world still referenced" errors
		if ( GIsEditor && !GIsPlayInEditorWorld && !CurrentObject->RootPackageHasAnyFlags(PKG_PlayInEditor) && Object->RootPackageHasAnyFlags(PKG_PlayInEditor) )
		{
			UE_LOG(LogGarbage, Warning, TEXT("GC detected illegal reference to PIE object from content [possibly via %s]:"), *ReferencingProperty->GetFullName());
			UE_LOG(LogGarbage, Warning, TEXT("      PIE object: %s"), *Object->GetFullName());
			UE_LOG(LogGarbage, Warning, TEXT("  NON-PIE object: %s"), *CurrentObject->GetFullName());
		}
#endif

		// Mark it as reachable.
		Object->ClearFlags( RF_Unreachable );

		// Add it to the list of objects to serialize.
		ObjectsToSerialize.Add( Object );
	}

	virtual void HandleObjectReference( UObject*& InObject, const UObject* InReferencingObject, const UObject* InReferencingProperty ) override
	{
		checkSlow( !InObject || InObject->IsValidLowLevel() );		
		if( InObject )
		{
			UObject* Object = const_cast<UObject*>(InObject);
			// Remove references to pending kill objects if we're allowed to do so.
			UObject* ObjectToAdd = Object->HasAnyFlags( RF_Unreachable ) ? Object : NULL;
			if (Object && Object->HasAnyFlags(ReferenceSearchFlags))
			{
				// Stop recursing, and add to the list of references
				if (FoundReferencesList)
				{
					if (!CurrentReferenceInfo)
					{
						CurrentReferenceInfo = new(FoundReferencesList->ExternalReferences) FReferencerInformation(CurrentObject);
					}
					if ( InReferencingObject != NULL )
					{
						CurrentReferenceInfo->ReferencingProperties.AddUnique(dynamic_cast<const UProperty*>(InReferencingObject));
					}
					CurrentReferenceInfo->TotalReferences++;
				}
				if (ObjectToAdd)
				{
					// Mark it as reachable.
					Object->ClearFlags( RF_Unreachable );
				}
			}
			else if (ObjectToAdd)
			{
				// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
				AddToObjectList(InReferencingObject, InReferencingProperty, ObjectToAdd);
			}

		}
	}

	/** Object we're currently serializing */
	UObject*			CurrentObject;
	/** Growing array of objects that require serialization */
	TArray<UObject*>	ObjectsToSerialize;
	/** Ignore any references from objects that match these flags */
	EObjectFlags		ReferenceSearchFlags;
	/** List of found references to fill in, if valid */
	FReferencerInformationList*	FoundReferencesList;
	/** Current reference info being filled out */
	FReferencerInformation *CurrentReferenceInfo;
};

/**
 * Returns whether an object is referenced, not counting the one
 * reference at Obj.
 *
 * @param	Obj			Object to check
 * @param	KeepFlags	Objects with these flags will be considered as being referenced. If RF_NoFlags, check all objects
 * @param	bCheckSubObjects	Treat subobjects as if they are the same as passed in object
 * @param	FoundReferences		If non-NULL fill in with list of objects that hold references
 * @return true if object is referenced, false otherwise
 */
bool IsReferenced( UObject*& Obj, EObjectFlags KeepFlags, bool bCheckSubObjects, FReferencerInformationList* FoundReferences )
{
	check(!Obj->HasAnyFlags(RF_Unreachable));

	FScopedObjectFlagMarker ObjectFlagMarker;
	bool bTempReferenceList = false;

	// Tag objects.
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		Object->ClearFlags( RF_TagGarbageTemp );
	}
	// Ignore this object and possibly subobjects
	Obj->SetFlags( RF_TagGarbageTemp );

	if (FoundReferences)
	{
		// Clear old references
		FoundReferences->ExternalReferences.Empty();
		FoundReferences->InternalReferences.Empty();
	}

	if (bCheckSubObjects)
	{
		if (!FoundReferences)
		{
			// Allocate a temporary reference list
			FoundReferences = new FReferencerInformationList;
			bTempReferenceList = true;
		}
		Obj->TagSubobjects( RF_TagGarbageTemp );
	}

	FCollectorTagUsedNonRecursive ObjectReferenceTagger;
	// Exclude passed in object when peforming reachability analysis.
	ObjectReferenceTagger.PerformReachabilityAnalysis( KeepFlags, RF_TagGarbageTemp, FoundReferences );

	bool bIsReferenced = false;
	if (FoundReferences)
	{
		bIsReferenced = FoundReferences->ExternalReferences.Num() > 0 || !Obj->HasAnyFlags( RF_Unreachable );
		// Move some from external to internal before returning
		for (int32 i = 0; i < FoundReferences->ExternalReferences.Num(); i++)
		{
			FReferencerInformation *OldRef = &FoundReferences->ExternalReferences[i];
			if (OldRef->Referencer == Obj)
			{
				FoundReferences->ExternalReferences.RemoveAt(i);
				i--;
			}
			else if (OldRef->Referencer->IsIn(Obj))
			{
				FReferencerInformation *NewRef = new(FoundReferences->InternalReferences) FReferencerInformation(OldRef->Referencer, OldRef->TotalReferences, OldRef->ReferencingProperties);
				FoundReferences->ExternalReferences.RemoveAt(i);
				i--;
			}
		}
	}
	else
	{
		// Return whether the object was referenced and restore original state.
		bIsReferenced = !Obj->HasAnyFlags( RF_Unreachable );
	}
	
	if (bTempReferenceList)
	{
		// We allocated a temp list
		delete FoundReferences;
	}

	return bIsReferenced;
}


FArchive& FScriptInterface::Serialize(FArchive& Ar, UClass* InterfaceType)
{
	UObject* ObjectValue = GetObject();
	Ar << ObjectValue;
	SetObject(ObjectValue);
	if (Ar.IsLoading())
	{
		SetInterface(ObjectValue != NULL ? ObjectValue->GetInterfaceAddress(InterfaceType) : NULL);
	}
	return Ar;
}

/** A struct used as stub for deleted ones. */
UScriptStruct* GetFallbackStruct()
{
	static UScriptStruct* FallbackStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("FallbackStruct"));
	return FallbackStruct;
}

UObject* FObjectInitializer::CreateDefaultSubobject(UObject* Outer, FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bAbstract, bool bIsTransient) const
{
	if (SubobjectFName == NAME_None)
	{
		UE_LOG(LogClass, Fatal, TEXT("Illegal default subobject name: %s"), *SubobjectFName.ToString());
	}

	UObject* Result = NULL;
	UClass* OverrideClass = ComponentOverrides.Get(SubobjectFName, ReturnType, ClassToCreateByDefault, *this);
	if (!OverrideClass && bIsRequired)
	{
		OverrideClass = ClassToCreateByDefault;
		UE_LOG(LogClass, Warning, TEXT("Ignored DoNotCreateDefaultSubobject for %s as it's marked as required. Creating %s."), *SubobjectFName.ToString(), *OverrideClass->GetName());
	}
	if (OverrideClass)
	{
		check(OverrideClass->IsChildOf(ReturnType));

		// Abstract sub-objects are only allowed when explicitly created with CreateAbstractDefaultSubobject.
		if (!OverrideClass->HasAnyClassFlags(CLASS_Abstract) || !bAbstract)
		{
			UObject* Template = OverrideClass->GetDefaultObject(); // force the CDO to be created if it hasn't already
			const EObjectFlags SubobjectFlags = Outer->GetMaskedFlags(RF_PropagateToSubObjects);
			Result = StaticConstructObject(OverrideClass, Outer, SubobjectFName, SubobjectFlags);
			if (!bIsTransient && !Outer->GetArchetype()->GetClass()->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
			{
				// The archetype of the outer is not native, so we need to copy properties to the subobjects after the C++ constructor chain for the outer has run (because those sets properties on the subobjects)
				UObject* MaybeTemplate = Outer->GetArchetype()->GetClass()->GetDefaultSubobjectByName(SubobjectFName);
				if (MaybeTemplate && MaybeTemplate->IsA(ReturnType) && Template != MaybeTemplate)
				{
					ComponentInits.Add(Result, MaybeTemplate);
				}
			}
			if (Outer->HasAnyFlags(RF_ClassDefaultObject) && Outer->GetClass()->GetSuperClass())
			{
				Outer->GetClass()->AddDefaultSubobject(Result, ReturnType);
			}
			Result->SetFlags(RF_DefaultSubObject);
		}
	}
	return Result;
}

UObject* FObjectInitializer::CreateEditorOnlyDefaultSubobject(UObject* Outer, FName SubobjectName, UClass* ReturnType, bool bTransient /*= false*/) const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		UObject* EditorSubobject = CreateDefaultSubobject(Outer, SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ false, /*bIsAbstract =*/ false, bTransient);
		if (EditorSubobject)
		{
			EditorSubobject->MarkAsEditorOnlySubobject();
		}
		return EditorSubobject;
	}
#endif
	return nullptr;
}
