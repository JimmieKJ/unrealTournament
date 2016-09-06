// Copyright 2014 Allegorithmic All Rights Reserved.

#pragma once

#include "Factories.h"
#include "SubstanceImportOptionsUi.h"
#include "SubstanceFactory.generated.h"

UCLASS(hideCategories=Object, customConstructor)
class USubstanceFactory : public UFactory
{
	GENERATED_UCLASS_BODY()
	
public:
	USubstanceFactory(const class FObjectInitializer& PCIP);
	
	// Begin UFactory Interface
	virtual UObject* FactoryCreateBinary(UClass*, UObject*, FName, EObjectFlags, UObject*, const TCHAR*, const uint8*&, const uint8*, FFeedbackContext*) override;
	// Begin UFactory Interface

	/**
	 * Suppresses the dialog box that, when importing over an existing texture, asks if the users wishes to overwrite its settings.
	 * This is primarily for reimporting textures.
	 */
//	static void SuppressImportOverwriteDialog();

private:
	/** This variable is static because in StaticImportObject() the type of the factory is not known. */
	static bool bSuppressImportOverwriteDialog;
};

namespace Substance
{
	struct FSubstanceImportOptions
	{
		// General options
		bool bCreateMaterial;
		bool bCreateInstance;
		bool bForceCreateInstance;

		FString InstanceName;
		FString MaterialName;

		FString InstanceDestinationPath;
		FString MaterialDestinationPath;
	};

	void GetImportOptions( 
		FString Name,
		FString ParentName,
		FSubstanceImportOptions& InOutImportOptions,
		bool& OutOperationCanceled);

	void ApplyImportUIToImportOptions(USubstanceImportOptionsUi* ImportUI, FSubstanceImportOptions& InOutImportOptions);
}
