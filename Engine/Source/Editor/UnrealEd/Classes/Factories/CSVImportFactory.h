// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Factory for importing table from text (CSV)
 */

#pragma once
#include "CSVImportFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCSVImportFactory, Log, All);

UCLASS(hidecategories=Object)
class UCSVImportFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual UObject* FactoryCreateText( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn ) override;
	virtual bool DoesSupportClass(UClass * Class) override;
	
	// Begin UFactory Interface

	/* Reimport an object that was created based on a CSV */
	bool ReimportCSV(UObject* Obj);

private:
	/* Reimport object from the given path*/
	bool Reimport(UObject* Obj, const FString& Path  );

};

