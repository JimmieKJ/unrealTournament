//! @file SubstanceFactoryExporter.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.


#include "SubstanceEditorPrivatePCH.h"
#include "SubstanceFactoryExporter.h"
#include "SubstanceFPackage.h"
#include "framework/details/detailslinkdata.h"

USubstanceFactoryExporter::USubstanceFactoryExporter(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	SupportedClass = USubstanceInstanceFactory::StaticClass();
	bText = false;
	FormatExtension.Add(TEXT("sbsar"));
	FormatDescription.Add(TEXT("Substance archive"));
}

bool USubstanceFactoryExporter::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	USubstanceInstanceFactory* InstanceFactory = CastChecked<USubstanceInstanceFactory>( Object );
	Substance::Details::LinkDataAssembly* LinkData = (Substance::Details::LinkDataAssembly*)InstanceFactory->SubstancePackage->getLinkData().get();
	
	std::string sbsasm = LinkData->getAssembly();
	uint8* sbsasmptr = (uint8*)sbsasm.c_str();
	uint32 sbsasmsize = sbsasm.length()+1;
		
	for( uint32 j=0; j<sbsasmsize; j++ )
	{
		Ar << *sbsasmptr++;
	}

	return true;
}
