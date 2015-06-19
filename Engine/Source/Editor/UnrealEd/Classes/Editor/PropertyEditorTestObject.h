// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "PropertyEditorTestObject.generated.h"

UENUM()
enum PropertEditorTestEnum
{	
	/** This comment should appear above enum 1*/
	PropertyEditorTest_Enum1 UMETA(Hidden),
	/** This comment should appear above enum 2*/
	PropertyEditorTest_Enum2,
	/** This comment should appear above enum 3*/
	PropertyEditorTest_Enum3 UMETA(Hidden),
	/** This comment should appear above enum 4*/
	PropertyEditorTest_Enum4,
	/** This comment should appear above enum 5*/
	PropertyEditorTest_Enum5 UMETA(Hidden),
	/** This comment should appear above enum 6*/
	PropertyEditorTest_Enum6,
	PropertyEditorTest_MAX,
};

UENUM()
enum ArrayLabelEnum
{
	ArrayIndex0,
	ArrayIndex1,
	ArrayIndex2,
	ArrayIndex3,
	ArrayIndex4,
	ArrayIndex5,
	ArrayIndex_MAX,
};


USTRUCT()
struct FPropertyEditorTestSubStruct
{
	GENERATED_USTRUCT_BODY()

	FPropertyEditorTestSubStruct()
		: FirstProperty( 7897789 )
		, SecondProperty( 342432432  )
	{
	}

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	int32 FirstProperty;

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	int32 SecondProperty;

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	FLinearColor CustomizedStructInsideUncustomizedStruct;

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	FStringAssetReference CustomizedStructInsideUncustomizedStruct2;
};

/**
 * This structs properties should be pushed out to categories inside its parent category unless it is in an array
 */
USTRUCT()
struct FPropertyEditorTestBasicStruct
{
	GENERATED_USTRUCT_BODY()

	FPropertyEditorTestBasicStruct()
		: IntPropertyInsideAStruct( 0 )
		, FloatPropertyInsideAStruct( 0.0f )
		, ObjectPropertyInsideAStruct( NULL )
	{
	}

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	int32 IntPropertyInsideAStruct;

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	float FloatPropertyInsideAStruct;

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	UObject* ObjectPropertyInsideAStruct;

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	FPropertyEditorTestSubStruct InnerStruct;
};

UCLASS(transient)
class UPropertyEditorTestObject : public UObject
{
    GENERATED_UCLASS_BODY()

	// Integer
	UPROPERTY(EditAnywhere, Category=BasicProperties)
	int32 IntProperty32;

	// Byte
	UPROPERTY(EditAnywhere, Category=BasicProperties)
	uint8 ByteProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	float FloatProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FName NameProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	bool BoolProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FString StringProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FText TextProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FVector Vector3Property;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FVector2D Vector2Property;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FVector4 Vector4Property;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FRotator RotatorProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	UObject* ObjectProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	UClass* ClassProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FLinearColor LinearColorProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FColor ColorProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	TEnumAsByte<enum PropertEditorTestEnum> EnumProperty;

	// Integer
	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<int32> IntProperty32Array;

	// Byte
	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<uint8> BytePropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<float> FloatPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FName> NamePropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<bool> BoolPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FString> StringPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FText> TextPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FVector> Vector3PropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FVector2D> Vector2PropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FVector4> Vector4PropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FRotator> RotatorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<UObject*> ObjectPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<AActor*> ActorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FLinearColor> LinearColorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FColor> ColorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<TEnumAsByte<enum PropertEditorTestEnum> > EnumPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FPropertyEditorTestBasicStruct> StructPropertyArray;

	UPROPERTY(EditAnywhere, editfixedsize, Category=ArraysOfProperties)
	TArray<int32> FixedArrayOfInts;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	int32 StaticArrayOfInts[5];

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	int32 StaticArrayOfIntsWithEnumLabels[ArrayIndex_MAX];

	/** This is a float property tooltip that is overridden */
	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "50.0", ToolTip = "This is a custom tooltip that should be shown"))
	float FloatPropertyWithClampedRange;

	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "50" ))
	int32 IntPropertyWithClampedRange;

	UPROPERTY(VisibleAnywhere, Category=AdvancedProperties)
	int32 IntThatCannotBeChanged;

	UPROPERTY(VisibleAnywhere, Category=AdvancedProperties)
	FString StringThatCannotBeChanged;

	UPROPERTY(VisibleAnywhere, Category=AdvancedProperties)
	UPrimitiveComponent* ObjectThatCannotBeChanged;

	UPROPERTY(EditAnywhere, Category=SingleStruct, meta=(ShowOnlyInnerProperties))
	FPropertyEditorTestBasicStruct ThisIsBrokenIfItsVisibleInADetailsView;

	UPROPERTY(EditAnywhere, Category=StructTests)
	FPropertyEditorTestBasicStruct StructWithMultipleInstances1;

	UPROPERTY(EditAnywhere, Category=StructTests, meta=(editcondition = "bEditCondition"))
	FPropertyEditorTestBasicStruct StructWithMultipleInstances2;

	UPROPERTY(EditAnywhere, Category=StructTests)
	FStringAssetReference AssetReferenceCustomStruct;

	UPROPERTY(EditAnywhere, Category=StructTests, meta=(DisplayThumbnail = "true"))
	FStringAssetReference AssetReferenceCustomStructWithThumbnail;

	UPROPERTY()
	bool bEditCondition;

	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(editcondition = "bEditCondition"))
	int32 SimplePropertyWithEditCondition;

	UPROPERTY(EditAnywhere, Category=StructTests, meta=(editcondition = "bEditCondition"))
	FStringAssetReference AssetReferenceCustomStructWithEditCondition;

	UPROPERTY(EditAnywhere, Category=StructTests)
	TArray<FPropertyEditorTestBasicStruct> ArrayOfStructs;

	UPROPERTY(EditAnywhere, Category=EditInlineProps)
	UStaticMeshComponent* EditInlineNewStaticMeshComponent;

	UPROPERTY(EditAnywhere, Category=EditInlineProps)
	TArray<UStaticMeshComponent*> ArrayOfEditInlineNewSMCs;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	UTexture* TextureProp;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	UStaticMesh* StaticMeshProp;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	UMaterialInterface* AnyMaterialInterface;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	AActor* OnlyActorsAllowed;
};