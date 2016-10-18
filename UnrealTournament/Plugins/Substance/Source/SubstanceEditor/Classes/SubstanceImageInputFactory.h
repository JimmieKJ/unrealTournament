// Copyright 2014 Allegorithmic All Rights Reserved.

#pragma once

#include "Factories.h"

#include "SubstanceImageInputFactory.generated.h"

class USubstanceImageInput;

UCLASS(hideCategories=Object, customConstructor)
class USubstanceImageInputFactory : public UFactory
{
	GENERATED_UCLASS_BODY()
	
	/** If enabled, a material will automatically be created for the texture */
	UPROPERTY()
	uint32 bCreateMaterial:1;
	
	UPROPERTY()
	uint32 bCreateDefaultInstance:1;

public:

	USubstanceImageInputFactory(
		const class FObjectInitializer& PCIP);
	
	// Begin UFactory Interface
	virtual UObject* FactoryCreateBinary(UClass* Class, 
		UObject* InParent, 
		FName Name, 
		EObjectFlags Flags,
		UObject* Context,
		const TCHAR* Type, 
		const uint8*& Buffer, 
		const uint8* BufferEnd, 
		FFeedbackContext* Warn ) override;
	// Begin UFactory Interface
	

	/**
	 * Suppresses the dialog box that, when importing over an existing texture, asks if the users wishes to overwrite its settings.
	 * This is primarily for reimporting textures.
	 */
	static void SuppressImportOverwriteDialog();

protected:
	USubstanceImageInput* FactoryCreateBinaryFromTexture(
		UObject* InParent, 
		FName Name,
		UTexture2D* ContextTexture,
		const uint8*& Buffer,
		const uint8*	BufferEnd,
		FFeedbackContext* Warn);

	USubstanceImageInput* FactoryCreateBinaryFromTga(
		UObject* InParent, 
		FName Name,
		const uint8*& Buffer,
		const uint8*	BufferEnd,
		FFeedbackContext* Warn);

	USubstanceImageInput* FactoryCreateBinaryFromJpeg(
		UObject* InParent, 
		FName Name,
		const uint8*& Buffer,
		const uint8*	BufferEnd,
		FFeedbackContext* Warn);

private:
	/** This variable is static because in StaticImportObject() the type of the factory is not known. */
	static bool bSuppressImportOverwriteDialog;
};
