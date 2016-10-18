// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AlembicImportFactory.generated.h"

class UAbcImportSettings;
class FAbcImporter;
class UGeometryCache;
class SAlembicImportOptions;
class SAlembicTrackSelectionWindow;

class FAbcImporter;

UCLASS(hidecategories=Object)
class UAlembicImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	/** Object used to show import options for Alembic */
	UPROPERTY()
	UAbcImportSettings* ImportSettings;

	//~ Begin UObject Interface
	void PostInitProperties();
	//~ End UObject Interface

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	//~ End UFactory Interface

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;

	void ShowImportOptionsWindow(TSharedPtr<SAlembicImportOptions>& Options, FString FilePath, const FAbcImporter& Importer);

	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface
		
	/**
	* Imports a StaticMesh (using AbcImporter) from the Alembic File
	*
	* @param Importer - AbcImporter instance
	* @param InParent - Parent for the StaticMesh
	* @param Flags - Creation Flags for the StaticMesh	
	* @return UObject*
	*/
	TArray<UObject*> ImportStaticMesh(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags);
	
	/**
	* ImportGeometryCache
	*	
	* @param Importer - AbcImporter instance
	* @param InParent - Parent for the GeometryCache asset
	* @param Flags - Creation Flags for the GeometryCache asset
	* @return UObject*
	*/
	UObject* ImportGeometryCache(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags);

	/**
	* ImportGeometryCache
	*	
	* @param Importer - AbcImporter instance
	* @param InParent - Parent for the GeometryCache asset
	* @param Flags - Creation Flags for the GeometryCache asset
	* @return UObject*
	*/
	UObject* ImportSkeletalMesh(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags);

	/**
	* ReimportStaticMesh
	*
	* @param Mesh - Static mesh instance to re import
	* @return EReimportResult::Type
	*/
	EReimportResult::Type ReimportStaticMesh(UStaticMesh* Mesh);

	/**
	* ReimportGeometryCache
	*
	* @param Cache - Geometry cache instance to re import
	* @return EReimportResult::Type
	*/
	EReimportResult::Type ReimportGeometryCache(UGeometryCache* Cache);

	/**
	* ReimportGeometryCache
	*
	* @param Cache - Geometry cache instance to re import
	* @return EReimportResult::Type
	*/
	EReimportResult::Type ReimportSkeletalMesh(USkeletalMesh* SkeletalMesh);
};