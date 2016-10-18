// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "PropertyTableColumn.h"
#include "IPropertyTableRow.h"
#include "IPropertyTable.h"
#include "PropertyTableCell.h"
#include "DataSource.h"
#include "PropertyPath.h"
#include "PropertyEditorHelpers.h"

#define LOCTEXT_NAMESPACE "PropertyTableColumn"

template< typename UPropertyType >
struct FCompareUObjectByPropertyAscending
{
public:

	FCompareUObjectByPropertyAscending( const TWeakObjectPtr< UPropertyType >& InUProperty )
		: Property( InUProperty )
	{}

	FCompareUObjectByPropertyAscending( const UPropertyType* const InUProperty )
		: Property( InUProperty )
	{}

	FCompareUObjectByPropertyAscending( const TWeakObjectPtr< UProperty >& InUProperty )
		: Property( Cast< UPropertyType >( InUProperty.Get() ) )
	{}

	FCompareUObjectByPropertyAscending( const UProperty* const InUProperty )
		: Property( Cast< UPropertyType >( InUProperty ) )
	{}

	FORCEINLINE bool operator()( const TWeakObjectPtr< UObject >& Lhs, const TWeakObjectPtr< UObject >& Rhs ) const
	{
		const bool LhsObjectValid = Lhs.IsValid();
		if ( !LhsObjectValid )
		{
			return true;
		}

		const bool RhsObjectValid = Rhs.IsValid();
		if ( !RhsObjectValid )
		{
			return false;
		}

		const UStruct* const PropertyTargetType = Property->GetOwnerStruct();
		const UClass* const LhsClass = Lhs->GetClass();

		if ( !LhsClass->IsChildOf( PropertyTargetType ) )
		{
			return true;
		}

		const UClass* const RhsClass = Rhs->GetClass();

		if ( !RhsClass->IsChildOf( PropertyTargetType ) )
		{
			return false;
		}

		typename UPropertyType::TCppType LhsValue = Property->GetPropertyValue_InContainer(Lhs.Get());
		typename UPropertyType::TCppType RhsValue = Property->GetPropertyValue_InContainer(Rhs.Get());

		return ComparePropertyValue( LhsValue, RhsValue );
	}


private:

	FORCEINLINE bool ComparePropertyValue( const typename UPropertyType::TCppType& LhsValue, const typename UPropertyType::TCppType& RhsValue ) const
	{
		return LhsValue < RhsValue;
	}


private:

	const TWeakObjectPtr< UPropertyType > Property;
};

// UByteProperty objects may in fact represent Enums - so they need special handling for alphabetic Enum vs. numerical Byte sorting.
template<>
FORCEINLINE bool FCompareUObjectByPropertyAscending<UByteProperty>::ComparePropertyValue( const uint8& LhsValue, const uint8& RhsValue ) const
{
	// Bytes are trivially sorted numerically
	UEnum* PropertyEnum = Property->GetIntPropertyEnum();
	if( PropertyEnum == NULL )
	{
		return LhsValue < RhsValue;
	}
	else
	{
		// But Enums are sorted alphabetically based on the full enum entry name - must be sure that values are within Enum bounds!
		bool bLhsEnumValid = LhsValue < PropertyEnum->NumEnums();
		bool bRhsEnumValid = RhsValue < PropertyEnum->NumEnums();
		if(bLhsEnumValid && bRhsEnumValid)
		{
			FName LhsEnumName( PropertyEnum->GetEnum( LhsValue ) );
			FName RhsEnumName( PropertyEnum->GetEnum( RhsValue ) );
			return LhsEnumName.Compare( RhsEnumName ) < 0;
		}
		else if(bLhsEnumValid)
		{
			return true;
		}
		else if(bRhsEnumValid)
		{
			return false;
		}
		else
		{
			return LhsValue < RhsValue;
		}
	}
}

template<>
FORCEINLINE bool FCompareUObjectByPropertyAscending<UNameProperty>::ComparePropertyValue( const FName& LhsValue, const FName& RhsValue ) const
{
	return LhsValue.Compare( RhsValue ) < 0;
}

template<>
FORCEINLINE bool FCompareUObjectByPropertyAscending<UTextProperty>::ComparePropertyValue( const FText& LhsValue, const FText& RhsValue ) const
{
	return LhsValue.CompareTo( RhsValue ) < 0;
}

template< typename UPropertyType >
struct FCompareUObjectByPropertyDescending
{
public:

	FCompareUObjectByPropertyDescending( const TWeakObjectPtr< UPropertyType >& InUProperty )
		: Compare( InUProperty )
	{}

	FCompareUObjectByPropertyDescending( const UPropertyType* const InUProperty )
		: Compare( InUProperty )
	{}

	FCompareUObjectByPropertyDescending( const TWeakObjectPtr< UProperty >& InUProperty )
		: Compare( InUProperty )
	{}

	FCompareUObjectByPropertyDescending( const UProperty* const InUProperty )
		: Compare( InUProperty )
	{}

	FORCEINLINE bool operator()( const TWeakObjectPtr< UObject >& Lhs, const TWeakObjectPtr< UObject >& Rhs ) const
	{
		return !Compare(Lhs, Rhs);
	}

private:

	const FCompareUObjectByPropertyAscending< UPropertyType > Compare;
};

template< typename UPropertyType >
struct FCompareRowByColumnAscending
{
public:
	FCompareRowByColumnAscending( const TSharedRef< IPropertyTableColumn >& InColumn, const TWeakObjectPtr< UPropertyType >& InUProperty )
		: Property( InUProperty )
		, Column( InColumn )
	{

	}

	FORCEINLINE bool operator()( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
	{
		const TSharedRef< IPropertyTableCell > LhsCell = Column->GetCell( Lhs );
		const TSharedRef< IPropertyTableCell > RhsCell = Column->GetCell( Rhs );

		const TSharedPtr< FPropertyNode > LhsPropertyNode = LhsCell->GetNode();
		if ( !LhsPropertyNode.IsValid() )
		{
			return true;
		}

		const TSharedPtr< FPropertyNode > RhsPropertyNode = RhsCell->GetNode();
		if ( !RhsPropertyNode.IsValid() )
		{
			return false;
		}

		const TSharedPtr< IPropertyHandle > LhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( LhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !LhsPropertyHandle.IsValid() )
		{
			return true;
		}

		const TSharedPtr< IPropertyHandle > RhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( RhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !RhsPropertyHandle.IsValid() )
		{
			return false;
		}

		return ComparePropertyValue( LhsPropertyHandle, RhsPropertyHandle );
	}

private:

	FORCEINLINE bool ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
	{
		typename UPropertyType::TCppType LhsValue; 
		LhsPropertyHandle->GetValue( LhsValue );

		typename UPropertyType::TCppType RhsValue; 
		RhsPropertyHandle->GetValue( RhsValue );

		return LhsValue < RhsValue;
	}

private:

	const TWeakObjectPtr< UPropertyType > Property;
	TSharedRef< IPropertyTableColumn > Column;
};

template< typename UPropertyType >
struct FCompareRowByColumnDescending
{
public:
	FCompareRowByColumnDescending( const TSharedRef< IPropertyTableColumn >& InColumn, const TWeakObjectPtr< UPropertyType >& InUProperty )
		: Comparer( InColumn, InUProperty )
	{

	}

	FORCEINLINE bool operator()( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
	{
		return !Comparer( Lhs, Rhs );
	}


private:

	const FCompareRowByColumnAscending< UPropertyType > Comparer;
};


// UByteProperty objects may in fact represent Enums - so they need special handling for alphabetic Enum vs. numerical Byte sorting.
template<>
FORCEINLINE bool FCompareRowByColumnAscending<UByteProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	// Get the basic uint8 values
	uint8 LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	uint8 RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	// Bytes are trivially sorted numerically
	UEnum* PropertyEnum = Property->GetIntPropertyEnum();
	if( PropertyEnum == NULL )
	{
		return LhsValue < RhsValue;
	}
	else
	{
		// But Enums are sorted alphabetically based on the full enum entry name - must be sure that values are within Enum bounds!
		bool bLhsEnumValid = LhsValue < PropertyEnum->NumEnums();
		bool bRhsEnumValid = RhsValue < PropertyEnum->NumEnums();
		if(bLhsEnumValid && bRhsEnumValid)
		{
			FName LhsEnumName( PropertyEnum->GetEnum( LhsValue ) );
			FName RhsEnumName( PropertyEnum->GetEnum( RhsValue ) );
			return LhsEnumName.Compare( RhsEnumName ) < 0;
		}
		else if(bLhsEnumValid)
		{
			return true;
		}
		else if(bRhsEnumValid)
		{
			return false;
		}
		else
		{
			return LhsValue < RhsValue;
		}
	}
}

template<>
FORCEINLINE bool FCompareRowByColumnAscending<UNameProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	FName LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	FName RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	return LhsValue.Compare( RhsValue ) < 0;
}

template<>
FORCEINLINE bool FCompareRowByColumnAscending<UObjectProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	UObject* LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	if ( LhsValue == NULL )
	{
		return true;
	}

	UObject* RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	if ( RhsValue == NULL )
	{
		return false;
	}

	return LhsValue->GetFullName() < RhsValue->GetFullName();
}


FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject )
	: Cells()
	, DataSource( MakeShareable( new UObjectDataSource( InObject.Get() ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( FPropertyPath::CreateEmpty() )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
	GenerateColumnDisplayName();
}

FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TSharedRef< FPropertyPath >& InPropertyPath )
	: Cells()
	, DataSource( MakeShareable( new PropertyPathDataSource( InPropertyPath ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( FPropertyPath::CreateEmpty() )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
	GenerateColumnDisplayName();
}

FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< class IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject, const TSharedRef< FPropertyPath >& InPartialPropertyPath )
	: Cells()
	, DataSource( MakeShareable( new UObjectDataSource( InObject.Get() ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( InPartialPropertyPath )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
}


void FPropertyTableColumn::GenerateColumnId()
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	TSharedPtr< FPropertyPath > PropertyPath = DataSource->AsPropertyPath();

	// Use partial path for a valid column ID if we have one. We are pointing to a container with an array, but all columns must be unique
	if ( PartialPath->GetNumProperties() > 0 )
	{
		Id = FName( *PartialPath->ToString());
	}
	else if ( Object.IsValid() )
	{
		Id = Object->GetFName();
	}
	else if ( PropertyPath.IsValid() )
	{
		Id = FName( *PropertyPath->ToString() );
	}
	else
	{
		Id = NAME_None;
	}
}

void FPropertyTableColumn::GenerateColumnDisplayName()
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	TSharedPtr< FPropertyPath > PropertyPath = DataSource->AsPropertyPath();

	if ( Object.IsValid() )
	{
		if ( Object->IsA( UProperty::StaticClass() ) )
		{
			const UProperty* Property = Cast< UProperty >( Object.Get() );
			DisplayName = FText::FromString(UEditorEngine::GetFriendlyName(Property)); 
		}
		else
		{
			DisplayName = FText::FromString(Object->GetFName().ToString());
		}
	}
	else if ( PropertyPath.IsValid() )
	{
		//@todo unify this logic with all the property editors [12/11/2012 Justin.Sargent]
		FString NewName;
		bool FirstAddition = true;
		const FPropertyInfo* PreviousPropInfo = NULL;
		for (int PropertyIndex = 0; PropertyIndex < PropertyPath->GetNumProperties(); PropertyIndex++)
		{
			const FPropertyInfo& PropInfo = PropertyPath->GetPropertyInfo( PropertyIndex );

			if ( !(PropInfo.Property->IsA( UArrayProperty::StaticClass() ) && PropertyIndex != PropertyPath->GetNumProperties() - 1 ) )
			{
				if ( !FirstAddition )
				{
					NewName += TEXT( "->" );
				}

				FString PropertyName = PropInfo.Property->GetDisplayNameText().ToString();

				if ( PropertyName.IsEmpty() )
				{
					PropertyName = PropInfo.Property->GetName();

					const bool bIsBoolProperty = Cast<const UBoolProperty>( PropInfo.Property.Get() ) != NULL;

					if ( PreviousPropInfo != NULL )
					{
						const UStructProperty* ParentStructProperty = Cast<const UStructProperty>( PreviousPropInfo->Property.Get() );
						if( ParentStructProperty && ParentStructProperty->Struct->GetFName() == NAME_Rotator )
						{
							if( PropInfo.Property->GetFName() == "Roll" )
							{
								PropertyName = TEXT("X");
							}
							else if( PropInfo.Property->GetFName() == "Pitch" )
							{
								PropertyName = TEXT("Y");
							}
							else if( PropInfo.Property->GetFName() == "Yaw" )
							{
								PropertyName = TEXT("Z");
							}
							else
							{
								check(0);
							}
						}
					}

					PropertyName = FName::NameToDisplayString( PropertyName, bIsBoolProperty );
				}

				NewName += PropertyName;

				if ( PropInfo.ArrayIndex != INDEX_NONE )
				{
					NewName += FString::Printf( TEXT( "[%d]" ), PropInfo.ArrayIndex );
				}

				PreviousPropInfo = &PropInfo;
				FirstAddition = false;
			}
		}

		DisplayName = FText::FromString(*NewName);
	}
	else
	{
		DisplayName = LOCTEXT( "InvalidColumnName", "Invalid Property" );
	}
}

FName FPropertyTableColumn::GetId() const 
{ 
	return Id;
}

FText FPropertyTableColumn::GetDisplayName() const 
{ 
	return DisplayName;
}

TSharedRef< IPropertyTableCell > FPropertyTableColumn::GetCell( const TSharedRef< class IPropertyTableRow >& Row ) 
{
	//@todo Clean Cells cache when rows get updated [11/27/2012 Justin.Sargent]
	TSharedRef< IPropertyTableCell >* CellPtr = Cells.Find( Row );

	if( CellPtr != NULL )
	{
		return *CellPtr;
	}

	TSharedRef< IPropertyTableCell > Cell = MakeShareable( new FPropertyTableCell( SharedThis( this ), Row ) );
	Cells.Add( Row, Cell );

	return Cell;
}

void FPropertyTableColumn::RemoveCellsForRow( const TSharedRef< class IPropertyTableRow >& Row )
{
	Cells.Remove( Row );
}

TSharedRef< class IPropertyTable > FPropertyTableColumn::GetTable() const
{
	return Table.Pin().ToSharedRef();
}

bool FPropertyTableColumn::CanSortBy() const
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	UProperty* Property = Cast< UProperty >( Object.Get() );

	TSharedPtr< FPropertyPath > Path = DataSource->AsPropertyPath();
	if ( Property == NULL && Path.IsValid() )
	{
		Property = Path->GetLeafMostProperty().Property.Get();
	}

	if ( Property != NULL )
	{
		return Property->IsA( UByteProperty::StaticClass() )  ||
			Property->IsA( UIntProperty::StaticClass() )   ||
			Property->IsA( UBoolProperty::StaticClass() )  ||
			Property->IsA( UFloatProperty::StaticClass() ) ||
			Property->IsA( UNameProperty::StaticClass() )  ||
			Property->IsA( UStrProperty::StaticClass() )   ||
			( Property->IsA( UObjectProperty::StaticClass() ) && !Property->HasAnyPropertyFlags(CPF_InstancedReference) );
			//Property->IsA( UTextProperty::StaticClass() );
	}

	return false;
}

void FPropertyTableColumn::Sort( TArray< TSharedRef< class IPropertyTableRow > >& Rows, const EColumnSortMode::Type SortMode )
{
	if ( SortMode == EColumnSortMode::None )
	{
		return;
	}

	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	UProperty* Property = Cast< UProperty >( Object.Get() );

	TSharedPtr< FPropertyPath > Path = DataSource->AsPropertyPath();
	if ( Property == NULL && Path.IsValid() )
	{
		Property = Path->GetLeafMostProperty().Property.Get();
	}

	if ( Property->IsA( UByteProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UByteProperty> ByteProperty( Cast< UByteProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UByteProperty >( SharedThis( this ), ByteProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UByteProperty >( SharedThis( this ), ByteProperty ) );
		}
	}
	else if ( Property->IsA( UIntProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UIntProperty> IntProperty( Cast< UIntProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UIntProperty >( SharedThis( this ), IntProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UIntProperty >( SharedThis( this ), IntProperty ) );
		}
	}
	else if ( Property->IsA( UBoolProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UBoolProperty> BoolProperty( Cast< UBoolProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UBoolProperty >( SharedThis( this ), BoolProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UBoolProperty >( SharedThis( this ), BoolProperty ) );
		}
	}
	else if ( Property->IsA( UFloatProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UFloatProperty> FloatProperty( Cast< UFloatProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UFloatProperty >( SharedThis( this ), FloatProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UFloatProperty >( SharedThis( this ), FloatProperty ) );
		}
	}
	else if ( Property->IsA( UNameProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UNameProperty> NameProperty( Cast< UNameProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UNameProperty >( SharedThis( this ), NameProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UNameProperty >( SharedThis( this ), NameProperty ) );
		}
	}
	else if ( Property->IsA( UStrProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UStrProperty> StrProperty( Cast< UStrProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UStrProperty >( SharedThis( this ), StrProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UStrProperty >( SharedThis( this ), StrProperty ) );
		}
	}
	else if ( Property->IsA( UObjectProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UObjectProperty> ObjectProperty( Cast< UObjectProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UObjectProperty >( SharedThis( this ), ObjectProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UObjectProperty >( SharedThis( this ), ObjectProperty ) );
		}
	}
	//else if ( Property->IsA( UTextProperty::StaticClass() ) )
	//{
	//	if ( SortMode == EColumnSortMode::Ascending )
	//	{
	//		Rows.Sort( FCompareRowByColumnAscending< UTextProperty >( SharedThis( this ) ) );
	//	}
	//	else
	//	{
	//		Rows.Sort( FCompareRowByColumnDescending< UTextProperty >( SharedThis( this ) ) );
	//	}
	//}
	else
	{
		check( false && "Cannot Sort Rows by this Column!" );
	}
}

void FPropertyTableColumn::Tick()
{
	if ( !DataSource->AsPropertyPath().IsValid() )
	{
		const TSharedRef< IPropertyTable > TableRef = GetTable();
		const TWeakObjectPtr< UObject > Object = DataSource->AsUObject();

		if ( !Object.IsValid() )
		{
			TableRef->RemoveColumn( SharedThis( this ) );
		}
		else
		{
			const TSharedRef< FObjectPropertyNode > Node = TableRef->GetObjectPropertyNode( Object );
			EPropertyDataValidationResult Result = Node->EnsureDataIsValid();

			if ( Result == EPropertyDataValidationResult::ObjectInvalid )
			{
				TableRef->RemoveColumn( SharedThis( this ) );
			}
			else if ( Result == EPropertyDataValidationResult::ArraySizeChanged )
			{
				TableRef->RequestRefresh();
			}
		}
	}
}

void FPropertyTableColumn::SetFrozen(bool InIsFrozen)
{
	bIsFrozen = InIsFrozen;
	FrozenStateChanged.Broadcast( SharedThis(this) );
}

#undef LOCTEXT_NAMESPACE
