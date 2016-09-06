// Copyright 2014 Allegorithmic All Rights Reserved.

#pragma once

#include "SubstanceGraphInstance.generated.h"

namespace Substance
{
	struct FGraphInstance;
}

class USubstanceInstanceFactory;

UENUM(BlueprintType)
	enum ESubstanceInputType
{
	SIT_Float               =0,
	SIT_Float2              =1,
	SIT_Float3              =2,
	SIT_Float4              =3,
	SIT_Integer             =4,
	SIT_Image               =5,
	SIT_Unused_6            =6,
	SIT_Unused_7            =7,
	SIT_Integer2            =8,
	SIT_Integer3            =9,
	SIT_Integer4            =10,
	SIT_MAX                 =11,
};


USTRUCT(BlueprintType)
struct FSubstanceInputDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	FString Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TEnumAsByte<ESubstanceInputType> Type;
};


USTRUCT(BlueprintType)
struct FSubstanceIntInputDesc : public FSubstanceInputDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TArray<int32> Min;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TArray<int32> Max;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TArray<int32> Default;
};


USTRUCT(BlueprintType)
struct FSubstanceFloatInputDesc : public FSubstanceInputDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TArray<float> Min;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TArray<float> Max;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TArray<float> Default;
};


USTRUCT(BlueprintType)
struct FSubstanceInstanceDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	FString Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Values")
	TArray<FSubstanceInputDesc> Inputs;
};


UCLASS(hideCategories=Object, MinimalAPI, BlueprintType)
class USubstanceGraphInstance : public UObject
{
	GENERATED_UCLASS_BODY()

public:
    Substance::FGraphInstance* Instance;

	UPROPERTY()
    USubstanceInstanceFactory* Parent;

	UPROPERTY()
	TArray<UObject*> ImageSources;

	UFUNCTION(BlueprintCallable, Category="Substance")
	TArray<FString> GetInputNames();

	UFUNCTION(BlueprintCallable, Category="Substance")
	ESubstanceInputType GetInputType(FString InputName);
	
	UFUNCTION(BlueprintCallable, Category="Substance")
	void SetInputInt(FString Identifier, const TArray<int32>& Value);

	UFUNCTION(BlueprintCallable, Category="Substance")
	void SetInputFloat(FString Identifier, const TArray<float>& InputValues);

	UFUNCTION(BlueprintCallable, Category="Substance")
	TArray< int32 > GetInputInt(FString Identifier);

	UFUNCTION(BlueprintCallable, Category="Substance")
	TArray< float > GetInputFloat(FString Identifier);

	UFUNCTION(BlueprintCallable, Category="Substance")
	FSubstanceIntInputDesc GetIntInputDesc(FString Identifier);

	UFUNCTION(BlueprintCallable, Category="Substance")
	FSubstanceFloatInputDesc GetFloatInputDesc(FString Identifier);

	UFUNCTION(BlueprintCallable, Category="Substance")
	FSubstanceInstanceDesc GetInstanceDesc();

	UPROPERTY(BlueprintReadOnly, Category = "Substance")
	bool bFreezed;
	
	bool bCooked;

	UFUNCTION(BlueprintCallable, Category = "Substance")
	bool SetInputImg(const FString& InputName, class UObject* Value);

    class UObject* GetInputImg(const FString& InputName);
  
	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR
	virtual bool CanEditChange( const UProperty* InProperty ) const;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
#endif
	// End UObject interface.
};
