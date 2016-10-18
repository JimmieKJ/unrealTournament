// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPrivatePCH.h"
#include "GameplayDebuggerLocalController.h"
#include "GameplayDebuggerPlayerManager.h"
#include "GameplayDebuggerAddonManager.h"
#include "GameplayDebuggerExtension.h"
#include "GameplayDebuggerConfig.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Selection.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/Pawn.h"

UGameplayDebuggerLocalController::UGameplayDebuggerLocalController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSimulateMode = false;
	bNeedsCleanup = false;
	bIsSelectingActor = false;
	bIsLocallyEnabled = false;
	bPrevLocallyEnabled = false;
	ActiveRowIdx = 0;
}

void UGameplayDebuggerLocalController::Initialize(AGameplayDebuggerCategoryReplicator& Replicator, AGameplayDebuggerPlayerManager& Manager)
{
	CachedReplicator = &Replicator;
	CachedPlayerManager = &Manager;
	bSimulateMode = FGameplayDebuggerAddonBase::IsSimulateInEditor();

	UDebugDrawService::Register(bSimulateMode ? TEXT("DebugAI") : TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UGameplayDebuggerLocalController::OnDebugDraw));
	if (bSimulateMode)
	{
		USelection::SelectionChangedEvent.AddUObject(this, &UGameplayDebuggerLocalController::OnSelectionChanged);
	}

	FGameplayDebuggerAddonManager& AddonManager = FGameplayDebuggerAddonManager::GetCurrent();
	AddonManager.OnCategoriesChanged.AddUObject(this, &UGameplayDebuggerLocalController::OnCategoriesChanged);
	OnCategoriesChanged();

	const UGameplayDebuggerConfig* SettingsCDO = UGameplayDebuggerConfig::StaticClass()->GetDefaultObject<UGameplayDebuggerConfig>();
	const FKey NumpadKeys[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour,
		EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	const FKey CategorySlots[] = { SettingsCDO->CategorySlot0, SettingsCDO->CategorySlot1, SettingsCDO->CategorySlot2, SettingsCDO->CategorySlot3, SettingsCDO->CategorySlot4,
		SettingsCDO->CategorySlot5, SettingsCDO->CategorySlot6, SettingsCDO->CategorySlot7, SettingsCDO->CategorySlot8, SettingsCDO->CategorySlot9 };

	bool bIsNumpadOnly = true;
	for (int32 Idx = 0; Idx < ARRAY_COUNT(CategorySlots); Idx++)
	{
		bool bHasPattern = false;
		for (int32 PatternIdx = 0; PatternIdx < ARRAY_COUNT(NumpadKeys); PatternIdx++)
		{
			if (CategorySlots[Idx] == NumpadKeys[PatternIdx])
			{
				bHasPattern = true;
				break;
			}
		}

		if (!bHasPattern)
		{
			bIsNumpadOnly = false;
			break;
		}
	}

	ActivationKeyDesc = GetKeyDescriptionLong(SettingsCDO->ActivationKey);
	RowUpKeyDesc = GetKeyDescriptionShort(SettingsCDO->CategoryRowPrevKey);
	RowDownKeyDesc = GetKeyDescriptionShort(SettingsCDO->CategoryRowNextKey);
	CategoryKeysDesc = bIsNumpadOnly ? TEXT("{yellow}Numpad{white}") : TEXT("highlighted keys");

	PaddingLeft = SettingsCDO->DebugCanvasPaddingLeft;
	PaddingRight = SettingsCDO->DebugCanvasPaddingRight;
	PaddingTop = SettingsCDO->DebugCanvasPaddingTop;
	PaddingBottom = SettingsCDO->DebugCanvasPaddingBottom;

	bNeedsCleanup = true;
}

void UGameplayDebuggerLocalController::Cleanup()
{
	if (bSimulateMode)
	{
		USelection::SelectionChangedEvent.RemoveAll(this);
	}

	bNeedsCleanup = false;
}

void UGameplayDebuggerLocalController::BeginDestroy()
{
	Super::BeginDestroy();
	if (bNeedsCleanup)
	{
		Cleanup();
	}
}

void UGameplayDebuggerLocalController::OnDebugDraw(class UCanvas* Canvas, class APlayerController* PC)
{
	if (CachedReplicator && CachedReplicator->IsEnabled())
	{
		FGameplayDebuggerCanvasContext CanvasContext(Canvas, GEngine->GetSmallFont());
		CanvasContext.CursorX = CanvasContext.DefaultX = PaddingLeft;
		CanvasContext.CursorY = CanvasContext.DefaultY = PaddingTop;

		if (bSimulateMode)
		{
			DrawSimulateHeader(CanvasContext);
		}
		else
		{
			DrawHeader(CanvasContext);
		}

		if (DataPackMap.Num() != NumCategories)
		{
			RebuildDataPackMap();
		}

		const bool bHasDebugActor = CachedReplicator->HasDebugActor();
		for (int32 Idx = 0; Idx < NumCategories; Idx++)
		{
			TSharedRef<FGameplayDebuggerCategory> Category = CachedReplicator->GetCategory(Idx);
			if (Category->ShouldDrawCategory(bHasDebugActor))
			{
				if (Category->IsCategoryHeaderVisible())
				{
					DrawCategoryHeader(Idx, Category, CanvasContext);
				}

				Category->DrawCategory(CachedReplicator->GetReplicationOwner(), CanvasContext);
			}
		}
	}
}

extern RENDERCORE_API FTexture* GWhiteTexture;

void UGameplayDebuggerLocalController::DrawHeader(FGameplayDebuggerCanvasContext& CanvasContext)
{
	const int32 NumRows = (NumCategorySlots + 9) / 10;
	const float LineHeight = CanvasContext.GetLineHeight();
	const int32 NumExtensions = CachedReplicator->GetNumExtensions();
	const int32 NumExtensionRows = (NumExtensions > 0) ? 1 : 0;
	const float CanvasSizeX = CanvasContext.Canvas->SizeX - PaddingLeft - PaddingRight;
	
	const float BackgroundPadding = 5.0f;
	const float BackgroundPaddingBothSides = BackgroundPadding * 2.0f;

	if (NumRows > 1)
	{
		FCanvasTileItem TileItemUpper(FVector2D(0, 0), GWhiteTexture, FVector2D(CanvasSizeX + BackgroundPaddingBothSides, (LineHeight * (ActiveRowIdx + NumExtensionRows + 1)) + BackgroundPadding), FLinearColor(0, 0, 0, 0.2f));
		FCanvasTileItem ActiveRowTileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(CanvasSizeX + BackgroundPaddingBothSides, LineHeight), FLinearColor(0, 0.5f, 0, 0.3f));
		FCanvasTileItem TileItemLower(FVector2D(0, 0), GWhiteTexture, FVector2D(CanvasSizeX + BackgroundPaddingBothSides, LineHeight * ((NumRows - ActiveRowIdx - 1)) + BackgroundPadding), FLinearColor(0, 0, 0, 0.2f));

		TileItemUpper.BlendMode = SE_BLEND_Translucent;
		ActiveRowTileItem.BlendMode = SE_BLEND_Translucent;
		TileItemLower.BlendMode = SE_BLEND_Translucent;

		CanvasContext.DrawItem(TileItemUpper, PaddingLeft - BackgroundPadding, PaddingTop - BackgroundPadding);
		CanvasContext.DrawItem(ActiveRowTileItem, PaddingLeft - BackgroundPadding, PaddingTop - BackgroundPadding + TileItemUpper.Size.Y);
		CanvasContext.DrawItem(TileItemLower, PaddingLeft - BackgroundPadding, PaddingTop - BackgroundPadding + TileItemUpper.Size.Y + ActiveRowTileItem.Size.Y);
	}
	else
	{
		FCanvasTileItem TileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(CanvasSizeX, LineHeight * (NumRows + NumExtensionRows + 1)) + BackgroundPaddingBothSides, FLinearColor(0, 0, 0, 0.2f));
		TileItem.BlendMode = SE_BLEND_Translucent;
		CanvasContext.DrawItem(TileItem, PaddingLeft - BackgroundPadding, PaddingTop - BackgroundPadding);
	}

	CanvasContext.Printf(TEXT("Tap {yellow}%s{white} to close, use %s to toggle catories."), *ActivationKeyDesc, *CategoryKeysDesc);

	const FString DebugActorDesc = FString::Printf(TEXT("Debug actor: {cyan}%s"), *CachedReplicator->GetDebugActorName().ToString());
	float DebugActorSizeX = 0.0f, DebugActorSizeY = 0.0f;
	CanvasContext.MeasureString(DebugActorDesc, DebugActorSizeX, DebugActorSizeY);
	CanvasContext.PrintAt(CanvasContext.Canvas->SizeX - PaddingRight - DebugActorSizeX, PaddingTop, DebugActorDesc);

	const FString TimestampDesc = FString::Printf(TEXT("Time: %.2fs"), CachedReplicator->GetWorld()->GetTimeSeconds());
	float TimestampSizeX = 0.0f, TimestampSizeY = 0.0f;
	CanvasContext.MeasureString(TimestampDesc, TimestampSizeX, TimestampSizeY);
	CanvasContext.PrintAt((CanvasSizeX - TimestampSizeX) * 0.5f, PaddingTop, TimestampDesc);

	if (NumRows > 1)
	{
		const FString ChangeRowDesc = FString::Printf(TEXT("Prev row: {yellow}%s\n{white}Next row: {yellow}%s"), *RowUpKeyDesc, *RowDownKeyDesc);
		float RowDescSizeX = 0.0f, RowDescSizeY = 0.0f;
		CanvasContext.MeasureString(ChangeRowDesc, RowDescSizeX, RowDescSizeY);
		CanvasContext.PrintAt(CanvasContext.Canvas->SizeX - PaddingRight - RowDescSizeX, PaddingTop + LineHeight * (NumExtensionRows + 1), ChangeRowDesc);
	}

	if (NumExtensionRows)
	{
		FString ExtensionRowDesc;
		for (int32 ExtensionIdx = 0; ExtensionIdx < NumExtensions; ExtensionIdx++)
		{
			TSharedRef<FGameplayDebuggerExtension> Extension = CachedReplicator->GetExtension(ExtensionIdx);
			FString ExtensionDesc = Extension->GetDescription();
			ExtensionDesc.ReplaceInline(TEXT("\n"), TEXT(""));

			if (ExtensionDesc.Len())
			{
				if (ExtensionRowDesc.Len())
				{
					ExtensionRowDesc += FGameplayDebuggerCanvasStrings::SeparatorSpace;
				}

				ExtensionRowDesc += ExtensionDesc;
			}
		}

		CanvasContext.Print(ExtensionRowDesc);
	}

	for (int32 RowIdx = 0; RowIdx < NumRows; RowIdx++)
	{
		FString CategoryRowDesc;
		for (int32 Idx = 0; Idx < 10; Idx++)
		{
			const int32 CategorySlotIdx = (RowIdx * 10) + Idx;
			if (SlotCategoryIds.IsValidIndex(CategorySlotIdx) && 
				SlotNames.IsValidIndex(CategorySlotIdx) &&
				SlotCategoryIds[CategorySlotIdx].Num())
			{
				TSharedRef<FGameplayDebuggerCategory> Category0 = CachedReplicator->GetCategory(SlotCategoryIds[CategorySlotIdx][0]);
				const bool bIsEnabled = Category0->IsCategoryEnabled();
				const FString CategoryColorName = (RowIdx == ActiveRowIdx) && (NumRows > 1) ?
					(bIsEnabled ? *FGameplayDebuggerCanvasStrings::ColorNameEnabledActiveRow : *FGameplayDebuggerCanvasStrings::ColorNameDisabledActiveRow) :
					(bIsEnabled ? *FGameplayDebuggerCanvasStrings::ColorNameEnabled : *FGameplayDebuggerCanvasStrings::ColorNameDisabled);

				const FString CategoryDesc = (RowIdx == ActiveRowIdx) ?
					FString::Printf(TEXT("%s{%s}%d:{%s}%s"),
						Idx ? *FGameplayDebuggerCanvasStrings::SeparatorSpace : TEXT(""),
						*FGameplayDebuggerCanvasStrings::ColorNameInput,
						Idx,
						*CategoryColorName,
						*SlotNames[CategorySlotIdx]) :
					FString::Printf(TEXT("%s{%s}%s"),
						Idx ? *FGameplayDebuggerCanvasStrings::Separator : TEXT(""),
						*CategoryColorName,
						*SlotNames[CategorySlotIdx]);

				CategoryRowDesc += CategoryDesc;
			}
		}

		CanvasContext.Print(CategoryRowDesc);
	}

	CanvasContext.DefaultY = CanvasContext.CursorY + LineHeight;
}

void UGameplayDebuggerLocalController::DrawSimulateHeader(FGameplayDebuggerCanvasContext& CanvasContext)
{
	const float LineHeight = CanvasContext.GetLineHeight();
	const float NewCategoryPosY = (CanvasContext.DefaultY * 2.0f) + (LineHeight * 3.0f);
	const float BackgroundPadding = 5.0f;

	const FString TimestampDesc = FString::Printf(TEXT("GameplayDebugger time: %.2fs"), CachedReplicator->GetWorld()->GetTimeSeconds());
	float TimestampSizeX = 0.0f, TimestampSizeY = 0.0f;
	CanvasContext.MeasureString(TimestampDesc, TimestampSizeX, TimestampSizeY);

	const FString DebugActorDesc = FString::Printf(TEXT("Selected actor: {cyan}%s"), *CachedReplicator->GetDebugActorName().ToString());
	float DebugActorSizeX = 0.0f, DebugActorSizeY = 0.0f;
	CanvasContext.MeasureString(DebugActorDesc, DebugActorSizeX, DebugActorSizeY);

	const float BackgroundSizeX = FMath::Max(DebugActorSizeX, TimestampSizeX) + (BackgroundPadding * 2.f);
	FCanvasTileItem TileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(BackgroundSizeX, (2.0f * LineHeight) + (2.0f * BackgroundPadding)), FLinearColor(0, 0, 0, 0.2f));
	TileItem.BlendMode = SE_BLEND_Translucent;
	CanvasContext.DrawItem(TileItem, (CanvasContext.Canvas->SizeX - BackgroundSizeX) * 0.5f, PaddingTop - BackgroundPadding);

	CanvasContext.PrintAt((CanvasContext.Canvas->SizeX - TimestampSizeX) * 0.5f, PaddingTop, TimestampDesc);
	CanvasContext.PrintAt((CanvasContext.Canvas->SizeX - DebugActorSizeX) * 0.5f, PaddingTop + LineHeight, DebugActorDesc);

	CanvasContext.DefaultY = CanvasContext.CursorY = NewCategoryPosY;
}

void UGameplayDebuggerLocalController::DrawCategoryHeader(int32 CategoryId, TSharedRef<FGameplayDebuggerCategory> Category, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FString DataPackDesc;
	
	if (DataPackMap.IsValidIndex(CategoryId) &&
		!Category->IsCategoryAuth() &&
		!Category->ShouldDrawReplicationStatus() &&
		Category->GetNumDataPacks() > 0)
	{
		// collect brief data pack status, detailed info is displayed only when ShouldDrawReplicationStatus is true
		const int32 CurrentSyncCounter = CachedReplicator->GetDebugActorCounter();

		DataPackDesc = TEXT("{white} ver[");
		bool bIsPrevOutdated = false;
		bool bAddSeparator = false;

		for (int32 Idx = 0; Idx < DataPackMap[CategoryId].Num(); Idx++)
		{
			TSharedRef<FGameplayDebuggerCategory> MappedCategory = CachedReplicator->GetCategory(DataPackMap[CategoryId][Idx]);
			for (int32 DataPackIdx = 0; DataPackIdx < MappedCategory->GetNumDataPacks(); DataPackIdx++)
			{
				FGameplayDebuggerDataPack::FHeader DataHeader = MappedCategory->GetDataPackHeaderCopy(DataPackIdx);
				const bool bIsOutdated = (DataHeader.SyncCounter != CurrentSyncCounter);

				if (bAddSeparator)
				{
					DataPackDesc += TEXT(';');
				}

				if (bIsOutdated != bIsPrevOutdated)
				{
					DataPackDesc += bIsOutdated ? TEXT("{red}") : TEXT("{white}");
					bIsPrevOutdated = bIsOutdated;
				}

				DataPackDesc += TTypeToString<int16>::ToString(DataHeader.DataVersion);
				bAddSeparator = true;
			}
		}

		if (bIsPrevOutdated)
		{
			DataPackDesc += TEXT("{white}");
		}

		DataPackDesc += TEXT(']');
	}

	CanvasContext.MoveToNewLine();
	CanvasContext.Printf(FColor::Green, TEXT("[CATEGORY: %s]%s"), *Category->GetCategoryName().ToString(), *DataPackDesc);
}

void UGameplayDebuggerLocalController::BindInput(UInputComponent& InputComponent)
{
	TSet<FName> UsedBindings;

	const UGameplayDebuggerConfig* SettingsCDO = UGameplayDebuggerConfig::StaticClass()->GetDefaultObject<UGameplayDebuggerConfig>();
	InputComponent.BindKey(SettingsCDO->ActivationKey, IE_Pressed, this, &UGameplayDebuggerLocalController::OnActivationPressed);
	InputComponent.BindKey(SettingsCDO->ActivationKey, IE_Released, this, &UGameplayDebuggerLocalController::OnActivationReleased);
	UsedBindings.Add(SettingsCDO->ActivationKey.GetFName());

	if (bIsLocallyEnabled)
	{
		InputComponent.BindKey(SettingsCDO->CategorySlot0, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory0Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot1, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory1Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot2, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory2Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot3, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory3Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot4, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory4Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot5, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory5Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot6, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory6Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot7, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory7Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot8, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory8Pressed);
		InputComponent.BindKey(SettingsCDO->CategorySlot9, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategory9Pressed);

		InputComponent.BindKey(SettingsCDO->CategoryRowPrevKey, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategoryRowUpPressed);
		InputComponent.BindKey(SettingsCDO->CategoryRowNextKey, IE_Pressed, this, &UGameplayDebuggerLocalController::OnCategoryRowDownPressed);

		UsedBindings.Add(SettingsCDO->CategorySlot0.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot1.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot2.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot3.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot4.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot5.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot6.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot7.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot8.GetFName());
		UsedBindings.Add(SettingsCDO->CategorySlot9.GetFName());
		UsedBindings.Add(SettingsCDO->CategoryRowPrevKey.GetFName());
		UsedBindings.Add(SettingsCDO->CategoryRowNextKey.GetFName());

		for (int32 Idx = 0; Idx < NumCategories; Idx++)
		{
			TSharedRef<FGameplayDebuggerCategory> Category = CachedReplicator->GetCategory(Idx);
			const int32 NumInputHandlers = Category->GetNumInputHandlers();

			for (int32 HandlerIdx = 0; HandlerIdx < NumInputHandlers; HandlerIdx++)
			{
				FGameplayDebuggerInputHandler& HandlerData = Category->GetInputHandler(HandlerIdx);
				if (HandlerData.Modifier.bPressed || HandlerData.Modifier.bReleased)
				{
					FInputChord InputChord(FKey(HandlerData.KeyName), HandlerData.Modifier.bShift, HandlerData.Modifier.bCtrl, HandlerData.Modifier.bAlt, HandlerData.Modifier.bCmd);
					FInputKeyBinding NewBinding(InputChord, HandlerData.Modifier.bPressed ? IE_Pressed : IE_Released);
					NewBinding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &UGameplayDebuggerLocalController::OnCategoryBindingEvent, Idx, HandlerIdx);

					InputComponent.KeyBindings.Add(NewBinding);
					UsedBindings.Add(HandlerData.KeyName);
				}
			}
		}

		const int32 NumExtentions = CachedReplicator->GetNumExtensions();
		for (int32 Idx = 0; Idx < NumExtentions; Idx++)
		{
			TSharedRef<FGameplayDebuggerExtension> Extension = CachedReplicator->GetExtension(Idx);
			const int32 NumInputHandlers = Extension->GetNumInputHandlers();

			for (int32 HandlerIdx = 0; HandlerIdx < NumInputHandlers; HandlerIdx++)
			{
				FGameplayDebuggerInputHandler& HandlerData = Extension->GetInputHandler(HandlerIdx);
				if (HandlerData.Modifier.bPressed || HandlerData.Modifier.bReleased)
				{
					FInputChord InputChord(FKey(HandlerData.KeyName), HandlerData.Modifier.bShift, HandlerData.Modifier.bCtrl, HandlerData.Modifier.bAlt, HandlerData.Modifier.bCmd);
					FInputKeyBinding NewBinding(InputChord, HandlerData.Modifier.bPressed ? IE_Pressed : IE_Released);
					NewBinding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &UGameplayDebuggerLocalController::OnExtensionBindingEvent, Idx, HandlerIdx);

					InputComponent.KeyBindings.Add(NewBinding);
					UsedBindings.Add(HandlerData.KeyName);
				}
			}
		}
	}

	if (CachedReplicator && CachedReplicator->GetReplicationOwner() && CachedReplicator->GetReplicationOwner()->PlayerInput)
	{
		TSet<FName> RemovedMasks = MaskedDebugBindings.Difference(UsedBindings);
		TSet<FName> AddedMasks = UsedBindings.Difference(MaskedDebugBindings);

		UPlayerInput* Input = CachedReplicator->GetReplicationOwner()->PlayerInput;
		for (int32 Idx = 0; Idx < Input->DebugExecBindings.Num(); Idx++)
		{
			FKeyBind& DebugBinding = Input->DebugExecBindings[Idx];
			const bool bRemoveMask = RemovedMasks.Contains(DebugBinding.Key.GetFName());
			const bool bAddMask = AddedMasks.Contains(DebugBinding.Key.GetFName());

			if (bAddMask || bRemoveMask)
			{
				DebugBinding.bDisabled = bAddMask;
			}
		}

		MaskedDebugBindings = UsedBindings;
	}
}

void UGameplayDebuggerLocalController::OnActivationPressed()
{
	bPrevLocallyEnabled = bIsLocallyEnabled;
	if (CachedReplicator)
	{
		CachedReplicator->GetWorldTimerManager().SetTimer(StartSelectingActorHandle, this, &UGameplayDebuggerLocalController::OnStartSelectingActor, 0.2f);
	}
}

void UGameplayDebuggerLocalController::OnActivationReleased()
{
	if (CachedReplicator)
	{
		UWorld* World = CachedReplicator->GetWorld();
		if (StartSelectingActorHandle.IsValid())
		{
			bIsLocallyEnabled = !CachedReplicator->IsEnabled();
			CachedReplicator->SetEnabled(bIsLocallyEnabled);

			if (bIsLocallyEnabled)
			{
				DebugActorCandidate = nullptr;
				OnSelectActorTick();
			}
		}

		World->GetTimerManager().ClearTimer(StartSelectingActorHandle);
		World->GetTimerManager().ClearTimer(SelectActorTickHandle);

		CachedReplicator->MarkComponentsRenderStateDirty();
	}

	StartSelectingActorHandle.Invalidate();
	SelectActorTickHandle.Invalidate();
	bIsSelectingActor = false;

	if (CachedReplicator && (bPrevLocallyEnabled != bIsLocallyEnabled))
	{
		CachedPlayerManager->RefreshInputBindings(*CachedReplicator);
	}
}

void UGameplayDebuggerLocalController::OnCategory0Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 0);
}

void UGameplayDebuggerLocalController::OnCategory1Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 1);
}

void UGameplayDebuggerLocalController::OnCategory2Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 2);
}

void UGameplayDebuggerLocalController::OnCategory3Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 3);
}

void UGameplayDebuggerLocalController::OnCategory4Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 4);
}

void UGameplayDebuggerLocalController::OnCategory5Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 5);
}

void UGameplayDebuggerLocalController::OnCategory6Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 6);
}

void UGameplayDebuggerLocalController::OnCategory7Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 7);
}

void UGameplayDebuggerLocalController::OnCategory8Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 8);
}

void UGameplayDebuggerLocalController::OnCategory9Pressed()
{
	ToggleSlotState((ActiveRowIdx * 10) + 9);
}

void UGameplayDebuggerLocalController::OnCategoryRowUpPressed()
{
	const int32 NumRows = (NumCategorySlots + 9) / 10;
	ActiveRowIdx = (NumRows > 1) ? ((ActiveRowIdx + NumRows - 1) % NumRows) : 0;
}

void UGameplayDebuggerLocalController::OnCategoryRowDownPressed()
{
	const int32 NumRows = (NumCategorySlots + 9) / 10;
	ActiveRowIdx = (NumRows > 1) ? ((ActiveRowIdx + 1) % NumRows) : 0;
}

void UGameplayDebuggerLocalController::OnCategoryBindingEvent(int32 CategoryId, int32 HandlerId)
{
	if (CachedReplicator)
	{
		CachedReplicator->SendCategoryInputEvent(CategoryId, HandlerId);
	}
}

void UGameplayDebuggerLocalController::OnExtensionBindingEvent(int32 ExtensionId, int32 HandlerId)
{
	if (CachedReplicator)
	{
		CachedReplicator->SendExtensionInputEvent(ExtensionId, HandlerId);
	}
}

void UGameplayDebuggerLocalController::OnStartSelectingActor()
{
	StartSelectingActorHandle.Invalidate();
	if (CachedReplicator)
	{
		if (!CachedReplicator->IsEnabled())
		{
			bIsLocallyEnabled = true;
			CachedReplicator->SetEnabled(bIsLocallyEnabled);
		}

		bIsSelectingActor = true;
		DebugActorCandidate = nullptr;

		const bool bLooping = true;
		CachedReplicator->GetWorldTimerManager().SetTimer(SelectActorTickHandle, this, &UGameplayDebuggerLocalController::OnSelectActorTick, 0.01f, bLooping);

		OnSelectActorTick();
	}
}

void UGameplayDebuggerLocalController::OnSelectActorTick()
{
	APlayerController* OwnerPC = CachedReplicator ? CachedReplicator->GetReplicationOwner() : nullptr;
	if (OwnerPC)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		OwnerPC->GetPlayerViewPoint(CameraLocation, CameraRotation);

		// TODO: move to module's settings
		const float MaxScanDistance = 25000.0f;
		const float MinViewDirDot = 0.8f;

		AActor* BestCandidate = nullptr;
		float BestScore = MinViewDirDot;
		
		const FVector ViewDir = CameraRotation.Vector();
		for (FConstPawnIterator It = OwnerPC->GetWorld()->GetPawnIterator(); It; ++It)
		{
			APawn* TestPawn = *It;
			if (TestPawn && !TestPawn->bHidden && TestPawn->GetActorEnableCollision() && TestPawn != OwnerPC->GetPawn())
			{
				FVector DirToPawn = (TestPawn->GetActorLocation() - CameraLocation);
				float DistToPawn = DirToPawn.Size();
				if (FMath::IsNearlyZero(DistToPawn))
				{
					DirToPawn = ViewDir;
					DistToPawn = 1.0f;
				}
				else
				{
					DirToPawn /= DistToPawn;
				}

				const float ViewDot = FVector::DotProduct(ViewDir, DirToPawn);
				if (DistToPawn < MaxScanDistance && ViewDot > BestScore)
				{
					BestScore = ViewDot;
					BestCandidate = TestPawn;
				}
			}
		}

		// cache to avoid multiple RPC with the same actor
		if (DebugActorCandidate != BestCandidate)
		{
			DebugActorCandidate = BestCandidate;
			CachedReplicator->SetDebugActor(BestCandidate);
		}
	}
}

void UGameplayDebuggerLocalController::ToggleSlotState(int32 SlotIdx)
{
	if (CachedReplicator && SlotCategoryIds.IsValidIndex(SlotIdx) && SlotCategoryIds[SlotIdx].Num())
	{
		const bool bIsEnabled = CachedReplicator->IsCategoryEnabled(SlotCategoryIds[SlotIdx][0]);
		for (int32 Idx = 0; Idx < SlotCategoryIds[SlotIdx].Num(); Idx++)
		{
			const int32 CategoryId = SlotCategoryIds[SlotIdx][Idx];
			CachedReplicator->SetCategoryEnabled(CategoryId, !bIsEnabled);
		}

		CachedReplicator->MarkComponentsRenderStateDirty();
	}
}

FString UGameplayDebuggerLocalController::GetKeyDescriptionShort(const FKey& KeyBind) const
{
	return FString::Printf(TEXT("[%s]"), *KeyBind.GetFName().ToString());
}

FString UGameplayDebuggerLocalController::GetKeyDescriptionLong(const FKey& KeyBind) const
{
	const FString KeyDisplay = KeyBind.GetDisplayName().ToString();
	const FString KeyName = KeyBind.GetFName().ToString();
	return (KeyDisplay == KeyName) ? FString::Printf(TEXT("[%s]"), *KeyDisplay) : FString::Printf(TEXT("%s [%s key])"), *KeyDisplay, *KeyName);
}

void UGameplayDebuggerLocalController::OnSelectionChanged(UObject* Object)
{
	USelection* Selection = Cast<USelection>(Object);
	if (Selection && CachedReplicator)
	{
		APawn* SelectedPawn = nullptr;
		for (int32 Idx = 0; Idx < Selection->Num(); Idx++)
		{
			AController* SelectedController = Cast<AController>(Selection->GetSelectedObject(Idx));
			SelectedPawn = SelectedController ? SelectedController->GetPawn() : Cast<APawn>(Selection->GetSelectedObject(Idx));
			if (SelectedPawn)
			{
				break;
			}
		}

		CachedReplicator->SetDebugActor(SelectedPawn);
	}
}

void UGameplayDebuggerLocalController::OnCategoriesChanged()
{
	FGameplayDebuggerAddonManager& AddonManager = FGameplayDebuggerAddonManager::GetCurrent();

	SlotNames.Reset();
	SlotNames.Append(AddonManager.GetSlotNames());

	// categories are already sorted using AddonManager.SlotMap, build Slot to Category Id map accordingly
	const TArray< TArray<int32> >& SlotMap = AddonManager.GetSlotMap();
	SlotCategoryIds.Reset();
	SlotCategoryIds.AddDefaulted(SlotMap.Num());

	int32 CategoryId = 0;
	for (int32 SlotIdx = 0; SlotIdx < SlotMap.Num(); SlotIdx++)
	{
		for (int32 InnerIdx = 0; InnerIdx < SlotMap[SlotIdx].Num(); InnerIdx++)
		{
			SlotCategoryIds[SlotIdx].Add(CategoryId);
			CategoryId++;
		}
	}

	NumCategorySlots = SlotCategoryIds.Num();
	NumCategories = AddonManager.GetNumCategories();

	DataPackMap.Reset();
}

void UGameplayDebuggerLocalController::RebuildDataPackMap()
{
	DataPackMap.SetNum(NumCategories);
	
	// category: get all categories from slot and combine data pack data if category header is not displayed
	for (int32 SlotIdx = 0; SlotIdx < NumCategorySlots; SlotIdx++)
	{
		TArray<int32> NoHeaderCategories;
		int32 FirstVisibleCategoryId = INDEX_NONE;

		for (int32 InnerIdx = 0; InnerIdx < SlotCategoryIds[SlotIdx].Num(); InnerIdx++)
		{
			const int32 CategoryId = SlotCategoryIds[SlotIdx][InnerIdx];
			
			TSharedRef<FGameplayDebuggerCategory> Category = CachedReplicator->GetCategory(CategoryId);
			if (!Category->IsCategoryHeaderVisible())
			{
				NoHeaderCategories.Add(CategoryId);
			}
			else
			{
				DataPackMap[CategoryId].Add(CategoryId);
				
				if (FirstVisibleCategoryId == INDEX_NONE)
				{
					FirstVisibleCategoryId = CategoryId;
				}
			}
		}

		if ((FirstVisibleCategoryId != INDEX_NONE) && NoHeaderCategories.Num())
		{
			DataPackMap[FirstVisibleCategoryId].Append(NoHeaderCategories);
		}
	}
}
