// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// ObjectExporterT3D
//=============================================================================

#pragma once
#include "ObjectExporterT3D.generated.h"

UCLASS()
class UObjectExporterT3D : public UExporter
{
public:
	GENERATED_BODY()

public:
	UObjectExporterT3D(const FObjectInitializer& ObjectInitializer = FObjectInitializer());


	// Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	// End UExporter Interface
};



