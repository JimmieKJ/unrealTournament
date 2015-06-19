// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// PolysExporterT3D
//=============================================================================

#pragma once
#include "PolysExporterT3D.generated.h"

UCLASS()
class UPolysExporterT3D : public UExporter
{
public:
	GENERATED_BODY()

public:
	UPolysExporterT3D(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	// Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	// End UExporter Interface
};



