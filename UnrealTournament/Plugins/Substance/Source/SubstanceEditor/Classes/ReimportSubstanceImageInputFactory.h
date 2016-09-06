//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceImageInputFactory.h"

#include "ReimportSubstanceImageInputFactory.generated.h"

class USubstanceImageInput;

UCLASS(hideCategories=Object)
class UReimportSubstanceImageInputFactory : public USubstanceImageInputFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FString> ReimportPaths;

	// Begin FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	// End FReimportHandler interface

	/**
	 * Suppresses the dialog box that, when importing over an existing texture, asks if the users wishes to overwrite its settings.
	 * This is primarily for reimporting textures.
	 */
	static void SuppressImportOverwriteDialog();

private:
	/** This variable is static because in StaticImportObject() the type of the factory is not known. */
	static bool bSuppressImportOverwriteDialog;
};
