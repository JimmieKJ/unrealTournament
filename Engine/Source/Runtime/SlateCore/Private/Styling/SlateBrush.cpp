// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* FSlateBrush constructors
 *****************************************************************************/

FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FLinearColor& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize( InImageSize )
	, DrawAs( InDrawType )
	, Margin( InMargin )
	, TintColor( InTint )
	, Tiling( InTiling )
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType( InImageType )
	, ResourceObject( InObjectResource )
	, ResourceName( InResourceName )
	, bIsDynamicallyLoaded( bInDynamicallyLoaded )
	, UVRegion( ForceInit )
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}


FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize( InImageSize )
	, DrawAs( InDrawType )
	, Margin( InMargin )
	, TintColor( InTint )
	, Tiling( InTiling )
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType( InImageType )
	, ResourceObject( InObjectResource )
	, ResourceName( InResourceName )
	, bIsDynamicallyLoaded( bInDynamicallyLoaded )
	, UVRegion( ForceInit )
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}


FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FSlateColor& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize(InImageSize)
	, DrawAs(InDrawType)
	, Margin(InMargin)
	, TintColor(InTint)
	, Tiling(InTiling)
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType(InImageType)
	, ResourceObject(InObjectResource)
	, ResourceName(InResourceName)
	, bIsDynamicallyLoaded(bInDynamicallyLoaded)
	, UVRegion(ForceInit)
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}


/* FSlateBrush static functions
 *****************************************************************************/

const FString FSlateBrush::UTextureIdentifier( )
{
	return FString(TEXT("texture:/"));
}
