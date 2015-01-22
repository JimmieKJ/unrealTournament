// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SoundExporterOGG.generated.h"

UCLASS()
class USoundExporterOGG : public UExporter
{
    GENERATED_UCLASS_BODY()
	// Begin UExporter Interface
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
	virtual bool SupportsObject(UObject* Object) const override;
	// End UExporter Interface
};


