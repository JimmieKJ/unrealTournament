// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Internationalization/InternationalizationMetadata.h"

DEFINE_LOG_CATEGORY_STATIC(LogInternationalizationMetadata, Log, All);

const FString FLocMetadataObject::COMPARISON_MODIFIER_PREFIX = TEXT("*");

void FLocMetadataValue::ErrorMessage( const FString& InType )
{
	UE_LOG(LogInternationalizationMetadata, Fatal, TEXT("LocMetadata Value of type '%s' used as a '%s'."), *GetType(), *InType);
}

FLocMetadataObject::FLocMetadataObject( const FLocMetadataObject& Other )
{
	for( auto KeyIter = Other.Values.CreateConstIterator(); KeyIter; ++KeyIter )
	{
		const FString& Key = (*KeyIter).Key;
		const TSharedPtr< FLocMetadataValue > Value = (*KeyIter).Value;

		if( Value.IsValid() )
		{
			this->Values.Add( Key, Value->Clone() );
		}
	}
}

void FLocMetadataObject::SetField( const FString& FieldName, const TSharedPtr<FLocMetadataValue>& Value )
{
	this->Values.Add( FieldName, Value );
}

void FLocMetadataObject::RemoveField(const FString& FieldName)
{
	this->Values.Remove(FieldName);
}

FString FLocMetadataObject::GetStringField(const FString& FieldName)
{
	return GetField<ELocMetadataType::String>(FieldName)->AsString();
}

void FLocMetadataObject::SetStringField( const FString& FieldName, const FString& StringValue )
{
	this->Values.Add( FieldName, MakeShareable(new FLocMetadataValueString(StringValue)) );
}

bool FLocMetadataObject::GetBoolField(const FString& FieldName)
{
	return GetField<ELocMetadataType::Boolean>(FieldName)->AsBool();
}

void FLocMetadataObject::SetBoolField( const FString& FieldName, bool InValue )
{
	this->Values.Add( FieldName, MakeShareable( new FLocMetadataValueBoolean(InValue) ) );
}

TArray< TSharedPtr<FLocMetadataValue> > FLocMetadataObject::GetArrayField(const FString& FieldName)
{
	return GetField<ELocMetadataType::Array>(FieldName)->AsArray();
}

void FLocMetadataObject::SetArrayField( const FString& FieldName, const TArray< TSharedPtr<FLocMetadataValue> >& Array )
{
	this->Values.Add( FieldName, MakeShareable( new FLocMetadataValueArray(Array) ) );
}

TSharedPtr<FLocMetadataObject> FLocMetadataObject::GetObjectField(const FString& FieldName)
{
	return GetField<ELocMetadataType::Object>(FieldName)->AsObject();
}

void FLocMetadataObject::SetObjectField( const FString& FieldName, const TSharedPtr<FLocMetadataObject>& LocMetadataObject )
{
	if (LocMetadataObject.IsValid())
	{
		this->Values.Add( FieldName, MakeShareable(new FLocMetadataValueObject(LocMetadataObject.ToSharedRef())) );
	}
}

FLocMetadataObject& FLocMetadataObject::operator=( const FLocMetadataObject& Other )
{
	if( this != &Other )
	{
		this->Values.Empty( Other.Values.Num() );
		for( auto KeyIter = Other.Values.CreateConstIterator(); KeyIter; ++KeyIter )
		{
			const FString& Key = (*KeyIter).Key;
			const TSharedPtr< FLocMetadataValue > Value = (*KeyIter).Value;

			if( Value.IsValid() )
			{
				this->Values.Add( Key, Value->Clone() );
			}
		}
	}
	return *this;
}

bool FLocMetadataObject::operator==( const FLocMetadataObject& Other ) const
{
	if( Values.Num() != Other.Values.Num() )
	{
		return false;
	}

	for( auto KeyIter = Values.CreateConstIterator(); KeyIter; ++KeyIter )
	{
		const FString& Key = (*KeyIter).Key;
		const TSharedPtr< FLocMetadataValue > Value = (*KeyIter).Value;

		const TSharedPtr< FLocMetadataValue >* OtherValue = Other.Values.Find( Key );

		if( OtherValue && (*OtherValue).IsValid() )
		{
			// In the case where the name starts with the comparison modifier, we ignore the type and value.  Note, the contents of an array or object
			//  with this comparison modifier will not be checked even if those contents do not have the modifier.
			if( Key.StartsWith( COMPARISON_MODIFIER_PREFIX ) )
			{
				continue;
			}
			else if( Value->Type == (*OtherValue)->Type && *(Value) == *(*OtherValue) )
			{
				continue;
			}
			else
			{
				return false;
			}
		}
	}
	return true;
}


bool FLocMetadataObject::IsExactMatch( const FLocMetadataObject& Other ) const
{
	if( Values.Num() != Other.Values.Num() )
	{
		return false;
	}

	for( auto KeyIter = Values.CreateConstIterator(); KeyIter; ++KeyIter )
	{
		const FString& Key = (*KeyIter).Key;
		const TSharedPtr< FLocMetadataValue > Value = (*KeyIter).Value;

		const TSharedPtr< FLocMetadataValue >* OtherValue = Other.Values.Find( Key );

		if( OtherValue && (*OtherValue).IsValid() )
		{
			if( Value->Type == (*OtherValue)->Type && *(Value) == *(*OtherValue) )
			{
				continue;
			}
			else
			{
				return false;
			}
		}
	}
	return true;
}

bool FLocMetadataObject::operator<( const FLocMetadataObject& Other ) const
{
	TArray< FString > MetaKeys;
	Values.GetKeys( MetaKeys );

	TArray< FString > OtherMetaKeys;
	Other.Values.GetKeys( OtherMetaKeys );

	MetaKeys.Sort( TLess<FString>() );
	OtherMetaKeys.Sort( TLess<FString>() );

	for( int32 MetaKeyIdx = 0; MetaKeyIdx < MetaKeys.Num(); ++MetaKeyIdx )
	{
		if( MetaKeyIdx >= OtherMetaKeys.Num() )
		{
			// We are not less than the other we are comparing list because it ran out of metadata keys while we still have some
			return false;
		}
		else if ( MetaKeys[ MetaKeyIdx ] != OtherMetaKeys[ MetaKeyIdx ] )
		{
			return MetaKeys[ MetaKeyIdx ] < OtherMetaKeys[ MetaKeyIdx ];
		}
	}

	if( OtherMetaKeys.Num() > MetaKeys.Num() )
	{
		// All the keys are the same up to this point but other has additional keys
		return true;
	}

	// If we get here, we know that all the keys are the same so now we start comparing values.
	for( int32 MetaKeyIdx = 0; MetaKeyIdx < MetaKeys.Num(); ++MetaKeyIdx )
	{
		const TSharedPtr< FLocMetadataValue >* Value = Values.Find( MetaKeys[ MetaKeyIdx ] );
		const TSharedPtr< FLocMetadataValue >* OtherValue = Other.Values.Find( MetaKeys[ MetaKeyIdx ] );

		if( !Value && !OtherValue )
		{
			continue;
		}
		else if( (Value == NULL) != (OtherValue == NULL) )
		{
			return (OtherValue == NULL);
		}

		if( !(*Value).IsValid() && !(*OtherValue).IsValid() )
		{
			continue;
		}
		else if( (*Value).IsValid() != (*OtherValue).IsValid() )
		{
			return (*OtherValue).IsValid();
		}
		else if(*(*Value) < *(*OtherValue))
		{
			 return true;
		}
		else if(!(*(*Value) == *(*OtherValue)) )
		{
			return false;
		}

	}
	return false;
}

bool FLocMetadataObject::IsMetadataExactMatch( TSharedPtr<FLocMetadataObject> MetadataA, TSharedPtr<FLocMetadataObject> MetadataB )
{
	if( !MetadataA.IsValid() && !MetadataB.IsValid() )
	{
		return true;
	}
	else if( MetadataA.IsValid() != MetadataB.IsValid() )
	{
		// If we are in here, we know that one of the metadata entries is null, if the other
		//  contains zero entries we will still consider them equivalent.
		if( (MetadataA.IsValid() && MetadataA->Values.Num() == 0) ||
			(MetadataB.IsValid() && MetadataB->Values.Num() == 0) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	// Note: Since the standard source comparison operator handles * prefixed meta data in a special way, we use an exact match
	//  check here instead.
	return MetadataA->IsExactMatch( *(MetadataB) );
}


bool FLocMetadataValueString::EqualTo( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueString* OtherObj = (FLocMetadataValueString*) &Other;
	return (Value == OtherObj->Value);
}

bool FLocMetadataValueString::LessThan( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueString* OtherObj = (FLocMetadataValueString*) &Other;
	return (Value < OtherObj->Value);
}

TSharedRef<FLocMetadataValue> FLocMetadataValueString::Clone() const
{
	return MakeShareable( new FLocMetadataValueString( Value ) );
}

bool FLocMetadataValueBoolean::EqualTo( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueBoolean* OtherObj = (FLocMetadataValueBoolean*) &Other;
	return (Value == OtherObj->Value);
}

bool FLocMetadataValueBoolean::LessThan( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueBoolean* OtherObj = (FLocMetadataValueBoolean*) &Other;
	return (Value != OtherObj->Value) ? (!Value) : false;
}

TSharedRef<FLocMetadataValue> FLocMetadataValueBoolean::Clone() const
{
	return MakeShareable( new FLocMetadataValueBoolean( Value ) );
}

struct FCompareMetadataValue
{
	FORCEINLINE bool operator()( TSharedPtr<FLocMetadataValue> A, TSharedPtr<FLocMetadataValue> B ) const
	{
		if( !A.IsValid() && !B.IsValid() )
		{
			return false;
		}
		else if( A.IsValid() != B.IsValid() )
		{
			return B.IsValid();
		}
		return *A < *B;
	}
};

bool FLocMetadataValueArray::EqualTo( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueArray* OtherObj = (FLocMetadataValueArray*) &Other; 

	if( Value.Num() != OtherObj->Value.Num() )
	{
		return false;
	}

	TArray< TSharedPtr<FLocMetadataValue> > Sorted = Value;
	TArray< TSharedPtr<FLocMetadataValue> > OtherSorted = OtherObj->Value;

	Sorted.Sort( FCompareMetadataValue() );
	OtherSorted.Sort( FCompareMetadataValue() );

	for( int32 idx = 0; idx < Sorted.Num(); ++idx )
	{
		if( !(*(Sorted[idx]) == *(OtherSorted[idx]) ) )
		{
			return false;
		}
	}

	return true;
}

bool FLocMetadataValueArray::LessThan( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueArray* OtherObj = (FLocMetadataValueArray*) &Other;

	TArray< TSharedPtr<FLocMetadataValue> > Sorted = Value;
	TArray< TSharedPtr<FLocMetadataValue> > OtherSorted = OtherObj->Value;

	Sorted.Sort( FCompareMetadataValue() );
	OtherSorted.Sort( FCompareMetadataValue() );

	for( int32 idx = 0; idx < Sorted.Num(); ++idx )
	{
		if( idx >= OtherSorted.Num() )
		{
			return false;
		}
		else if( !( *(Sorted[idx]) == *(OtherSorted[idx]) ) )
		{
			return *(Sorted[idx]) < *(OtherSorted[idx]);
		}
	}

	if( OtherSorted.Num() > Sorted.Num() )
	{
		return true;
	}

	return false;
}

TSharedRef<FLocMetadataValue> FLocMetadataValueArray::Clone() const
{
	TArray< TSharedPtr<FLocMetadataValue> > NewValue;
	for( auto Iter = Value.CreateConstIterator(); Iter; ++Iter )
	{
		NewValue.Add( (*Iter)->Clone() );
	}
	return MakeShareable( new FLocMetadataValueArray( NewValue ) );
}

bool FLocMetadataValueObject::EqualTo( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueObject* OtherObj = (FLocMetadataValueObject*) &Other;  
	if( !Value.IsValid() && !OtherObj->Value.IsValid() )
	{
		return true;
	}
	else if( Value.IsValid() != OtherObj->Value.IsValid() )
	{
		return false;
	}
	return *(Value) == *(OtherObj->Value);
}

bool FLocMetadataValueObject::LessThan( const FLocMetadataValue& Other ) const
{
	const FLocMetadataValueObject* OtherObj = (FLocMetadataValueObject*) &Other;
	if( !Value.IsValid() && !OtherObj->Value.IsValid() )
	{
		return false;
	}
	else if( Value.IsValid() != OtherObj->Value.IsValid() )
	{
		return OtherObj->Value.IsValid();
	}
	return *(Value) < *(OtherObj->Value);
}

TSharedRef<FLocMetadataValue> FLocMetadataValueObject::Clone() const
{
	TSharedRef<FLocMetadataObject> NewLocMetadataObject = MakeShareable( new FLocMetadataObject( *(this->Value) ) );
	return MakeShareable( new FLocMetadataValueObject( NewLocMetadataObject ) );
}
