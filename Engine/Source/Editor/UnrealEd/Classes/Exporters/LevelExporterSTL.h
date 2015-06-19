// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LevelExporterSTL
//=============================================================================

#pragma once
#include "LevelExporterSTL.generated.h"

UCLASS()
class ULevelExporterSTL : public UExporter
{
public:
	GENERATED_BODY()

public:
	ULevelExporterSTL(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	// Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	// End UExporter Interface
};



