//! @file SubstanceInstanceExporter.h
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceInstanceExporter.generated.h"

UCLASS()
class USubstanceInstanceExporter : public UExporter
{
	GENERATED_UCLASS_BODY()

	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
};
