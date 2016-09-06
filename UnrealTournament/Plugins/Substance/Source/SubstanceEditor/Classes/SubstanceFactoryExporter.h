//! @file SubstanceFactoryExporter.h
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceFactoryExporter.generated.h"

UCLASS()
class USubstanceFactoryExporter : public UExporter
{
	GENERATED_UCLASS_BODY()

	bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 );
};
