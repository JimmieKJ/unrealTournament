// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserDefinedStructEditorData.generated.h"

USTRUCT()
struct FStructVariableDescription
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName VarName;

	UPROPERTY()
	FGuid VarGuid;

	UPROPERTY()
	FString FriendlyName;

	UPROPERTY()
	FString DefaultValue;

	// TYPE DATA
	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString SubCategory;

	UPROPERTY()
	TAssetPtr<UObject> SubCategoryObject;

	UPROPERTY()
	bool bIsArray;

	UPROPERTY(Transient)
	bool bInvalidMember;

	// CurrentDefaultValue stores the actual default value, after the DefaultValue was changed, and before the struct was recompiled
	UPROPERTY()
	FString CurrentDefaultValue;

	UPROPERTY()
	FString ToolTip;

	UPROPERTY()
	bool bDontEditoOnInstance;

	UPROPERTY()
	bool bEnableMultiLineText;

	UPROPERTY()
	bool bEnable3dWidget;

	UNREALED_API bool SetPinType(const struct FEdGraphPinType& VarType);

	UNREALED_API FEdGraphPinType ToPinType() const;

	FStructVariableDescription()
		: bIsArray(false)
		, bInvalidMember(false)
		, bDontEditoOnInstance(false)
		, bEnableMultiLineText(false)
		, bEnable3dWidget(false)
	{ }
};

class FStructOnScopeMember : public FStructOnScope
{
public:
	FStructOnScopeMember() : FStructOnScope() {}

	void Recreate(const UStruct* InScriptStruct)
	{
		Destroy();
		ScriptStruct = InScriptStruct;
		Initialize();
	}
};

UCLASS()
class UNREALED_API UUserDefinedStructEditorData : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	// the property is used to generate an uniqe name id for member variable
	UPROPERTY(NonTransactional) 
	uint32 UniqueNameId;

public:
	UPROPERTY()
	TArray<FStructVariableDescription> VariablesDescriptions;

	UPROPERTY()
	FString ToolTip;

	// optional super struct
	UPROPERTY()
	UScriptStruct* NativeBase;

protected:
	FStructOnScopeMember DefaultStructInstance;

public:
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// UObject interface.
	virtual TSharedPtr<ITransactionObjectAnnotation> GetTransactionAnnotation() const override;
	virtual void PostEditUndo() override;
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;
	virtual void PostLoadSubobjects(struct FObjectInstancingGraph* OuterInstanceGraph) override;
	// End of UObject interface.

	uint32 GenerateUniqueNameIdForMemberVariable();
	class UUserDefinedStruct* GetOwnerStruct() const;

	const uint8* GetDefaultInstance() const;
	void RecreateDefaultInstance(FString* OutLog = nullptr);
	void CleanDefaultInstance();
};