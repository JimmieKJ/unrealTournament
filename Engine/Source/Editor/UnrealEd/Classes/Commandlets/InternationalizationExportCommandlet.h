// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Commandlets/Commandlet.h"
#include "InternationalizationExportCommandlet.generated.h"

class FPortableObjectFormatDOM;

struct FPortableObjectEntryIdentity
{
	FString MsgCtxt;
	FString MsgId;
	FString MsgIdPlural;
};
bool operator==(const FPortableObjectEntryIdentity& LHS, const FPortableObjectEntryIdentity& RHS);
uint32 GetTypeHash(const FPortableObjectEntryIdentity& ID);

/**
 *	UInternationalizationExportCommandlet: Commandlet used to export internationalization data to various standard formats. 
 */
UCLASS()
class UInternationalizationExportCommandlet : public UGatherTextCommandletBase
{
    GENERATED_BODY()

public:
	UInternationalizationExportCommandlet(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
		, HasPreservedComments(false)
	{}

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
private:
	bool DoExport(const FString& SourcePath, const FString& DestinationPath, const FString& Filename);
	bool DoImport(const FString& SourcePath, const FString& DestinationPath, const FString& Filename);

	bool LoadPOFile(const FString& POFilePath, FPortableObjectFormatDOM& OutPortableObject);
	void PreserveExtractedCommentsForPersistence(FPortableObjectFormatDOM& PortableObject);

	TArray<FString> CulturesToGenerate;
	FString ConfigPath;
	FString SectionName;

	bool ShouldPersistComments;
	bool HasPreservedComments;
	TMap< FPortableObjectEntryIdentity, TArray<FString> > POEntryToCommentMap;
};
