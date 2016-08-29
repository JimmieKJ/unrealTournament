// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace WidgetMaterialTrackUtilities
{
	/** Gets a slate brush from a property on a widget by the properties FName path. */
	UMG_API FSlateBrush* GetBrush( UWidget* Widget, const TArray<FName>& BrushPropertyNamePath );

	/** Gets the property paths on a widget which are slate brush properties, and who's slate brush has a valid material. */
	UMG_API void GetMaterialBrushPropertyPaths( UWidget* Widget, TArray<TArray<UProperty*>>& MaterialBrushPropertyPaths ); 

	/** Converts a property name path into a single name which is appropriate for a track name. */
	UMG_API FName GetTrackNameFromPropertyNamePath( const  TArray<FName>& PropertyNamePath );
}