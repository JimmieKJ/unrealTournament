// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Visibility.h"
#include "SlateBasics.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "WorkflowOrientedApp/SContentReference.h"
#include "SCheckBox.h"
#include "SNameComboBox.h"
#include "AssetThumbnail.h"
#include "SNiagaraEffectEditorViewport.h"
#include "Components/NiagaraComponent.h"
#include "Engine/NiagaraEffect.h"
#include "Engine/NiagaraSimulation.h"
#include "Engine/NiagaraEffectRenderer.h"
#include "ComponentReregisterContext.h"
#include "SNumericEntryBox.h"

#define NGED_SECTION_BORDER SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(2.0f).HAlign(HAlign_Left)
#define NGED_SECTION_LIGHTBORDER SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.LightGroupBorder")).Padding(2.0f).HAlign(HAlign_Left)
#define NGED_SECTION_DARKBORDER SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder")).Padding(2.0f).HAlign(HAlign_Left)

const FLinearColor RedCol(0.5, 0.1, 0.1);
const FLinearColor GreenCol(0.1, 0.5, 0.1);
const FLinearColor BlueCol(0.1, 0.1, 0.5);
const FLinearColor GrayCol(0.3, 0.3, 0.3);

class SVectorConstantWidget : public SCompoundWidget, public FNotifyHook
{
private:	
	FName ConstantName;
	TSharedPtr<FNiagaraSimulation> Emitter;

	//SNumericEntryBox<float> XText, YText, ZText, WText;

public:
	SLATE_BEGIN_ARGS(SVectorConstantWidget) :
	_Emitter(nullptr),
	_ConstantName("Undefined")
	{
	}
	SLATE_ARGUMENT(TSharedPtr<FNiagaraSimulation>, Emitter)
	SLATE_ARGUMENT(FName, ConstantName)
	SLATE_END_ARGS()

	TOptional<float> GetConstX() const	
	{
		FVector4 *ConstPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (ConstPtr)
		{
			return ConstPtr->X;
		}
		return 0.0f;
	}

	TOptional<float> GetConstY() const
	{
		FVector4 *ConstPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (ConstPtr)
		{
			return ConstPtr->Y;
		}
		return 0.0f;
	}

	TOptional<float> GetConstZ() const
	{
		FVector4 *ConstPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (ConstPtr)
		{
			return ConstPtr->Z;
		}
		return 0.0f;
	}

	TOptional<float> GetConstW() const
	{
		FVector4 *ConstPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (ConstPtr)
		{
			return ConstPtr->W;
		}
		return 0.0f;
	}

	void OnConstantChangedX(float InVal)	{ OnConstantChangedX(InVal, ETextCommit::Default); }
	void OnConstantChangedY(float InVal)	{ OnConstantChangedY(InVal, ETextCommit::Default); }
	void OnConstantChangedZ(float InVal)	{ OnConstantChangedZ(InVal, ETextCommit::Default); }
	void OnConstantChangedW(float InVal)	{ OnConstantChangedW(InVal, ETextCommit::Default); }

	void OnConstantChangedX(float InVal, ETextCommit::Type Type)
	{
		FVector4 Constant(InVal, GetConstY().GetValue(), GetConstZ().GetValue(), GetConstW().GetValue());
		Emitter->GetProperties()->ExternalConstants.SetOrAdd(ConstantName, Constant);
		/*
		FVector4 *VecPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (VecPtr)
		{
			VecPtr->X = InVal;
		}
		*/
	}

	void OnConstantChangedY(float InVal, ETextCommit::Type Type)
	{
		FVector4 Constant(GetConstX().GetValue(), InVal, GetConstZ().GetValue(), GetConstW().GetValue());
		Emitter->GetProperties()->ExternalConstants.SetOrAdd(ConstantName, Constant);
		/*
		FVector4 *VecPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (VecPtr)
		{
			VecPtr->Y = InVal;
		}
		*/
	}

	void OnConstantChangedZ(float InVal, ETextCommit::Type Type)
	{
		FVector4 Constant(GetConstX().GetValue(), GetConstY().GetValue(), InVal, GetConstW().GetValue());
		Emitter->GetProperties()->ExternalConstants.SetOrAdd(ConstantName, Constant);
		/*
		FVector4 *VecPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (VecPtr)
		{
			VecPtr->Z = InVal;
		}
		*/
	}

	void OnConstantChangedW(float InVal, ETextCommit::Type Type)
	{
		FVector4 Constant(GetConstX().GetValue(), GetConstY().GetValue(), GetConstZ().GetValue(), InVal);
		Emitter->GetProperties()->ExternalConstants.SetOrAdd(ConstantName, Constant);
		/*
		FVector4 *VecPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		if (VecPtr)
		{
			VecPtr->W = InVal;
		}
		*/
	}

	void Construct(const FArguments& InArgs)
	{
		Emitter = InArgs._Emitter;
		ConstantName = InArgs._ConstantName;

		FVector4 *ConstPtr = Emitter->GetProperties()->ExternalConstants.FindVector(ConstantName);
		FVector4 ConstVal;
		if (ConstPtr)
		{
			ConstVal = *ConstPtr;
		}
		
		this->ChildSlot
		[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(4)[SNew(STextBlock).Text(FText::FromName(ConstantName))]
		+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(4)[SNew(SNumericEntryBox<float>).Value(this, &SVectorConstantWidget::GetConstX)
		.OnValueCommitted(this, &SVectorConstantWidget::OnConstantChangedX).OnValueChanged(this, &SVectorConstantWidget::OnConstantChangedX)
			.Label()
			[
				SNumericEntryBox<float>::BuildLabel(FText::FromString("X"), FLinearColor::White, RedCol)
			]
		]

			+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(4)[SNew(SNumericEntryBox<float>).Value(this, &SVectorConstantWidget::GetConstY)
			.OnValueCommitted(this, &SVectorConstantWidget::OnConstantChangedY).OnValueChanged(this, &SVectorConstantWidget::OnConstantChangedY)
			.Label()
			[
				SNumericEntryBox<float>::BuildLabel(FText::FromString("Y"), FLinearColor::White, GreenCol)
			]
		]

			+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(4)[SNew(SNumericEntryBox<float>).Value(this, &SVectorConstantWidget::GetConstZ)
			.OnValueCommitted(this, &SVectorConstantWidget::OnConstantChangedZ).OnValueChanged(this, &SVectorConstantWidget::OnConstantChangedZ)
			.Label()
			[
				SNumericEntryBox<float>::BuildLabel(FText::FromString("Z"), FLinearColor::White, BlueCol)
			]
		]

			+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(4)[SNew(SNumericEntryBox<float>).Value(this, &SVectorConstantWidget::GetConstW)
			.OnValueCommitted(this, &SVectorConstantWidget::OnConstantChangedW).OnValueChanged(this, &SVectorConstantWidget::OnConstantChangedW)
			.Label()
			[
				SNumericEntryBox<float>::BuildLabel(FText::FromString("W"), FLinearColor::White, GrayCol)
			]
		]
		];
		
	}
};


class SEmitterWidget : public SCompoundWidget, public FNotifyHook
{
private:
	TSharedPtr<FNiagaraSimulation> Emitter;
	FNiagaraEffectInstance *EffectInstance;
	TSharedPtr<STextBlock> UpdateComboText, SpawnComboText;
	TSharedPtr<SComboButton> UpdateCombo, SpawnCombo;
	TSharedPtr<SContentReference> UpdateScriptSelector, SpawnScriptSelector;
	TSharedPtr<SWidget> ThumbnailWidget;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> MaterialThumbnail;
	UNiagaraScript *CurUpdateScript, *CurSpawnScript;
	UMaterial *CurMaterial;

	TSharedPtr<IDetailsView> RendererPropertiesView;

	TArray<TSharedPtr<FString> > RenderModuleList;

	TSharedPtr< SListView<TSharedPtr<EditorExposedVectorConstant>>> UpdateScriptConstantList;
	SVerticalBox::FSlot *UpdateScriptConstantListSlot;
	TSharedPtr< SListView<TSharedPtr<EditorExposedVectorConstant>>> SpawnScriptConstantList;
	SVerticalBox::FSlot *SpawnScriptConstantListSlot;

public:
	SLATE_BEGIN_ARGS(SEmitterWidget) :
		_Emitter(nullptr)
	{
	}
	SLATE_ARGUMENT(TSharedPtr<FNiagaraSimulation>, Emitter)
	SLATE_ARGUMENT(FNiagaraEffectInstance*, Effect)
	SLATE_END_ARGS()

	TSharedRef<ITableRow> OnGenerateConstantListRow(TSharedPtr<EditorExposedVectorConstant> InItem, const TSharedRef< STableViewBase >& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Content()
		[
			SNew(SVectorConstantWidget).ConstantName(InItem->ConstName).Emitter(Emitter)
		];
	}


	void OnEmitterNameChanged(const FText &InText)
	{
		Emitter->GetProperties()->Name = InText.ToString();
	}

	FText GetEmitterName() const
	{
		return FText::FromString(Emitter->GetProperties()->Name);
	}

	void OnUpdateScriptSelectedFromPicker(UObject *Asset);
	void OnSpawnScriptSelectedFromPicker(UObject *Asset);

	void OnMaterialSelected(UObject *Asset)
	{
		MaterialThumbnail->SetAsset(Asset);
		MaterialThumbnail->RefreshThumbnail();
		MaterialThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		ThumbnailPool->Tick(0);
		CurMaterial = Cast<UMaterial>(Asset);
		Emitter->GetEffectRenderer()->SetMaterial(CurMaterial, ERHIFeatureLevel::SM5);
		Emitter->GetProperties()->Material = CurMaterial;
		TComponentReregisterContext<UNiagaraComponent> ComponentReregisterContext;
	}

	UObject *GetMaterial() const
	{
		return Emitter->GetProperties()->Material;
	}

	UObject *GetUpdateScript() const
	{
		return Emitter->GetProperties()->UpdateScript;
	}

	UObject *GetSpawnScript() const
	{
		return Emitter->GetProperties()->SpawnScript;
	}

	void OnEmitterEnabledChanged(ECheckBoxState NewCheckedState)
	{
		const bool bNewEnabledState = (NewCheckedState == ECheckBoxState::Checked);
		Emitter->GetProperties()->bIsEnabled = bNewEnabledState;
	}

	ECheckBoxState IsEmitterEnabled() const
	{
		return Emitter->IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnRenderModuleChanged(TSharedPtr<FString> ModName, ESelectInfo::Type SelectInfo);


	void OnSpawnRateChanged(const FText &InText)
	{
		float Rate = FCString::Atof(*InText.ToString());
		Emitter->GetProperties()->SpawnRate = Rate;
	}

	FText GetSpawnRateText() const
	{
		TCHAR Txt[32];
		//FCString::Snprintf(Txt, 32, TEXT("%f"), Emitter->GetSpawnRate() );
		FCString::Snprintf(Txt, 32, TEXT("%f"), Emitter->GetProperties()->SpawnRate );

		return FText::FromString(Txt);
	}

	FString GetRenderModuleText() const
	{
		if (Emitter->GetProperties()->RenderModuleType == RMT_None)
		{
			return FString("None");
		}

		return *RenderModuleList[Emitter->GetProperties()->RenderModuleType-1];
	}

	TSharedRef<SWidget>GenerateRenderModuleComboWidget(TSharedPtr<FString> WidgetString )
	{
		FString Txt = *WidgetString;
		return SNew(STextBlock).Text(Txt);
	}


	FString GetStatsText() const
	{
		TCHAR Txt[128];
		FCString::Snprintf(Txt, 128, TEXT("%i Particles  |  %.2f ms  |  %.1f MB"), Emitter->GetNumParticles(), Emitter->GetTotalCPUTime(), Emitter->GetTotalBytesUsed()/(1024.0*1024.0));
		return FString(Txt);
	}

	void Construct(const FArguments& InArgs);




};




class SNiagaraEffectEditorWidget : public SCompoundWidget, public FNotifyHook
{
private:
	UNiagaraEffect *EffectObj;
	FNiagaraEffectInstance *EffectInstance;

	/** Notification list to pass messages to editor users  */
	TSharedPtr<SNotificationList> NotificationListPtr;
	SOverlay::FOverlaySlot* DetailsViewSlot;
	TSharedPtr<SNiagaraEffectEditorViewport>	Viewport;
	
	TSharedPtr< SListView<TSharedPtr<FNiagaraSimulation> > > ListView;

public:
	SLATE_BEGIN_ARGS(SNiagaraEffectEditorWidget)
		: _EffectObj(nullptr)
	{
	}
	SLATE_ARGUMENT(UNiagaraEffect*, EffectObj)
	SLATE_ARGUMENT(TSharedPtr<SWidget>, TitleBar)
	SLATE_ARGUMENT(FNiagaraEffectInstance*, EffectInstance)
	SLATE_END_ARGS()


	/** Function to check whether we should show read-only text in the panel */
	EVisibility ReadOnlyVisibility() const
	{
		return EVisibility::Hidden;
	}

	void UpdateList()
	{
		ListView->RequestListRefresh();
	}

	TSharedPtr<SNiagaraEffectEditorViewport> GetViewport()	{ return Viewport; }

	TSharedRef<ITableRow> OnGenerateWidgetForList(TSharedPtr<FNiagaraSimulation> InItem, const TSharedRef< STableViewBase >& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
			.Content()
			[
				SNew(SBorder)
				//.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(2.0f)
				[
					SNew(SEmitterWidget).Emitter(InItem).Effect(EffectInstance)
				]
			];
	}



	void Construct(const FArguments& InArgs);

};

