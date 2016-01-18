// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine.h"
#include "Net/UnrealNetwork.h"
#include "Net/RepLayout.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "GameplayDebuggerBaseObject.generated.h"

#define ENABLED_GAMEPLAY_DEBUGGER (1 && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

class AActorChannel;
class UFont;
class FCanvasItem;
struct FCanvasIcon;
class UCanvas;
class UFont;

namespace DebugTools
{
	static const FVector InvalidLocation = FVector(FLT_MAX);
}

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGameplayDebugger, Display, All);

UENUM()
enum class EGameplayDebuggerShapeElement : uint8
{
	Invalid = 0,
	SinglePoint, // individual points. 
	Segment, // pairs of points 
	Box,
	Cone,
	Cylinder,
	Capsule,
	Polygon,
	String,
};

enum class EGameplayDebuggerReplicate : uint8
{
	WithoutCompression = 0,
	WithCompressed 
};


USTRUCT()
struct ENGINE_API FGameplayDebuggerShapeElement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Description;

	UPROPERTY()
	TArray<FVector> Points;

	UPROPERTY()
	FMatrix TransformationMatrix;

	UPROPERTY()
	TEnumAsByte<EGameplayDebuggerShapeElement> Type;

	UPROPERTY()
	uint8 Color;

	UPROPERTY()
	uint16 ThicknesOrRadius;

	FGameplayDebuggerShapeElement(EGameplayDebuggerShapeElement InType = EGameplayDebuggerShapeElement::Invalid);
	FGameplayDebuggerShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness);
	void SetColor(const FColor& InColor);
	EGameplayDebuggerShapeElement GetType() const;
	void SetType(EGameplayDebuggerShapeElement InType);
	FColor GetFColor() const;
};

UCLASS(Transient, Abstract)
class ENGINE_API UGameplayDebuggerHelper : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	struct FPrintContext
	{
	public:
		TWeakObjectPtr<UCanvas> Canvas;
		TWeakObjectPtr<UFont> Font;
		float CursorX, CursorY;
		float DefaultX, DefaultY;
		FFontRenderInfo FontRenderInfo;
	public:
		FPrintContext() {}
		FPrintContext(class UFont* InFont, class UCanvas* InCanvas, float InX, float InY) : Canvas(InCanvas), Font(InFont), CursorX(InX), CursorY(InY), DefaultX(InX), DefaultY(InY) {}
	};

	// Begin - helper for gameplay debugger API
	static void SetOverHeadContext(const FPrintContext& InContext) { OverHeadContext = InContext; }
	static void SetDefaultContext(const FPrintContext& InContext) { DefaultContext = InContext; }
	static FPrintContext& GetOverHeadContext() { return OverHeadContext; }
	static FPrintContext& GetDefaultContext() { return DefaultContext; }

	UFUNCTION(BlueprintCallable, Category = GameplayDebugger)
	void PrintDebugString(const FString& InString);

	static void PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FString& InString);
	static void PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FString& InString, float X, float Y);
	static void PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FColor& InColor, const FString& InString);
	static void PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FColor& InColor, const FString& InString, float X, float Y);
	static void CalulateStringSize(const UGameplayDebuggerHelper::FPrintContext& Context, UFont* Font, const FString& InString, float& OutX, float& OutY);
	static void CalulateTextSize(const UGameplayDebuggerHelper::FPrintContext& Context, UFont* Font, const FText& InText, float& OutX, float& OutY);
	static FVector ProjectLocation(const UGameplayDebuggerHelper::FPrintContext& Context, const FVector& Location);
	static void DrawItem(const UGameplayDebuggerHelper::FPrintContext& Context, class FCanvasItem& Item, float X, float Y);
	static void DrawIcon(const UGameplayDebuggerHelper::FPrintContext& Context, const FColor& InColor, const FCanvasIcon& Icon, float X, float Y, float Scale = 0.f);

	static FGameplayDebuggerShapeElement MakeString(const FString& Description, const FVector& Location);
	static FGameplayDebuggerShapeElement MakeLine(const FVector& Start, const FVector& End, FColor Color, float Thickness = 0);
	static FGameplayDebuggerShapeElement MakePoint(const FVector& Location, FColor Color, float Radius = 2);
	static FGameplayDebuggerShapeElement MakeCylinder(FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color);
	// End - helper for gameplay debugger API

protected:
	static FPrintContext OverHeadContext;
	static FPrintContext DefaultContext;
};

ENGINE_API FArchive& operator<<(FArchive& Ar, FGameplayDebuggerShapeElement& ShapeElement);

USTRUCT()
struct FGameplayDebuggerReplicatedBlob
{
	GENERATED_USTRUCT_BODY()

	FGameplayDebuggerReplicatedBlob() {}
	FGameplayDebuggerReplicatedBlob(const TArray<uint8>& InRepData, int32 InVersionNum, int32 InRepDataOffset, int32 InRepDataSize) 
		: RepData(InRepData), VersionNum(InVersionNum), RepDataOffset(InRepDataOffset), RepDataSize(InRepDataSize) {}

	UPROPERTY()
	TArray<uint8> RepData;

	UPROPERTY()
	int32 VersionNum;
	
	UPROPERTY()
	int32 RepDataOffset;
	
	UPROPERTY()
	int32 RepDataSize;
};

UCLASS(Transient, Abstract)
class ENGINE_API UGameplayDebuggerBaseObject : public UGameplayDebuggerHelper
{
	GENERATED_UCLASS_BODY()

public:
	// Begin UObject interface
	virtual bool IsSupportedForNetworking() const override { return true; }
	int32 GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack) override; //override needed to have Client/Server function for UObject derived class
	bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;  //override needed to have Client/Server function for UObject derived class
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}
		return GetOuter()->GetWorld();
	}
	// End UObject interface

	UFUNCTION(Reliable, Server, WithValidation)
	void CollectDataToReplicateOnServer(APlayerController* MyPC, AActor *SelectedActor);

	/** Set custom, additional keyboard bindings */
	virtual void BindKeyboardImput(class UInputComponent*& InputComponent);

	virtual void DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor = nullptr);

	virtual FString GetCategoryName() const { return TEXT("Basic"); };

	virtual class FDebugRenderSceneProxy* CreateSceneProxy(const UPrimitiveComponent* InComponent, UWorld* World, AActor* SelectedActor) { return nullptr; }

	void MarkRenderStateDirty() { bRenderStateDirty = true; }

	void CleanRenderStateDirtyFlag() { bRenderStateDirty = false; }

	bool IsRenderStateDirty() { return !!bRenderStateDirty; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnClassRegisterChangeEvent, class UGameplayDebuggerBaseObject*);
	static FOnClassRegisterChangeEvent& GetOnClassRegisterEvent();
	static FOnClassRegisterChangeEvent& GetOnClassUnregisterEvent();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_GenericShapeElements)
	TArray<FGameplayDebuggerShapeElement> GenericShapeElements;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedBlob)
	FGameplayDebuggerReplicatedBlob ReplicatedBlob;

	struct FReplicationDataHeader
	{
		uint32 UncompressedSize : 31;
		uint32 bUseCompression : 1;
	};

	FGameplayDebuggerReplicatedBlob LocalReplicationData;
	double DataPreparationStartTime;

	virtual void CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor);

	UFUNCTION(Reliable, Server, WithValidation)
	virtual void ServerRequestPacket(int32 Offset);

	void RequestDataReplication(const FString& DataName, const TArray<uint8>& BufferToReplicate, EGameplayDebuggerReplicate Replicate = EGameplayDebuggerReplicate::WithCompressed);
	virtual void OnDataReplicationComplited(const TArray<uint8>& ReplicatedData) {}

	UFUNCTION()
	virtual void OnRep_ReplicatedBlob();

	UFUNCTION()
	virtual void OnRep_GenericShapeElements();

private:
	int32 bRenderStateDirty : 1;
};
