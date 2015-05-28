// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LiveEditorTypes.generated.h"

UENUM()
namespace ELiveEditControllerType UMETA(BlueprintType="true")
{
	enum Type
	{
		NotSet						UMETA(DisplayName="Not Set"),
		NoteOnOff					UMETA(DisplayName="Note On/Off"),
		ControlChangeContinuous		UMETA(DisplayName="Control Continuous"),
		ControlChangeFixed			UMETA(DisplayName="Control Fixed"),

		Count						UMETA(Hidden)
	};
}

USTRUCT()
struct FLiveEditBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 Channel;

	UPROPERTY()
	int32 ControlID;

	UPROPERTY()
	TEnumAsByte<ELiveEditControllerType::Type> ControlType;

	UPROPERTY()
	int32 LastValue;

public:
	FLiveEditBinding()
		: Channel(-1)
		, ControlID(-1)
		, ControlType(ELiveEditControllerType::NotSet)
		, LastValue(0)
	{}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar,FLiveEditBinding& Binding)
	{
		return Ar << Binding.Channel << Binding.ControlID << Binding.ControlType << Binding.LastValue;
	}
};

USTRUCT(BlueprintType)
struct FLiveEditorCheckpointData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Live Editor")
	int32 IntValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Live Editor")
	float FloatValue;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar,FLiveEditorCheckpointData& Data)
	{
		return Ar << Data.IntValue << Data.FloatValue;
	}
};

UCLASS(Abstract, CustomConstructor)
class ULiveEditorTypes : public UObject
{
	GENERATED_UCLASS_BODY()

	ULiveEditorTypes(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) {}
};
