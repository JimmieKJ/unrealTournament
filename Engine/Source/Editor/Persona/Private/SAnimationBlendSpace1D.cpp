// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SAnimationBlendSpace1D.h"
#include "SAnimationSequenceBrowser.h"
#include "Persona.h"
#include "AssetData.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ScopedTransaction.h"
#include "SNotificationList.h"
#include "Animation/BlendSpaceBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlendSpace1D, Log, All);

#define LOCTEXT_NAMESPACE "BlendSpace1DDialog"

//////////////////////////////////////////////////////////////////////////
// FLineElement

bool FLineElement::PopulateElement(float ElementPosition, FEditorElement& Element) const
{
	if(ElementPosition < Start.Point.X)
	{
		check(bIsFirst); // We should never be processing a point before us unless we
		// are the first line in the chain
		Element.Indices[0] = Start.Index;
		Element.Weights[0] = 1.0f;
		return true;
	}
	else if(ElementPosition > End.Point.X)
	{
		if(!bIsLast)
		{
			return false;
		}
		Element.Indices[0] = End.Index;
		Element.Weights[0] = 1.0f;
		return true;
	}
	else
	{
		//Calc End Point Weight
		Element.Indices[0] = End.Index;
		Element.Weights[0] = (ElementPosition - Start.Point.X) / Range;

		//Start Point Weight is inverse of End Point
		Element.Indices[1] = Start.Index;
		Element.Weights[1] = (1.0f - Element.Weights[0]);
		return true;
	}
}

//////////////////////////////////////////////////////////////////////////
// FLineElementGenerator

void FLineElementGenerator::CalculateEditorElements()
{
	struct FLinePointSorter
	{
		bool operator()(const FVector& PointA, const FVector& PointB) const
		{
			return PointA.X < PointB.X;
		}
	};

	//Create EditorElements
	EditorElements.Empty(NumEditorPoints);
	EditorElements.AddUninitialized(NumEditorPoints);

	if(SamplePointList.Num() == 0)
	{
		for(int ElementIndex = 0; ElementIndex < EditorElements.Num(); ++ ElementIndex)
		{
			EditorElements.Add(FEditorElement());
		}
		return;
	}

	// Order points
	SamplePointList.Sort(FLinePointSorter());

	// Generate lines
	LineElements.Empty();
	for(int32 PointIndex = 0; PointIndex < SamplePointList.Num()-1; ++PointIndex)
	{
		int32 EndPointIndex = PointIndex + 1;

		FIndexLinePoint StartPoint(SamplePointList[PointIndex], PointIndex);
		FIndexLinePoint EndPoint(SamplePointList[EndPointIndex], EndPointIndex);

		LineElements.Add( FLineElement(StartPoint, EndPoint, PointIndex == 0, EndPointIndex == (SamplePointList.Num()-1)) );
	}

	int32 NumEditorDivisions = (NumEditorPoints - 1); //number of spaces between points in the editor

	float EditorRange = EndOfEditorRange - StartOfEditorRange;
	float EditorStep = EditorRange/NumEditorDivisions;

	if(LineElements.Num() == 0)
	{
		// no lines made, just make everything sample 0
		FEditorElement Element;
		Element.Indices[0] = 0;
		Element.Weights[0] = 1.0f;
		for(int ElementIndex = 0; ElementIndex < EditorElements.Num(); ++ElementIndex)
		{
			EditorElements[ElementIndex] = Element;
		}
	}
	else
	{
		for(int ElementIndex = 0; ElementIndex < EditorElements.Num(); ++ ElementIndex)
		{
			bool bPopulatedElement = false;

			float ElementPosition = (EditorStep * ElementIndex) + StartOfEditorRange;
			FEditorElement Element;

			for(int LineIndex = 0; LineIndex < LineElements.Num(); ++LineIndex)
			{
				if(LineElements[LineIndex].PopulateElement(ElementPosition, Element))
				{
					bPopulatedElement = true;
					break;
				}
			}

			check(bPopulatedElement);
			EditorElements[ElementIndex] = Element;
		}
	}
}

const FLineElement* FLineElementGenerator::GetLineElementForBlendInput(const FVector& BlendInput) const
{
	for(int32 LineIndex = 0; LineIndex < LineElements.Num(); ++LineIndex)
	{
		if(LineElements[LineIndex].IsBlendInputOnLine(BlendInput))
		{
			return &LineElements[LineIndex];
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// FEditorSpaceConverter

class FEditorSpaceConverter
{
public:
	bool bIsEditorVertical;

	float ParamMin;
	float ParamMax;
	float Range;

	float WidgetMin;
	float WidgetMax;
	float WidgetMiddle;
	float WidgetRange;

	float InputPerPixel;
	float PixelsPerInput;

	FEditorSpaceConverter(float InParamMin, float InParamMax, const FSlateRect& InWindowRect, bool bInIsEditorVertical)
		: bIsEditorVertical(bInIsEditorVertical), ParamMin(InParamMin), ParamMax(InParamMax), Range(InParamMax - InParamMin)
	{
		if(bIsEditorVertical)
		{
			WidgetMin = InWindowRect.Top;
			WidgetMax = InWindowRect.Bottom;
			WidgetMiddle = (InWindowRect.Left + InWindowRect.Right) / 2.0f;
		}
		else
		{
			WidgetMin = InWindowRect.Left;
			WidgetMax = InWindowRect.Right;
			WidgetMiddle = (InWindowRect.Top + InWindowRect.Bottom) / 2.0f;
		}
		WidgetRange = WidgetMax - WidgetMin;

		InputPerPixel = Range / WidgetRange;
		PixelsPerInput = WidgetRange / Range;
	}

	/** Tests the supplied value against the current editor range */
	bool IsInEditorRange(FVector Value)
	{
		float EditorValue = Value.X;
		return (EditorValue >= ParamMin && EditorValue <= ParamMax);
	}

	bool IsInWidgetRange(FVector2D Value)
	{
		float WidgetVal = bIsEditorVertical ? Value.Y : Value.X;
		return (WidgetVal >= WidgetMin && WidgetVal <= WidgetMax);
	}

	FVector2D EditorSpaceToWidgetSpace(FVector EditorValue)
	{
		float WidgetSpaceValue = (EditorValue.X - ParamMin) / InputPerPixel;

		if(bIsEditorVertical)
		{
			return FVector2D(WidgetMiddle,WidgetMax - WidgetSpaceValue);
		}
		else
		{
			return FVector2D(WidgetMin+WidgetSpaceValue,WidgetMiddle);
		}
	}

	FVector WidgetSpaceToEditorSpace(FVector2D WidgetValue)
	{
		float WidgetSpaceValue;
		if(bIsEditorVertical)
		{
			WidgetSpaceValue = WidgetMax - WidgetValue.Y;
		}
		else
		{
			WidgetSpaceValue = WidgetValue.X - WidgetMin;
		}
	
		return FVector((WidgetSpaceValue / PixelsPerInput)+ParamMin, 0.0f, 0.0f);
	}
};

void SBlendSpace1DWidget::Construct(const FArguments& InArgs)
{
	SBlendSpaceWidget::Construct(SBlendSpaceWidget::FArguments()
								 .BlendSpace(InArgs._BlendSpace1D)
								 .OnRefreshSamples(InArgs._OnRefreshSamples)
								 .OnNotifyUser(InArgs._OnNotifyUser));
	PreviewInput = InArgs._PreviewInput;
	check (BlendSpace);
}

FVector2D SBlendSpace1DWidget::ComputeDesiredSize( float ) const
{
	return FVector2D( 150.0f, 150.0f );
}

FText SBlendSpace1DWidget::GetInputText(const FVector& GridPos) const
{
	FFormatNamedArguments Args;
	Args.Add( TEXT("DisplayName"), FText::FromString( BlendSpace->GetBlendParameter(0).DisplayName ) );
	Args.Add( TEXT("XGridPos"), GridPos.X );

	return FText::Format( LOCTEXT("BlendSpace1DParamGridMessage", "{DisplayName} [{XGridPos}]\n"), Args );
}

FReply SBlendSpace1DWidget::UpdateLastMousePosition( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition, bool bClampToWindowRect, bool bSnapToGrid  )
{
	FSlateRect WindowRect = GetWindowRectFromGeometry(MyGeometry);
	FVector2D NewPoint = MyGeometry.AbsoluteToLocal(ScreenSpacePosition);

	if(bClampToWindowRect)
	{
		NewPoint.X = FMath::Clamp<float>(NewPoint.X, WindowRect.Left, WindowRect.Right);
		NewPoint.Y = FMath::Clamp<float>(NewPoint.Y, WindowRect.Top, WindowRect.Bottom);
	}

	TOptional<FVector> EditorPos = GetEditorPosFromWidgetPos(NewPoint, WindowRect);
	if (EditorPos.IsSet())
	{
		LastValidMouseEditorPoint = EditorPos.GetValue();

		if(bSnapToGrid)
		{
			LastValidMouseEditorPoint = SnapEditorPosToGrid(LastValidMouseEditorPoint);
		}
	}

	return FReply::Handled();
}

int SBlendSpace1DWidget::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled && IsEnabled() );

	FDrawLines LineBatcher;

	//TArray<FVector2D> LinePoints;

	int32 DefaultLayer = LayerId;
	int32 HighlightLayer = LayerId+1;
	int32 SampleLayer = LayerId+2;
	int32 TooltipLayer = LayerId+4;

	//FColor HighlightColor = FColor(241,146,88);
	//FColor ImportantColor = FColor(241,146,88);

	FSlateRect WindowRect = GetWindowRectFromGeometry(AllottedGeometry);

	FColor OutlineColour(40,40,40);
	FColor DivisionColour(90,90,90);
	FColor HighlightColor = FColor(241,146,88);

	int32 HighlightedSampleIndex = GetHighlightedSample(WindowRect);

	const FBlendParameter& Param = BlendSpace->GetBlendParameter(0);

	TOptional<FVector2D> MousePos = GetWidgetPosFromEditorPos(LastValidMouseEditorPoint, WindowRect);

	if(GetBlendSpace()->bDisplayEditorVertically)
	{
		// Draw bar outline
		float MidPoint = (WindowRect.Left + WindowRect.Right) / 2.0f;
		LineBatcher.AddLine(FVector2D(WindowRect.Left, WindowRect.Top), FVector2D(WindowRect.Right, WindowRect.Top), OutlineColour, DefaultLayer);
		LineBatcher.AddLine(FVector2D(WindowRect.Left, WindowRect.Bottom), FVector2D(WindowRect.Right, WindowRect.Bottom), OutlineColour, DefaultLayer);
		LineBatcher.AddLine(FVector2D(MidPoint, WindowRect.Top), FVector2D(MidPoint, WindowRect.Bottom), OutlineColour, DefaultLayer);

		//Draw Divisions
		float Span = WindowRect.Bottom - WindowRect.Top;
		float SegmentSize = Span / Param.GridNum;
		for(int Division = 0; Division < Param.GridNum-1; ++Division)
		{
			float DivisionPoint = WindowRect.Top + (SegmentSize * (Division+1));
			LineBatcher.AddLine(FVector2D(WindowRect.Left, DivisionPoint), FVector2D(WindowRect.Right, DivisionPoint), DivisionColour, DefaultLayer);
		}
	}
	else
	{
		// Draw bar outline
		float MidPoint = (WindowRect.Top + WindowRect.Bottom) / 2.0f;
		LineBatcher.AddLine(FVector2D(WindowRect.Left, WindowRect.Top), FVector2D(WindowRect.Left, WindowRect.Bottom), OutlineColour, DefaultLayer);
		LineBatcher.AddLine(FVector2D(WindowRect.Right, WindowRect.Top), FVector2D(WindowRect.Right, WindowRect.Bottom), OutlineColour, DefaultLayer);
		LineBatcher.AddLine(FVector2D(WindowRect.Left, MidPoint), FVector2D(WindowRect.Right,MidPoint), OutlineColour, DefaultLayer);

		//Draw Divisions
		float Span = WindowRect.Right - WindowRect.Left;
		float SegmentSize = Span / Param.GridNum;
		for(int Division = 0; Division < Param.GridNum-1; ++Division)
		{
			float DivisionPoint = WindowRect.Left + (SegmentSize * (Division+1));
			LineBatcher.AddLine(FVector2D(DivisionPoint, WindowRect.Top), FVector2D(DivisionPoint, WindowRect.Bottom), DivisionColour, DefaultLayer);
		}
	}

	if ( MousePos.IsSet() && !bBeingDragged )
	{
		bool bIsOnValidSample = (HighlightedSampleIndex != INDEX_NONE) && CachedSamples.IsValidIndex(HighlightedSampleIndex);

		if(bIsOnValidSample && CachedSamples[HighlightedSampleIndex].bIsValid == false)
		{
			const FText InvalidSampleMessage = LOCTEXT("BlendSpaceInvalidSample", "This sample is in an invalid location.\n\nPlease drag it to a grid point.");
			DrawToolTip( MousePos.GetValue(), InvalidSampleMessage, AllottedGeometry, MyClippingRect, FColor(255, 20, 20), OutDrawElements, TooltipLayer);
		}
		else if ( ElementGenerator.EditorElements.Num() > 0 )
		{
			// consolidate all samples and sort them, so that we can handle from biggest weight to smallest
			TArray<FBlendSampleData> SampleDataList;
			const TArray<struct FBlendSample>& SampleData = BlendSpace->GetBlendSamples();

			// if no sample data is found, return 
			if (BlendSpace->GetSamplesFromBlendInput(LastValidMouseEditorPoint, SampleDataList))
			{
				TArray<UAnimSequence*>	Seqs;
				TArray<float>			Weights;

				for (int32 I=0; I<SampleDataList.Num(); ++I)
				{
					const FBlendSample& Sample = SampleData[SampleDataList[I].SampleDataIndex];
					if (Sample.Animation)
					{
						UAnimSequence * Seq = Sample.Animation;
						Seqs.Add(Seq);
						Weights.Add(SampleDataList[I].GetWeight());
					}
				}		

				FText ToolTipMessage;
				if (bPreviewOn)
				{
					ToolTipMessage = FText::Format( LOCTEXT("Previewing", "Previewing\n\n{0}"), GetToolTipText(LastValidMouseEditorPoint, Seqs, Weights) );
				}
				else
				{
					ToolTipMessage = GetToolTipText(LastValidMouseEditorPoint, Seqs, Weights);
				}
				DrawToolTip(MousePos.GetValue(), ToolTipMessage, AllottedGeometry, MyClippingRect, FColor::White, OutDrawElements, TooltipLayer);
			}
		}
		else if (bIsOnValidSample) // If I'm on top of a sample
		{
			UAnimSequence * Seq = CachedSamples[HighlightedSampleIndex].Animation;
			const FText Sequence = (Seq)? FText::FromString( Seq->GetName() ): LOCTEXT("None", "None");
			const FText ToolTipMessage = FText::Format( LOCTEXT("Sequence", "{0}\nSequence ({1})"), GetInputText(LastValidMouseEditorPoint), Sequence );
			DrawToolTip(MousePos.GetValue(), ToolTipMessage, AllottedGeometry, MyClippingRect, FColor::White, OutDrawElements, TooltipLayer);
		}
	}

	if (bPreviewOn)
	{
		if(CachedSamples.Num() > 0)
		{
			TOptional<FVector2D> PreviewBSInput = GetWidgetPosFromEditorPos(PreviewInput.Get(), WindowRect);
			if (PreviewBSInput.IsSet())
			{
				DrawSamplePoint(PreviewBSInput.GetValue(), EBlendSpaceSampleState::Highlighted, AllottedGeometry, MyClippingRect, OutDrawElements, HighlightLayer);
			}
		}
		
		if(const FLineElement* LineElement = ElementGenerator.GetLineElementForBlendInput(PreviewInput.Get()))
			{
				TOptional<FVector2D> Start = GetWidgetPosFromEditorPos(LineElement->Start.Point, WindowRect);
				TOptional<FVector2D> End = GetWidgetPosFromEditorPos(LineElement->End.Point, WindowRect);
				if(Start.IsSet() && End.IsSet())
				{
					LineBatcher.AddLine(Start.GetValue(), End.GetValue(), HighlightColor, HighlightLayer);

					FEditorElement TempElement;
					LineElement->PopulateElement(PreviewInput.Get().X, TempElement);

					FVector2D TextOffset = GetBlendSpace()->bDisplayEditorVertically ? FVector2D(16,0) : FVector2D(0,16);

					FNumberFormattingOptions Options;
					Options.MinimumFractionalDigits = 5;

					for(int Index = 0; Index < FEditorElement::MAX_VERTICES; ++Index)
					{
						if(TempElement.Indices[Index] == LineElement->Start.Index)
						{
							DrawText( Start.GetValue() + TextOffset, FText::AsNumber(TempElement.Weights[Index], &Options), AllottedGeometry, MyClippingRect, HighlightColor, OutDrawElements, HighlightLayer);
						}
						else if (TempElement.Indices[Index] == LineElement->End.Index)
						{
							DrawText( End.GetValue() + TextOffset, FText::AsNumber(TempElement.Weights[Index], &Options), AllottedGeometry, MyClippingRect, HighlightColor, OutDrawElements, HighlightLayer);
						}
					}
				}
			}
		}

	DrawSamplePoints(WindowRect, AllottedGeometry, MyClippingRect, OutDrawElements, SampleLayer, HighlightedSampleIndex);
	
	LineBatcher.Draw(AllottedGeometry, MyClippingRect, OutDrawElements);

	return TooltipLayer + 1;
}

TOptional<FVector2D> SBlendSpace1DWidget::GetWidgetPosFromEditorPos(const FVector& EditorPos, const FSlateRect& WindowRect) const
{
	TOptional<FVector2D> OutWidgetPos;

	check(EditorPos.Y == 0.0f && EditorPos.Z == 0.0f);

	const FBlendParameter& Param = BlendSpace->GetBlendParameter(0);

	FEditorSpaceConverter Converter(Param.Min, Param.Max, WindowRect, GetBlendSpace()->bDisplayEditorVertically);

	if(Converter.IsInEditorRange(EditorPos))
	{
		FVector2D WidgetPos = Converter.EditorSpaceToWidgetSpace(EditorPos);
		OutWidgetPos = TOptional<FVector2D>(WidgetPos);
	}

	return OutWidgetPos;
}

TOptional<FVector> SBlendSpace1DWidget::GetEditorPosFromWidgetPos(const FVector2D& WidgetPos, const FSlateRect& WindowRect) const
{
	TOptional<FVector> OutGridPos;

	const FBlendParameter& Param = BlendSpace->GetBlendParameter(0);
	FEditorSpaceConverter Converter(Param.Min, Param.Max, WindowRect, GetBlendSpace()->bDisplayEditorVertically);

	if (Converter.IsInWidgetRange(WidgetPos))
	{
		FVector GridPos = Converter.WidgetSpaceToEditorSpace(WidgetPos);
		OutGridPos = TOptional<FVector>(GridPos);
	}

	return OutGridPos;
}

FVector SBlendSpace1DWidget::SnapEditorPosToGrid(const FVector& InPos) const
{
	FVector SnappedVector(0.0f);
	const FBlendParameter& Param = BlendSpace->GetBlendParameter(0);

	const float EditorStep = Param.GetRange() / (float)Param.GridNum;
	const float DiffFromStart = InPos.X - Param.Min;
	const float SnappedToStep = FMath::RoundToFloat(DiffFromStart / EditorStep) * EditorStep;

	SnappedVector.X = SnappedToStep + Param.Min;
	return SnappedVector;
}

void SBlendSpace1DWidget::ResampleData()
{
	SBlendSpaceWidget::ResampleData();

	const FBlendParameter& BlendParam = BlendSpace->GetBlendParameter(0);

	ElementGenerator.Init(BlendParam.Min, BlendParam.Max, BlendParam.GridNum+1);

	// If we have samples
	if (CachedSamples.Num())
	{
		for (int32 I=0; I<CachedSamples.Num(); ++I)
		{
			ElementGenerator.AddSamplePoint(CachedSamples[I].SampleValue);
		}

		ElementGenerator.CalculateEditorElements();

		BlendSpace->FillupGridElements(ElementGenerator.SamplePointList, ElementGenerator.EditorElements);
	}
}

SBlendSpaceEditor1D::~SBlendSpaceEditor1D()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandleDelegateHandle);
}

void SBlendSpaceEditor1D::Construct(const FArguments& InArgs)
{
	SBlendSpaceEditorBase::Construct(SBlendSpaceEditorBase::FArguments()
		.Persona(InArgs._Persona)
		.BlendSpace(InArgs._BlendSpace1D)
		);

	// Register to be notified when properties are edited
	OnPropertyChangedHandle = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateRaw(this, &SBlendSpaceEditor1D::OnPropertyChanged);
	OnPropertyChangedHandleDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedHandle);

	bCachedDisplayVerticalValue = GetBlendSpace()->bDisplayEditorVertically;

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeEditorHeader()
		]

		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			.Expose(EditorSlot)

			+SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(15)
			[
				SAssignNew(NotificationListPtr, SNotificationList)
				.Visibility(EVisibility::HitTestInvisible)
			]
		]
	];

	SAssignNew(BlendSpaceWidget, SBlendSpace1DWidget)
		.Cursor(EMouseCursor::Crosshairs)
		.BlendSpace1D(GetBlendSpace())
		.PreviewInput(this, &SBlendSpaceEditor1D::GetPreviewBlendInput)
		.OnRefreshSamples(this, &SBlendSpaceEditor1D::RefreshSampleDataPanel)
		.OnNotifyUser(this, &SBlendSpaceEditor1D::NotifyUser);

	DisplayOptionsPanel = MakeDisplayOptionsBox();

	SAssignNew(ParameterWidget, SBlendSpaceParameterWidget)
		.OnParametersUpdated(this, &SBlendSpaceEditor1D::OnBlendSpaceParamtersChanged)
		.BlendSpace(BlendSpace);

	SAssignNew(SamplesWidget, SBlendSpaceSamplesWidget)
		.BlendSpace(BlendSpace)
		.OnSamplesUpdated(this, &SBlendSpaceEditor1D::OnBlendSpaceSamplesChanged)
		.OnNotifyUser(this, &SBlendSpaceEditor1D::NotifyUser);

	ParameterWidget->ConstructParameterPanel();
	SamplesWidget->ConstructSamplesPanel();

	SAssignNew(Parameter_Min, STextBlock);
	SAssignNew(Parameter_Max, STextBlock);

	BuildEditorPanel(true);
}

TSharedRef<SWidget> SBlendSpaceEditor1D::MakeDisplayOptionsBox() const
{
	TSharedRef<SWidget> Widget = SBlendSpaceEditorBase::MakeDisplayOptionsBox();
	TSharedRef<SBlendSpaceDisplayOptionsWidget> OptionsBox = StaticCastSharedRef<SBlendSpaceDisplayOptionsWidget>(Widget);

	OptionsBox->AddCheckBoxSlot()
	[
		SNew(SCheckBox) 
		.IsChecked( this, &SBlendSpaceEditor1D::IsEditorDisplayedVertically )
		.OnCheckStateChanged( this, &SBlendSpaceEditor1D::OnChangeDisplayedVertically)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DisplayEditorVerticallyOptionLabel", "Display editor vertically"))
		]
	];
	return Widget;
}

void SBlendSpaceEditor1D::OnBlendSpaceParamtersChanged()
{
	UpdateBlendParameters();
}

void SBlendSpaceEditor1D::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if(UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(ObjectBeingModified))
	{
		BuildEditorPanel();
	}
}


void SBlendSpaceEditor1D::UpdateBlendParameters()
{
	const FBlendParameter& BlendParam = BlendSpace->GetBlendParameter(0);
	FString ParameterName = BlendParam.DisplayName;

	// update UI for parameters
	if (ParameterName==TEXT("None"))
	{
		ParameterName = TEXT("X");
	}

	FText LabelFormat;
	if(GetBlendSpace()->bDisplayEditorVertically)
	{
		LabelFormat = FText::FromString(TEXT("{0}\n[{1}]"));
	}
	else
	{
		LabelFormat = FText::FromString(TEXT("{0}[{1}]"));
	}

	static const FNumberFormattingOptions NumberFormat = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);

	Parameter_Min->SetText(FText::Format(LabelFormat, FText::FromString(ParameterName), FText::AsNumber(BlendParam.Min, &NumberFormat)));
	Parameter_Max->SetText(FText::Format(LabelFormat, FText::FromString(ParameterName), FText::AsNumber(BlendParam.Max, &NumberFormat)));

	BlendSpaceWidget->ResampleData();
}

ECheckBoxState SBlendSpaceEditor1D::IsEditorDisplayedVertically() const
{
	return GetBlendSpace()->bDisplayEditorVertically ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SBlendSpaceEditor1D::OnChangeDisplayedVertically( ECheckBoxState NewValue )
{
	bool bPreviousDisplayValue = GetBlendSpace()->bDisplayEditorVertically;
	switch(NewValue)
	{
		case ECheckBoxState::Checked:
		{
			const FScopedTransaction Transaction( LOCTEXT("Undo_SetDisplayVertically", "Set Blend Space To Display Vertically") );
			BlendSpace->Modify();
			GetBlendSpace()->bDisplayEditorVertically = true;
			break;
		}
		case ECheckBoxState::Unchecked:
		{
			const FScopedTransaction Transaction( LOCTEXT("Undo_UnsetDisplayVertically", "Set Blend Space To Not Display Vertically") );
			BlendSpace->Modify();
			GetBlendSpace()->bDisplayEditorVertically = false;
			break;
		}
		default :
		{
			check(false); //Unexpected value for NewValue
			break;
		}
	}

	if(bPreviousDisplayValue != GetBlendSpace()->bDisplayEditorVertically)
	{
		BuildEditorPanel();
	}
}

FOptionalSize SBlendSpaceEditor1D::GetHeightForOptionsBox() const
{
	float Height = SamplesWidget->GetDesiredSize().Y;

	return FOptionalSize(Height);
}

void SBlendSpaceEditor1D::BuildEditorPanel(bool bForce)
{
	bool bDisplayEditorVertically = GetBlendSpace()->bDisplayEditorVertically;
	if(bForce || (bDisplayEditorVertically != bCachedDisplayVerticalValue))
	{
		FName EditorBrush = TEXT("ToolPanel.GroupBorder");

		bCachedDisplayVerticalValue = bDisplayEditorVertically;
		if(bDisplayEditorVertically)
		{
			(*EditorSlot)
			[
				SNew(SHorizontalBox)
				// Axis labels
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2)
				.HAlign(HAlign_Left)
				[
					SNew(SVerticalBox) 

					+SVerticalBox::Slot()
					.FillHeight(1)
					.VAlign(VAlign_Top)
					[
						Parameter_Max.ToSharedRef()
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Top)
					[
						Parameter_Min.ToSharedRef()
					]
				]
				+SHorizontalBox::Slot() // Editor
				.FillWidth(1)
				.Padding(2)
				[
					SNew(SBorder)
					//.BorderImage(FEditorStyle::GetBrush(EditorBrush))
					[
						BlendSpaceWidget.ToSharedRef()
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2)
				.HAlign(HAlign_Fill)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.FillHeight(1)
					[
						SNew( SBorder )
						[
							SNew(SScrollBox)
							+SScrollBox::Slot()
							[
								// add parameter values
								SNew(SVerticalBox)

								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(5)
								[
									ParameterWidget.ToSharedRef()
								]

								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(5)
								[
									SamplesWidget.ToSharedRef()
								]
							]
						]
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Fill)
					[
						DisplayOptionsPanel.ToSharedRef()
					]
				]
			];
		}
		else // Else display horizontally
		{
			(*EditorSlot)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(5)
				[
					SNew(SBox)
					.HeightOverride(this, &SBlendSpaceEditor1D::GetHeightForOptionsBox)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							SNew( SBorder )
							[
								SNew(SScrollBox)
								+SScrollBox::Slot()
								[
									// add parameter values
									SNew(SVerticalBox)

									+SVerticalBox::Slot()
									.AutoHeight()
									.Padding(5)
									[
										ParameterWidget.ToSharedRef()
									]

									+SVerticalBox::Slot()
									.AutoHeight()
									.Padding(5)
									[
										SamplesWidget.ToSharedRef()
									]
								]
							]
						]
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							DisplayOptionsPanel.ToSharedRef()
						]
					]
				]
				+SVerticalBox::Slot() // Editor
				.AutoHeight()
				.Padding(5.0f)
				[
					SNew(SBorder)
					//.BorderImage(FEditorStyle::GetBrush(EditorBrush))
					[
						BlendSpaceWidget.ToSharedRef()
					]
				]

				//axis labels
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					[
						Parameter_Min.ToSharedRef()
					]

					+SHorizontalBox::Slot()
					.FillWidth(1)
					.HAlign(HAlign_Right)
					[
						Parameter_Max.ToSharedRef()
					]
				]
			];
		}
		UpdateBlendParameters();
	}
}

#undef LOCTEXT_NAMESPACE
