// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LevelExporterOBJ
//=============================================================================

#pragma once
#include "LevelExporterOBJ.generated.h"

UCLASS()
class ULevelExporterOBJ : public UExporter
{
public:
	GENERATED_BODY()

public:
	ULevelExporterOBJ(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	// Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	// End UExporter Interface
};



