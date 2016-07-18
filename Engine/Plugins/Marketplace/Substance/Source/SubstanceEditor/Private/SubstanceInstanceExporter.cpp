//! @file SubstanceInstanceExporter.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"
#include "SubstanceInstanceExporter.h"
#include "SubstanceCorePreset.h"
#include "SubstanceCoreXmlHelper.h"

USubstanceInstanceExporter::USubstanceInstanceExporter(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	SupportedClass = USubstanceGraphInstance::StaticClass();
	bText = true;
	FormatExtension.Add(TEXT("sbsprs"));
	FormatDescription.Add(TEXT("Substance preset"));
}

bool USubstanceInstanceExporter::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags )
{
	FString PresetString;
	USubstanceGraphInstance* GraphInstance = CastChecked<USubstanceGraphInstance>( Object );
	Substance::FPreset Preset;
	Preset.ReadFrom(GraphInstance->Instance);
	Substance::Helpers::WriteSubstanceXmlPreset(Preset, PresetString);
	Ar.Log( PresetString );
	return 1;
}

