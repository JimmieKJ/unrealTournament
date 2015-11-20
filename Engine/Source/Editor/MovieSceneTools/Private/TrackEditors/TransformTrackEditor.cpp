// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSection.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieScene3DTransformSection.h"
#include "ISequencerObjectChangeListener.h"
#include "ISequencerSection.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneTrackEditor.h"
#include "TransformTrackEditor.h"
#include "Matinee/InterpTrackMove.h"


#define LOCTEXT_NAMESPACE "MovieScene_TransformTrack"


/**
 * Class that draws a transform section in the sequencer
 */
class F3DTransformSection
	: public ISequencerSection
{
public:

	F3DTransformSection( UMovieSceneSection& InSection )
		: Section( InSection )
	{ }

	/** Virtual destructor. */
	~F3DTransformSection() { }

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}

	virtual FText GetDisplayName() const override
	{ 
		return NSLOCTEXT("FTransformSection", "DisplayName", "Transform");
	}
	
	virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>( &Section );

		TranslationXKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetTranslationCurve( EAxis::X ), TransformSection ) );
		TranslationYKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetTranslationCurve( EAxis::Y ), TransformSection ) );
		TranslationZKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetTranslationCurve( EAxis::Z ), TransformSection ) );

		RotationXKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetRotationCurve( EAxis::X ), TransformSection ) );
		RotationYKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetRotationCurve( EAxis::Y ), TransformSection ) );
		RotationZKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetRotationCurve( EAxis::Z ), TransformSection ) );

		ScaleXKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetScaleCurve( EAxis::X ), TransformSection ) );
		ScaleYKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetScaleCurve( EAxis::Y ), TransformSection ) );
		ScaleZKeyArea = MakeShareable( new FFloatCurveKeyArea( &TransformSection->GetScaleCurve( EAxis::Z ), TransformSection ) );

		// This generates the tree structure for the transform section
		LayoutBuilder.PushCategory( "Location", NSLOCTEXT("FTransformSection", "LocationArea", "Location") );
			LayoutBuilder.AddKeyArea("Location.X", NSLOCTEXT("FTransformSection", "LocXArea", "X"), TranslationXKeyArea.ToSharedRef() );
			LayoutBuilder.AddKeyArea("Location.Y", NSLOCTEXT("FTransformSection", "LocYArea", "Y"), TranslationYKeyArea.ToSharedRef() );
			LayoutBuilder.AddKeyArea("Location.Z", NSLOCTEXT("FTransformSection", "LocZArea", "Z"), TranslationZKeyArea.ToSharedRef() );
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory( "Rotation", NSLOCTEXT("FTransformSection", "RotationArea", "Rotation") );
			LayoutBuilder.AddKeyArea("Rotation.X", NSLOCTEXT("FTransformSection", "RotXArea", "X"), RotationXKeyArea.ToSharedRef() );
			LayoutBuilder.AddKeyArea("Rotation.Y", NSLOCTEXT("FTransformSection", "RotYArea", "Y"), RotationYKeyArea.ToSharedRef() );
			LayoutBuilder.AddKeyArea("Rotation.Z", NSLOCTEXT("FTransformSection", "RotZArea", "Z"), RotationZKeyArea.ToSharedRef() );
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory( "Scale", NSLOCTEXT("FTransformSection", "ScaleArea", "Scale") );
			LayoutBuilder.AddKeyArea("Scale.X", NSLOCTEXT("FTransformSection", "ScaleXArea", "X"), ScaleXKeyArea.ToSharedRef() );
			LayoutBuilder.AddKeyArea("Scale.Y", NSLOCTEXT("FTransformSection", "ScaleYArea", "Y"), ScaleYKeyArea.ToSharedRef() );
			LayoutBuilder.AddKeyArea("Scale.Z", NSLOCTEXT("FTransformSection", "ScaleZArea", "Z"), ScaleZKeyArea.ToSharedRef() );
		LayoutBuilder.PopCategory();
	}

	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override 
	{
		const ESlateDrawEffect::Type DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
		// Add a box for the section
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect,
			DrawEffects
		); 

		return LayerId;
	}

	void SetIntermediateValue(UMovieSceneTrack* Track, FTransformData IntermediateData )
	{
		if ( Section.GetOuter() == Track )
		{
			TranslationXKeyArea->SetIntermediateValue( IntermediateData.Translation.X );
			TranslationYKeyArea->SetIntermediateValue( IntermediateData.Translation.Y );
			TranslationZKeyArea->SetIntermediateValue( IntermediateData.Translation.Z );

			RotationXKeyArea->SetIntermediateValue( IntermediateData.Rotation.Euler().X );
			RotationYKeyArea->SetIntermediateValue( IntermediateData.Rotation.Euler().Y );
			RotationZKeyArea->SetIntermediateValue( IntermediateData.Rotation.Euler().Z );

			ScaleXKeyArea->SetIntermediateValue( IntermediateData.Scale.X );
			ScaleYKeyArea->SetIntermediateValue( IntermediateData.Scale.Y );
			ScaleZKeyArea->SetIntermediateValue( IntermediateData.Scale.Z );
		}
	}

	void ClearIntermediateValue()
	{
		TranslationXKeyArea->ClearIntermediateValue();
		TranslationYKeyArea->ClearIntermediateValue();
		TranslationZKeyArea->ClearIntermediateValue();

		RotationXKeyArea->ClearIntermediateValue();
		RotationYKeyArea->ClearIntermediateValue();
		RotationZKeyArea->ClearIntermediateValue();

		ScaleXKeyArea->ClearIntermediateValue();
		ScaleYKeyArea->ClearIntermediateValue();
		ScaleZKeyArea->ClearIntermediateValue();
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	mutable TSharedPtr<FFloatCurveKeyArea> TranslationXKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> TranslationYKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> TranslationZKeyArea;

	mutable TSharedPtr<FFloatCurveKeyArea> RotationXKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> RotationYKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> RotationZKeyArea;

	mutable TSharedPtr<FFloatCurveKeyArea> ScaleXKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> ScaleYKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> ScaleZKeyArea;
};


class F3DTransformTrackCommands
	: public TCommands<F3DTransformTrackCommands>
{
public:

	F3DTransformTrackCommands()
		: TCommands<F3DTransformTrackCommands>
	(
		"3DTransformTrack",
		NSLOCTEXT("Contexts", "3DTransformTrack", "3DTransformTrack"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{ }
		
	/** Sets a transform key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddTransformKey;

	/** Sets a translation key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddTranslationKey;

	/** Sets a rotation key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddRotationKey;

	/** Sets a scale key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddScaleKey;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};


void F3DTransformTrackCommands::RegisterCommands()
{
	UI_COMMAND( AddTransformKey, "Add Transform Key", "Add a transform key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EKeys::S) );
	UI_COMMAND( AddTranslationKey, "Add Translation Key", "Add a translation key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::W) );
	UI_COMMAND( AddRotationKey, "Add Rotation Key", "Add a rotation key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::E) );
	UI_COMMAND( AddScaleKey, "Add Scale Key", "Add a scale key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::R) );
}

FName F3DTransformTrackEditor::TransformPropertyName("Transform");

F3DTransformTrackEditor::F3DTransformTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FKeyframeTrackEditor<UMovieScene3DTransformTrack, UMovieScene3DTransformSection, FTransformKey>( InSequencer ) 
{
	// Listen for actor/component movement
	GEditor->OnBeginObjectMovement().AddRaw( this, &F3DTransformTrackEditor::OnPreTransformChanged );
	GEditor->OnEndObjectMovement().AddRaw( this, &F3DTransformTrackEditor::OnTransformChanged );

	// Listen for the viewport's viewed through camera starts and stops movement
	GEditor->OnBeginCameraMovement().AddRaw( this, &F3DTransformTrackEditor::OnPreTransformChanged );
	GEditor->OnEndCameraMovement().AddRaw( this, &F3DTransformTrackEditor::OnTransformChanged );

	F3DTransformTrackCommands::Register();
}


F3DTransformTrackEditor::~F3DTransformTrackEditor()
{
}


void F3DTransformTrackEditor::OnRelease()
{
	GEditor->OnBeginObjectMovement().RemoveAll( this );
	GEditor->OnEndObjectMovement().RemoveAll( this );

	GEditor->OnBeginCameraMovement().RemoveAll( this );
	GEditor->OnEndCameraMovement().RemoveAll( this );

	F3DTransformTrackCommands::Unregister();
}


TSharedRef<ISequencerTrackEditor> F3DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DTransformTrackEditor( InSequencer ) );
}


bool F3DTransformTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DTransformTrack::StaticClass();
}


void PasteInterpMoveTrack( UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack )
{
	float KeyTime = MoveTrack->GetKeyframeTime( 0 );
	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(TransformTrack->FindOrAddSection( KeyTime ));
	float SectionMin = Section->GetStartTime();
	float SectionMax = Section->GetEndTime();

	FRichCurve& TranslationXCurve = Section->GetTranslationCurve( EAxis::X );
	FRichCurve& TranslationYCurve = Section->GetTranslationCurve( EAxis::Y );
	FRichCurve& TranslationZCurve = Section->GetTranslationCurve( EAxis::Z );
	for ( const auto& Point : MoveTrack->PosTrack.Points )
	{
		MatineeImportTools::SetOrAddKey( TranslationXCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
		MatineeImportTools::SetOrAddKey( TranslationYCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
		MatineeImportTools::SetOrAddKey( TranslationZCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
		SectionMin = FMath::Min( SectionMin, Point.InVal );
		SectionMax = FMath::Max( SectionMax, Point.InVal );
	}

	FRichCurve& RotationXCurve = Section->GetRotationCurve( EAxis::X );
	FRichCurve& RotationYCurve = Section->GetRotationCurve( EAxis::Y );
	FRichCurve& RotationZCurve = Section->GetRotationCurve( EAxis::Z );
	for ( const auto& Point : MoveTrack->EulerTrack.Points )
	{
		MatineeImportTools::SetOrAddKey( RotationXCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
		MatineeImportTools::SetOrAddKey( RotationYCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
		MatineeImportTools::SetOrAddKey( RotationZCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
		SectionMin = FMath::Min( SectionMin, Point.InVal );
		SectionMax = FMath::Max( SectionMax, Point.InVal );
	}

	Section->SetStartTime( SectionMin );
	Section->SetEndTime( SectionMax );
}


void F3DTransformTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackMove* MoveTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MoveTrack = Cast<UInterpTrackMove>( CopyPasteObject );
		if ( MoveTrack != nullptr )
		{
			break;
		}
	}
	UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeTrack", "Paste Matinee Move Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeTrackTooltip", "Pastes keys from a Matinee move track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &PasteInterpMoveTrack, MoveTrack, TransformTrack ),
			FCanExecuteAction::CreateLambda( [=]()->bool { return MoveTrack != nullptr && MoveTrack->GetNumKeys() > 0 && TransformTrack != nullptr; } ) ) );
}


TSharedRef<ISequencerSection> F3DTransformTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	TSharedRef<F3DTransformSection> TransformSection = MakeShareable( new F3DTransformSection( SectionObject ) );
	OnSetIntermediateValueFromTransformChange.AddSP(TransformSection, &F3DTransformSection::SetIntermediateValue );
	GetSequencer()->OnGlobalTimeChanged().AddSP(TransformSection, &F3DTransformSection::ClearIntermediateValue );
	return TransformSection;
}


void F3DTransformTrackEditor::OnPreTransformChanged( UObject& InObject )
{
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	float AutoKeyTime = GetTimeForKey( MovieSceneSequence );

	if( IsAllowedToAutoKey() )
	{
		USceneComponent* SceneComponent = nullptr;
		AActor* Actor = Cast<AActor>( &InObject );
		if( Actor && Actor->GetRootComponent() )
		{
			SceneComponent = Actor->GetRootComponent();
		}
		else
		{
			// If the object wasn't an actor attempt to get it directly as a scene component 
			SceneComponent = Cast<USceneComponent>( &InObject );
		}

		if( SceneComponent )
		{
			// Cache off the existing transform so we can detect which components have changed
			// and keys only when something has changed
			FTransformData Transform( SceneComponent );

			ObjectToExistingTransform.Add( &InObject, Transform );
		}
	}
}


/**
 * Temp struct used because delegates only accept 4 or less payloads
 * FTransformKey is immutable and would require heavy re-architecting to fit here
 */
struct FTransformDataPair
{
	FTransformDataPair(FTransformData InTransformData, FTransformData InLastTransformData)
		: TransformData(InTransformData)
		, LastTransformData(InLastTransformData) {}

	FTransformData TransformData;
	FTransformData LastTransformData;
};


void F3DTransformTrackEditor::OnTransformChanged( UObject& InObject )
{
	AActor* Actor = nullptr;
	USceneComponent* SceneComponentThatChanged = nullptr;
	GetActorAndSceneComponentFromObject(&InObject, Actor, SceneComponentThatChanged);

	if( SceneComponentThatChanged != nullptr && Actor != nullptr )
	{
		// Find an existing transform if possible.  If one exists we will compare against the new one to decide what components of the transform need keys
		FTransformData ExistingTransform = ObjectToExistingTransform.FindRef( &InObject );

		// Remove it from the list of cached transforms. 
		// @todo sequencer livecapture: This can be made much for efficient by not removing cached state during live capture situation
		ObjectToExistingTransform.Remove( &InObject );

		// Build new transform data
		FTransformData NewTransformData( SceneComponentThatChanged );

		FKeyParams KeyParams;
		{
			KeyParams.bCreateHandleIfMissing = false;
			KeyParams.bCreateTrackIfMissing = false;
			KeyParams.bCreateKeyIfUnchanged = false;
			KeyParams.bCreateKeyIfEmpty = false;
			KeyParams.bCreateKeyOnlyWhenAutoKeying = true;
		}

		const bool bUnwindRotation = GetSequencer()->IsRecordingLive();

		AddTransformKeys(Actor, ExistingTransform, NewTransformData, EKey3DTransformChannel::All, bUnwindRotation, KeyParams);
	}
}


void F3DTransformTrackEditor::AddKey(const FGuid& ObjectGuid, UObject* AdditionalAsset)
{
	// @todo - Sequencer - The key params should be updated to do what the artists expect.
	FKeyParams KeyParams;
	KeyParams.bCreateTrackIfMissing = true;
	AddTransformKeysForHandle( ObjectGuid, EKey3DTransformChannel::All, KeyParams);
}


void F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects( EKey3DTransformChannel::Type Channel )
{
	// WASD hotkeys to fly the viewport can conflict with hotkeys for setting keyframes (ie. s). 
	// If the viewport is moving, disregard setting keyframes.
	for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
	{		
		FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
		if (LevelVC && LevelVC->IsMovingCamera())
		{
			return;
		}
	}

	FKeyParams KeyParams;
	KeyParams.bCreateHandleIfMissing = true;
	KeyParams.bCreateTrackIfMissing = true;
	KeyParams.bCreateKeyIfUnchanged = true;
	KeyParams.bCreateKeyIfEmpty = true;

	USelection* CurrentSelection = GEditor->GetSelectedActors();
	TArray<UObject*> SelectedActors;
	CurrentSelection->GetSelectedObjects( AActor::StaticClass(), SelectedActors );

	for (TArray<UObject*>::TIterator It(SelectedActors); It; ++It)
	{
		AddTransformKeysForObject(*It, Channel, KeyParams);
	}
}


void F3DTransformTrackEditor::BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings)
{
	const F3DTransformTrackCommands& Commands = F3DTransformTrackCommands::Get();

	SequencerCommandBindings->MapAction(
		Commands.AddTransformKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::All ) );

	SequencerCommandBindings->MapAction(
		Commands.AddTranslationKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::Translation ) );

	SequencerCommandBindings->MapAction(
		Commands.AddRotationKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::Rotation ) );

	SequencerCommandBindings->MapAction(
		Commands.AddScaleKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::Scale ) );
}


void F3DTransformTrackEditor::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectGuid, const UClass* ObjectClass)
{
	TArray<UObject*> OutObjects;
	GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectGuid, OutObjects);

	TWeakObjectPtr<ACameraActor> CameraActor;

	for (UObject* Object : OutObjects)
	{
		ACameraActor* Actor = Cast<ACameraActor>( Object );
		if (Actor)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (!CameraActor.IsValid())
	{
		return;
	}

	// If this is a camera track, add a button to lock the viewport to the camera
	EditBox.Get()->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(SCheckBox)
				.IsChecked(this, &F3DTransformTrackEditor::IsCameraLocked, ObjectGuid)
				.OnCheckStateChanged(this, &F3DTransformTrackEditor::OnLockCameraClicked, ObjectGuid)
				.ToolTipText(this, &F3DTransformTrackEditor::GetLockCameraToolTip, ObjectGuid)
				.ForegroundColor(FLinearColor::White)
				.CheckedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
				.CheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
				.CheckedPressedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
				.UncheckedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
				.UncheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
				.UncheckedPressedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
		];
}


void F3DTransformTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		FKeyParams KeyParams;
		KeyParams.bCreateTrackIfMissing = true;
		KeyParams.bCreateKeyIfUnchanged = true;

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "AddTransform", "Transform"),
			NSLOCTEXT("Sequencer", "AddPTransformTooltip", "Adds a transform track."),
			FSlateIcon(),
			FUIAction( 
				FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::AddTransformKeysForHandle, ObjectBinding, EKey3DTransformChannel::All, KeyParams ),
				FCanExecuteAction::CreateSP( this, &F3DTransformTrackEditor::CanAddTransformTrackForActorHandle, ObjectBinding ) ) );
	}
}


bool F3DTransformTrackEditor::CanAddTransformTrackForActorHandle( FGuid ObjectBinding ) const
{
	FName Transform("Transform");
	if (GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(ObjectBinding, Transform))
	{
		return false;
	}
	return true;
}


ECheckBoxState F3DTransformTrackEditor::IsCameraLocked(FGuid ObjectGuid) const
{
	TArray<UObject*> OutObjects;
	GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectGuid, OutObjects);

	TWeakObjectPtr<ACameraActor> CameraActor;

	for (UObject* Object : OutObjects)
	{
		ACameraActor* Actor = Cast<ACameraActor>( Object );
		if (Actor)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (CameraActor.IsValid())
	{
		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
			if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() && LevelVC->GetViewMode() != VMI_Unknown && CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
			{
				return ECheckBoxState::Checked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}


void F3DTransformTrackEditor::OnLockCameraClicked(ECheckBoxState CheckBoxState, FGuid ObjectGuid)
{
	TArray<UObject*> OutObjects;
	GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectGuid, OutObjects);

	TWeakObjectPtr<ACameraActor> CameraActor;

	for (UObject* Object : OutObjects)
	{
		ACameraActor* Actor = Cast<ACameraActor>( Object );
		if (Actor)
		{
			CameraActor = Actor;
			break;
		}
	}

	// If toggle is on, lock the active viewport to the camera
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		// Set the active viewport or any viewport if there is no active viewport
		FViewport* ActiveViewport = GEditor->GetActiveViewport();

		FLevelEditorViewportClient* LevelVC = nullptr;

		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* Viewport = GEditor->LevelViewportClients[i];
			if (Viewport && Viewport->IsPerspective() && Viewport->GetViewMode() != VMI_Unknown)
			{
				LevelVC = Viewport;

				if (LevelVC->Viewport == ActiveViewport)
				{
					break;
				}
			}
		}

		if (LevelVC != nullptr && CameraActor.IsValid())
		{
			GetSequencer()->SetPerspectiveViewportCameraCutEnabled(false);
			LevelVC->SetMatineeActorLock(nullptr);
			LevelVC->SetActorLock(CameraActor.Get());
			LevelVC->bLockedCameraView = true;
			LevelVC->UpdateViewForLockedActor();
			LevelVC->Invalidate();
		}
	}
	// Otherwise, clear all locks on the camera
	else
	{
		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
			if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() && LevelVC->GetViewMode() != VMI_Unknown && CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
			{
				LevelVC->SetMatineeActorLock(nullptr);
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->ViewFOV = LevelVC->FOVAngle;
				LevelVC->RemoveCameraRoll();
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
	}
}


FText F3DTransformTrackEditor::GetLockCameraToolTip(FGuid ObjectGuid) const
{
	TArray<UObject*> OutObjects;
	GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectGuid, OutObjects);

	TWeakObjectPtr<ACameraActor> CameraActor;

	for (UObject* Object : OutObjects)
	{
		ACameraActor* Actor = Cast<ACameraActor>( Object );
		if (Actor)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (CameraActor.IsValid())
	{
		return IsCameraLocked(ObjectGuid) == ECheckBoxState::Checked ?
			FText::Format(LOCTEXT("UnlockCamera", "Unlock {0} from Viewport"), FText::FromString(CameraActor.Get()->GetActorLabel())) :
			FText::Format(LOCTEXT("LockCamera", "Lock {0} to Selected Viewport"), FText::FromString(CameraActor.Get()->GetActorLabel()));
	}
	return FText();
}

void F3DTransformTrackEditor::GetActorAndSceneComponentFromObject( UObject* Object, AActor*& OutActor, USceneComponent*& OutSceneComponent )
{
	OutActor = Cast<AActor>( Object );
	if ( OutActor != nullptr && OutActor->GetRootComponent() )
	{
		OutSceneComponent = OutActor->GetRootComponent();
	}
	else
	{
		// If the object wasn't an actor attempt to get it directly as a scene component and then get the actor from there.
		OutSceneComponent = Cast<USceneComponent>( Object );
		if ( OutSceneComponent != nullptr )
		{
			OutActor = Cast<AActor>( OutSceneComponent->GetOuter() );
		}
	}
}

void GetKeysForVector( bool LastVectorIsValid, const FVector& LastVector, const FVector& CurrentVector, EKey3DTransformChannel::Type KeyChannel, bool bUnwindRotation, TArray<FTransformKey>& OutKeys )
{
	if ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.X, CurrentVector.X ) == false )
	{
		OutKeys.Add( FTransformKey( KeyChannel, EAxis::X, CurrentVector.X, bUnwindRotation ) );
	}
	if ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.Y, CurrentVector.Y ) == false )
	{
		OutKeys.Add( FTransformKey( KeyChannel, EAxis::Y, CurrentVector.Y, bUnwindRotation ) );
	}
	if ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.Z, CurrentVector.Z ) == false )
	{
		OutKeys.Add( FTransformKey( KeyChannel, EAxis::Z, CurrentVector.Z, bUnwindRotation ) );
	}
}

void F3DTransformTrackEditor::GetChangedTransformKeys( const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, TArray<FTransformKey>& OutKeys )
{
	if ( ChannelsToKey & EKey3DTransformChannel::Translation )
	{
		GetKeysForVector(LastTransform.IsValid(), LastTransform.Translation, CurrentTransform.Translation, EKey3DTransformChannel::Translation, bUnwindRotation, OutKeys);
	}
	if ( ChannelsToKey & EKey3DTransformChannel::Rotation )
	{
		GetKeysForVector( LastTransform.IsValid(), LastTransform.Rotation.Euler(), CurrentTransform.Rotation.Euler(), EKey3DTransformChannel::Rotation, bUnwindRotation, OutKeys );
	}
	if ( ChannelsToKey & EKey3DTransformChannel::Scale )
	{
		GetKeysForVector( LastTransform.IsValid(), LastTransform.Scale, CurrentTransform.Scale, EKey3DTransformChannel::Scale, bUnwindRotation, OutKeys );
	}
}


void F3DTransformTrackEditor::AddTransformKeysForHandle( FGuid ObjectHandle, EKey3DTransformChannel::Type ChannelToKey, FKeyParams KeyParams )
{
	TArray<UObject*> OutObjects;
	GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectHandle, OutObjects );

	for ( UObject* Object : OutObjects )
	{
		AddTransformKeysForObject(Object, ChannelToKey, KeyParams);
	}
}


void F3DTransformTrackEditor::AddTransformKeysForObject( UObject* Object, EKey3DTransformChannel::Type ChannelToKey, FKeyParams KeyParams )
{
	AActor* Actor = nullptr;
	USceneComponent* SceneComponent = nullptr;
	GetActorAndSceneComponentFromObject( Object, Actor, SceneComponent );
	if ( Actor != nullptr && SceneComponent != nullptr )
	{
		FTransformData CurrentTransform( SceneComponent );
		AddTransformKeys( Actor, FTransformData(), CurrentTransform, ChannelToKey, false, KeyParams );
	}
}


void F3DTransformTrackEditor::AddTransformKeys( AActor* ActorToKey, const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, FKeyParams KeyParams )
{
	TArray<FTransformKey> Keys;
	GetChangedTransformKeys(LastTransform, CurrentTransform, ChannelsToKey, bUnwindRotation, Keys);

	AnimatablePropertyChanged( FOnKeyProperty::CreateRaw(this, &F3DTransformTrackEditor::OnAddTransformKeys, ActorToKey, &Keys, CurrentTransform, KeyParams ) );
}


bool F3DTransformTrackEditor::OnAddTransformKeys( float Time, AActor* ActorToKey, TArray<FTransformKey>* Keys, FTransformData CurrentTransform, FKeyParams KeyParams )
{
	TArray<UObject*> ObjectsToKey;
	ObjectsToKey.Add(ActorToKey);
	
	FOnSetIntermediateValue OnSetIntermediateValue;
	OnSetIntermediateValue.BindRaw( this, &F3DTransformTrackEditor::SetIntermediateValueFromTransformChange, CurrentTransform );

	FOnInitializeNewTrack OnInitializeNewTrack;
	OnInitializeNewTrack.BindLambda([this](UMovieScene3DTransformTrack* NewTrack) {
		NewTrack->SetPropertyNameAndPath(TransformPropertyName, TransformPropertyName.ToString());
	});
	
	return AddKeysToObjects(ObjectsToKey, Time, *Keys, KeyParams, UMovieScene3DTransformTrack::StaticClass(), TransformPropertyName, OnInitializeNewTrack, OnSetIntermediateValue);
}


void F3DTransformTrackEditor::SetIntermediateValueFromTransformChange( UMovieSceneTrack* Track, FTransformData TransformData )
{
	OnSetIntermediateValueFromTransformChange.Broadcast(Track, TransformData);
}


#undef LOCTEXT_NAMESPACE
