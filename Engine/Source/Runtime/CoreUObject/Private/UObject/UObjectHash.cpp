// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectHash.cpp: Unreal object name hashes
=============================================================================*/

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectHash, Log, All);

/**
 * This implementation will use more space than the UE3 implementation. The goal was to make UObjects smaller to save L2 cache space. 
 * The hash is rarely used at runtime. A more space-efficient implementation is possible.
 */


/**
 * The object hash size to use
 *
 * NOTE: This must be power of 2 so that (size - 1) turns on all bits!
 * NOTE2: With the new implementation, there is no particular memory advantage to having this be small
*/
#define OBJECT_HASH_BINS (1024*1024)

static TMultiMap<int32,class UObjectBase*> ObjectHash;
static TMultiMap<int32,class UObjectBase*> ObjectHashOuter;

/**
 * Calculates the object's hash just using the object's name index
 *
 * @param ObjName the object's name to use the index of
 */
static FORCEINLINE int32 GetObjectHash(FName ObjName)
{
	return (ObjName.GetComparisonIndex() ^ ObjName.GetNumber()) & (OBJECT_HASH_BINS - 1);
}

/**
 * Calculates the object's hash just using the object's name index
 * XORed with the outer. Yields much better spread in the hash
 * buckets, but requires knowledge of the outer, which isn't available
 * in all cases.
 *
 * @param ObjName the object's name to use the index of
 * @param Outer the object's outer pointer treated as an int32
 */
static FORCEINLINE int32 GetObjectOuterHash(FName ObjName,PTRINT Outer)
{
	return ((ObjName.GetComparisonIndex() ^ ObjName.GetNumber()) ^ (Outer >> 4)) & (OBJECT_HASH_BINS - 1);
}

/**
 * Variation of StaticFindObjectFast that uses explicit path.
 *
 * @param	ObjectClass		The to be found object's class
 * @param	ObjectName		The to be found object's class
 * @param	ObjectPathName	Full path name for the object to search for
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* StaticFindObjectFastExplicit( UClass* ObjectClass, FName ObjectName, const FString& ObjectPathName, bool bExactClass, EObjectFlags ExcludeFlags/*=0*/ )
{
	checkSlow(FPackageName::IsShortPackageName(ObjectName)); //@Package name transition, we aren't checking the name here because we know this is only used for texture
	// Find an object with the specified name and (optional) class, in any package; if bAnyPackage is false, only matches top-level packages
	int32 Hash = GetObjectHash( ObjectName );
	for(TMultiMap<int32,class UObjectBase*>::TConstKeyIterator HashIt(ObjectHash,Hash); HashIt; ++HashIt)
	{
		UObject *Object = (UObject *)HashIt.Value();
		if
		(	(Object->GetFName()==ObjectName)

		/* Don't return objects that have any of the exclusive flags set */
		&&	!Object->HasAnyFlags(ExcludeFlags)

		/** If a class was specified, check that the object is of the correct class */
		&&	(ObjectClass==NULL || (bExactClass ? Object->GetClass()==ObjectClass : Object->IsA(ObjectClass)))
		)

		{
			FString ObjectPath = Object->GetPathName();
			/** Finally check the explicit path */
			if (ObjectPath == ObjectPathName)
			{
				checkf( !Object->HasAnyFlags(RF_Unreachable), TEXT("%s"), *Object->GetFullName() );
				return Object;
			}
		}
	}

	return NULL;
}

UObject* StaticFindObjectFastInternal( UClass* ObjectClass, UObject* ObjectPackage, FName ObjectName, bool bExactClass, bool bAnyPackage, EObjectFlags ExcludeFlags )
{
	INC_DWORD_STAT(STAT_FindObjectFast);
	check(ObjectPackage != ANY_PACKAGE); // this could never have returned anything but NULL
	// If they specified an outer use that during the hashing
	UObject* Result = NULL;
	if (ObjectPackage != NULL)
	{
		int32 Hash = GetObjectOuterHash( ObjectName, (PTRINT)ObjectPackage );
		for(TMultiMap<int32,class UObjectBase*>::TConstKeyIterator HashIt(ObjectHashOuter,Hash); HashIt; ++HashIt)
		{
			UObject *Object = (UObject *)HashIt.Value();
			if
			/* check that the name matches the name we're searching for */
			(	(Object->GetFName()==ObjectName)

			/* Don't return objects that have any of the exclusive flags set */
			&&	!Object->HasAnyFlags(ExcludeFlags)

			/* check that the object has the correct Outer */
			&&	Object->GetOuter() == ObjectPackage

			/** If a class was specified, check that the object is of the correct class */
			&&	(ObjectClass==NULL || (bExactClass ? Object->GetClass()==ObjectClass : Object->IsA(ObjectClass))) )
			{
				checkf( !Object->HasAnyFlags(RF_Unreachable), TEXT("%s"), *Object->GetFullName() );
				if (Result)
				{
					UE_LOG(LogUObjectHash, Warning, TEXT("Ambiguous search, could be %s or %s"), *GetFullNameSafe(Result), *GetFullNameSafe(Object));
				}
				else
				{
					Result = Object;
				}
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
				break;
#endif
			}
		}
	}
	else
	{
		// Find an object with the specified name and (optional) class, in any package; if bAnyPackage is false, only matches top-level packages
		FName ActualObjectName = ObjectName;
		const FString ObjectNameString = ObjectName.ToString();
		const int32 DotIndex = FMath::Max<int32>(ObjectNameString.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd), 
												 ObjectNameString.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
		if (DotIndex != INDEX_NONE)
		{
			ActualObjectName = FName(*ObjectNameString.Mid(DotIndex + 1));
		}
		const int32 Hash = GetObjectHash( ActualObjectName );
		for(TMultiMap<int32,class UObjectBase*>::TConstKeyIterator HashIt(ObjectHash,Hash); HashIt; ++HashIt)
		{
			UObject *Object = (UObject *)HashIt.Value();
			if
			(	(Object->GetFName()==ActualObjectName)

			/* Don't return objects that have any of the exclusive flags set */
			&&	!Object->HasAnyFlags(ExcludeFlags)

			/*If there is no package (no InObjectPackage specified, and InName's package is "")
				and the caller specified any_package, then accept it, regardless of its package.
				Or, if the object is a top-level package then accept it immediately.*/
			&&	(bAnyPackage ||	!Object->GetOuter())
			

			/** If a class was specified, check that the object is of the correct class */
			&&	(ObjectClass==NULL || (bExactClass ? Object->GetClass()==ObjectClass : Object->IsA(ObjectClass))) 

			/** Ensure that the partial path provided matches the object found */
			&&  (Object->GetPathName().EndsWith(ObjectNameString)) )
			{
				checkf( !Object->HasAnyFlags(RF_Unreachable), TEXT("%s"), *Object->GetFullName() );
				if (Result)
				{
					UE_LOG(LogUObjectHash, Warning, TEXT("Ambiguous search, could be %s or %s"), *GetFullNameSafe(Result), *GetFullNameSafe(Object));
				}
				else
				{
					Result = Object;
				}
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
				break;
#endif
			}
		}
	}
	// Not found.
	return Result;
}

/** Map of object to their outers, used to avoid an object iterator to find such things. **/
static TMap<UObjectBase*, TSet<UObjectBase*> > ObjectOuterMap;
static TMap<UClass*, TSet<UObjectBase*> > ClassToObjectListMap;
static TMap<UClass*, TSet<UClass*> > ClassToChildListMap;

static void AddToOuterMap(UObjectBase* Object)
{
	TSet<UObjectBase*>& Inners = ObjectOuterMap.FindOrAdd(Object->GetOuter());
	bool bIsAlreadyInSetPtr = false;
	Inners.Add(Object, &bIsAlreadyInSetPtr);
	check(!bIsAlreadyInSetPtr); // if it already exists, something is wrong with the external code
}

static void AddToClassMap(UObjectBase* Object)
{
	{
		check(Object->GetClass());
		TSet<UObjectBase*>& ObjectList = ClassToObjectListMap.FindOrAdd(Object->GetClass());
		bool bIsAlreadyInSetPtr = false;
		ObjectList.Add(Object, &bIsAlreadyInSetPtr);
		check(!bIsAlreadyInSetPtr); // if it already exists, something is wrong with the external code
	}

	UObjectBaseUtility* ObjectWithUtility = static_cast<UObjectBaseUtility*>(Object);
	if ( ObjectWithUtility->IsA(UClass::StaticClass()) )
	{
		UClass* Class = static_cast<UClass*>(ObjectWithUtility);
		UClass* SuperClass = Class->GetSuperClass();
		if ( SuperClass )
		{
			TSet<UClass*>& ChildList = ClassToChildListMap.FindOrAdd(SuperClass);
			bool bIsAlreadyInSetPtr = false;
			ChildList.Add(Class, &bIsAlreadyInSetPtr);
			check(!bIsAlreadyInSetPtr); // if it already exists, something is wrong with the external code
		}
	}
}

static void RemoveFromOuterMap(UObjectBase* Object)
{
	TSet<UObjectBase*>& Inners = ObjectOuterMap.FindOrAdd(Object->GetOuter());
	int32 NumRemoved = Inners.Remove(Object);
    if (NumRemoved != 1)
	{
		UE_LOG(LogUObjectHash, Error, TEXT("Internal Error: RemoveFromOuterMap NumRemoved = %d  for %s"), NumRemoved, *GetFullNameSafe((UObjectBaseUtility*)Object));
	}
	check(NumRemoved == 1); // must have existed, else something is wrong with the external code
	if (!Inners.Num())
	{
		ObjectOuterMap.Remove(Object->GetOuter());
	}
}

static void RemoveFromClassMap(UObjectBase* Object)
{
	UObjectBaseUtility* ObjectWithUtility = static_cast<UObjectBaseUtility*>(Object);

	{
		TSet<UObjectBase*>& ObjectList = ClassToObjectListMap.FindOrAdd(Object->GetClass());
		int32 NumRemoved = ObjectList.Remove(Object);
		if (NumRemoved != 1)
		{
			UE_LOG(LogUObjectHash, Error, TEXT("Internal Error: RemoveFromClassMap NumRemoved = %d from object list for %s"), NumRemoved, *GetFullNameSafe(ObjectWithUtility));
		}
		check(NumRemoved == 1); // must have existed, else something is wrong with the external code
		if (!ObjectList.Num())
		{
			ClassToObjectListMap.Remove(Object->GetClass());
		}
	}

	if ( ObjectWithUtility->IsA(UClass::StaticClass()) )
	{
		UClass* Class = static_cast<UClass*>(ObjectWithUtility);
		UClass* SuperClass = Class->GetSuperClass();
		if ( SuperClass )
		{
			// Remove the class from the SuperClass' child list
			TSet<UClass*>& ChildList = ClassToChildListMap.FindOrAdd(SuperClass);
			int32 NumRemoved = ChildList.Remove(Class);
			if (NumRemoved != 1)
			{
				UE_LOG(LogUObjectHash, Error, TEXT("Internal Error: RemoveFromClassMap NumRemoved = %d from child list for %s"), NumRemoved, *GetFullNameSafe(ObjectWithUtility));
			}
			check(NumRemoved == 1); // must have existed, else something is wrong with the external code
			if (!ChildList.Num())
			{
				ClassToChildListMap.Remove(SuperClass);
			}
		}
	}
}

void GetObjectsWithOuter(const class UObjectBase* Outer, TArray<UObject *>& Results, bool bIncludeNestedObjects, EObjectFlags ExclusionFlags)
{
	// We don't want to return any objects that are currently being background loaded unless we're using the object iterator during async loading.
	ExclusionFlags |= RF_Unreachable;
	if( !GIsAsyncLoading )
	{
		ExclusionFlags = EObjectFlags(ExclusionFlags | RF_AsyncLoading);
	}
	int32 StartNum = Results.Num();
	TSet<UObjectBase*> const* Inners = ObjectOuterMap.Find(Outer);
	if (Inners)
	{
		for(TSet<UObjectBase*>::TConstIterator It(*Inners); It; ++It)
		{
			UObject *Object = static_cast<UObject *>(*It);
			if (!Object->HasAnyFlags(ExclusionFlags))
			{
				Results.Add(Object);
			}
		}
		int32 MaxResults = GUObjectArray.GetObjectArrayNum();
		while (StartNum != Results.Num() && bIncludeNestedObjects) 
		{
			int32 RangeStart = StartNum;
			int32 RangeEnd = Results.Num();
			StartNum = RangeEnd;
			for (int32 Index = RangeStart; Index < RangeEnd; Index++)
			{
				TSet<UObjectBase*> const* InnerInners = ObjectOuterMap.Find(Results[Index]);
				if (InnerInners)
				{
					for(TSet<UObjectBase*>::TConstIterator It(*InnerInners); It; ++It)
					{
						UObject *Object = static_cast<UObject *>(*It);
						if (!Object->HasAnyFlags(ExclusionFlags))
						{
							Results.Add(Object);
						}
					}
				}
			}
			check(Results.Num() <= MaxResults); // otherwise we have a cycle in the outer chain, which should not be possible
		} 
	}
}

UObjectBase* FindObjectWithOuter(class UObjectBase* Outer, class UClass* ClassToLookFor, FName NameToLookFor)
{
	check(Outer && ClassToLookFor);
	// We don't want to return any objects that are currently being background loaded unless we're using the object iterator during async loading.
	EObjectFlags ExclusionFlags = RF_Unreachable;
	if( !GIsAsyncLoading )
	{
		ExclusionFlags = EObjectFlags(ExclusionFlags | RF_AsyncLoading);
	}

	if (NameToLookFor != NAME_None)
	{
		return StaticFindObjectFastInternal( ClassToLookFor, static_cast<UObject*>(Outer), NameToLookFor, false, false, ExclusionFlags );
	}

	UObject *Result = NULL;
	TSet<UObjectBase*> const* Inners = ObjectOuterMap.Find(Outer);
	if (Inners)
	{

		for(TSet<UObjectBase*>::TConstIterator It(*Inners); It; ++It)
		{
			UObject *Object = static_cast<UObject *>(*It);
			if (Object->HasAnyFlags(ExclusionFlags))
			{
				continue;
			}
			if (!Object->IsA(ClassToLookFor))
			{
				continue;
			}
			Result = Object;
			break;
		}
	}
	return Result;
}

/** Helper function that returns all the children of the specified class recursively */
static void RecursivelyPopulateDerivedClasses(UClass* ParentClass, TSet<UClass*>& OutAllDerivedClass)
{
	TSet<UClass*>* ChildSet = ClassToChildListMap.Find(ParentClass);
	if ( ChildSet )
	{
		for ( auto ChildIt = ChildSet->CreateConstIterator(); ChildIt; ++ChildIt )
		{
			UClass* ChildClass = *ChildIt;
			if ( !OutAllDerivedClass.Contains(ChildClass) )
			{
				OutAllDerivedClass.Add(ChildClass);
				RecursivelyPopulateDerivedClasses(ChildClass, OutAllDerivedClass);
			}
		}
	}
}

void GetObjectsOfClass(UClass* ClassToLookFor, TArray<UObject *>& Results, bool bIncludeDerivedClasses, EObjectFlags AdditionalExcludeFlags)
{
	// We don't want to return any objects that are currently being background loaded unless we're using the object iterator during async loading.
	EObjectFlags ExclusionFlags = RF_Unreachable;
	if( !GIsAsyncLoading )
	{
		ExclusionFlags |= RF_AsyncLoading;
	}
	ExclusionFlags |= AdditionalExcludeFlags;

	TSet<UClass*> ClassesToSearch;
	ClassesToSearch.Add(ClassToLookFor);
	if ( bIncludeDerivedClasses )
	{
		RecursivelyPopulateDerivedClasses(ClassToLookFor, ClassesToSearch);
	}

	const int32 MaxResults = GUObjectArray.GetObjectArrayNum();
	for ( auto ClassIt = ClassesToSearch.CreateConstIterator(); ClassIt; ++ClassIt )
	{
		TSet<UObjectBase*> const* List = ClassToObjectListMap.Find(*ClassIt);

		if ( List )
		{
			for( auto ObjectIt = List->CreateConstIterator(); ObjectIt; ++ObjectIt )
			{
				UObject *Object = static_cast<UObject *>(*ObjectIt);
				if (!Object->HasAnyFlags(ExclusionFlags))
				{
					Results.Add(Object);
				}
			}
		}
	}

	check(Results.Num() <= MaxResults); // otherwise we have a cycle in the outer chain, which should not be possible
}

void GetDerivedClasses(UClass* ClassToLookFor, TArray<UClass *>& Results, bool bRecursive)
{
	if ( bRecursive )
	{
		TSet<UClass*> AllDerivedClasses;
		RecursivelyPopulateDerivedClasses(ClassToLookFor, AllDerivedClasses);
		Results.Append( AllDerivedClasses.Array() );
	}
	else
	{
		TSet<UClass*>* DerivedClasses = ClassToChildListMap.Find(ClassToLookFor);
		if ( DerivedClasses )
		{
			Results.Append( DerivedClasses->Array() );
		}
	}
}

/**
 * Add an object to the name hash tables
 *
 * @param	Object		Object to add to the hash tables
 */
void HashObject(UObjectBase* Object)
{
	FName Name = Object->GetFName();
	if (Name == NAME_None)
	{
		return;
	}

	int32 Hash = GetObjectHash( Name );
	checkSlow(!ObjectHash.FindPair(Hash,Object));  // if it already exists, something is wrong with the external code
	ObjectHash.Add(Hash,Object);

	Hash = GetObjectOuterHash(Name,(PTRINT)Object->GetOuter());
	checkSlow(!ObjectHashOuter.FindPair(Hash,Object));  // if it already exists, something is wrong with the external code
	ObjectHashOuter.Add(Hash,Object);

	AddToOuterMap(Object);
	AddToClassMap(Object);
}

/**
 * Remove an object to the name hash tables
 *
 * @param	Object		Object to remove from the hash tables
 */
void UnhashObject(UObjectBase* Object)
{
	FName Name = Object->GetFName();
	if (Name == NAME_None)
	{
		return;
	}

	int32 Hash = GetObjectHash( Name );
	int32 NumRemoved = ObjectHash.RemoveSingle(Hash,Object);
	check(NumRemoved == 1); // must have existed, else something is wrong with the external code

	Hash = GetObjectOuterHash(Name,(PTRINT)Object->GetOuter());
	NumRemoved = ObjectHashOuter.RemoveSingle(Hash,Object);
	check(NumRemoved == 1); // must have existed, else something is wrong with the external code

	RemoveFromOuterMap(Object);
	RemoveFromClassMap(Object);
}
