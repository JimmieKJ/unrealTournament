// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "MathStructCustomizations.h"
#include "IPropertyUtilities.h"
#include "SColorPicker.h"
#include "SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "FMathStructCustomization"


TSharedRef<IPropertyTypeCustomization> FMathStructCustomization::MakeInstance() 
{
	return MakeShareable( new FMathStructCustomization );
}

void FMathStructCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	GetSortedChildren( StructPropertyHandle, SortedChildHandles );

	MakeHeaderRow( StructPropertyHandle, HeaderRow );
}

void FMathStructCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	for( int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

		// Add the individual properties as children as well so the vector can be expanded for more room
		StructBuilder.AddChildProperty( ChildHandle );
	}
}

void FMathStructCustomization::MakeHeaderRow( TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row )
{
	// We'll set up reset to default ourselves
	const bool bDisplayResetToDefault = false;
	const FText DisplayNameOverride = FText::GetEmpty();
	const FText DisplayToolTipOverride = FText::GetEmpty();

	TWeakPtr<IPropertyHandle> StructWeakHandlePtr = StructPropertyHandle;

	TSharedPtr<SHorizontalBox> HorizontalBox;

	Row.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget( DisplayNameOverride, DisplayToolTipOverride, bDisplayResetToDefault )
	]
	.ValueContent()
	// Make enough space for each child handle
	.MinDesiredWidth(125.0f * SortedChildHandles.Num() )
	.MaxDesiredWidth(125.0f * SortedChildHandles.Num() )
	[
		SAssignNew( HorizontalBox, SHorizontalBox )
		.IsEnabled( this, &FMathStructCustomization::IsValueEnabled, StructWeakHandlePtr )
	];

	for( int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

		const bool bLastChild = SortedChildHandles.Num()-1 == ChildIndex;
		// Make a widget for each property.  The vector component properties  will be displayed in the header

		HorizontalBox->AddSlot()
		.Padding( FMargin(0.0f, 2.0f, bLastChild ? 0.0f : 3.0f, 2.0f ) )
		[
			MakeChildWidget(StructPropertyHandle, ChildHandle)
		];
	}

	if ( StructPropertyHandle->GetProperty()->HasMetaData("AllowPreserveRatio") )
	{
		if ( !GConfig->GetBool(TEXT("SelectionDetails"), *(StructPropertyHandle->GetProperty()->GetName() + TEXT("_PreserveScaleRatio")), bPreserveScaleRatio, GEditorPerProjectIni) )
		{
			bPreserveScaleRatio = true;
		}

		HorizontalBox->AddSlot()
		.AutoWidth()
		.MaxWidth( 18.0f )
		[
			// Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
			SNew( SCheckBox )
			.IsChecked(this, &FMathStructCustomization::IsPreserveScaleRatioChecked)
			.OnCheckStateChanged(this, &FMathStructCustomization::OnPreserveScaleRatioToggled, StructWeakHandlePtr)
			.Style( FEditorStyle::Get(), "TransparentCheckBox" )
			.ToolTipText( LOCTEXT("PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled" ) )
			[
				SNew( SImage )
				.Image(this, &FMathStructCustomization::GetPreserveScaleRatioImage)
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		];
	}
}

const FSlateBrush* FMathStructCustomization::GetPreserveScaleRatioImage() const
{
	return bPreserveScaleRatio ? FEditorStyle::GetBrush(TEXT("GenericLock")) : FEditorStyle::GetBrush(TEXT("GenericUnlock"));
}

ECheckBoxState FMathStructCustomization::IsPreserveScaleRatioChecked() const
{
	return bPreserveScaleRatio ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMathStructCustomization::OnPreserveScaleRatioToggled(ECheckBoxState NewState, TWeakPtr<IPropertyHandle> PropertyHandle)
{
	bPreserveScaleRatio = ( NewState == ECheckBoxState::Checked ) ? true : false;

	if ( PropertyHandle.IsValid() )
	{
		FString SettingKey = ( PropertyHandle.Pin()->GetProperty()->GetName() + TEXT("_PreserveScaleRatio") );
		GConfig->SetBool(TEXT("SelectionDetails"), *SettingKey, bPreserveScaleRatio, GEditorPerProjectIni);
	}
}

void FMathStructCustomization::GetSortedChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren )
{
	OutChildren.Empty();

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		OutChildren.Add( StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef() );
	}
}

template<typename NumericType>
void ExtractNumericMetadata(TSharedRef<IPropertyHandle>& PropertyHandle, TOptional<NumericType>& MinValue, TOptional<NumericType>& MaxValue, TOptional<NumericType>& SliderMinValue, TOptional<NumericType>& SliderMaxValue, NumericType& SliderExponent, NumericType& Delta)
{
	UProperty* Property = PropertyHandle->GetProperty();

	const FString& MetaUIMinString = Property->GetMetaData(TEXT("UIMin"));
	const FString& MetaUIMaxString = Property->GetMetaData(TEXT("UIMax"));
	const FString& SliderExponentString = Property->GetMetaData(TEXT("SliderExponent"));
	const FString& DeltaString = Property->GetMetaData(TEXT("Delta"));
	const FString& ClampMinString = Property->GetMetaData(TEXT("ClampMin"));
	const FString& ClampMaxString = Property->GetMetaData(TEXT("ClampMax"));

	// If no UIMin/Max was specified then use the clamp string
	const FString& UIMinString = MetaUIMinString.Len() ? MetaUIMinString : ClampMinString;
	const FString& UIMaxString = MetaUIMaxString.Len() ? MetaUIMaxString : ClampMaxString;

	NumericType ClampMin = TNumericLimits<NumericType>::Lowest();
	NumericType ClampMax = TNumericLimits<NumericType>::Max();

	if ( !ClampMinString.IsEmpty() )
	{
		TTypeFromString<NumericType>::FromString(ClampMin, *ClampMinString);
	}

	if ( !ClampMaxString.IsEmpty() )
	{
		TTypeFromString<NumericType>::FromString(ClampMax, *ClampMaxString);
	}

	NumericType UIMin = TNumericLimits<NumericType>::Lowest();
	NumericType UIMax = TNumericLimits<NumericType>::Max();
	TTypeFromString<NumericType>::FromString(UIMin, *UIMinString);
	TTypeFromString<NumericType>::FromString(UIMax, *UIMaxString);

	SliderExponent = NumericType(1);
	if ( SliderExponentString.Len() )
	{
		TTypeFromString<NumericType>::FromString(SliderExponent, *SliderExponentString);
	}

	Delta = NumericType(0);
	if ( DeltaString.Len() )
	{
		TTypeFromString<NumericType>::FromString(Delta, *DeltaString);
	}

	if ( ClampMin >= ClampMax && ( ClampMinString.Len() || ClampMaxString.Len() ) )
	{
		//UE_LOG(LogPropertyNode, Warning, TEXT("Clamp Min (%s) >= Clamp Max (%s) for Ranged Numeric"), *ClampMinString, *ClampMaxString);
	}

	const NumericType ActualUIMin = FMath::Max(UIMin, ClampMin);
	const NumericType ActualUIMax = FMath::Min(UIMax, ClampMax);

	MinValue = ClampMinString.Len() ? ClampMin : TOptional<NumericType>();
	MaxValue = ClampMaxString.Len() ? ClampMax : TOptional<NumericType>();
	SliderMinValue = ( UIMinString.Len() ) ? ActualUIMin : TOptional<NumericType>();
	SliderMaxValue = ( UIMaxString.Len() ) ? ActualUIMax : TOptional<NumericType>();

	if ( ActualUIMin >= ActualUIMax && ( MetaUIMinString.Len() || MetaUIMaxString.Len() ) )
	{
		//UE_LOG(LogPropertyNode, Warning, TEXT("UI Min (%s) >= UI Max (%s) for Ranged Numeric"), *UIMinString, *UIMaxString);
	}
}

template<typename NumericType>
TSharedRef<SWidget> FMathStructCustomization::MakeNumericWidget(
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle)
{
	TOptional<NumericType> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
	NumericType SliderExponent, Delta;
	ExtractNumericMetadata(StructurePropertyHandle, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta);

	TWeakPtr<IPropertyHandle> WeakHandlePtr = PropertyHandle;

	return 
		SNew( SNumericEntryBox<NumericType> )
		.IsEnabled( this, &FMathStructCustomization::IsValueEnabled, WeakHandlePtr )
		.Value( this, &FMathStructCustomization::OnGetValue, WeakHandlePtr )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.UndeterminedString( NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values") )
		.OnValueCommitted( this, &FMathStructCustomization::OnValueCommitted<NumericType>, WeakHandlePtr )
		.OnValueChanged( this, &FMathStructCustomization::OnValueChanged<NumericType>, WeakHandlePtr )
		.OnBeginSliderMovement( this, &FMathStructCustomization::OnBeginSliderMovement )
		.OnEndSliderMovement( this, &FMathStructCustomization::OnEndSliderMovement<NumericType> )
		.LabelVAlign(VAlign_Center)
		// Only allow spin on handles with one object.  Otherwise it is not clear what value to spin
		.AllowSpin( PropertyHandle->GetNumOuterObjects() == 1 )
		.MinValue(MinValue)
		.MaxValue(MaxValue)
		.MinSliderValue(SliderMinValue)
		.MaxSliderValue(SliderMaxValue)
		.SliderExponent(SliderExponent)
		.Delta(Delta)
		.Label()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( PropertyHandle->GetPropertyDisplayName() )
		];
}

TSharedRef<SWidget> FMathStructCustomization::MakeChildWidget( 
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle )
{
	const UClass* PropertyClass = PropertyHandle->GetPropertyClass();
	
	if (PropertyClass == UFloatProperty::StaticClass())
	{
		return MakeNumericWidget<float>(StructurePropertyHandle, PropertyHandle);
	}
	
	if (PropertyClass == UIntProperty::StaticClass())
	{
		return MakeNumericWidget<int32>(StructurePropertyHandle, PropertyHandle);
	}

	if (PropertyClass == UByteProperty::StaticClass())
	{
		return MakeNumericWidget<uint8>(StructurePropertyHandle, PropertyHandle);
	}

	check(0); // Unsupported class
	return SNullWidget::NullWidget;
}

template<typename NumericType>
TOptional<NumericType> FMathStructCustomization::OnGetValue( TWeakPtr<IPropertyHandle> WeakHandlePtr ) const
{
	NumericType NumericVal = 0;
	if( WeakHandlePtr.Pin()->GetValue( NumericVal ) == FPropertyAccess::Success )
	{
		return TOptional<NumericType>( NumericVal );
	}

	// Value couldn't be accessed.  Return an unset value
	return TOptional<NumericType>();
}

template<typename NumericType>
void FMathStructCustomization::OnValueCommitted( NumericType NewValue, ETextCommit::Type CommitType, TWeakPtr<IPropertyHandle> WeakHandlePtr )
{
	EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags;
	SetValue(NewValue, Flags, WeakHandlePtr);
}	

template<typename NumericType>
void FMathStructCustomization::OnValueChanged( NumericType NewValue, TWeakPtr<IPropertyHandle> WeakHandlePtr )
{
	if( bIsUsingSlider )
	{
		EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::InteractiveChange;
		SetValue(NewValue, Flags, WeakHandlePtr);
	}
}

template<typename NumericType>
void FMathStructCustomization::SetValue(NumericType NewValue, EPropertyValueSetFlags::Type Flags, TWeakPtr<IPropertyHandle> WeakHandlePtr)
{
	if ( bPreserveScaleRatio )
	{
		// Get the value for each object for the modified component
		TArray<FString> OldValues;
		if ( WeakHandlePtr.Pin()->GetPerObjectValues(OldValues) == FPropertyAccess::Success )
		{
			// Loop through each object and scale based on the new ratio for each object individually
			for ( int32 OutputIndex = 0; OutputIndex < OldValues.Num(); ++OutputIndex )
			{
				NumericType OldValue;
				TTypeFromString<NumericType>::FromString(OldValue, *OldValues[OutputIndex]);

				// Account for the previous scale being zero.  Just set to the new value in that case?
				NumericType Ratio = OldValue == 0 ? NewValue : NewValue / OldValue;
				if ( Ratio == 0 )
				{
					Ratio = NewValue;
				}

				// Loop through all the child handles (each component of the math struct, like X, Y, Z...etc)
				for ( int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex )
				{
					// Ignore scaling our selves.
					TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];
					if ( ChildHandle != WeakHandlePtr.Pin() )
					{
						// Get the value for each object.
						TArray<FString> ObjectChildValues;
						if ( ChildHandle->GetPerObjectValues(ObjectChildValues) == FPropertyAccess::Success )
						{
							// Individually scale each object's components by the same ratio.
							for ( int32 ChildOutputIndex = 0; ChildOutputIndex < ObjectChildValues.Num(); ++ChildOutputIndex )
							{
								NumericType ChildOldValue;
								TTypeFromString<NumericType>::FromString(ChildOldValue, *ObjectChildValues[ChildOutputIndex]);

								NumericType ChildNewValue = ChildOldValue * Ratio;
								ObjectChildValues[ChildOutputIndex] = TTypeToString<NumericType>::ToSanitizedString(ChildNewValue);
							}

							ChildHandle->SetPerObjectValues(ObjectChildValues);
						}
					}
				}
			}
		}
	}

	WeakHandlePtr.Pin()->SetValue(NewValue, Flags);
}

bool FMathStructCustomization::IsValueEnabled(TWeakPtr<IPropertyHandle> WeakHandlePtr) const
{
	if ( WeakHandlePtr.IsValid() )
	{
		return !WeakHandlePtr.Pin()->IsEditConst();
	}

	return false;
}

void FMathStructCustomization::OnBeginSliderMovement()
{
	bIsUsingSlider = true;

	GEditor->BeginTransaction( NSLOCTEXT("FMathStructCustomization", "SetVectorProperty", "Set Vector Property") );
}

template<typename NumericType>
void FMathStructCustomization::OnEndSliderMovement( NumericType NewValue )
{
	bIsUsingSlider = false;

	GEditor->EndTransaction();
}

TSharedRef<IPropertyTypeCustomization> FRotatorStructCustomization::MakeInstance() 
{
	return MakeShareable( new FRotatorStructCustomization );
}

void FRotatorStructCustomization::GetSortedChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren )
{
	static const FName Roll("Roll");
	static const FName Pitch("Pitch");
	static const FName Yaw("Yaw");

	TSharedPtr< IPropertyHandle > RotatorChildren[3];

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if( PropertyName == Roll )
		{
			RotatorChildren[0] = ChildHandle;
		}
		else if( PropertyName == Pitch )
		{
			RotatorChildren[1] = ChildHandle;
		}
		else
		{
			check( PropertyName == Yaw );
			RotatorChildren[2] = ChildHandle;
		}
	}

	OutChildren.Add( RotatorChildren[0].ToSharedRef() );
	OutChildren.Add( RotatorChildren[1].ToSharedRef() );
	OutChildren.Add( RotatorChildren[2].ToSharedRef() );
}


TSharedRef<IPropertyTypeCustomization> FColorStructCustomization::MakeInstance() 
{
	return MakeShareable( new FColorStructCustomization );
}

void FColorStructCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	StructPropertyHandle = InStructPropertyHandle;

	bIsLinearColor = CastChecked<UStructProperty>( StructPropertyHandle->GetProperty() )->Struct->GetFName() == NAME_LinearColor;
	bIgnoreAlpha = StructPropertyHandle->GetProperty()->HasMetaData(TEXT("HideAlphaChannel"));

	auto PropertyUtils = StructCustomizationUtils.GetPropertyUtilities();
	bDontUpdateWhileEditing = PropertyUtils.IsValid() ? PropertyUtils->DontUpdateValueWhileEditing() : false;

	FMathStructCustomization::CustomizeHeader( InStructPropertyHandle, InHeaderRow, StructCustomizationUtils );
}

void FColorStructCustomization::MakeHeaderRow( TSharedRef<class IPropertyHandle>& InStructPropertyHandle, FDetailWidgetRow& Row )
{
	// We'll set up reset to default ourselves
	const bool bDisplayResetToDefault = false;
	const FText DisplayNameOverride = FText::GetEmpty();
	const FText DisplayToolTipOverride = FText::GetEmpty();

	Row.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget( DisplayNameOverride, DisplayToolTipOverride, bDisplayResetToDefault )
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(250.0f)
	[
		CreateColorWidget()
	];
}

TSharedRef<SWidget> FColorStructCustomization::CreateColorWidget()
{
	FSlateFontInfo NormalText = IDetailLayoutBuilder::GetDetailFont();

	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew( SOverlay )
			+SOverlay::Slot()
			[
				// Displays the color with alpha unless it is ignored
				SAssignNew( ColorPickerParentWidget, SColorBlock )
				.Color( this, &FColorStructCustomization::OnGetColorForColorBlock )
				.ShowBackgroundForAlpha(true)
				.IgnoreAlpha( bIgnoreAlpha )
				.OnMouseButtonDown( this, &FColorStructCustomization::OnMouseButtonDownColorBlock )
				.Size( FVector2D( 35.0f, 12.0f ) )
			]
			+SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
				.Font(NormalText)
				.ColorAndOpacity(FSlateColor(FLinearColor::Black)) // we know the background is always white, so can safely set this to black
				.Visibility(this, &FColorStructCustomization::GetMultipleValuesTextVisibility)
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			// Displays the color without alpha
			SNew( SColorBlock )
			.Color( this, &FColorStructCustomization::OnGetColorForColorBlock )
			.ShowBackgroundForAlpha(false)
			.IgnoreAlpha(true)
			.OnMouseButtonDown( this, &FColorStructCustomization::OnMouseButtonDownColorBlock )
			.Size( FVector2D( 35.0f, 12.0f ) )
		];
}

void FColorStructCustomization::GetSortedChildren( TSharedRef<IPropertyHandle> InStructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren )
{
	static const FName Red("R");
	static const FName Green("G");
	static const FName Blue("B");
	static const FName Alpha("A");

	// We control the order of the colors via this array so it always ends up R,G,B,A
	TSharedPtr< IPropertyHandle > ColorProperties[4];

	uint32 NumChildren;
	InStructPropertyHandle->GetNumChildren( NumChildren );

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = InStructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if( PropertyName == Red )
		{
			ColorProperties[0] = ChildHandle;
		}
		else if( PropertyName == Green )
		{
			ColorProperties[1] = ChildHandle;
		}
		else if( PropertyName == Blue )
		{
			ColorProperties[2] = ChildHandle;
		}
		else
		{
			ColorProperties[3] = ChildHandle;
		}
	}

	OutChildren.Add( ColorProperties[0].ToSharedRef() );
	OutChildren.Add( ColorProperties[1].ToSharedRef() );
	OutChildren.Add( ColorProperties[2].ToSharedRef() );

	// Alpha channel may not be used
	if( !bIgnoreAlpha && ColorProperties[3].IsValid() )
	{
		OutChildren.Add( ColorProperties[3].ToSharedRef() );
	}
}

void FColorStructCustomization::CreateColorPicker( bool bUseAlpha, bool bOnlyRefreshOnOk )
{
	int32 NumObjects = StructPropertyHandle->GetNumOuterObjects();

	SavedPreColorPickerColors.Empty();
	TArray<FString> PerObjectValues;
	StructPropertyHandle->GetPerObjectValues( PerObjectValues );

	for( int32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex )
	{
		if( bIsLinearColor )
		{
			FLinearColor Color;
			Color.InitFromString( PerObjectValues[ObjectIndex] );
			SavedPreColorPickerColors.Add( Color );	
		}
		else
		{
			FColor Color;
			Color.InitFromString( PerObjectValues[ObjectIndex] );
			SavedPreColorPickerColors.Add( Color.ReinterpretAsLinear() );
		}
	}

	FLinearColor InitialColor;
	GetColorAsLinear(InitialColor);

	// This needs to be meta data.  Other colors could benefit from this
	const bool bRefreshOnlyOnOk = bOnlyRefreshOnOk || StructPropertyHandle->GetProperty()->GetOwnerClass()->IsChildOf(UMaterialExpressionConstant3Vector::StaticClass());

	FColorPickerArgs PickerArgs;
	PickerArgs.bUseAlpha = !bIgnoreAlpha;
	PickerArgs.bOnlyRefreshOnMouseUp = false;
	PickerArgs.bOnlyRefreshOnOk = bRefreshOnlyOnOk;
	PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP( this, &FColorStructCustomization::OnSetColorFromColorPicker );
	PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP( this, &FColorStructCustomization::OnColorPickerCancelled );
	PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP( this, &FColorStructCustomization::OnColorPickerInteractiveBegin );
	PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP( this, &FColorStructCustomization::OnColorPickerInteractiveEnd );
	PickerArgs.InitialColorOverride = InitialColor;
	PickerArgs.ParentWidget = ColorPickerParentWidget;

	OpenColorPicker(PickerArgs);
}

TSharedRef<SColorPicker> FColorStructCustomization::CreateInlineColorPicker()
{
	int32 NumObjects = StructPropertyHandle->GetNumOuterObjects();

	SavedPreColorPickerColors.Empty();
	TArray<FString> PerObjectValues;
	StructPropertyHandle->GetPerObjectValues( PerObjectValues );

	for( int32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex )
	{
		if( bIsLinearColor )
		{
			FLinearColor Color;
			Color.InitFromString( PerObjectValues[ObjectIndex] );
			SavedPreColorPickerColors.Add( Color );	
		}
		else
		{
			FColor Color;
			Color.InitFromString( PerObjectValues[ObjectIndex] );
			SavedPreColorPickerColors.Add( Color.ReinterpretAsLinear() );
		}
	}

	FLinearColor InitialColor;
	GetColorAsLinear(InitialColor);

	// This needs to be meta data.  Other colors could benefit from this
	const bool bRefreshOnlyOnOk = bDontUpdateWhileEditing || StructPropertyHandle->GetProperty()->GetOwnerClass()->IsChildOf(UMaterialExpressionConstant3Vector::StaticClass());
	
	return SNew(SColorPicker)
		.Visibility(this, &FColorStructCustomization::GetInlineColorPickerVisibility)
		.DisplayInlineVersion(true)
		.OnlyRefreshOnMouseUp(false)
		.OnlyRefreshOnOk(bRefreshOnlyOnOk)
		.DisplayGamma(TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) ))
		.OnColorCommitted(FOnLinearColorValueChanged::CreateSP( this, &FColorStructCustomization::OnSetColorFromColorPicker ))
		.OnColorPickerCancelled(FOnColorPickerCancelled::CreateSP( this, &FColorStructCustomization::OnColorPickerCancelled ))
		.OnInteractivePickBegin(FSimpleDelegate::CreateSP( this, &FColorStructCustomization::OnColorPickerInteractiveBegin ))
		.OnInteractivePickEnd(FSimpleDelegate::CreateSP( this, &FColorStructCustomization::OnColorPickerInteractiveEnd ))
		.TargetColorAttribute(InitialColor);
}

void FColorStructCustomization::OnSetColorFromColorPicker( FLinearColor NewColor )
{
	FString ColorString;
	if( bIsLinearColor )
	{
		ColorString = NewColor.ToString();
	}
	else
	{
		// Handled by the color picker
		const bool bSRGB = false;
		FColor NewFColor = NewColor.ToFColor(bSRGB);
		ColorString = NewFColor.ToString();
	}

	StructPropertyHandle->SetValueFromFormattedString( ColorString, bIsInteractive ? EPropertyValueSetFlags::InteractiveChange : 0 );
}

void FColorStructCustomization::OnColorPickerCancelled( FLinearColor OriginalColor )
{
	TArray<FString> PerObjectColors;

	for( int32 ColorIndex = 0; ColorIndex < SavedPreColorPickerColors.Num(); ++ColorIndex )
	{
		if( bIsLinearColor )
		{
			PerObjectColors.Add( SavedPreColorPickerColors[ColorIndex].ToString() );
		}
		else
		{
			const bool bSRGB = false;
			FColor Color = SavedPreColorPickerColors[ColorIndex].ToFColor( bSRGB );
			PerObjectColors.Add( Color.ToString() );
		}
	}

	StructPropertyHandle->SetPerObjectValues( PerObjectColors );

	PerObjectColors.Empty();
}

void FColorStructCustomization::OnColorPickerInteractiveBegin()
{
	bIsInteractive = true;

	GEditor->BeginTransaction( FText::Format( NSLOCTEXT("FColorStructCustomization", "SetColorProperty", "Edit {0}"), StructPropertyHandle->GetPropertyDisplayName() ) );
}

void FColorStructCustomization::OnColorPickerInteractiveEnd()
{
	bIsInteractive = false;

	if (!bDontUpdateWhileEditing)
	{
		// pushes the last value from the interactive change without the interactive flag
		FString ColorString;
		StructPropertyHandle->GetValueAsFormattedString(ColorString);
		StructPropertyHandle->SetValueFromFormattedString(ColorString);
	}

	GEditor->EndTransaction();
}

FLinearColor FColorStructCustomization::OnGetColorForColorBlock() const
{
	FLinearColor Color;
	GetColorAsLinear(Color);
	return Color;
}

FPropertyAccess::Result FColorStructCustomization::GetColorAsLinear(FLinearColor& OutColor) const
{
	// Get each color component 
	for(int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
	{
		FPropertyAccess::Result ValueResult = FPropertyAccess::Fail;

		if(bIsLinearColor)
		{
			float ComponentValue = 0;
			ValueResult = SortedChildHandles[ChildIndex]->GetValue(ComponentValue);

			OutColor.Component(ChildIndex) = ComponentValue;
		}
		else
		{
			uint8 ComponentValue = 0;
			ValueResult = SortedChildHandles[ChildIndex]->GetValue(ComponentValue);

			// Convert the FColor to a linear equivalent
			OutColor.Component(ChildIndex) = ComponentValue/255.0f;
		}

		switch(ValueResult)
		{
		case FPropertyAccess::MultipleValues:
			{
				// Default the color to white if we've got multiple values selected
				OutColor = FLinearColor::White;
				return FPropertyAccess::MultipleValues;
			}

		case FPropertyAccess::Fail:
			return FPropertyAccess::Fail;

		default:
			break;
		}
	}

	// If we've got this far, we have to have successfully read a color with a single value
	return FPropertyAccess::Success;
}

EVisibility FColorStructCustomization::GetMultipleValuesTextVisibility() const
{
	FLinearColor Color;
	const FPropertyAccess::Result ValueResult = GetColorAsLinear(Color);
	return (ValueResult == FPropertyAccess::MultipleValues) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FColorStructCustomization::OnMouseButtonDownColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton )
	{
		return FReply::Unhandled();
	}
	
	CreateColorPicker( /*bUseAlpha*/ true, bDontUpdateWhileEditing);

	//bIsInlineColorPickerVisible = !bIsInlineColorPickerVisible;

	return FReply::Handled();
}

EVisibility FColorStructCustomization::GetInlineColorPickerVisibility() const
{
	return bIsInlineColorPickerVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FColorStructCustomization::OnOpenFullColorPickerClicked()
{
	CreateColorPicker( /*bUseAlpha*/ true, bDontUpdateWhileEditing);

	bIsInlineColorPickerVisible = false;

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
