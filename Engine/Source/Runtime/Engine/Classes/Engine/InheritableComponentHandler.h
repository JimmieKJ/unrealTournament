// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InheritableComponentHandler.generated.h"

class  USCS_Node;
class  UActorComponent;
struct FUCSComponentId;
class  UBlueprint;
class  UBlueprintGeneratedClass;

USTRUCT()
struct ENGINE_API FComponentKey
{
	GENERATED_USTRUCT_BODY()

	FComponentKey()
		: OwnerClass(nullptr)
	{}

	FComponentKey(const USCS_Node* SCSNode);
#if WITH_EDITOR
	FComponentKey(UBlueprint* Blueprint, const FUCSComponentId& UCSComponentID);
#endif 

	bool Match(const FComponentKey OtherKey) const;

	bool IsSCSKey() const
	{
		return (SCSVariableName != NAME_None) && AssociatedGuid.IsValid();
	}

	bool IsUCSKey() const
	{
		return AssociatedGuid.IsValid() && (SCSVariableName == NAME_None);
	}

	bool IsValid() const
	{
		return OwnerClass && AssociatedGuid.IsValid() && (!IsSCSKey() || (SCSVariableName != NAME_None));
	}

	USCS_Node* FindSCSNode() const;
	UActorComponent* GetOriginalTemplate() const;
	bool RefreshVariableName();

	class UBlueprintGeneratedClass* GetComponentOwner()  const { return OwnerClass; }
	FName   GetSCSVariableName() const { return SCSVariableName; }
	FGuid   GetAssociatedGuid()  const { return AssociatedGuid; }

private: 
	UPROPERTY()
	class UBlueprintGeneratedClass* OwnerClass;

	UPROPERTY()
	FName SCSVariableName;

	UPROPERTY()
	FGuid AssociatedGuid;
};

USTRUCT()
struct FComponentOverrideRecord
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UActorComponent* ComponentTemplate;

	UPROPERTY()
	FComponentKey ComponentKey;

	UPROPERTY()
	FBlueprintCookedComponentInstancingData CookedComponentInstancingData;

	FComponentOverrideRecord()
		: ComponentTemplate(nullptr)
	{}
};

UCLASS()
class ENGINE_API UInheritableComponentHandler : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
private:
	bool IsRecordValid(const FComponentOverrideRecord& Record) const;
	bool IsRecordNecessary(const FComponentOverrideRecord& Record) const;

public:

	UActorComponent* CreateOverridenComponentTemplate(FComponentKey Key);
	void RemoveOverridenComponentTemplate(FComponentKey Key);
	void UpdateOwnerClass(UBlueprintGeneratedClass* OwnerClass);
	void ValidateTemplates();
	bool IsValid() const;
	UActorComponent* FindBestArchetype(FComponentKey Key) const;

	bool IsEmpty() const
	{
		return 0 == Records.Num();
	}

	bool RefreshTemplateName(FComponentKey OldKey);

	FComponentKey FindKey(UActorComponent* ComponentTemplate) const;
#endif

public:

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	void PreloadAllTempates();
	void PreloadAll();

	FComponentKey FindKey(const FName VariableName) const;

	UActorComponent* GetOverridenComponentTemplate(FComponentKey Key) const;
	const FBlueprintCookedComponentInstancingData* GetOverridenComponentTemplateData(FComponentKey Key) const;

	TArray<FComponentOverrideRecord>::TIterator CreateRecordIterator()
	{
		return Records.CreateIterator();
	}

	void GetAllTemplates(TArray<UActorComponent*>& OutArray) const
	{
		for (auto Record : Records)
		{
			OutArray.Add(Record.ComponentTemplate);
		}
	}

private:
	const FComponentOverrideRecord* FindRecord(const FComponentKey Key) const;
	
	UPROPERTY()
	TArray<FComponentOverrideRecord> Records;
};
