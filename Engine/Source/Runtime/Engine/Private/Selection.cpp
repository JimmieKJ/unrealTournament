// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "UObjectAnnotation.h"
#include "Engine/Selection.h"

DEFINE_LOG_CATEGORY_STATIC(LogSelection, Log, All);

USelection::FOnSelectionChanged	USelection::SelectionChangedEvent;
USelection::FOnSelectionChanged	USelection::SelectObjectEvent;								
FSimpleMulticastDelegate		USelection::SelectNoneEvent;


USelection::USelection(const FObjectInitializer& ObjectInitializer)
:	UObject(ObjectInitializer)
,	SelectionMutex( 0 )
,	bIsBatchDirty(false)
{
	SelectedClasses.MaxItems = 5;
}


void USelection::Select(UObject* InObject)
{
	check( InObject );

	// Warn if we attempt to select a PIE object.
	if ( InObject->GetOutermost()->PackageFlags & PKG_PlayInEditor )
	{
		UE_LOG(LogSelection, Warning, TEXT("PIE object was selected: \"%s\""), *InObject->GetFullName() );
	}

	const bool bSelectionChanged = !InObject->IsSelected();
	GSelectedAnnotation.Set(InObject);

	// Add to selected lists.
	SelectedObjects.AddUnique( InObject );
	SelectedClasses.AddUnique( InObject->GetClass() );

	if( !IsBatchSelecting() )
	{
		// Call this after the item has been added from the selection set.
		USelection::SelectObjectEvent.Broadcast( InObject );
	}

	if ( bSelectionChanged )
	{
		MarkBatchDirty();
	}
}


void USelection::Deselect(UObject* InObject)
{
	check( InObject );

	const bool bSelectionChanged = InObject->IsSelected();
	GSelectedAnnotation.Clear(InObject);

	// Remove from selected list.
	SelectedObjects.Remove( InObject );

	if( !IsBatchSelecting() )
	{
		// Call this after the item has been removed from the selection set.
		USelection::SelectObjectEvent.Broadcast( InObject );
	}

	if ( bSelectionChanged )
	{
		MarkBatchDirty();
	}
}


void USelection::Select(UObject* InObject, bool bSelect)
{
	if( bSelect )
	{
		Select( InObject );
	}
	else
	{
		Deselect( InObject );
	}
}


void USelection::ToggleSelect( UObject* InObject )
{
	check( InObject );

	Select( InObject, !InObject->IsSelected() );
}

void USelection::DeselectAll( UClass* InClass )
{
	// Fast path for deselecting all UObjects with any flags
	if ( InClass == UObject::StaticClass() )
	{
		InClass = NULL;
	}

	bool bSelectionChanged = false;

	// Remove from the end to minimize memmoves.
	for ( int32 i = SelectedObjects.Num()-1 ; i >= 0 ; --i )
	{
		UObject* Object = GetSelectedObject(i);
		// Remove NULLs from SelectedObjects array.
		if ( !Object )
		{
			SelectedObjects.RemoveAt( i );
		}
		// If its the right class and has the right flags. 
		// Note that if InFlags is 0, HasAllFlags(0) always returns true.
		else if( !InClass || Object->IsA( InClass ) )
		{
			GSelectedAnnotation.Clear(Object);
			SelectedObjects.RemoveAt( i );

			// Call this after the item has been removed from the selection set.
			USelection::SelectObjectEvent.Broadcast( Object );

			bSelectionChanged = true;
		}
	}

	if ( bSelectionChanged )
	{
		MarkBatchDirty();
		if ( !IsBatchSelecting() )
		{
			USelection::SelectionChangedEvent.Broadcast(this);
		}
	}
}

void USelection::MarkBatchDirty()
{
	if ( IsBatchSelecting() )
	{
		bIsBatchDirty = true;
	}
}



bool USelection::IsSelected(const UObject* InObject) const
{
	if ( InObject )
	{
		return SelectedObjects.Contains(InObject);
	}

	return false;
}


void USelection::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );
	Ar << SelectedObjects;
}

bool USelection::Modify(bool bAlwaysMarkDirty/* =true */)
{
	// If the selection currently contains any PIE objects we should not be including it in the transaction buffer
	for (auto ObjectPtr : SelectedObjects)
	{
		UObject* Object = ObjectPtr.Get();
		if (Object && (Object->GetOutermost()->PackageFlags & (PKG_PlayInEditor | PKG_ContainsScript | PKG_CompiledIn)) != 0)
		{
			return false;
		}
	}

	return Super::Modify(bAlwaysMarkDirty);
}