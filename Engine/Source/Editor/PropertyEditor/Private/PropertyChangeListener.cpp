// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "PropertyEditorPrivatePCH.h"
#include "PropertyChangeListener.h"
#include "ObjectPropertyNode.h"
#include "PropertyEditorHelpers.h"

void FPropertyChangeListener::SetObject( UObject& Object, const FPropertyListenerSettings& InPropertyListenerSettings  )
{
	PropertyListenerSettings = InPropertyListenerSettings;

	bool bInitNode = false;
	if( !RootPropertyNode.IsValid() )
	{
		RootPropertyNode = MakeShareable( new FObjectPropertyNode );

	}

	RootPropertyNode->AddObject( &Object );

	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = NULL;
	InitParams.Property = NULL;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	InitParams.bAllowChildren = true;
	InitParams.bForceHiddenPropertyVisibility = false;
	InitParams.bCreateCategoryNodes = false;

	RootPropertyNode->InitNode( InitParams );

	TSharedRef<FPropertyNode> Start = RootPropertyNode.ToSharedRef();
	CreatePropertyCaches( Start, &Object );
}

/**
 * Caches a single property value
 */
class FValueCache
{
public:
	/**
	 * A union which allows a single address to be represented as a pointer to a uint8
	 * or a pointer to a UObject.
	 */
	union FPropertyValueRoot
	{
		UObject*	OwnerObject;
		uint8*		ValueAddress;
	};

	FValueCache( TSharedRef<FPropertyNode> InPropertyNode, UObject* InOwnerObject )
		: PropertyNode( InPropertyNode )
	{
		PropertyValueRoot.OwnerObject = InOwnerObject;

		UProperty* Property = InPropertyNode->GetProperty();

		check(Property);
		check(PropertyValueRoot.OwnerObject);

		FPropertyNode* ParentNode		= InPropertyNode->GetParentNode();


		//UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
		UArrayProperty* OuterArrayProp = Cast<UArrayProperty>(Property->GetOuter());

		// calculate the values for the current object
		
		PropertyValueBaseAddress = OuterArrayProp == NULL
			? InPropertyNode->GetValueBaseAddress(PropertyValueRoot.ValueAddress)
			: ParentNode->GetValueBaseAddress(PropertyValueRoot.ValueAddress);

		PropertyValueAddress = InPropertyNode->GetValueAddress(PropertyValueRoot.ValueAddress);
	}

	
	/**
	 * @return The property node we are inspecting
	 */
	TSharedPtr<FPropertyNode> GetPropertyNode() const { return PropertyNode.Pin(); }

	/**
	 * @return The address of the property's value.
	 */
	uint8* GetPropertyValueAddress() const { return PropertyValueAddress; }

	/**
	 * @return The address of the owning object's archetype
	 */
	FPropertyValueRoot GetPropertyValueRoot() const { return PropertyValueRoot; }

	/**
	 * Caches the property value
	 */
	void CacheValue()
	{
		Data.Reset();

		FPropertyNode& PropertyNodeRef = *PropertyNode.Pin();
		UProperty* Property = PropertyNodeRef.GetProperty();
		{
			// Not supported yet
			check( !Property->IsA( UArrayProperty::StaticClass() ) )

			if( PropertyNodeRef.GetArrayIndex() == INDEX_NONE && Property->ArrayDim > 1 )
			{
				// Check static arrays
				Data.AddZeroed( Property->ArrayDim * Property->ElementSize );
				Property->CopyCompleteValue( Data.GetData(), PropertyValueAddress);
			}
			else
			{
				// Regular properties
				Data.AddZeroed( Property->ElementSize );
				Property->CopySingleValue( Data.GetData(), PropertyValueAddress );
			}
		}
	}

	/**
	 * Scans for changes to the value
	 *
	 * @param bRecacheNewValues	If true, recaches new values found
	 * @return true if any changes were found
	 */
	bool ScanForChanges( bool bRecacheNewValues )
	{
		FPropertyNode& PropertyNodeRef = *PropertyNode.Pin();
		UProperty* Property = PropertyNodeRef.GetProperty();

		bool bPropertyValid = true;
		bool bChanged = false;

		UArrayProperty* OuterArrayProperty = Cast<UArrayProperty>( Property->GetOuter() );
		if ( OuterArrayProperty != NULL )
		{
			// make sure we're not trying to compare against an element that doesn't exist
			if ( PropertyNodeRef.GetArrayIndex() >= FScriptArrayHelper::Num( PropertyValueBaseAddress ) )
			{
				bPropertyValid = false;
			}
		}

		if( bPropertyValid )
		{
			bChanged = !Property->Identical( PropertyValueAddress, Data.GetData() );
			if( bRecacheNewValues )
			{
				CacheValue();
			}
		}

		return bChanged;
	}

private:
	/** Cached value data */
	TArray<uint8> Data;

	/** The property node we are inspecting */
	TWeakPtr<FPropertyNode> PropertyNode;

	/** The top level object owning the value (e.g if the value is from a property on a component, the top level object is most likely the actor owner of the component) */
	TWeakObjectPtr<UObject> CachedTopLevelObject;

	/** The address of the owning object */
	FPropertyValueRoot PropertyValueRoot;

	/**
	 * The address of this property's value.
	 */
	uint8* PropertyValueAddress;

	/**
	 * The base address of this property's value.  i.e. for dynamic arrays, the location of the FScriptArray which
	 * contains the array property's value
	 */
	uint8* PropertyValueBaseAddress;
};


bool FPropertyChangeListener::ScanForChanges( bool bRecacheNewValues )
{
	bool bChangesFound = false;

	// Check each value to see if it has been changed
	for( int32 ValueIndex = 0; ValueIndex < CachedValues.Num(); ++ValueIndex )
	{
		FValueCache& ValueCache = *CachedValues[ValueIndex];
		
		bool bChanged = ValueCache.ScanForChanges( bRecacheNewValues );
		if( bChanged )
		{
			// The value has changed.  Call a delegate to notify users
			UE_LOG( LogPropertyNode, Verbose, TEXT("Property changed: %s"), *ValueCache.GetPropertyNode()->GetProperty()->GetName() );
			TSharedPtr<FPropertyNode> PropertyNode = ValueCache.GetPropertyNode();

			// Find the root most object parent as that contains the main object(s) being changed
			FObjectPropertyNode* ObjectNode = PropertyNode->FindRootObjectItemParent();

			// Get each object that changed
			TArray<UObject*> ObjectsThatChanged;
			if (ObjectNode)
			{
				for (auto It = ObjectNode->ObjectConstIterator(); It; ++It)
				{
					ObjectsThatChanged.Add(It->Get());
				}
			}

			if( ObjectsThatChanged.Num() > 0 )
			{
				bChangesFound = true;
				TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle( PropertyNode.ToSharedRef(), NULL, NULL );
				OnPropertyChangedDelegate.Broadcast( ObjectsThatChanged, *Handle );
			}
		}
		
	}

	return bChangesFound;
}

void FPropertyChangeListener::TriggerAllPropertiesChangedDelegate()
{
	// Check each value to see if it has been changed
	for( int32 ValueIndex = 0; ValueIndex < CachedValues.Num(); ++ValueIndex )
	{
		FValueCache& ValueCache = *CachedValues[ValueIndex];
		
		TSharedPtr<FPropertyNode> PropertyNode = ValueCache.GetPropertyNode();

		// Find the root most object parent as that contains the main object(s) being changed
		FObjectPropertyNode* ObjectNode = PropertyNode->FindRootObjectItemParent();

		// Get each object that changed
		TArray<UObject*> ObjectsThatChanged;
		if (ObjectNode)
		{
			for (auto It = ObjectNode->ObjectConstIterator(); It; ++It)
			{
				ObjectsThatChanged.Add(It->Get());
			}
		}

		if( ObjectsThatChanged.Num() > 0 )
		{
			TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle( PropertyNode.ToSharedRef(), NULL, NULL );
			OnPropertyChangedDelegate.Broadcast( ObjectsThatChanged, *Handle );
		}
	}
}

void FPropertyChangeListener::CreatePropertyCaches( TSharedRef<FPropertyNode>& PropertyNode, UObject* ParentObject )
{
	FObjectPropertyNode* ObjectNode = PropertyNode->AsObjectNode();
	UProperty* Property = PropertyNode->GetProperty();

	bool bIsBuiltInStructProp = PropertyEditorHelpers::IsBuiltInStructProperty( Property );

	if( PropertyNode->AsItemPropertyNode() && Property )
	{
		// Check whether or not we should ignore object properties
		bool bValidProperty = ( !PropertyListenerSettings.bIgnoreObjectProperties || !Property->IsA( UObjectPropertyBase::StaticClass() ) );
		// Check whether or not we should ignore array properties
		bValidProperty &= ( !PropertyListenerSettings.bIgnoreArrayProperties || !Property->IsA(UArrayProperty::StaticClass() ) );
		// Check whether or not the required property flags are set
		bValidProperty &= ( PropertyListenerSettings.RequiredPropertyFlags == 0 || Property->HasAllPropertyFlags( PropertyListenerSettings.RequiredPropertyFlags ) );
		// Check to make sure the disallowed property flags are not set
		bValidProperty &= (PropertyListenerSettings.DisallowedPropertyFlags == 0 || !Property->HasAnyPropertyFlags( PropertyListenerSettings.DisallowedPropertyFlags ) );
		// Only examine struct properties if they are built in (they are treated as whole units).  Otherwise just examine the children
		bValidProperty &= ( bIsBuiltInStructProp || !Property->IsA( UStructProperty::StaticClass() ) );

		if( bValidProperty )
		{
			TSharedPtr<FValueCache> ValueCache( new FValueCache( PropertyNode, ParentObject ) );
			ValueCache->CacheValue();
			CachedValues.Add( ValueCache );
		}
	}

	// Built in struct types (Vector, rotator, etc) are stored as a single value 
	if( !bIsBuiltInStructProp )
	{
		// Only one object supported
		UObject* NewParent = ObjectNode ? ObjectNode->GetUObject(0) : ParentObject;

		// Cache each child value
		for( int32 ChildIndex = 0; ChildIndex < PropertyNode->GetNumChildNodes(); ++ChildIndex )
		{
			TSharedRef<FPropertyNode> ChildNode = PropertyNode->GetChildNode( ChildIndex ).ToSharedRef();
			CreatePropertyCaches( ChildNode, NewParent );
		}
	}
}
