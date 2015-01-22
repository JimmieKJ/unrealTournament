// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// SequenceExporterT3D
//=============================================================================

#pragma once
#include "SequenceExporterT3D.generated.h"

UCLASS()
class USequenceExporterT3D : public UExporter
{
	GENERATED_UCLASS_BODY()


	// Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	// End UExporter Interface
};

