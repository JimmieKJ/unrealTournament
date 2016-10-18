// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "UMGPrivatePCH.h"
#include "WidgetMaterialTrackUtilities.h"

const FString SlateBrushStructName = "SlateBrush";

FSlateBrush* GetPropertyValueByPath( void* DataObject, UStruct* PropertySource, const TArray<FName>& PropertyPath, int32 PathIndex )
{
	if ( DataObject != nullptr && PathIndex < PropertyPath.Num() )
	{
		for ( TFieldIterator<UProperty> PropertyIterator( PropertySource ); PropertyIterator; ++PropertyIterator )
		{
			UProperty* Property = *PropertyIterator;
			if ( Property != nullptr && Property->GetFName() == PropertyPath[PathIndex] )
			{
				// Only struct properties are relevant for the search.
				UStructProperty* StructProperty = Cast<UStructProperty>( Property );
				if ( StructProperty == nullptr )
				{
					return nullptr;
				}

				if ( PathIndex == PropertyPath.Num() - 1 )
				{
					if ( StructProperty->Struct->GetName() == SlateBrushStructName )
					{
						return Property->ContainerPtrToValuePtr<FSlateBrush>( DataObject );
					}
					else
					{
						return nullptr;
					}
				}
				else
				{
					return GetPropertyValueByPath( Property->ContainerPtrToValuePtr<void>( DataObject ), StructProperty->Struct, PropertyPath, PathIndex + 1 );
				}
			}
		}
	}
	return nullptr;
}


FSlateBrush* WidgetMaterialTrackUtilities::GetBrush( UWidget* Widget, const TArray<FName>& BrushPropertyNamePath )
{
	return GetPropertyValueByPath( Widget, Widget->GetClass(), BrushPropertyNamePath, 0 );
}


void GetMaterialBrushPropertyPathsRecursive(void* DataObject, UStruct* PropertySource, TArray<UProperty*>& PropertyPath, TArray<TArray<UProperty*>>& MaterialBrushPropertyPaths )
{
	if ( DataObject != nullptr )
	{
		for ( TFieldIterator<UProperty> PropertyIterator( PropertySource ); PropertyIterator; ++PropertyIterator )
		{
			UProperty* Property = *PropertyIterator;
			if ( Property != nullptr && Property->HasAnyPropertyFlags( CPF_Deprecated ) == false )
			{
				PropertyPath.Add( Property );

				UStructProperty* StructProperty = Cast<UStructProperty>( Property );
				if ( StructProperty != nullptr )
				{
					if ( StructProperty->Struct->GetName() == SlateBrushStructName )
					{
						FSlateBrush* SlateBrush = Property->ContainerPtrToValuePtr<FSlateBrush>( DataObject );
						UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>( SlateBrush->GetResourceObject() );
						if ( MaterialInterface != nullptr )
						{
							MaterialBrushPropertyPaths.Add( PropertyPath );
						}
					}
					else
					{
						GetMaterialBrushPropertyPathsRecursive( StructProperty->ContainerPtrToValuePtr<void>( DataObject ), StructProperty->Struct, PropertyPath, MaterialBrushPropertyPaths );
					}
				}

				PropertyPath.RemoveAt( PropertyPath.Num() - 1 );
			}
		}
	}
}


void WidgetMaterialTrackUtilities::GetMaterialBrushPropertyPaths( UWidget* Widget, TArray<TArray<UProperty*>>& MaterialBrushPropertyPaths )
{
	TArray<UProperty*> PropertyPath;
	GetMaterialBrushPropertyPathsRecursive( Widget, Widget->GetClass(), PropertyPath, MaterialBrushPropertyPaths );
}

FName WidgetMaterialTrackUtilities::GetTrackNameFromPropertyNamePath( const TArray<FName>& PropertyNamePath )
{
	if ( PropertyNamePath.Num() == 0 )
	{
		return FName();
	}

	FString TrackName = PropertyNamePath[0].ToString();
	for ( int32 i = 1; i < PropertyNamePath.Num(); i++ )
	{
		TrackName.AppendChar( '.' );
		TrackName.Append( PropertyNamePath[i].ToString() );
	}

	return FName( *TrackName );
}