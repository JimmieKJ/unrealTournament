// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ComponentTransformDetails.h"
#include "SVectorInputBox.h"
#include "SRotatorInputBox.h"
#include "PropertyCustomizationHelpers.h"
#include "ActorEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/ComponentEditorUtils.h"
#include "ScopedTransaction.h"
#include "IPropertyUtilities.h"

#include "UnitConversion.h"
#include "NumericUnitTypeInterface.inl"

#define LOCTEXT_NAMESPACE "FComponentTransformDetails"

class FScopedSwitchWorldForObject
{
public:
	FScopedSwitchWorldForObject( UObject* Object )
		: PrevWorld( NULL )
	{
		bool bRequiresPlayWorld = false;
		if( GUnrealEd->PlayWorld && !GIsPlayInEditorWorld )
		{
			UPackage* ObjectPackage = Object->GetOutermost();
			bRequiresPlayWorld = !!(ObjectPackage->PackageFlags & PKG_PlayInEditor);
		}

		if( bRequiresPlayWorld )
		{
			PrevWorld = SetPlayInEditorWorld( GUnrealEd->PlayWorld );
		}
	}

	~FScopedSwitchWorldForObject()
	{
		if( PrevWorld )
		{
			RestoreEditorWorld( PrevWorld );
		}
	}

private:
	UWorld* PrevWorld;
};

static USceneComponent* GetSceneComponentFromDetailsObject(UObject* InObject)
{
	AActor* Actor = Cast<AActor>(InObject);
	if(Actor)
	{
		return Actor->GetRootComponent();
	}
	
	return Cast<USceneComponent>(InObject);
}

FComponentTransformDetails::FComponentTransformDetails( const TArray< TWeakObjectPtr<UObject> >& InSelectedObjects, const FSelectedActorInfo& InSelectedActorInfo, IDetailLayoutBuilder& DetailBuilder )
	: TNumericUnitTypeInterface(EUnit::Centimeters)
	, SelectedActorInfo( InSelectedActorInfo )
	, SelectedObjects( InSelectedObjects )
	, NotifyHook( DetailBuilder.GetPropertyUtilities()->GetNotifyHook() )
	, bPreserveScaleRatio( false )
	, bEditingRotationInUI( false )
	, HiddenFieldMask( 0 )
{
	GConfig->GetBool(TEXT("SelectionDetails"), TEXT("PreserveScaleRatio"), bPreserveScaleRatio, GEditorPerProjectIni);

}

TSharedRef<SWidget> FComponentTransformDetails::BuildTransformFieldLabel( ETransformField::Type TransformField )
{
	FText Label;
	switch( TransformField )
	{
	case ETransformField::Rotation:
		Label = LOCTEXT( "RotationLabel", "Rotation");
		break;
	case ETransformField::Scale:
		Label = LOCTEXT( "ScaleLabel", "Scale" );
		break;
	case ETransformField::Location:
	default:
		Label = LOCTEXT("LocationLabel", "Location");
		break;
	}

	FMenuBuilder MenuBuilder( true, NULL, NULL );

	FUIAction SetRelativeLocationAction
	(
		FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnSetRelativeTransform, TransformField ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FComponentTransformDetails::IsRelativeTransformChecked, TransformField )
	);

	FUIAction SetWorldLocationAction
	(
		FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnSetWorldTransform, TransformField ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FComponentTransformDetails::IsWorldTransformChecked, TransformField )
	);

	MenuBuilder.BeginSection( TEXT("TransformType"), FText::Format( LOCTEXT("TransformType", "{0} Type"), Label ) );

	MenuBuilder.AddMenuEntry
	(
		FText::Format( LOCTEXT( "RelativeLabel", "Relative"), Label ),
		FText::Format( LOCTEXT( "RelativeLabel_ToolTip", "{0} is relative to its parent"), Label ),
		FSlateIcon(),
		SetRelativeLocationAction,
		NAME_None, 
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry
	(
		FText::Format( LOCTEXT( "WorldLabel", "World"), Label ),
		FText::Format( LOCTEXT( "WorldLabel_ToolTip", "{0} is relative to the world"), Label ),
		FSlateIcon(),
		SetWorldLocationAction,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.EndSection();


	return 
		SNew(SComboButton)
		.ContentPadding( 0 )
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ForegroundColor( FSlateColor::UseForeground() )
		.MenuContent()
		[
			MenuBuilder.MakeWidget()
		]
		.ButtonContent()
		[
			SNew( SBox )
			.Padding( FMargin( 0.0f, 0.0f, 2.0f, 0.0f ) )
			[
				SNew(STextBlock)
				.Text(this, &FComponentTransformDetails::GetTransformFieldText, TransformField)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

FText FComponentTransformDetails::GetTransformFieldText( ETransformField::Type TransformField ) const
{
	switch (TransformField)
	{
	case ETransformField::Location:
		return GetLocationText();
		break;
	case ETransformField::Rotation:
		return GetRotationText();
		break;
	case ETransformField::Scale:
		return GetScaleText();
		break;
	default:
		return FText::GetEmpty();
		break;
	}
}

void FComponentTransformDetails::OnSetRelativeTransform(  ETransformField::Type TransformField )
{
	const bool bEnable = false;
	switch (TransformField)
	{
	case ETransformField::Location:
		OnToggleAbsoluteLocation(bEnable);
		break;
	case ETransformField::Rotation:
		OnToggleAbsoluteRotation( bEnable );
		break;
	case ETransformField::Scale:
		OnToggleAbsoluteScale( bEnable );
		break;
	default:
		break;
	}
}

bool FComponentTransformDetails::IsRelativeTransformChecked( ETransformField::Type TransformField ) const
{
	switch (TransformField)
	{
	case ETransformField::Location:
		return !bAbsoluteLocation;
		break;
	case ETransformField::Rotation:
		return !bAbsoluteRotation;
		break;
	case ETransformField::Scale:
		return !bAbsoluteScale;
		break;
	default:
		return false;
		break;
	}
}

void FComponentTransformDetails::OnSetWorldTransform( ETransformField::Type TransformField )
{
	const bool bEnable = true;
	switch (TransformField)
	{
	case ETransformField::Location:
		OnToggleAbsoluteLocation(bEnable);
		break;
	case ETransformField::Rotation:
		OnToggleAbsoluteRotation(bEnable);
		break;
	case ETransformField::Scale:
		OnToggleAbsoluteScale(bEnable);
		break;
	default:
		break;
	}
}

bool FComponentTransformDetails::IsWorldTransformChecked( ETransformField::Type TransformField ) const
{
	return !IsRelativeTransformChecked( TransformField );
}

bool FComponentTransformDetails::OnCanCopy( ETransformField::Type TransformField ) const
{
	// We can only copy values if the whole field is set.  If multiple values are defined we do not copy since we are unable to determine the value
	switch (TransformField)
	{
	case ETransformField::Location:
		return CachedLocation.IsSet();
		break;
	case ETransformField::Rotation:
		return CachedRotation.IsSet();
		break;
	case ETransformField::Scale:
		return CachedScale.IsSet();
		break;
	default:
		return false;
		break;
	}
}

void FComponentTransformDetails::OnCopy( ETransformField::Type TransformField )
{
	CacheTransform();

	FString CopyStr;
	switch (TransformField)
	{
	case ETransformField::Location:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), CachedLocation.X.GetValue(), CachedLocation.Y.GetValue(), CachedLocation.Z.GetValue());
		break;
	case ETransformField::Rotation:
		CopyStr = FString::Printf(TEXT("(Pitch=%f,Yaw=%f,Roll=%f)"), CachedRotation.Y.GetValue(), CachedRotation.Z.GetValue(), CachedRotation.X.GetValue());
		break;
	case ETransformField::Scale:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), CachedScale.X.GetValue(), CachedScale.Y.GetValue(), CachedScale.Z.GetValue());
		break;
	default:
		break;
	}

	if( !CopyStr.IsEmpty() )
	{
		FPlatformMisc::ClipboardCopy( *CopyStr );
	}
}

void FComponentTransformDetails::OnPaste( ETransformField::Type TransformField )
{
	FString PastedText;
	FPlatformMisc::ClipboardPaste(PastedText);

	switch (TransformField)
	{
		case ETransformField::Location:
		{
			FVector Location;
			if (Location.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteLocation", "Paste Location"));
				OnSetLocation(Location.X, ETextCommit::Default, 0);
				OnSetLocation(Location.Y, ETextCommit::Default, 1);
				OnSetLocation(Location.Z, ETextCommit::Default, 2);
			}
		}
		break;
	case ETransformField::Rotation:
		{
			FRotator Rotation;
			PastedText.ReplaceInline(TEXT("Pitch="), TEXT("P="));
			PastedText.ReplaceInline(TEXT("Yaw="), TEXT("Y="));
			PastedText.ReplaceInline(TEXT("Roll="), TEXT("R="));
			if (Rotation.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteRotation", "Paste Rotation"));
				OnSetRotation(Rotation.Roll, ETextCommit::Default, 0);
				OnSetRotation(Rotation.Pitch, ETextCommit::Default, 1);
				OnSetRotation(Rotation.Yaw, ETextCommit::Default, 2);
			}
		}
		break;
	case ETransformField::Scale:
		{
			FVector Scale;
			if (Scale.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteScale", "Paste Scale"));
				OnSetScale(Scale.X, ETextCommit::Default, 0);
				OnSetScale(Scale.Y, ETextCommit::Default, 1);
				OnSetScale(Scale.Z, ETextCommit::Default, 2);
			}
		}
		break;
	default:
		break;
	}
}

FUIAction FComponentTransformDetails::CreateCopyAction( ETransformField::Type TransformField ) const
{
	return
		FUIAction
		(
			FExecuteAction::CreateSP(this, &FComponentTransformDetails::OnCopy, TransformField ),
			FCanExecuteAction::CreateSP(this, &FComponentTransformDetails::OnCanCopy, TransformField )
		);
}

FUIAction FComponentTransformDetails::CreatePasteAction( ETransformField::Type TransformField ) const
{
	return 
		 FUIAction( FExecuteAction::CreateSP(this, &FComponentTransformDetails::OnPaste, TransformField ) );
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FComponentTransformDetails::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	UClass* SceneComponentClass = USceneComponent::StaticClass();
		
	FSlateFontInfo FontInfo = IDetailLayoutBuilder::GetDetailFont();

	const bool bHideLocationField = ( HiddenFieldMask & ( 1 << ETransformField::Location ) ) != 0;
	const bool bHideRotationField = ( HiddenFieldMask & ( 1 << ETransformField::Rotation ) ) != 0;
	const bool bHideScaleField = ( HiddenFieldMask & ( 1 << ETransformField::Scale ) ) != 0;

	// Location
	if(!bHideLocationField)
	{
		TSharedPtr<INumericTypeInterface<float>> TypeInterface;
		if( FUnitConversion::Settings().ShouldDisplayUnits() )
		{
			TypeInterface = SharedThis(this);
		}

		ChildrenBuilder.AddChildContent( LOCTEXT("LocationFilter", "Location") )
		.CopyAction( CreateCopyAction( ETransformField::Location ) )
		.PasteAction( CreatePasteAction( ETransformField::Location ) )
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			BuildTransformFieldLabel( ETransformField::Location )
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign( VAlign_Center )
			[
				SNew( SVectorInputBox )
				.X( this, &FComponentTransformDetails::GetLocationX )
				.Y( this, &FComponentTransformDetails::GetLocationY )
				.Z( this, &FComponentTransformDetails::GetLocationZ )
				.bColorAxisLabels( true )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXCommitted( this, &FComponentTransformDetails::OnSetLocation, 0 )
				.OnYCommitted( this, &FComponentTransformDetails::OnSetLocation, 1 )
				.OnZCommitted( this, &FComponentTransformDetails::OnSetLocation, 2 )
				.Font( FontInfo )
				.TypeInterface( TypeInterface )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				// Just take up space for alignment
				SNew( SBox )
				.WidthOverride( 18.0f )
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FComponentTransformDetails::OnLocationResetClicked)
				.Visibility(this, &FComponentTransformDetails::GetLocationResetVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.Content()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
				]
			]
		];
	}
	
	// Rotation
	if(!bHideRotationField)
	{
		TSharedPtr<INumericTypeInterface<float>> TypeInterface;
		if( FUnitConversion::Settings().ShouldDisplayUnits() )
		{
			TypeInterface = MakeShareable( new TNumericUnitTypeInterface<float>(EUnit::Degrees) );
		}

		ChildrenBuilder.AddChildContent( LOCTEXT("RotationFilter", "Rotation") )
		.CopyAction( CreateCopyAction(ETransformField::Rotation) )
		.PasteAction( CreatePasteAction(ETransformField::Rotation) )
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			BuildTransformFieldLabel(ETransformField::Rotation)
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign( VAlign_Center )
			[
				SNew( SRotatorInputBox )
				.AllowSpin( SelectedObjects.Num() == 1 ) 
				.Roll( this, &FComponentTransformDetails::GetRotationX )
				.Pitch( this, &FComponentTransformDetails::GetRotationY )
				.Yaw( this, &FComponentTransformDetails::GetRotationZ )
				.bColorAxisLabels( true )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginRotatonSlider )
				.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndRotationSlider )
				.OnRollChanged( this, &FComponentTransformDetails::OnSetRotation, false, 0 )
				.OnPitchChanged( this, &FComponentTransformDetails::OnSetRotation, false, 1 )
				.OnYawChanged( this, &FComponentTransformDetails::OnSetRotation, false, 2 )
				.OnRollCommitted( this, &FComponentTransformDetails::OnRotationCommitted, 0 )
				.OnPitchCommitted( this, &FComponentTransformDetails::OnRotationCommitted, 1 )
				.OnYawCommitted( this, &FComponentTransformDetails::OnRotationCommitted, 2 )
				.TypeInterface(TypeInterface)
				.Font( FontInfo )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				// Just take up space for alignment
				SNew( SBox )
				.WidthOverride( 18.0f )
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FComponentTransformDetails::OnRotationResetClicked)
				.Visibility(this, &FComponentTransformDetails::GetRotationResetVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.Content()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
				]
			]
		];
	}
	
	// Scale
	if(!bHideScaleField)
	{
		ChildrenBuilder.AddChildContent( LOCTEXT("ScaleFilter", "Scale") )
		.CopyAction( CreateCopyAction(ETransformField::Scale) )
		.PasteAction( CreatePasteAction(ETransformField::Scale) )
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			BuildTransformFieldLabel(ETransformField::Scale)
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.VAlign( VAlign_Center )
			.FillWidth(1.0f)
			[
				SNew( SVectorInputBox )
				.X( this, &FComponentTransformDetails::GetScaleX )
				.Y( this, &FComponentTransformDetails::GetScaleY )
				.Z( this, &FComponentTransformDetails::GetScaleZ )
				.bColorAxisLabels( true )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXCommitted( this, &FComponentTransformDetails::OnSetScale, 0 )
				.OnYCommitted( this, &FComponentTransformDetails::OnSetScale, 1 )
				.OnZCommitted( this, &FComponentTransformDetails::OnSetScale, 2 )
				.ContextMenuExtenderX( this, &FComponentTransformDetails::ExtendXScaleContextMenu )
				.ContextMenuExtenderY( this, &FComponentTransformDetails::ExtendYScaleContextMenu )
				.ContextMenuExtenderZ( this, &FComponentTransformDetails::ExtendZScaleContextMenu )
				.Font( FontInfo )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth( 18.0f )
			[
				// Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
				SNew( SCheckBox )
				.IsChecked( this, &FComponentTransformDetails::IsPreserveScaleRatioChecked )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnCheckStateChanged( this, &FComponentTransformDetails::OnPreserveScaleRatioToggled )
				.Style( FEditorStyle::Get(), "TransparentCheckBox" )
				.ToolTipText( LOCTEXT("PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled" ) )
				[
					SNew( SImage )
					.Image( this, &FComponentTransformDetails::GetPreserveScaleRatioImage )
					.ColorAndOpacity( FSlateColor::UseForeground() )
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FComponentTransformDetails::OnScaleResetClicked)
				.Visibility(this, &FComponentTransformDetails::GetScaleResetVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.Content()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
				]
			]
		];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FComponentTransformDetails::Tick( float DeltaTime ) 
{
	CacheTransform();
	if (!FixedDisplayUnits.IsSet())
	{
		CacheCommonLocationUnits();
	}
}

void FComponentTransformDetails::CacheCommonLocationUnits()
{
	float LargestValue = 0.f;
	if (CachedLocation.X.IsSet() && CachedLocation.X.GetValue() > LargestValue)
	{
		LargestValue = CachedLocation.X.GetValue();
	}
	if (CachedLocation.Y.IsSet() && CachedLocation.Y.GetValue() > LargestValue)
	{
		LargestValue = CachedLocation.Y.GetValue();
	}
	if (CachedLocation.Z.IsSet() && CachedLocation.Z.GetValue() > LargestValue)
	{
		LargestValue = CachedLocation.Z.GetValue();
	}

	SetupFixedDisplay(LargestValue);
}

bool FComponentTransformDetails::GetIsEnabled() const
{
	return !GEditor->HasLockedActors() || SelectedActorInfo.NumSelected == 0;
}

const FSlateBrush* FComponentTransformDetails::GetPreserveScaleRatioImage() const
{
	return bPreserveScaleRatio ? FEditorStyle::GetBrush( TEXT("GenericLock") ) : FEditorStyle::GetBrush( TEXT("GenericUnlock") ) ;
}

ECheckBoxState FComponentTransformDetails::IsPreserveScaleRatioChecked() const
{
	return bPreserveScaleRatio ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FComponentTransformDetails::OnPreserveScaleRatioToggled( ECheckBoxState NewState )
{
	bPreserveScaleRatio = (NewState == ECheckBoxState::Checked) ? true : false;
	GConfig->SetBool(TEXT("SelectionDetails"), TEXT("PreserveScaleRatio"), bPreserveScaleRatio, GEditorPerProjectIni);
}

FText FComponentTransformDetails::GetLocationText() const
{
	return bAbsoluteLocation ? LOCTEXT( "AbsoluteLocation", "Absolute Location" ) : LOCTEXT( "Location", "Location" );
}

FText FComponentTransformDetails::GetRotationText() const
{
	return bAbsoluteRotation ? LOCTEXT( "AbsoluteRotation", "Absolute Rotation" ) : LOCTEXT( "Rotation", "Rotation" );
}

FText FComponentTransformDetails::GetScaleText() const
{
	return bAbsoluteScale ? LOCTEXT( "AbsoluteScale", "Absolute Scale" ) : LOCTEXT( "Scale", "Scale" );
}

void FComponentTransformDetails::OnToggleAbsoluteLocation( bool bEnable )
{
	UProperty* AbsoluteLocationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "bAbsoluteLocation" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
			if( SceneComponent && SceneComponent->bAbsoluteLocation != bEnable )
			{
				if( !bBeganTransaction )
				{
					// NOTE: One transaction per change, not per actor
					GEditor->BeginTransaction( LOCTEXT("ToggleAbsoluteLocation", "Toggle Absolute Location" ) );
					bBeganTransaction = true;
				}

				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				Object->PreEditChange( AbsoluteLocationProperty );

				if( NotifyHook )
				{
					NotifyHook->NotifyPreChange( AbsoluteLocationProperty );
				}

				SceneComponent->bAbsoluteLocation = !SceneComponent->bAbsoluteLocation;

				FPropertyChangedEvent PropertyChangedEvent( AbsoluteLocationProperty );
				Object->PostEditChangeProperty(PropertyChangedEvent);

				// If it's a template, propagate the change out to any current instances of the object
				if(SceneComponent->IsTemplate())
				{
					uint32 NewValue = SceneComponent->bAbsoluteLocation;
					uint32 OldValue = !NewValue;
					TSet<USceneComponent*> UpdatedInstances;
					FComponentEditorUtils::PropagateDefaultValueChange(SceneComponent, AbsoluteLocationProperty, OldValue, NewValue, UpdatedInstances);
				}

				if( NotifyHook )
				{
					NotifyHook->NotifyPostChange( PropertyChangedEvent, AbsoluteLocationProperty );
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();

		GUnrealEd->RedrawLevelEditingViewports();
	}

}

void FComponentTransformDetails::OnToggleAbsoluteRotation( bool bEnable )
{
	UProperty* AbsoluteRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "bAbsoluteRotation" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
			if( SceneComponent && SceneComponent->bAbsoluteRotation != bEnable )
			{
				if( !bBeganTransaction )
				{
					// NOTE: One transaction per change, not per actor
					GEditor->BeginTransaction( LOCTEXT("ToggleAbsoluteRotation", "Toggle Absolute Rotation" ) );
					bBeganTransaction = true;
				}

				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				Object->PreEditChange( AbsoluteRotationProperty );

				if( NotifyHook )
				{
					NotifyHook->NotifyPreChange( AbsoluteRotationProperty );
				}

				SceneComponent->bAbsoluteRotation = !SceneComponent->bAbsoluteRotation;

				FPropertyChangedEvent PropertyChangedEvent( AbsoluteRotationProperty );
				Object->PostEditChangeProperty(PropertyChangedEvent);

				// If it's a template, propagate the change out to any current instances of the object
				if(SceneComponent->IsTemplate())
				{
					uint32 NewValue = SceneComponent->bAbsoluteRotation;
					uint32 OldValue = !NewValue;
					TSet<USceneComponent*> UpdatedInstances;
					FComponentEditorUtils::PropagateDefaultValueChange(SceneComponent, AbsoluteRotationProperty, OldValue, NewValue, UpdatedInstances);
				}

				if( NotifyHook )
				{
					NotifyHook->NotifyPostChange( PropertyChangedEvent, AbsoluteRotationProperty );
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();

		GUnrealEd->RedrawLevelEditingViewports();
	}
}

void FComponentTransformDetails::OnToggleAbsoluteScale( bool bEnable )
{
	UProperty* AbsoluteScaleProperty = FindField<UProperty>( USceneComponent::StaticClass(), "bAbsoluteScale" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
			if( SceneComponent && SceneComponent->bAbsoluteScale != bEnable )
			{
				if( !bBeganTransaction )
				{
					// NOTE: One transaction per change, not per actor
					GEditor->BeginTransaction( LOCTEXT("ToggleAbsoluteScale", "Toggle Absolute Scale" ) );
					bBeganTransaction = true;
				}

				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				Object->PreEditChange( AbsoluteScaleProperty );

				if( NotifyHook )
				{
					NotifyHook->NotifyPreChange( AbsoluteScaleProperty );
				}

				SceneComponent->bAbsoluteScale = !SceneComponent->bAbsoluteScale;

				FPropertyChangedEvent PropertyChangedEvent( AbsoluteScaleProperty );
				Object->PostEditChangeProperty(PropertyChangedEvent);

				// If it's a template, propagate the change out to any current instances of the object
				if(SceneComponent->IsTemplate())
				{
					uint32 NewValue = SceneComponent->bAbsoluteScale;
					uint32 OldValue = !NewValue;
					TSet<USceneComponent*> UpdatedInstances;
					FComponentEditorUtils::PropagateDefaultValueChange(SceneComponent, AbsoluteScaleProperty, OldValue, NewValue, UpdatedInstances);
				}

				if( NotifyHook )
				{
					NotifyHook->NotifyPostChange( PropertyChangedEvent, AbsoluteScaleProperty );
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();
		GUnrealEd->RedrawLevelEditingViewports();
	}

}

struct FGetRootComponentArchetype
{
	static USceneComponent* Get(UObject* Object)
	{
		auto RootComponent = Object ? GetSceneComponentFromDetailsObject(Object) : nullptr;
		return RootComponent ? Cast<USceneComponent>(RootComponent->GetArchetype()) : nullptr;
	}
};

FReply FComponentTransformDetails::OnLocationResetClicked()
{
	const FText TransactionName = LOCTEXT("ResetLocation", "Reset Location");
	FScopedTransaction Transaction(TransactionName);

	const USceneComponent* Archetype = FGetRootComponentArchetype::Get(SelectedObjects[0].Get());
	const FVector Data = Archetype ? Archetype->RelativeLocation : FVector();

	OnSetLocation(Data[0], ETextCommit::Default, 0);
	OnSetLocation(Data[1], ETextCommit::Default, 1);
	OnSetLocation(Data[2], ETextCommit::Default, 2);

	return FReply::Handled();
}

FReply FComponentTransformDetails::OnRotationResetClicked()
{
	const FText TransactionName = LOCTEXT("ResetRotation", "Reset Rotation");
	FScopedTransaction Transaction(TransactionName);

	const USceneComponent* Archetype = FGetRootComponentArchetype::Get(SelectedObjects[0].Get());
	const FRotator Data = Archetype ? Archetype->RelativeRotation : FRotator();

	OnSetRotation(Data.Roll, true, 0);
	OnSetRotation(Data.Pitch, true, 1);
	OnSetRotation(Data.Yaw, true, 2);

	return FReply::Handled();
}

FReply FComponentTransformDetails::OnScaleResetClicked()
{
	const FText TransactionName = LOCTEXT("ResetScale", "Reset Scale");
	FScopedTransaction Transaction(TransactionName);

	const USceneComponent* Archetype = FGetRootComponentArchetype::Get(SelectedObjects[0].Get());
	const FVector Data = Archetype ? Archetype->RelativeScale3D : FVector();

	ScaleObject(Data[0], 0, false, TransactionName);
	ScaleObject(Data[1], 1, false, TransactionName);
	ScaleObject(Data[2], 2, false, TransactionName);

	return FReply::Handled();
}

void FComponentTransformDetails::ExtendXScaleContextMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "ScaleOperations", LOCTEXT( "ScaleOperations", "Scale Operations" ) );
	MenuBuilder.AddMenuEntry( 
		LOCTEXT( "MirrorValueX", "Mirror X" ),  
		LOCTEXT( "MirrorValueX_Tooltip", "Mirror scale value on the X axis" ), 
		FSlateIcon(), 		
		FUIAction( FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnXScaleMirrored ), FCanExecuteAction() )
	);
	MenuBuilder.EndSection();
}

void FComponentTransformDetails::ExtendYScaleContextMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "ScaleOperations", LOCTEXT( "ScaleOperations", "Scale Operations" ) );
	MenuBuilder.AddMenuEntry( 
		LOCTEXT( "MirrorValueY", "Mirror Y" ),  
		LOCTEXT( "MirrorValueY_Tooltip", "Mirror scale value on the Y axis" ), 
		FSlateIcon(), 		
		FUIAction( FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnYScaleMirrored ), FCanExecuteAction() )
	);
	MenuBuilder.EndSection();
}

void FComponentTransformDetails::ExtendZScaleContextMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "ScaleOperations", LOCTEXT( "ScaleOperations", "Scale Operations" ) );
	MenuBuilder.AddMenuEntry( 
		LOCTEXT( "MirrorValueZ", "Mirror Z" ),  
		LOCTEXT( "MirrorValueZ_Tooltip", "Mirror scale value on the Z axis" ), 
		FSlateIcon(), 		
		FUIAction( FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnZScaleMirrored ), FCanExecuteAction() )
	);
	MenuBuilder.EndSection();
}

void FComponentTransformDetails::OnXScaleMirrored()
{
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Mouse);
	ScaleObject( 1.0f, 0, true, LOCTEXT( "MirrorActorScaleX", "Mirror actor scale X" ) );
}

void FComponentTransformDetails::OnYScaleMirrored()
{
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Mouse);
	ScaleObject(1.0f, 1, true, LOCTEXT("MirrorActorScaleY", "Mirror actor scale Y"));
}

void FComponentTransformDetails::OnZScaleMirrored()
{
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Mouse);
	ScaleObject(1.0f, 2, true, LOCTEXT("MirrorActorScaleZ", "Mirror actor scale Z"));
}

/**
 * Cache the entire transform at it is seen by the input boxes so we dont have to iterate over the selected actors multiple times                   
 */
void FComponentTransformDetails::CacheTransform()
{
	FVector CurLoc;
	FRotator CurRot;
	FVector CurScale;

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );

			FVector Loc;
			FRotator Rot;
			FVector Scale;
			if( SceneComponent )
			{
				Loc = SceneComponent->RelativeLocation;
				Rot = (bEditingRotationInUI && !Object->IsTemplate()) ? ObjectToRelativeRotationMap.FindOrAdd(Object) : SceneComponent->RelativeRotation;
				Scale = SceneComponent->RelativeScale3D;

				if( ObjectIndex == 0 )
				{
					// Cache the current values from the first actor to see if any values differ among other actors
					CurLoc = Loc;
					CurRot = Rot;
					CurScale = Scale;

					CachedLocation.Set( Loc );
					CachedRotation.Set( Rot );
					CachedScale.Set( Scale );

					bAbsoluteLocation = SceneComponent->bAbsoluteLocation;
					bAbsoluteScale = SceneComponent->bAbsoluteScale;
					bAbsoluteRotation = SceneComponent->bAbsoluteRotation;
				}
				else if( CurLoc != Loc || CurRot != Rot || CurScale != Scale )
				{
					// Check which values differ and unset the different values
					CachedLocation.X = Loc.X == CurLoc.X && CachedLocation.X.IsSet() ? Loc.X : TOptional<float>();
					CachedLocation.Y = Loc.Y == CurLoc.Y && CachedLocation.Y.IsSet() ? Loc.Y : TOptional<float>();
					CachedLocation.Z = Loc.Z == CurLoc.Z && CachedLocation.Z.IsSet() ? Loc.Z : TOptional<float>();

					CachedRotation.X = Rot.Roll == CurRot.Roll && CachedRotation.X.IsSet() ? Rot.Roll : TOptional<float>();
					CachedRotation.Y = Rot.Pitch == CurRot.Pitch && CachedRotation.Y.IsSet() ? Rot.Pitch : TOptional<float>();
					CachedRotation.Z = Rot.Yaw == CurRot.Yaw && CachedRotation.Z.IsSet() ? Rot.Yaw : TOptional<float>();

					CachedScale.X = Scale.X == CurScale.X && CachedScale.X.IsSet() ? Scale.X : TOptional<float>();
					CachedScale.Y = Scale.Y == CurScale.Y && CachedScale.Y.IsSet() ? Scale.Y : TOptional<float>();
					CachedScale.Z = Scale.Z == CurScale.Z && CachedScale.Z.IsSet() ? Scale.Z : TOptional<float>();

					// If all values are unset all values are different and we can stop looking
					const bool bAllValuesDiffer = !CachedLocation.IsSet() && !CachedRotation.IsSet() && !CachedScale.IsSet();
					if( bAllValuesDiffer )
					{
						break;
					}
				}
			}
		}
	}
}


void FComponentTransformDetails::OnSetLocation( float NewValue, ETextCommit::Type CommitInfo, int32 Axis )
{
	bool bBeganTransaction = false;

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
			if( SceneComponent )
			{
				FVector OldRelativeLocation = SceneComponent->RelativeLocation;
				FVector RelativeLocation = OldRelativeLocation;
				float OldValue = RelativeLocation[Axis];
				if( OldValue != NewValue )
				{
					if( !bBeganTransaction )
					{
						// Begin a transaction the first time an actors location is about to change.
						// NOTE: One transaction per change, not per actor
						if(Object->IsA<AActor>())
						{
							GEditor->BeginTransaction( LOCTEXT( "OnSetLocation", "Set actor location" ) );
						}
						else
						{
							GEditor->BeginTransaction( LOCTEXT( "OnSetLocation_ComponentDirect", "Modify Component(s)") );
						}
						
						bBeganTransaction = true;
					}

					if (Object->HasAnyFlags(RF_DefaultSubObject))
					{
						// Default subobjects must be included in any undo/redo operations
						Object->SetFlags(RF_Transactional);
					}

					// Begin a new movement event which will broadcast delegates before and after the actor moves
					FScopedObjectMovement ActorMoveEvent( Object );

					FScopedSwitchWorldForObject WorldSwitcher( Object );

					UProperty* RelativeLocationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeLocation" );
					Object->PreEditChange( RelativeLocationProperty );

					if( NotifyHook )
					{
						NotifyHook->NotifyPreChange( RelativeLocationProperty );
					}

					RelativeLocation[Axis] = NewValue;
					
					if( SelectedActorInfo.NumSelected == 0 )
					{
						// HACK: Set directly if no actors are selected since this causes Rot->Quat->Rot conversion
						// (recalculates relative rotation even though this is location)
						SceneComponent->RelativeLocation = RelativeLocation;
					}
					else
					{
						SceneComponent->SetRelativeLocation( RelativeLocation );
					}

					FPropertyChangedEvent PropertyChangedEvent( RelativeLocationProperty );
					Object->PostEditChangeProperty( PropertyChangedEvent );

					// If it's a template, propagate the change out to any current instances of the object
					if(SceneComponent->IsTemplate())
					{
						TSet<USceneComponent*> UpdatedInstances;
						FComponentEditorUtils::PropagateDefaultValueChange(SceneComponent, RelativeLocationProperty, OldRelativeLocation, RelativeLocation, UpdatedInstances);
					}

					if( NotifyHook )
					{
						NotifyHook->NotifyPostChange( PropertyChangedEvent, RelativeLocationProperty );
					}
				}

			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();
	}

	CacheTransform();

	GUnrealEd->RedrawLevelEditingViewports();
}

void FComponentTransformDetails::OnSetRotation( float NewValue, bool bCommitted, int32 Axis )
{
	// OnSetRotation is sent from the slider or when the value changes and we dont have slider and the value is being typed.
	// We should only change the value on commit when it is being typed
	const bool bAllowSpin = SelectedObjects.Num() == 1;

	if( bAllowSpin || bCommitted )
	{
		bool bBeganTransaction = false;
		for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
		{
			TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
			if( ObjectPtr.IsValid() )
			{
				UObject* Object = ObjectPtr.Get();
				USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
				if( SceneComponent )
				{
					const bool bIsEditingTemplateObject = Object->IsTemplate();

					FRotator& RelativeRotation = (bEditingRotationInUI && !bIsEditingTemplateObject) ? ObjectToRelativeRotationMap.FindOrAdd(Object) : SceneComponent->RelativeRotation;
					FRotator OldRelativeRotation = RelativeRotation;

					float& ValueToChange = Axis == 0 ? RelativeRotation.Roll : Axis == 1 ? RelativeRotation.Pitch : RelativeRotation.Yaw;

					if( ValueToChange != NewValue )
					{
						if( !bBeganTransaction && bCommitted )
						{
							// Begin a transaction the first time an actors rotation is about to change.
							// NOTE: One transaction per change, not per actor
							if(Object->IsA<AActor>())
							{
								GEditor->BeginTransaction( LOCTEXT( "OnSetRotation", "Set actor rotation" ) );
							}
							else
							{
								GEditor->BeginTransaction( LOCTEXT( "OnSetRotation_ComponentDirect", "Modify Component(s)") );
							}

							if (!bIsEditingTemplateObject)
							{
								// Broadcast the first time an actor is about to move
								GEditor->BroadcastBeginObjectMovement( *Object );
							}

							bBeganTransaction = true;
						}

						FScopedSwitchWorldForObject WorldSwitcher( Object );

						UProperty* RelativeRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeRotation" );
						if( bCommitted && !bEditingRotationInUI )
						{
							if (Object->HasAnyFlags(RF_DefaultSubObject))
							{
								// Default subobjects must be included in any undo/redo operations
								Object->SetFlags(RF_Transactional);
							}

							Object->PreEditChange( RelativeRotationProperty );
						}

						if( NotifyHook )
						{
							NotifyHook->NotifyPreChange( RelativeRotationProperty );
						}

						ValueToChange = NewValue;

						if(!bIsEditingTemplateObject)
						{
							if( SelectedActorInfo.NumSelected == 0 )
							{
								// HACK: Set directly if no actors are selected since this causes Rot->Quat->Rot conversion issues
								// (recalculates relative rotation from quat which can give an equivalent but different value than the user typed)
								SceneComponent->RelativeRotation = RelativeRotation;
							}
							else
							{
								SceneComponent->SetRelativeRotation( RelativeRotation );
							}
						}

						AActor* EditedActor = Cast<AActor>( Object );
						if( !EditedActor && SceneComponent )
						{
							EditedActor = SceneComponent->GetOwner();
						}

						if( EditedActor && !bIsEditingTemplateObject )
						{
							EditedActor->ReregisterAllComponents();
						}

						// If it's a template, propagate the change out to any current instances of the object
						if(SceneComponent->IsTemplate())
						{
							TSet<USceneComponent*> UpdatedInstances;
							FComponentEditorUtils::PropagateDefaultValueChange(SceneComponent, RelativeRotationProperty, OldRelativeRotation, RelativeRotation, UpdatedInstances);
						}

						FPropertyChangedEvent PropertyChangedEvent( RelativeRotationProperty, !bCommitted && bEditingRotationInUI ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet );

						if( NotifyHook )
						{
							NotifyHook->NotifyPostChange( PropertyChangedEvent, RelativeRotationProperty );
						}

						if( bCommitted )
						{
							if( !bEditingRotationInUI )
							{
								Object->PostEditChangeProperty( PropertyChangedEvent );	
							}
					
							if (!bIsEditingTemplateObject)
							{
								// The actor is done moving
								GEditor->BroadcastEndObjectMovement( *Object );
							}
						}
					}
				}
			}
		}

		if( bCommitted && bBeganTransaction )
		{
			GEditor->EndTransaction();
		}

		// Redraw
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

void FComponentTransformDetails::OnRotationCommitted(float NewValue, ETextCommit::Type CommitInfo, int32 Axis)
{
	OnSetRotation(NewValue, true, Axis);

	CacheTransform();
}

void FComponentTransformDetails::OnBeginRotatonSlider()
{
	bEditingRotationInUI = true;

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();

			// Start a new transation when a rotator slider begins to change
			// We'll end it when the slider is release
			// NOTE: One transaction per change, not per actor
			if(!bBeganTransaction)
			{
				if(Object->IsA<AActor>())
				{
					GEditor->BeginTransaction( LOCTEXT( "OnSetRotation", "Set actor rotation" ) );
				}
				else
				{
					GEditor->BeginTransaction( LOCTEXT( "OnSetRotation_ComponentDirect", "Modify Component(s)") );
				}

				bBeganTransaction = true;
			}

			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
			if( SceneComponent )
			{
				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				UProperty* RelativeRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeRotation" );
				Object->PreEditChange( RelativeRotationProperty );

				// Add/update cached rotation value prior to slider interaction
				ObjectToRelativeRotationMap.FindOrAdd(Object) = SceneComponent->RelativeRotation;
			}
		}
	}

	// Just in case we couldn't start a new transaction for some reason
	if(!bBeganTransaction)
	{
		GEditor->BeginTransaction( LOCTEXT( "OnSetRotation", "Set actor rotation" ) );
	}	
}

void FComponentTransformDetails::OnEndRotationSlider(float NewValue)
{
	bEditingRotationInUI = false;

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
			if( SceneComponent )
			{
				FScopedSwitchWorldForObject WorldSwitcher( Object );

				UProperty* RelativeRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeRotation" );
				FPropertyChangedEvent PropertyChangedEvent( RelativeRotationProperty );
				Object->PostEditChangeProperty( PropertyChangedEvent );
			}
		}
	}

	GEditor->EndTransaction();

	// Redraw
	GUnrealEd->RedrawLevelEditingViewports();
}

void FComponentTransformDetails::OnSetScale( const float NewValue, ETextCommit::Type CommitInfo, int32 Axis )
{
	ScaleObject( NewValue, Axis, false, LOCTEXT( "OnSetScale", "Set actor scale" ) );
}

void FComponentTransformDetails::ScaleObject( float NewValue, int32 Axis, bool bMirror, const FText& TransactionSessionName )
{
	UProperty* RelativeScale3DProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeScale3D" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* SceneComponent = GetSceneComponentFromDetailsObject( Object );
			if( SceneComponent )
			{
				FVector OldRelativeScale = SceneComponent->RelativeScale3D;
				FVector RelativeScale = OldRelativeScale;
				if( bMirror )
				{
					NewValue = -RelativeScale[Axis];
				}
				float OldValue = RelativeScale[Axis];
				if( OldValue != NewValue )
				{
					if( !bBeganTransaction )
					{
						// Begin a transaction the first time an actors scale is about to change.
						// NOTE: One transaction per change, not per actor
						GEditor->BeginTransaction( TransactionSessionName );
						bBeganTransaction = true;
					}

					FScopedSwitchWorldForObject WorldSwitcher( Object );
					
					if (Object->HasAnyFlags(RF_DefaultSubObject))
					{
						// Default subobjects must be included in any undo/redo operations
						Object->SetFlags(RF_Transactional);
					}

					// Begin a new movement event which will broadcast delegates before and after the actor moves
					FScopedObjectMovement ActorMoveEvent( Object );

					Object->PreEditChange( RelativeScale3DProperty );

					if( NotifyHook )
					{
						NotifyHook->NotifyPreChange( RelativeScale3DProperty );
					}

					// Set the new value for the corresponding axis
					RelativeScale[Axis] = NewValue;

					if( bPreserveScaleRatio )
					{
						// Account for the previous scale being zero.  Just set to the new value in that case?
						float Ratio = OldValue == 0.0f ? NewValue : NewValue/OldValue;

						// Change values on axes besides the one being directly changed
						switch( Axis )
						{
						case 0:
							RelativeScale.Y *= Ratio;
							RelativeScale.Z *= Ratio;
							break;
						case 1:
							RelativeScale.X *= Ratio;
							RelativeScale.Z *= Ratio;
							break;
						case 2:
							RelativeScale.X *= Ratio;
							RelativeScale.Y *= Ratio;
						}
					}

					SceneComponent->SetRelativeScale3D( RelativeScale );

					// Build property chain so the actor knows whether we changed the X, Y or Z
					FEditPropertyChain PropertyChain;

					if (!bPreserveScaleRatio)
					{
						UStruct* VectorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass()->GetOutermost(), TEXT("Vector"), false);

						UProperty* VectorValueProperty = NULL;
						switch( Axis )
						{
						case 0:
							VectorValueProperty = FindField<UFloatProperty>(VectorStruct, TEXT("X"));
							break;
						case 1:
							VectorValueProperty = FindField<UFloatProperty>(VectorStruct, TEXT("Y"));
							break;
						case 2:
							VectorValueProperty = FindField<UFloatProperty>(VectorStruct, TEXT("Z"));
						}

						PropertyChain.AddHead(VectorValueProperty);
					}
					PropertyChain.AddHead(RelativeScale3DProperty);

					FPropertyChangedEvent PropertyChangedEvent(RelativeScale3DProperty, EPropertyChangeType::ValueSet);
					FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);
					Object->PostEditChangeChainProperty( PropertyChangedChainEvent );

					// For backwards compatibility, as for some reason PostEditChangeChainProperty calls PostEditChangeProperty with the property set to "X" not "RelativeScale3D"
					// (it does that for all vector properties, and I don't want to be the one to change that)
					if (!bPreserveScaleRatio)
					{
						Object->PostEditChangeProperty( PropertyChangedEvent );
					}
					else
					{
						// If the other scale values have been updated, make sure we update the transform now (as the tick will be too late)
						// so they appear correct when their EditedText is fetched from the delegate.
						CacheTransform();
					}

					// If it's a template, propagate the change out to any current instances of the object
					if(SceneComponent->IsTemplate())
					{
						TSet<USceneComponent*> UpdatedInstances;
						FComponentEditorUtils::PropagateDefaultValueChange(SceneComponent, RelativeScale3DProperty, OldRelativeScale, RelativeScale, UpdatedInstances);
					}

					if( NotifyHook )
					{
						NotifyHook->NotifyPostChange( PropertyChangedEvent, RelativeScale3DProperty );
					}
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();
	}

	CacheTransform();

	// Redraw
	GUnrealEd->RedrawLevelEditingViewports();
}

#undef LOCTEXT_NAMESPACE
