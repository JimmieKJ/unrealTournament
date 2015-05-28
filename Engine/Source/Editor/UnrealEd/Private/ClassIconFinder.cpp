// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "SlateBasics.h"
#include "ClassIconFinder.h"
#include "AssetData.h"

TArray < const ISlateStyle* > FClassIconFinder::Styles;

void FClassIconFinder::RegisterIconSource(const ISlateStyle* StyleSet)
{
	Styles.AddUnique( StyleSet );
}

void FClassIconFinder::UnregisterIconSource(const ISlateStyle* StyleSet)
{
	Styles.Remove( StyleSet );
}

const FSlateBrush* FClassIconFinder::LookupBrush(FName IconName)
{
	const FSlateBrush* IconBrush = nullptr;

	for ( const ISlateStyle* Style : Styles )
	{
		IconBrush = Style->GetOptionalBrush(IconName, nullptr, nullptr);
		if ( IconBrush )
		{
			return IconBrush;
		}
	}

	return FEditorStyle::GetOptionalBrush(IconName, nullptr, nullptr);
}

const FSlateBrush* FClassIconFinder::FindIconForActors(const TArray< TWeakObjectPtr<AActor> >& InActors, UClass*& CommonBaseClass)
{
	// Get the common base class of the selected actors
	const FSlateBrush* CommonIcon = nullptr;
	for( int32 ActorIdx = 0; ActorIdx < InActors.Num(); ++ActorIdx )
	{
		TWeakObjectPtr<AActor> Actor = InActors[ActorIdx];
		UClass* ObjClass = Actor->GetClass();

		if (!CommonBaseClass)
		{
			CommonBaseClass = ObjClass;
		}
		while (!ObjClass->IsChildOf(CommonBaseClass))
		{
			CommonBaseClass = CommonBaseClass->GetSuperClass();
		}

		const FSlateBrush* ActorIcon = FindIconForActor(Actor);

		if (!CommonIcon)
		{
			CommonIcon = ActorIcon;
		}
		if (CommonIcon != ActorIcon)
		{
			CommonIcon = FindIconForClass(CommonBaseClass);
		}
	}

	return CommonIcon;
}

const FSlateBrush* FClassIconFinder::FindIconForActor( const TWeakObjectPtr<AActor>& InActor )
{
	return FClassIconFinder::LookupBrush( FindIconNameForActor(InActor) );
}

FName FClassIconFinder::FindIconNameForActor( const TWeakObjectPtr<AActor>& InActor )
{
	// Actor specific overrides to normal per-class icons
	AActor* Actor = InActor.Get();
	FName BrushName = NAME_None;

	if ( Actor )
	{
		ABrush* Brush = Cast< ABrush >( Actor );
		if ( Brush )
		{
			if (Brush_Add == Brush->BrushType)
			{
				BrushName = TEXT( "ClassIcon.BrushAdditive" );
			}
			else if (Brush_Subtract == Brush->BrushType)
			{
				BrushName = TEXT( "ClassIcon.BrushSubtractive" );
			}
		}

		// Actor didn't specify an icon - fallback on the class icon
		if ( BrushName.IsNone() )
		{
			BrushName = FindIconNameForClass( Actor->GetClass() );
		}
	}
	else
	{
		// If the actor reference is NULL it must have been deleted
		BrushName = TEXT( "ClassIcon.Deleted" );
	}

	return BrushName;
}

const FSlateBrush* FClassIconFinder::FindIconForClass(const UClass* InClass, const FName& InDefaultName )
{
	return FClassIconFinder::LookupBrush( FindIconNameForClass( InClass, InDefaultName ) );
}

FName FClassIconFinder::FindIconNameForClass(const UClass* InClass, const FName& InDefaultName )
{
	return FindIconNameImpl( InClass, InDefaultName );
}

const FSlateBrush* FClassIconFinder::FindThumbnailForClass(const UClass* InClass, const FName& InDefaultName )
{
	return FClassIconFinder::LookupBrush( FindThumbnailNameForClass( InClass, InDefaultName ) );
}

FName FClassIconFinder::FindThumbnailNameForClass(const UClass* InClass, const FName& InDefaultName )
{
	return FindIconNameImpl( InClass, InDefaultName, TEXT("ClassThumbnail") );
}

const UClass* FClassIconFinder::GetIconClassForBlueprint(const UBlueprint* InBlueprint)
{
	if ( !InBlueprint )
	{
		return nullptr;
	}

	// If we're loaded and have a generated class, just use that
	if ( const UClass* GeneratedClass = Cast<UClass>(InBlueprint->GeneratedClass) )
	{
		return GeneratedClass;
	}

	// We don't have a generated class, so instead try and use the parent class from our meta-data
	return GetIconClassForAssetData(FAssetData(InBlueprint));
}

const UClass* FClassIconFinder::GetIconClassForAssetData(const FAssetData& InAssetData, bool* bOutIsClassType)
{
	if ( bOutIsClassType )
	{
		*bOutIsClassType = false;
	}

	UClass* AssetClass = FindObject<UClass>(ANY_PACKAGE, *InAssetData.AssetClass.ToString());
	if ( !AssetClass )
	{
		return nullptr;
	}

	if ( AssetClass == UClass::StaticClass() )
	{
		if ( bOutIsClassType )
		{
			*bOutIsClassType = true;
		}

		return FindObject<UClass>(ANY_PACKAGE, *InAssetData.AssetName.ToString());
	}
	
	if ( AssetClass == UBlueprint::StaticClass() )
	{
		static const FName NativeParentClassTag = "NativeParentClass";
		static const FName ParentClassTag = "ParentClass";

		if ( bOutIsClassType )
		{
			*bOutIsClassType = true;
		}

		// We need to use the asset data to get the parent class as the blueprint may not be loaded
		const FString* ParentClassNamePtr = InAssetData.TagsAndValues.Find(NativeParentClassTag);
		if ( !ParentClassNamePtr )
		{
			ParentClassNamePtr = InAssetData.TagsAndValues.Find(ParentClassTag);
		}
		if ( ParentClassNamePtr && !ParentClassNamePtr->IsEmpty() )
		{
			UObject* Outer = nullptr;
			FString ParentClassName = *ParentClassNamePtr;
			ResolveName(Outer, ParentClassName, false, false);
			return FindObject<UClass>(ANY_PACKAGE, *ParentClassName);
		}
	}

	// Default to using the class for the asset type
	return AssetClass;
}

FName FClassIconFinder::FindIconNameImpl(const UClass* InClass, const FName& InDefaultName, const TCHAR* StyleRoot )
{
	FName BrushName;
	const FSlateBrush* Brush = nullptr;

	if ( InClass )
	{
		// walk up class hierarchy until we find an icon
		const UClass* CurrentClass = InClass;
		while( !Brush && CurrentClass )
		{
			BrushName = *FString::Printf( TEXT( "%s.%s" ), StyleRoot, *CurrentClass->GetName() );
			Brush = FClassIconFinder::LookupBrush( BrushName );
			CurrentClass = CurrentClass->GetSuperClass();
		}
	}

	if( !Brush )
	{
		// If we didn't supply an override name for the default icon use default class icon.
		if( InDefaultName.IsNone() )
		{
			BrushName = *FString::Printf( TEXT( "%s.Default" ), StyleRoot);
		}
		else
		{
			BrushName = InDefaultName;
		}
	}

	return BrushName;
}

