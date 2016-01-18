// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// PersonaOptions
//
// A configuration class that holds information for the setup of the Persona.
// Supplied so that the editor 'remembers' the last setup the user had.
//=============================================================================

#pragma once
#include "PersonaOptions.generated.h"

UCLASS(hidecategories=Object, config=EditorPerProjectUserSettings)
class UNREALED_API UPersonaOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowFloor:1;

	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowSky:1;

	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowGrid:1;

	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bHighlightOrigin:1;

	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bMuteAudio:1;

	// currently Stats can have None, Basic and Detailed. Please refer to EDisplayInfoMode.
	UPROPERTY(EditAnywhere, config, Category = Options, meta=(ClampMin ="0", ClampMax = "2", UIMin = "0", UIMax = "2"))
	int32 ShowMeshStats;

	UPROPERTY(EditAnywhere, config, Category=Options)
	int32 GridSize;

	UPROPERTY(EditAnywhere, config, Category=Options)
	TEnumAsByte<EViewModeIndex> ViewModeIndex;

	UPROPERTY(EditAnywhere, config, Category=Options)
	FLinearColor ViewportBackgroundColor;

	UPROPERTY(EditAnywhere, config, Category=Options)
	float ViewFOV;

	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 DefaultLocalAxesSelection;

	UPROPERTY(EditAnywhere, config, Category = Options)
	FLinearColor SectionTimingNodeColor;

	UPROPERTY(EditAnywhere, config, Category = Options)
	FLinearColor NotifyTimingNodeColor;

	UPROPERTY(EditAnywhere, config, Category = Options)
	FLinearColor BranchingPointTimingNodeColor;

public:
	void SetViewportBackgroundColor( const FLinearColor& InViewportBackgroundColor);
	void SetShowGrid( bool bInShowGrid );
	void SetHighlightOrigin( bool bInHighlightOrigin );
	void SetShowFloor( bool bInShowFloor );
	void SetShowSky( bool bInShowSky );
	void SetMuteAudio( bool bInMuteAudio );
	void SetGridSize( int32 InGridSize );
	void SetViewModeIndex( EViewModeIndex InViewModeIndex );
	void SetViewFOV( float InViewFOV );
	void SetDefaultLocalAxesSelection( uint32 InDefaultLocalAxesSelection );
	void SetShowMeshStats( int32 InShowMeshStats );
	void SetSectionTimingNodeColor(const FLinearColor& InColor);
	void SetNotifyTimingNodeColor(const FLinearColor& InColor);
	void SetBranchingPointTimingNodeColor(const FLinearColor& InColor);
};
