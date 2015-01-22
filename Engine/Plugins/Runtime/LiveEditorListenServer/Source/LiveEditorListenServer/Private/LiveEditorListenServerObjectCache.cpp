// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorListenServerPrivatePCH.h"
#include "LiveEditorListenServerObjectCache.h"

namespace nLiveEditorListenServer
{

namespace nObjectCache
{
	FORCEINLINE bool ObjectBaseIsA( const UObjectBase *ObjectBase, const UClass* SomeBase )
	{
		if ( SomeBase==NULL )
		{
			return true;
		}
		for ( UClass* TempClass=ObjectBase->GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
		{
			if ( TempClass == SomeBase )
			{
				return true;
			}
		}
		return false;
	}

	FORCEINLINE bool IsCachePartner( const class UObject &EditorObject, const class UObject &TestObject )
	{
		if ( EditorObject.HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject) )
		{
			return TestObject.IsA( EditorObject.GetClass() );
		}
		else
		{
			return false;
		}
	}
}

FObjectCache::FObjectCache()
: bCacheActive(false)
{
}

FObjectCache::~FObjectCache()
{
}

void FObjectCache::FindObjectDependants( const class UObject* EditorObject, TArray< TWeakObjectPtr<UObject> >& OutValues )
{
	if ( EditorObject == NULL || !bCacheActive )
		return;

	if ( ObjectLookupCache.Contains(EditorObject) )
	{
		ObjectLookupCache.MultiFind( EditorObject, OutValues );
	}
	else
	{
		for(TObjectIterator<UObject> It; It; ++It)
		{
			UObject *Object = *It;
			
			check( !GIsEditor );
			//UWorld *World = GEngine->GetWorldFromContextObject( Object, false );
			//if ( World == NULL )
			//	continue;

			if ( !nObjectCache::IsCachePartner(*EditorObject, *Object) )
				continue;

			ObjectLookupCache.AddUnique( EditorObject, Object );
			TrackedObjects.Add( Object );

			OutValues.Add( Object );
		}
	}
}

void FObjectCache::StartCache()
{
	TrackedObjects.Empty();
	ObjectLookupCache.Empty();
	bCacheActive = true;
}

void FObjectCache::EndCache()
{
	bCacheActive = false;
	TrackedObjects.Empty();
	ObjectLookupCache.Empty();
}

void FObjectCache::OnObjectCreation( const class UObjectBase* ObjectBase )
{
	if ( !CacheStarted() || GEngine == NULL )
		return;

	PendingObjectCreations.Add(ObjectBase);
}

void FObjectCache::OnObjectDeletion( const class UObjectBase* ObjectBase )
{
	if ( !bCacheActive )
		return;

	UObject *AsObject = const_cast<UObject*>( static_cast<const UObject*>(ObjectBase) );

	//if deleted object was a value, remove specific associations
	TArray<const UObject*>::TConstIterator It(TrackedObjects);
	for ( ; It; It++ )
	{
		ObjectLookupCache.Remove( *It, AsObject );
	}

	//if deleted object was a Key, remove all associations
	ObjectLookupCache.Remove( AsObject );
	TrackedObjects.Remove( AsObject );

	for ( int32 i = PendingObjectCreations.Num()-1; i >= 0; --i )
	{
		if ( PendingObjectCreations[i] == ObjectBase )
		{
			PendingObjectCreations.RemoveAt(i);
		}
	}
}

void FObjectCache::EvaluatePendingCreations( TArray<UObject*> &NewTrackedObjects )
{
	if ( !CacheStarted() || ObjectLookupCache.Num() <= 0 || GEngine == NULL )
		return;

	for ( TArray< const UObjectBase* >::TIterator PendingObjIt(PendingObjectCreations); PendingObjIt; ++PendingObjIt )
	{
		const UObjectBase *ObjectBase = (*PendingObjIt);
		if ( ObjectBase != NULL )
		{
			if ( !nObjectCache::ObjectBaseIsA(ObjectBase, UObject::StaticClass()) || (ObjectBase->GetFlags() & RF_ClassDefaultObject) != 0 )
				continue;
			UObject *AsObject = const_cast<UObject*>( static_cast<const UObject*>(ObjectBase) );
	
			UWorld *World = GEngine->GetWorldFromContextObject( AsObject, false );
			if ( World == NULL || World->WorldType != EWorldType::Game )
				continue;

			for ( TMultiMap< const UObject*, TWeakObjectPtr<UObject> >::TConstIterator ObjIt(ObjectLookupCache); ObjIt; ++ObjIt )
			{
				const UObject *Key = (*ObjIt).Key;
				if ( !nObjectCache::IsCachePartner(*Key, *AsObject) )
					continue;

				ObjectLookupCache.AddUnique( Key, AsObject );
				TrackedObjects.Add( AsObject );
				NewTrackedObjects.Add( AsObject );
			}
		}
	}
	PendingObjectCreations.Empty();
}

bool FObjectCache::CacheStarted() const
{
	return (bCacheActive && ObjectLookupCache.Num() > 0);
}

}