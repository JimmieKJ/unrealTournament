// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SAnimationBlendSpaceBase.h"
#include "SAnimationSequenceBrowser.h"
#include "Persona.h"
#include "AssetData.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ScopedTransaction.h"
#include "AnimPreviewInstance.h"
#include "SExpandableArea.h"
#include "SNotificationList.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlendSpaceBase, Log, All);

static FSlateFontInfo GetSmallLayoutFont()
{
	static FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );
	return SmallLayoutFont;
}

#define LOCTEXT_NAMESPACE "BlendSpaceEditorBase"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FText GetErrorMessageForSequence(UBlendSpaceBase* BlendSpace, UAnimSequence* AnimSequence, FVector SampleValue, int32 OriginalIndex)
{
	if (AnimSequence && AnimSequence->GetSkeleton() &&
		BlendSpace && BlendSpace->GetSkeleton() &&
		! AnimSequence->GetSkeleton()->IsCompatible(BlendSpace->GetSkeleton()))
	{
		return LOCTEXT("BlendSpaceDialogSkeletonNotCompatible", "Skeleton of animation is not compatible with blend spaces's skeleton.");
	}
	else if (BlendSpace && BlendSpace->IsTooCloseToExistingSamplePoint(SampleValue, OriginalIndex))
	{
		return LOCTEXT("BlendSpaceDialogTooCloseToExistingPoint", "You cannot place sample so close to existing one.");
	}
	else // default message
	{
		return LOCTEXT("BlendSpaceDialogMixedAdditiveMode", "You cannot mix animations with different additive types in a BlendSpace.");
	}
}

struct FAnimWeightSorted
{
	const UAnimSequence* AnimSequence;
	float Weight;
	FAnimWeightSorted(UAnimSequence * InSeq, float InWeight) 
		: AnimSequence(InSeq), Weight(InWeight) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FSampleDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FSampleDragDropOp, FDragDropOperation)

	int32 OriginalSampleIndex;
	FBlendSample Sample;
	TWeakPtr<SBlendSpaceWidget> BlendSpaceWidget;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			[		
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush( TEXT( "BlendSpace.SamplePoint_Highlight" )) )
				]

				+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock) 
						.Text( this, &FSampleDragDropOp::GetHoverText )
					]
			];
	}

	FText GetHoverText() const
	{
		FText HoverText = LOCTEXT("Invalid", "Invalid");

		if (BlendSpaceWidget.IsValid() && Sample.Animation)
		{
			HoverText = FText::Format( LOCTEXT("AnimName(X,Y)->New(X,Y) - NewSample", "{0}({1})->New({2})"), FText::FromString(Sample.Animation->GetName()), FText::FromString(Sample.SampleValue.ToString()), 
				FText::FromString(BlendSpaceWidget.Pin()->GetLastValidMouseGridPoint().ToString()) ); //UNDO REMOVE FUNCTION CALL
		}

		return HoverText;
	}

	static TSharedRef<FSampleDragDropOp> New()
	{
		TSharedRef<FSampleDragDropOp> Operation = MakeShareable(new FSampleDragDropOp);
		Operation->Construct();
		return Operation;
	}
};

////////////////////////////////////////////////////////////////////////////////////
// SSampleEditPopUp
// Simple text entry popup, usually used within a MenuStack

class SSampleEditPopUp : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSampleEditPopUp )
		: _SampleIndex(0)
	{}

		/** The sample we are editing */
		SLATE_ARGUMENT( int32, SampleIndex )
		/** The parent blend space */
		SLATE_ARGUMENT( UBlendSpaceBase*, BlendSpace)
		/** The editor widget */
		SLATE_ARGUMENT( SBlendSpaceWidget*, BlendSpaceEditorWidget)
		/** Called when the Delete button is clicked */
		SLATE_EVENT( FOnClicked, OnDelete )
		/** Called when the Open Asset button is clicked */
		SLATE_EVENT( FOnClicked, OnOpenAsset )

	SLATE_END_ARGS()

	float& GetVectorValueForParam(int32 InParam, FVector& InVec)
	{
		switch(InParam)
		{
			case 0:
			{
				return InVec.X;
				break;
			}
			case 1:
			{
				return InVec.Y;
				break;
			}
			default:
			{
				check(false); //no support yet
			}
		}
		return InVec.X; //return something to stop compile errors but should not get here
	}

	void Construct( const FArguments& InArgs )
	{
		BlendSpace = InArgs._BlendSpace;
		BlendSpaceEditorWidget = InArgs._BlendSpaceEditorWidget;
		int32 SampleIndex = InArgs._SampleIndex;

		const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
		FVector SamplePosition = Samples[SampleIndex].SampleValue;

		TSharedPtr<SVerticalBox> ParamBox = SNew(SVerticalBox);

		for(int32 ParamIndex = 0; ParamIndex < BlendSpace->NumOfDimension; ++ParamIndex)
		{
			const FBlendParameter& Param = BlendSpace->GetBlendParameter(ParamIndex);

			ParamBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text( FText::FromString(Param.DisplayName) )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(10.0f)
					.Text(FText::AsNumber(GetVectorValueForParam(ParamIndex, SamplePosition), NULL, FInternationalization::Get().GetInvariantCulture()))
					.OnTextCommitted( this, &SSampleEditPopUp::OnValueModified, SampleIndex, ParamIndex  )
				]
			];
		}

		ParamBox->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.Text(LOCTEXT("Delete", "Delete"))
			.OnClicked( InArgs._OnDelete )
		];

		ParamBox->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.Text(LOCTEXT("OpenAsset", "Open Asset"))
			.OnClicked(InArgs._OnOpenAsset)
		];

		this->ChildSlot
		[
			SNew(SBorder)
			. BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
			. Padding(10)
			[
				ParamBox->AsShared()
			]
		];
	}

private:
	void OnValueModified(const FText& NewText, ETextCommit::Type CommitInfo, int32 SampleIndex, int32 ParamIndex)
	{
		const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
		check (Samples.IsValidIndex(SampleIndex));

		const FBlendSample& Sample = Samples[SampleIndex];
		float NewValue = FCString::Atof(*NewText.ToString());

		FVector SampleVector = Sample.SampleValue;
		if (NewValue != GetVectorValueForParam(ParamIndex, SampleVector))
		{
			GetVectorValueForParam(ParamIndex, SampleVector) = NewValue;

			if (BlendSpace->EditSample(Sample, SampleVector))
			{
				//update editor widget
				BlendSpaceEditorWidget->ResampleData();
			}
		}

		if (CommitInfo == ETextCommit::OnEnter)
		{
			FSlateApplication::Get().DismissAllMenus();
		}
	}

	/** The blend space we are editing */
	UBlendSpaceBase* BlendSpace;

	/** The blend space editor widget */
	SBlendSpaceWidget* BlendSpaceEditorWidget;
};

////////////////////////////////////////////////////////////////////////////////////
// SBlendSpaceParameterWidget

void SBlendSpaceParameterWidget::Construct(const FArguments& InArgs)
{
	BlendSpace = InArgs._BlendSpace;
	OnParametersUpdated = InArgs._OnParametersUpdated;

	this->ChildSlot
	[
		SNew(SExpandableArea)
		.AreaTitle( NSLOCTEXT("BlendSpace.Parameter", "ParameterTitle", "Parameters") )
		.BodyContent()
		[
			SAssignNew(ParameterPanel, SVerticalBox)
		]
	];
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBlendSpaceParameterWidget::ConstructParameterPanel()
{
	ParameterPanel->ClearChildren();

	static const FText ParameterNames[3] = { LOCTEXT("X-AxisParameterName", "X"), LOCTEXT("Y-AxisParameterName", "Y"), LOCTEXT("Z-AxisParameterName", "Z") };

	for (int32 Param = 0; Param < BlendSpace->NumOfDimension; ++Param)
	{
		const FBlendParameter& BlendParam = BlendSpace->GetBlendParameter(Param);

		const FText AxisNameLabel = FText::Format( LOCTEXT("AxisName", "{0} Axis Label"), ParameterNames[Param]);
		const FText AxisRangeLabel = FText::Format( LOCTEXT("AxisRange", "{0} Axis Range"), ParameterNames[Param]);
		const FText AxisGridCountLabel = FText::Format( LOCTEXT("AxisGridCount", "{0} Axis Divisions"), ParameterNames[Param]);

		ParameterPanel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(2.0f, 2.0f, 2.0f, 6.0f))
			[
				SNew(SVerticalBox)

				// Axis name row
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2)  
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.FillWidth(0.35f)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("NoBorder"))
						.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(AxisNameLabel)
							.Font(GetSmallLayoutFont())
						]
					]

					+SHorizontalBox::Slot()
						.FillWidth(0.65f)
						[
							SAssignNew(Param_DisplayNames[Param], SEditableTextBox)
							.MinDesiredWidth(75)
						]
				]

				// Axis range row
				+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2)  
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.FillWidth(0.35f)
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("NoBorder"))
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(AxisRangeLabel)
								.Font(GetSmallLayoutFont())
							]
						]

						+SHorizontalBox::Slot()
							.FillWidth(0.65f)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.FillWidth(1)
								.HAlign(HAlign_Left)
								[
									SAssignNew(Param_MinValues[Param], SEditableTextBox)
									.MinDesiredWidth(75)
								]

								+SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Font( GetSmallLayoutFont() )
										.Text(LOCTEXT("MinValue - MaxValue Separator", " - "))
									]

								+SHorizontalBox::Slot()
									.FillWidth(1)
									.HAlign(HAlign_Right)
									[
										SAssignNew(Param_MaxValues[Param], SEditableTextBox)
										.MinDesiredWidth(75)
									]
							]
					]

				// Axis sample count row
				+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2)  
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.FillWidth(0.35f)
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("NoBorder"))
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(AxisGridCountLabel)
								.Font(GetSmallLayoutFont())
							]
						]

						+SHorizontalBox::Slot()
							.FillWidth(0.65f)
							[
								SAssignNew(Param_GridNumbers[Param], SEditableTextBox)
								.MinDesiredWidth(75)
							]
					]
			];
	}

	ParameterPanel->AddSlot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SButton) .HAlign(HAlign_Center)
			.Text(LOCTEXT("ApplyChanges", "Apply Parameter Changes"))
			.OnClicked( this, &SBlendSpaceParameterWidget::OnApplyParameterChanges )
		];

	UpdateParametersFromBlendSpace();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SBlendSpaceParameterWidget::UpdateParametersFromBlendSpace()
{
	for(int I = 0; I < BlendSpace->NumOfDimension; ++I)
	{
		const FBlendParameter& BlendParam = BlendSpace->GetBlendParameter(I);

		Param_DisplayNames[I]->SetText(FText::FromString(BlendParam.DisplayName));
		Param_MinValues[I]->SetText(FText::AsNumber(BlendParam.Min, NULL, FInternationalization::Get().GetInvariantCulture()));
		Param_MaxValues[I]->SetText(FText::AsNumber(BlendParam.Max, NULL, FInternationalization::Get().GetInvariantCulture()));
		Param_GridNumbers[I]->SetText(FText::AsNumber(BlendParam.GridNum, NULL, FInternationalization::Get().GetInvariantCulture()));
	}
}

FReply SBlendSpaceParameterWidget::OnApplyParameterChanges()
{
	FBlendParameter BlendParameter;

	for (int32 I = 0; I < BlendSpace->NumOfDimension; ++I)
	{
		BlendParameter.DisplayName = Param_DisplayNames[I]->GetText().ToString();
		BlendParameter.Min = FCString::Atof(*Param_MinValues[I]->GetText().ToString());
		BlendParameter.Max = FCString::Atof(*Param_MaxValues[I]->GetText().ToString());
		BlendParameter.GridNum = FCString::Atoi(*Param_GridNumbers[I]->GetText().ToString());
		if (BlendSpace->UpdateParameter(I, BlendParameter)==false)
		{
			// wrong parameter
			UE_LOG(LogBlendSpaceBase, Warning, TEXT("Parameter invalid"));
		}
	}

	UpdateParametersFromBlendSpace();
	OnParametersUpdated.ExecuteIfBound();

	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////
// SBlendSpaceSamplesWidget

void SBlendSpaceSamplesWidget::Construct(const FArguments& InArgs)
{
	BlendSpace = InArgs._BlendSpace;

	OnSamplesUpdated = InArgs._OnSamplesUpdated;
	OnNotifyUser = InArgs._OnNotifyUser;

	this->ChildSlot
	[
		SNew(SExpandableArea)
		.AreaTitle( NSLOCTEXT("BlendSpace.Sample", "SampleTitle", "Samples") )
		.BodyContent()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(10)
			[
				SNew(STextBlock)
				.WrapTextAt(300.f)
				.Text(NSLOCTEXT("BlendSpace.Sample", "ToReplaceSample", "To replace sample, you can drag animation onto the sample point in the BlendSpace from Asset Browser or replace in the list below"))
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush( "DetailsView.GroupSection" ) ) //@TODO: Decouple!
				.BorderBackgroundColor( FLinearColor(.2f,.2f,.2f,.2f) )
				.Padding(2.0f)
				[
					SAssignNew(SampleDataPanel, SVerticalBox)
				]
			]
		]
	];
}

void SBlendSpaceSamplesWidget::ConstructSamplesPanel()
{
	SampleDataPanel->ClearChildren();

	const TArray<struct FBlendSample>& SampleData = BlendSpace->GetBlendSamples();

	for(int32 I=0; I<SampleData.Num(); ++I)
	{
		const FBlendSample& Sample = SampleData[I];

		if (Sample.Animation)
		{
			SampleDataPanel->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot() .HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Font( GetSmallLayoutFont() )
					.Text(FText::Format(LOCTEXT("BlendSpaceSamplesLabel", "{0}]"), FText::AsNumber(I+1)))
				]
				+SHorizontalBox::Slot() .FillWidth(1) .HAlign(HAlign_Left)
				[
					SNew(SEditableText)
					.Font( GetSmallLayoutFont() )
					.Text( FText::FromName(Sample.Animation->GetFName() ) )
				]
				+SHorizontalBox::Slot().AutoWidth() .HAlign(HAlign_Center)
				[
					SNew( SButton )
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.OnClicked( this, &SBlendSpaceSamplesWidget::GotoSample, I ) 
					.ToolTipText(NSLOCTEXT("BlendSpace.Sample", "FindAnimation", "Find this animation in Content Browser"))
					.ContentPadding(0)
					.ForegroundColor( FSlateColor::UseForeground() )
					[ 
						SNew( SImage )
						.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Browse") )
						.ColorAndOpacity( FSlateColor::UseForeground() )
					]
				]
				+SHorizontalBox::Slot().AutoWidth() .HAlign(HAlign_Center)
				[
					SNew( SButton )
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.OnClicked( this, &SBlendSpaceSamplesWidget::ReplaceSample, I ) 
					.ToolTipText(NSLOCTEXT("BlendSpace.Sample", "ReplaceWithCurrent", "Replace with current selected animation from Content Browser"))
					.ContentPadding(0)
					.ForegroundColor( FSlateColor::UseForeground() )
					[ 
						SNew( SImage )
						.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Use") )
						.ColorAndOpacity( FSlateColor::UseForeground() )
					] 
				]
				+SHorizontalBox::Slot().AutoWidth() .HAlign(HAlign_Center)
				[
					SNew( SButton )
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.OnClicked( this, &SBlendSpaceSamplesWidget::DeleteSample, I ) 
					.ToolTipText(NSLOCTEXT("BlendSpace.Sample", "RemoveSample", "Remove this sample"))
					.ContentPadding(0)
					.ForegroundColor( FSlateColor::UseForeground() )
					[ 
						SNew( SImage )
						.Image( FEditorStyle::GetBrush( "PropertyWindow.Button_Clear" ) )
						.ColorAndOpacity( FSlateColor::UseForeground() )
					] 
				]
			];
		}
	}

	SampleDataPanel->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Center) 
	.Padding(10)
	[
		SNew(SButton) .HAlign(HAlign_Center)
		.Text(NSLOCTEXT("BlendSpace.Sample", "RemoveAllSamples", "Remove all samples"))
		.OnClicked( this, &SBlendSpaceSamplesWidget::OnClearSamples )
	];
}

FReply SBlendSpaceSamplesWidget::GotoSample(int32 indexOfSample)
{
	if (BlendSpace->SampleData.IsValidIndex(indexOfSample))
	{
		UObject* ObjectToGoTo = BlendSpace->SampleData[indexOfSample].Animation;
		if (ObjectToGoTo)
		{
			TArray<UObject*> Objects;
			Objects.Add(ObjectToGoTo);
			GEditor->SyncBrowserToObjects(Objects);
		}
	}

	return FReply::Handled();
}

FReply SBlendSpaceSamplesWidget::ReplaceSample(int32 indexOfSample)
{
	if (BlendSpace->SampleData.IsValidIndex(indexOfSample))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> SelectedAssetData;
		ContentBrowserModule.Get().GetSelectedAssets(SelectedAssetData);

		if ( SelectedAssetData.Num() )
		{
			for ( auto AssetIt = SelectedAssetData.CreateConstIterator(); AssetIt; ++AssetIt )
			{
				const FAssetData& Asset = *AssetIt;

				if (Asset.GetClass() == UAnimSequence::StaticClass())
				{
					// then replace with current one
					bool bEditSampleSuccess = BlendSpace->EditSampleAnimation(BlendSpace->SampleData[indexOfSample], Cast<UAnimSequence>(Asset.GetAsset()));
					if (! bEditSampleSuccess)
					{
						FFormatNamedArguments Args;
						Args.Add( TEXT("ErrorMessage"), GetErrorMessageForSequence( BlendSpace, Cast<UAnimSequence>(Asset.GetAsset()), BlendSpace->SampleData[indexOfSample].SampleValue, indexOfSample ) );
						FNotificationInfo Info( FText::Format( LOCTEXT("BlendSpaceDialogFail", "BlendSpace\n{ErrorMessage}"), Args ) );

						OnNotifyUser.ExecuteIfBound( Info );
					}

					// refresh
					OnSamplesUpdated.ExecuteIfBound();
					ConstructSamplesPanel();
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Handled();
}

FReply SBlendSpaceSamplesWidget::DeleteSample(int32 indexOfSample)
{
	if (BlendSpace->SampleData.IsValidIndex(indexOfSample))
	{
		// refresh data
		BlendSpace->DeleteSample(BlendSpace->SampleData[indexOfSample]);
		OnSamplesUpdated.ExecuteIfBound();
		ConstructSamplesPanel();
	}

	return FReply::Handled();
}

FReply SBlendSpaceSamplesWidget::OnClearSamples()
{
	BlendSpace->ClearAllSamples();
	OnSamplesUpdated.ExecuteIfBound();
	ConstructSamplesPanel();
	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////
// SBlendSpaceDisplayOptionsWidget

void SBlendSpaceDisplayOptionsWidget::Construct(const FArguments& InArgs)
{
	this->ChildSlot
	[
		SNew(SBorder)
		.Padding( FMargin(5,10,5,5) )
		[
			SAssignNew(CheckBoxArea, SVerticalBox)
		]
	];
}

SVerticalBox::FSlot& SBlendSpaceDisplayOptionsWidget::AddCheckBoxSlot()
{
	SVerticalBox::FSlot& NewSlot = CheckBoxArea->AddSlot();
	NewSlot.AutoHeight();
	NewSlot.Padding(10);
	return NewSlot;
}

////////////////////////////////////////////////////////////////////////////////////
// SBlendSpaceWidget

void SBlendSpaceWidget::Construct(const FArguments& InArgs)
{
	LastValidMouseEditorPoint = FVector::ZeroVector;
	bTooltipOn = true;
	bPreviewOn = true;
	bBeingDragged = false;
	BlendSpace = InArgs._BlendSpace;
	OnNotifyUser = InArgs._OnNotifyUser;
	OnRefreshSamples = InArgs._OnRefreshSamples;
}

FReply SBlendSpaceWidget::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	FSlateRect WindowRect = GetWindowRectFromGeometry(MyGeometry);

	TSharedPtr< FDragDropOperation > Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return FReply::Unhandled();
	}

	if ( Operation->IsOfType<FAssetDragDropOp>() )
	{
		TSharedPtr<FAssetDragDropOp> AssetOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
		UAnimSequence *DroppedSequence = FAssetData::GetFirstAsset<UAnimSequence>(AssetOp->AssetData);

		if (DroppedSequence)
		{
			HandleSampleDrop(MyGeometry, DragDropEvent.GetScreenSpacePosition(), DroppedSequence, INDEX_NONE);
			return FReply::Handled();
		}
	}
	else if (Operation->IsOfType<FSampleDragDropOp>())
	{
		TSharedPtr<FSampleDragDropOp> DragDropOp = StaticCastSharedPtr<FSampleDragDropOp>(Operation);
		UAnimSequence *DroppedSequence = DragDropOp->Sample.Animation;
		int32 OriginalIndex = DragDropOp->OriginalSampleIndex;

		// the widget should be same, if different we can't guarantee index will be still same
		if (DragDropOp->BlendSpaceWidget.HasSameObject(this) &&  CachedSamples.IsValidIndex(OriginalIndex))
		{
			HandleSampleDrop(MyGeometry, DragDropEvent.GetScreenSpacePosition(), DroppedSequence, OriginalIndex);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceWidget::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) 
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		FSlateRect WindowRect = GetWindowRectFromGeometry(MyGeometry);
		// if sample is highlighted, overwrite the animation for it
		int32 HighlightedSample = GetHighlightedSample(WindowRect);

		if (HighlightedSample!=INDEX_NONE)
		{
			check (CachedSamples.IsValidIndex(HighlightedSample));

			FBlendSample& Sample = CachedSamples[HighlightedSample];

			TSharedRef<FSampleDragDropOp> Operation = FSampleDragDropOp::New();
			Operation->OriginalSampleIndex = HighlightedSample;
			Operation->Sample = Sample;
			Operation->BlendSpaceWidget = SharedThis(this);

			return FReply::Handled().BeginDragDrop(Operation);
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceWidget::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		FSlateRect WindowRect = GetWindowRectFromGeometry(MyGeometry);
		// if sample is highlighted, overwrite the animation for it
		int32 HighlightedSample = GetHighlightedSample(WindowRect);

		if (HighlightedSample!=INDEX_NONE)
		{
			return FReply::Handled().DetectDrag( SharedThis(this), EKeys::LeftMouseButton );
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceWidget::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	int32 HighlightedSampleIndex = GetHighlightedSample(GetWindowRectFromGeometry(MyGeometry));
	if (HighlightedSampleIndex!=INDEX_NONE)
	{
		ShowPopupEditWindow(HighlightedSampleIndex, MouseEvent);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceWidget::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return UpdateLastMousePosition(MyGeometry, MouseEvent.GetScreenSpacePosition());
}

FReply SBlendSpaceWidget::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// if being dragged, update the mouse pos
	if (DragDropEvent.GetOperationAs<FAssetDragDropOp>().IsValid() || DragDropEvent.GetOperationAs<FSampleDragDropOp>().IsValid())
	{
		bBeingDragged = true;
		return UpdateLastMousePosition(MyGeometry, DragDropEvent.GetScreenSpacePosition());
	}

	return FReply::Unhandled();
}

void SBlendSpaceWidget::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	// should we cancel the action?	
	if (bBeingDragged)
	{
		bBeingDragged = false;
	}
}

void SBlendSpaceWidget::HandleSampleDrop(const FGeometry& MyGeometry, const FVector2D& ScreenSpaceDropPosition, UAnimSequence* DroppedSequence, int32 OriginalIndex)
{
	FSlateRect WindowRect = GetWindowRectFromGeometry(MyGeometry);
	UpdateLastMousePosition(MyGeometry, ScreenSpaceDropPosition, true, true);

	if (BlendSpace)
	{
		bool bAddSampleSuccess = false;

		int32 HighlightedIndex = GetHighlightedSample(WindowRect);

		// if sample is highlighted, overwrite the animation for it
		if (HighlightedIndex==INDEX_NONE)
		{
			if(OriginalIndex == INDEX_NONE)
			{
				// new sample, just added it
				const FScopedTransaction Transaction( LOCTEXT("AddNewSample", "Add New Sample") );
				BlendSpace->Modify();

				bAddSampleSuccess = BlendSpace->AddSample( FBlendSample(DroppedSequence, LastValidMouseEditorPoint, true) );
			}
			else
			{
				// existing sample, do a move instead
				const FScopedTransaction Transaction( LOCTEXT("MoveSample", "Move Sample") );
				BlendSpace->Modify();

				FBlendSample& Sample = CachedSamples[OriginalIndex];
				// otherwise overwrite current animation
				BlendSpace->DeleteSample(Sample);
				FBlendSample NewSample(DroppedSequence, LastValidMouseEditorPoint, true);
				bAddSampleSuccess = BlendSpace->AddSample(NewSample);
			}
		}
		else
		{
			//Overwrite current animation
			const FScopedTransaction Transaction( LOCTEXT("ReplaceSampleAnimation", "Replace Sample Animation") );
			BlendSpace->Modify();

			if(OriginalIndex != INDEX_NONE)
			{
				//Clean up dragging sample
				FBlendSample& OriginalSample = CachedSamples[OriginalIndex];
				BlendSpace->DeleteSample(OriginalSample);
			}

			check (CachedSamples.IsValidIndex(HighlightedIndex));

			FBlendSample& HighlightedSample = CachedSamples[HighlightedIndex];
			BlendSpace->DeleteSample(HighlightedSample);
			HighlightedSample.Animation = DroppedSequence;
			bAddSampleSuccess = BlendSpace->AddSample(HighlightedSample);
		}

		if ( !bAddSampleSuccess )
		{
			// Tell the user that the drop failed
			FFormatNamedArguments Args;
			Args.Add( TEXT("ErrorMessage"), GetErrorMessageForSequence( BlendSpace, DroppedSequence, LastValidMouseEditorPoint, OriginalIndex ) );
			FNotificationInfo Info( FText::Format( LOCTEXT("BlendSpaceDialogFail", "BlendSpace\n{ErrorMessage}"), Args ) );

			OnNotifyUser.ExecuteIfBound( Info );
		}

		// need to refresh panel of editor
		ResampleData();

		OnRefreshSamples.ExecuteIfBound();
	}

	bBeingDragged = false;
}

void SBlendSpaceWidget::ResampleData()
{
	BlendSpace->ValidateSampleData();
	CachedSamples.Empty();
	BlendSpace->GetBlendSamplePoints(CachedSamples);
	ValidateSamplePositions();
}

void SBlendSpaceWidget::ValidateSamplePositions()
{
	for(int i = 0; i < CachedSamples.Num(); ++i)
	{
		const FVector& OriginalValue = CachedSamples[i].SampleValue;
		FVector SnappedValue = SnapEditorPosToGrid(OriginalValue);
		CachedSamples[i].bIsValid = SnappedValue.Equals(OriginalValue);
	}
}

void SBlendSpaceWidget::DrawToolTip(const FVector2D& LeftTopPos, const FText& Text, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, const FColor& InColor, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const
{
	if (bTooltipOn)
	{
		FSlateFontInfo MyFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FVector2D DrawSize = FontMeasureService->Measure(Text, MyFont);
		FVector2D Pos = LeftTopPos;

		// if end point is outside
		FVector2D OutMargin = AllottedGeometry.Size-(Pos+DrawSize);
		// if one of them is outside
		if (OutMargin.X<0)
		{
			Pos.X += OutMargin.X;
		}
		if (OutMargin.Y<0)
		{
			Pos.Y += OutMargin.Y;
		}

		FPaintGeometry MyGeometry =	AllottedGeometry.ToPaintGeometry( Pos, DrawSize );
		const FSlateBrush* StyleInfo = FEditorStyle::GetBrush( TEXT( "ToolBar.Background" ) );
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId, 
			MyGeometry, 
			StyleInfo, 
			MyClippingRect,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.7f)
			);


		FSlateDrawElement::MakeText( 
			OutDrawElements,
			LayerId+1,
			MyGeometry,
			Text,
			MyFont,
			MyClippingRect,
			ESlateDrawEffect::None,
			InColor
			);
	}
}

void SBlendSpaceWidget::DrawSamplePoint( const FVector2D& Point, EBlendSpaceSampleState::Type SampleState, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const
{
	const FSlateBrush* StyleInfo = NULL;

	switch (SampleState)
	{
		case EBlendSpaceSampleState::Normal:
		{
			StyleInfo = FEditorStyle::GetBrush( TEXT( "BlendSpace.SamplePoint" ) );
			break;
		}
		case EBlendSpaceSampleState::Highlighted:
		{
			StyleInfo = FEditorStyle::GetBrush( TEXT( "BlendSpace.SamplePoint_Highlight" ) );
			break;
		}
		case EBlendSpaceSampleState::Invalid:
		{
			StyleInfo = FEditorStyle::GetBrush( TEXT( "BlendSpace.SamplePoint_Invalid") );
			break;
		}
		default:
		{
			check(false); // Invalid value for SampleState
		}
	}

	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry( Point-FVector2D(8,8), FVector2D(16,16) ),
		StyleInfo, 
		MyClippingRect
		);
}

void SBlendSpaceWidget::DrawSamplePoints( const FSlateRect& WindowRect, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, int32 HighlightedSampleIndex ) const
{
	for (int32 I=0; I<CachedSamples.Num(); ++I)
	{

		EBlendSpaceSampleState::Type SampleState = EBlendSpaceSampleState::Normal;
		if(HighlightedSampleIndex == I)
		{
			SampleState = EBlendSpaceSampleState::Highlighted;
		}
		else if(!CachedSamples[I].bIsValid)
		{
			SampleState = EBlendSpaceSampleState::Invalid;
		}
		TOptional<FVector2D> Pos = GetWidgetPosFromEditorPos(CachedSamples[I].SampleValue, WindowRect);
		DrawSamplePoint(Pos.GetValue(), SampleState, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId);
	}
}

void SBlendSpaceWidget::DrawText( const FVector2D& Point, const FText& Text, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, const FColor& InColor, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const
{
	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FSlateFontInfo MyFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

	FVector2D DrawSize = FontMeasureService->Measure(Text, MyFont);
	FVector2D TextPos = Point;

	// if end point is outside
	FVector2D OutMargin = AllottedGeometry.Size-(TextPos+DrawSize);
	// if one of them is outside
	if (OutMargin.X<0)
	{
		TextPos.X += OutMargin.X;
	}
	if (OutMargin.Y<0)
	{
		TextPos.Y += OutMargin.Y;
	}

	FSlateDrawElement::MakeText( 
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( TextPos ),
		Text,
		MyFont,
		MyClippingRect,
		ESlateDrawEffect::None,
		InColor
		);
}

FText SBlendSpaceWidget::GetToolTipText(const FVector& GridPos, const TArray<UAnimSequence*> AnimSeqs, const TArray<float> BlendWeights) const
{
	// need to consolidate animseqs to be unique and in the order of weights
	check (AnimSeqs.Num() == BlendWeights.Num());

	TArray<FAnimWeightSorted> Anims;

	for (int32 I=0; I<AnimSeqs.Num(); ++I)
	{
		bool bAddNew=true;

		for (int32 J=0; J<Anims.Num(); ++J)
		{
			// if same is found, accumulate the weight
			if (Anims[J].AnimSequence == AnimSeqs[I])
			{
				Anims[J].Weight += BlendWeights[I];
				bAddNew=false;
			}
		}

		// if it's not same, add new one
		if (bAddNew)
		{
			Anims.Add(FAnimWeightSorted(AnimSeqs[I], BlendWeights[I]));
		}
	}

	/** Used to sort FPoint from -X->+X, -Y->+Y, -Z->+Z. */
	struct FCompareWeight
	{
		FORCEINLINE bool operator()( const FAnimWeightSorted& A, const FAnimWeightSorted& B ) const
		{
			return A.Weight < B.Weight;
		}
	};
	// sort all points
	Anims.Sort( FCompareWeight() );

	//@todo localize properly [10/11/2013 justin.sargent]
	FString OutputToolTip = GetInputText(GridPos).ToString();

	for (int32 I=Anims.Num()-1; I>=0; --I)
	{
		if (Anims[I].Weight > ZERO_ANIMWEIGHT_THRESH)
		{
			OutputToolTip += FString::Printf(TEXT("\n%s (%0.4f)"), *Anims[I].AnimSequence->GetName(), Anims[I].Weight);
		}
	}

	return FText::FromString( OutputToolTip );
}

int32 SBlendSpaceWidget::GetHighlightedSample(const FSlateRect& WindowRect) const
{
	int32 HighlightedSampleIndex = INDEX_NONE;

	TOptional<FVector2D> MousePos = GetWidgetPosFromEditorPos(LastValidMouseEditorPoint, WindowRect);

	if (MousePos.IsSet())
	{
		for (int32 I=0; I<CachedSamples.Num(); ++I)
		{
			TOptional<FVector2D> Pos = GetWidgetPosFromEditorPos(CachedSamples[I].SampleValue, WindowRect);
			// for this, use mouse position, since it's window space, it's more valid - compare to using grid pos
			// the delta can be various per case
			if (Pos.IsSet() && (Pos.GetValue()-MousePos.GetValue()).IsNearlyZero(10.f))
			{
				HighlightedSampleIndex = I;
				break;
			}
		}
	}

	return HighlightedSampleIndex;
}

void SBlendSpaceWidget::ShowPopupEditWindow(int32 SampleIndex, const FPointerEvent& MouseEvent)
{
	if (CachedSamples.IsValidIndex(SampleIndex))
	{
		FBlendSample Sample = CachedSamples[SampleIndex];

		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

		// Show dialog to enter new function name
		FSlateApplication::Get().PushMenu(
			SharedThis( this ),
			WidgetPath,
			SNew(SSampleEditPopUp)
			.BlendSpace(BlendSpace)
			.BlendSpaceEditorWidget(this)
			.SampleIndex(SampleIndex)
			.OnDelete( this, &SBlendSpaceWidget::DeleteSample, SampleIndex )
			.OnOpenAsset(this, &SBlendSpaceWidget::OpenAsset, SampleIndex),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
			);
	}
}

FReply SBlendSpaceWidget::DeleteSample(int32 SampleIndex)
{
	check (CachedSamples.IsValidIndex(SampleIndex));

	const FScopedTransaction Transaction(LOCTEXT("DeleteSample_T", "Delete Blendspace Sample"));

	BlendSpace->Modify();
	BlendSpace->DeleteSample(CachedSamples[SampleIndex]);
	ResampleData();
	OnRefreshSamples.ExecuteIfBound();

	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

FReply SBlendSpaceWidget::OpenAsset( int32 SampleIndex )
{
	check(CachedSamples.IsValidIndex(SampleIndex));
	UAnimationAsset* AnimAsset = CachedSamples[SampleIndex].Animation;

	FPersonaModule* PersonaModule = &FModuleManager::Get().LoadModuleChecked<FPersonaModule>("Persona");
	PersonaModule->CreatePersona(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), BlendSpace->GetSkeleton(), NULL, AnimAsset, NULL);

	return FReply::Handled();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SBlendSpaceEditorBase::~SBlendSpaceEditorBase()
{
	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo(this);
	}
}

void SBlendSpaceEditorBase::Construct(const FArguments& InArgs)
{
	bIsActiveTimerRegistered = false;
	BlendSpace = InArgs._BlendSpace;
	PersonaPtr = InArgs._Persona;
	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SBlendSpaceEditorBase::PostUndo ) );

	bIsActiveTimerRegistered = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlendSpaceEditorBase::UpdatePreview));
}

EActiveTimerReturnType SBlendSpaceEditorBase::UpdatePreview( double InCurrentTime, float InDeltaTime )
{
	// Update the preview as long as its enabled
	if (BlendSpaceWidget->bPreviewOn)
	{
		UpdatePreviewParameter();
		return EActiveTimerReturnType::Continue;
	}

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SBlendSpaceEditorBase::PostUndo()
{
	BlendSpaceWidget->ResampleData();
	RefreshSampleDataPanel();
}

TSharedRef<SWidget> SBlendSpaceEditorBase::MakeEditorHeader() const
{
	FString DocumentLink;

	if (Cast<UAimOffsetBlendSpace>(BlendSpace) || Cast<UAimOffsetBlendSpace1D>(BlendSpace))
	{
		DocumentLink = TEXT("Engine/Animation/AimOffset");
	}
	else
	{
		DocumentLink = TEXT("Engine/Animation/Blendspaces");
	}
	
	return SNew(SBorder)
	. BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
	. HAlign(HAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		. VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font( FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14 ) )
			.ColorAndOpacity( FLinearColor(1,1,1,0.5) )
			.Text( this, &SBlendSpaceEditorBase::GetBlendSpaceDisplayName )
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			IDocumentation::Get()->CreateAnchor(DocumentLink)
		]
	];
}

TSharedRef<SWidget> SBlendSpaceEditorBase::MakeDisplayOptionsBox() const
{
	TSharedRef<SBlendSpaceDisplayOptionsWidget> DisplayOptionsPanel = SNew(SBlendSpaceDisplayOptionsWidget);

	DisplayOptionsPanel->AddCheckBoxSlot()
	[
		SNew(SCheckBox) 
		.IsChecked( this, &SBlendSpaceEditorBase::IsPreviewOn )
		.OnCheckStateChanged( this, &SBlendSpaceEditorBase::ShowPreview_OnIsCheckedChanged)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EnablePreview", "Enable Preview BlendSpace"))
		]
	];
	
	DisplayOptionsPanel->AddCheckBoxSlot()
	[
		SNew(SCheckBox) 
		.IsChecked( true ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
		.OnCheckStateChanged( this, &SBlendSpaceEditorBase::ShowToolTip_OnIsCheckedChanged)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EnableToolTip", "Enable Tooltip Display"))
		]
	];
	return DisplayOptionsPanel;
}

ECheckBoxState SBlendSpaceEditorBase::IsPreviewOn() const
{
	class UDebugSkelMeshComponent* Component = PersonaPtr.Pin()->GetPreviewMeshComponent();

	if (Component != NULL && Component->IsPreviewOn())
	{
		if (Component->PreviewInstance->CurrentAsset == BlendSpace)
		{
			return ECheckBoxState::Checked;
		}
	}
	return ECheckBoxState::Unchecked;
}

void SBlendSpaceEditorBase::ShowPreview_OnIsCheckedChanged( ECheckBoxState NewValue )
{
	bool bPreviewEnabled = (NewValue != ECheckBoxState::Unchecked);
	class UDebugSkelMeshComponent* Component = PersonaPtr.Pin()->GetPreviewMeshComponent();

	if ( Component != NULL )
	{
		Component->EnablePreview( bPreviewEnabled, BlendSpace, NULL );

		BlendSpaceWidget->bPreviewOn = bPreviewEnabled;

		if ( bPreviewEnabled && !bIsActiveTimerRegistered )
		{
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SBlendSpaceEditorBase::UpdatePreview ) );
		}
	}
}

void SBlendSpaceEditorBase::ShowToolTip_OnIsCheckedChanged( ECheckBoxState NewValue )
{
	if (BlendSpaceWidget.IsValid())
	{
		BlendSpaceWidget->bTooltipOn = (NewValue != ECheckBoxState::Unchecked);
	}
}

void SBlendSpaceEditorBase::UpdatePreviewParameter() const
{
	class UDebugSkelMeshComponent* Component = PersonaPtr.Pin()->GetPreviewMeshComponent();

	if (Component != nullptr && Component->IsPreviewOn())
	{
		if (Component->PreviewInstance->CurrentAsset == BlendSpace)
		{
			FVector BlendInput = BlendSpaceWidget->LastValidMouseEditorPoint;
			Component->PreviewInstance->SetBlendSpaceInput(BlendInput);

			if (PersonaPtr.IsValid())
			{
				PersonaPtr.Pin()->RefreshViewport();
			}
		}
	}
}

void SBlendSpaceEditorBase::RefreshSampleDataPanel()
{
	SamplesWidget->ConstructSamplesPanel();
}

void SBlendSpaceEditorBase::OnBlendSpaceSamplesChanged()
{
	BlendSpaceWidget->ResampleData();
}

FVector SBlendSpaceEditorBase::GetPreviewBlendInput() const
{
	if (PersonaPtr.IsValid())
	{
		UDebugSkelMeshComponent * Mesh = PersonaPtr.Pin()->GetPreviewMeshComponent();
		if ( Mesh )
		{
			UAnimSingleNodeInstance * SingleNodeInstance = Mesh->GetSingleNodeInstance();
			if ( SingleNodeInstance )
			{
				return SingleNodeInstance->BlendFilter.GetFilterLastOutput();
			}
		}
	}	

	return FVector::ZeroVector;
}

void SBlendSpaceEditorBase::NotifyUser( FNotificationInfo& Info )
{
	Info.bUseLargeFont = false;
	Info.ExpireDuration = 5.0f;

	TSharedPtr<SNotificationItem> Notification = NotificationListPtr->AddNotification(Info);
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}

#undef LOCTEXT_NAMESPACE
