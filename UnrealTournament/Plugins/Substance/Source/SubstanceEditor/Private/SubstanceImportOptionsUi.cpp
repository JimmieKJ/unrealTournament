//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"

#include "SubstanceImportOptionsUi.h"


USubstanceImportOptionsUi::USubstanceImportOptionsUi(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	bOverrideFullName = false;
	bCreateInstance = true;
	bCreateMaterial = true;
}
