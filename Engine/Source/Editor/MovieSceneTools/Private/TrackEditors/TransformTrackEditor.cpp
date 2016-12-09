// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/TransformTrackEditor.h"
#include "GameFramework/Actor.h"
#include "Framework/Commands/Commands.h"
#include "Animation/AnimSequence.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "GameFramework/Character.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "UnrealEdGlobals.h"
#include "ISectionLayoutBuilder.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "FloatCurveKeyArea.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "MovieScene_TransformTrack"


/**
 * Class that draws a transform section in the sequencer
 */
class F3DTransformSection
	: public ISequencerSection
{
public:

	F3DTransformSection(ISequencer* InSequencer, FGuid InObjectBinding, UMovieSceneSection& InSection)
		: Section( InSection )
		, Sequencer(InSequencer)
		, ObjectBinding(InObjectBinding)
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
		static const FLinearColor BlueKeyAreaColor(0.0f, 0.0f, 0.7f, 0.5f);
		static const FLinearColor GreenKeyAreaColor(0.0f, 0.7f, 0.0f, 0.5f);
		static const FLinearColor RedKeyAreaColor(0.7f, 0.0f, 0.0f, 0.5f);

		UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>( &Section );

		// Translation
		TAttribute<TOptional<float>> TranslationXExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetTranslationValue, EAxis::X));
		TSharedRef<FFloatCurveKeyArea> TranslationXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::X), TranslationXExternalValue, TransformSection, RedKeyAreaColor));

		TAttribute<TOptional<float>> TranslationYExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetTranslationValue, EAxis::Y));
		TSharedRef<FFloatCurveKeyArea> TranslationYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::Y), TranslationYExternalValue, TransformSection, GreenKeyAreaColor));

		TAttribute<TOptional<float>> TranslationZExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetTranslationValue, EAxis::Z));
		TSharedRef<FFloatCurveKeyArea> TranslationZKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::Z), TranslationZExternalValue, TransformSection, BlueKeyAreaColor));

		// Rotation
		TAttribute<TOptional<float>> RotationXExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetRotationValue, EAxis::X));
		TSharedRef<FFloatCurveKeyArea> RotationXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetRotationCurve(EAxis::X), RotationXExternalValue, TransformSection, RedKeyAreaColor));

		TAttribute<TOptional<float>> RotationYExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetRotationValue, EAxis::Y));
		TSharedRef<FFloatCurveKeyArea> RotationYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetRotationCurve(EAxis::Y), RotationYExternalValue, TransformSection, GreenKeyAreaColor));

		TAttribute<TOptional<float>> RotationZExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetRotationValue, EAxis::Z));
		TSharedRef<FFloatCurveKeyArea> RotationZKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetRotationCurve(EAxis::Z), RotationZExternalValue, TransformSection, BlueKeyAreaColor));

		// Scale
		TAttribute<TOptional<float>> ScaleXExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetScaleValue, EAxis::X));
		TSharedRef<FFloatCurveKeyArea> ScaleXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::X), ScaleXExternalValue, TransformSection, RedKeyAreaColor));

		TAttribute<TOptional<float>> ScaleYExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetScaleValue, EAxis::Y));
		TSharedRef<FFloatCurveKeyArea> ScaleYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::Y), ScaleYExternalValue, TransformSection, GreenKeyAreaColor));

		TAttribute<TOptional<float>> ScaleZExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F3DTransformSection::GetScaleValue, EAxis::Z));
		TSharedRef<FFloatCurveKeyArea> ScaleZKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::Z), ScaleZExternalValue, TransformSection, BlueKeyAreaColor));

		// This generates the tree structure for the transform section
		LayoutBuilder.PushCategory( "Location", NSLOCTEXT("FTransformSection", "LocationArea", "Location") );
			LayoutBuilder.AddKeyArea("Location.X", NSLOCTEXT("FTransformSection", "LocXArea", "X"), TranslationXKeyArea );
			LayoutBuilder.AddKeyArea("Location.Y", NSLOCTEXT("FTransformSection", "LocYArea", "Y"), TranslationYKeyArea );
			LayoutBuilder.AddKeyArea("Location.Z", NSLOCTEXT("FTransformSection", "LocZArea", "Z"), TranslationZKeyArea );
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory( "Rotation", NSLOCTEXT("FTransformSection", "RotationArea", "Rotation") );
			LayoutBuilder.AddKeyArea("Rotation.X", NSLOCTEXT("FTransformSection", "RotXArea", "X"), RotationXKeyArea );
			LayoutBuilder.AddKeyArea("Rotation.Y", NSLOCTEXT("FTransformSection", "RotYArea", "Y"), RotationYKeyArea );
			LayoutBuilder.AddKeyArea("Rotation.Z", NSLOCTEXT("FTransformSection", "RotZArea", "Z"), RotationZKeyArea );
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory( "Scale", NSLOCTEXT("FTransformSection", "ScaleArea", "Scale") );
			LayoutBuilder.AddKeyArea("Scale.X", NSLOCTEXT("FTransformSection", "ScaleXArea", "X"), ScaleXKeyArea );
			LayoutBuilder.AddKeyArea("Scale.Y", NSLOCTEXT("FTransformSection", "ScaleYArea", "Y"), ScaleYKeyArea );
			LayoutBuilder.AddKeyArea("Scale.Z", NSLOCTEXT("FTransformSection", "ScaleZArea", "Z"), ScaleZKeyArea );
		LayoutBuilder.PopCategory();
	}

	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override 
	{
		return InPainter.PaintSectionBackground();
	}



private:

	TOptional<float> GetTranslationValue(EAxis::Type Axis) const
	{
		USceneComponent* SceneComponent = GetSceneComponent();
		if (SceneComponent != nullptr)
		{
			switch (Axis)
			{
			case EAxis::X:
				return TOptional<float>(SceneComponent->GetRelativeTransform().GetTranslation().X);
			case EAxis::Y:
				return TOptional<float>(SceneComponent->GetRelativeTransform().GetTranslation().Y);
			case EAxis::Z:
				return TOptional<float>(SceneComponent->GetRelativeTransform().GetTranslation().Z);
			}
		}
		return TOptional<float>();
	}

	TOptional<float> GetRotationValue(EAxis::Type Axis) const
	{
		USceneComponent* SceneComponent = GetSceneComponent();
		if (SceneComponent != nullptr)
		{
			FRotator Rotator = SceneComponent->RelativeRotation;

			switch (Axis)
			{
			case EAxis::X:
				return TOptional<float>(Rotator.Roll);
			case EAxis::Y:
				return TOptional<float>(Rotator.Pitch);
			case EAxis::Z:
				return TOptional<float>(Rotator.Yaw);
			}
		}
		return TOptional<float>();
	}

	TOptional<float> GetScaleValue(EAxis::Type Axis) const
	{
		USceneComponent* SceneComponent = GetSceneComponent();
		if (SceneComponent != nullptr)
		{
			switch (Axis)
			{
			case EAxis::X:
				return TOptional<float>(SceneComponent->GetRelativeTransform().GetScale3D().X);
			case EAxis::Y:
				return TOptional<float>(SceneComponent->GetRelativeTransform().GetScale3D().Y);
			case EAxis::Z:
				return TOptional<float>(SceneComponent->GetRelativeTransform().GetScale3D().Z);
			}
		}
		return TOptional<float>();
	}

	USceneComponent* GetSceneComponent() const
	{
		if (Sequencer != nullptr)
		{
			TArrayView<TWeakObjectPtr<UObject>> RuntimeObjects = Sequencer->FindBoundObjects(ObjectBinding, Sequencer->GetFocusedTemplateID());
			if (RuntimeObjects.Num() == 1)
			{
				return MovieSceneHelpers::SceneComponentFromRuntimeObject(RuntimeObjects[0].Get());
			}
		}
		return nullptr;
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	ISequencer* Sequencer;

	FGuid ObjectBinding;
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


void CopyInterpMoveTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack)
{
	if (FMatineeImportTools::CopyInterpMoveTrack(MoveTrack, TransformTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}


bool CanCopyInterpMoveTrack(UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack)
{
	if (!MoveTrack || !TransformTrack)
	{
		return false;
	}

	bool bHasKeyframes = MoveTrack->GetNumKeyframes() != 0;

	for (auto SubTrack : MoveTrack->SubTracks)
	{
		if (SubTrack->IsA(UInterpTrackMoveAxis::StaticClass()))
		{
			UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>(SubTrack);
			if (MoveSubTrack)
			{
				if (MoveSubTrack->FloatTrack.Points.Num() > 0)
				{
					bHasKeyframes = true;
					break;
				}
			}
		}
	}
		
	return bHasKeyframes;
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
		NSLOCTEXT( "Sequencer", "PasteMatineeMoveTrack", "Paste Matinee Move Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeMoveTrackTooltip", "Pastes keys from a Matinee move track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &CopyInterpMoveTrack, GetSequencer().ToSharedRef(), MoveTrack, TransformTrack ),
			FCanExecuteAction::CreateStatic( &CanCopyInterpMoveTrack, MoveTrack, TransformTrack ) ) );

	//		FCanExecuteAction::CreateLambda( [=]()->bool { return MoveTrack != nullptr && MoveTrack->GetNumKeys() > 0 && TransformTrack != nullptr; } ) ) );

	auto AnimSubMenuDelegate = [](FMenuBuilder& InMenuBuilder, TSharedRef<ISequencer> InSequencer, UMovieScene3DTransformTrack* InTransformTrack)
	{
		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.Filter.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateStatic(&F3DTransformTrackEditor::ImportAnimSequenceTransforms, InSequencer, InTransformTrack);

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		InMenuBuilder.AddWidget(
			SNew(SBox)
			.WidthOverride(200.0f)
			.HeightOverride(400.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			], 
			FText(), true, false);
	};

	const bool bInOpenSubMenuOnClick = true;

	MenuBuilder.AddSubMenu(
		NSLOCTEXT( "Sequencer", "ImportTransforms", "Import From Animation Root" ),
		NSLOCTEXT( "Sequencer", "ImportTransformsTooltip", "Import transform keys from an animation sequence's root motion." ),
		FNewMenuDelegate::CreateLambda(AnimSubMenuDelegate, GetSequencer().ToSharedRef(), TransformTrack), 
		bInOpenSubMenuOnClick);

	MenuBuilder.AddMenuSeparator();
	FKeyframeTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}


TSharedRef<ISequencerSection> F3DTransformTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShareable(new F3DTransformSection(GetSequencer().Get(), ObjectBinding, SectionObject));
}


void F3DTransformTrackEditor::OnPreTransformChanged( UObject& InObject )
{
	float AutoKeyTime = GetTimeForKey();

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

		const bool bUnwindRotation = GetSequencer()->IsRecordingLive();

		AddTransformKeys(Actor, ExistingTransform, NewTransformData, EKey3DTransformChannel::All, bUnwindRotation, ESequencerKeyMode::AutoKey);
	}
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

	USelection* CurrentSelection = GEditor->GetSelectedActors();
	TArray<UObject*> SelectedActors;
	CurrentSelection->GetSelectedObjects( AActor::StaticClass(), SelectedActors );

	for (TArray<UObject*>::TIterator It(SelectedActors); It; ++It)
	{
		AddTransformKeysForObject(*It, Channel, ESequencerKeyMode::ManualKeyForced);
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
	bool bHasCameraComponent = false;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		if (Actor != nullptr)
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(Actor);
			if (CameraComponent)
			{
				bHasCameraComponent = true;
			}
		}
	}

	if (bHasCameraComponent)
	{
		// If this is a camera track, add a button to lock the viewport to the camera
		EditBox.Get()->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(4, 0, 0, 0)
			[
				SNew(SCheckBox)		
					.IsFocusable(false)
					.Visibility(this, &F3DTransformTrackEditor::IsCameraVisible, ObjectGuid)
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
}


void F3DTransformTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AActor::StaticClass()) || ObjectClass->IsChildOf(USceneComponent::StaticClass()))
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "AddTransform", "Transform"),
			NSLOCTEXT("Sequencer", "AddPTransformTooltip", "Adds a transform track."),
			FSlateIcon(),
			FUIAction( 
				FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::AddTransformKeysForHandle, ObjectBinding, EKey3DTransformChannel::All, ESequencerKeyMode::ManualKey ),
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

EVisibility
F3DTransformTrackEditor::IsCameraVisible(FGuid ObjectGuid) const
{
	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		
		if (Actor != nullptr)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

ECheckBoxState F3DTransformTrackEditor::IsCameraLocked(FGuid ObjectGuid) const
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		
		if (Actor != nullptr)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (CameraActor.IsValid())
	{
		// First, check the active viewport
		FViewport* ActiveViewport = GEditor->GetActiveViewport();

		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];

			if (LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				if (LevelVC->Viewport == ActiveViewport)
				{
					if (CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
					{
						return ECheckBoxState::Checked;
					}
					else
					{
						return ECheckBoxState::Unchecked;
					}
				}
			}
		}

		// Otherwise check all other viewports
		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];

			if (LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown && CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
			{
				return ECheckBoxState::Checked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}


void F3DTransformTrackEditor::OnLockCameraClicked(ECheckBoxState CheckBoxState, FGuid ObjectGuid)
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());

		if (Actor != nullptr)
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
			if (LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown && CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
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
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());

		if (Actor != nullptr)
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

void GetKeysForVector( bool LastVectorIsValid, const FVector& LastVector, const FVector& CurrentVector, EKey3DTransformChannel::Type VectorChannel, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, TArray<FTransformKey>& OutNewKeys, TArray<FTransformKey>& OutDefaultKeys)
{
	bool bKeyChannel = ChannelsToKey == EKey3DTransformChannel::All || ChannelsToKey == VectorChannel;

	TArray<FTransformKey>& XKeys = bKeyChannel && ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.X, CurrentVector.X ) == false ) ? OutNewKeys : OutDefaultKeys;
	XKeys.Add( FTransformKey( VectorChannel, EAxis::X, CurrentVector.X, bUnwindRotation ) );
	
	TArray<FTransformKey>& YKeys = bKeyChannel && ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.Y, CurrentVector.Y ) == false ) ? OutNewKeys : OutDefaultKeys;
	YKeys.Add( FTransformKey( VectorChannel, EAxis::Y, CurrentVector.Y, bUnwindRotation ) );

	TArray<FTransformKey>& ZKeys = bKeyChannel && ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.Z, CurrentVector.Z ) == false ) ? OutNewKeys : OutDefaultKeys;
	ZKeys.Add( FTransformKey( VectorChannel, EAxis::Z, CurrentVector.Z, bUnwindRotation ) );
}

void F3DTransformTrackEditor::GetTransformKeys( const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, TArray<FTransformKey>& OutNewKeys, TArray<FTransformKey>& OutDefaultKeys )
{
	bool bLastVectorIsValid = LastTransform.IsValid();

	// If key all is enabled, for a key on all the channels
	if (GetSequencer()->GetKeyAllEnabled())
	{
		bLastVectorIsValid = false;
		ChannelsToKey = EKey3DTransformChannel::All;
	}

	GetKeysForVector( bLastVectorIsValid, LastTransform.Translation, CurrentTransform.Translation, EKey3DTransformChannel::Translation, ChannelsToKey, bUnwindRotation, OutNewKeys, OutDefaultKeys );
	GetKeysForVector( bLastVectorIsValid, LastTransform.Rotation.Euler(), CurrentTransform.Rotation.Euler(), EKey3DTransformChannel::Rotation, ChannelsToKey, bUnwindRotation, OutNewKeys, OutDefaultKeys );
	GetKeysForVector( bLastVectorIsValid, LastTransform.Scale, CurrentTransform.Scale, EKey3DTransformChannel::Scale, ChannelsToKey, bUnwindRotation, OutNewKeys, OutDefaultKeys );
}

void F3DTransformTrackEditor::AddTransformKeysForHandle( FGuid ObjectHandle, EKey3DTransformChannel::Type ChannelToKey, ESequencerKeyMode KeyMode )
{
	for ( TWeakObjectPtr<UObject> Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectHandle) )
	{
		AddTransformKeysForObject(Object.Get(), ChannelToKey, KeyMode);
	}
}


void F3DTransformTrackEditor::AddTransformKeysForObject( UObject* Object, EKey3DTransformChannel::Type ChannelToKey, ESequencerKeyMode KeyMode )
{
	AActor* Actor = nullptr;
	USceneComponent* SceneComponent = nullptr;
	GetActorAndSceneComponentFromObject( Object, Actor, SceneComponent );
	if ( Actor != nullptr && SceneComponent != nullptr )
	{
		FTransformData CurrentTransform( SceneComponent );
		if(Object->GetClass()->IsChildOf(AActor::StaticClass()))
		{
			AddTransformKeys( Actor, FTransformData(), CurrentTransform, ChannelToKey, false, KeyMode );
		}
		else if(Object->GetClass()->IsChildOf(USceneComponent::StaticClass()))
		{
			AddTransformKeys( SceneComponent, FTransformData(), CurrentTransform, ChannelToKey, false, KeyMode );
		}
	}
}


void F3DTransformTrackEditor::AddTransformKeys( UObject* ObjectToKey, const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, ESequencerKeyMode KeyMode )
{
	TArray<FTransformKey> NewKeys;
	TArray<FTransformKey> DefaultKeys;
	GetTransformKeys(LastTransform, CurrentTransform, ChannelsToKey, bUnwindRotation, NewKeys, DefaultKeys);

	AnimatablePropertyChanged( FOnKeyProperty::CreateRaw(this, &F3DTransformTrackEditor::OnAddTransformKeys, ObjectToKey, &NewKeys, &DefaultKeys, CurrentTransform, KeyMode ) );
}


bool F3DTransformTrackEditor::OnAddTransformKeys( float Time, UObject* ObjectToKey, TArray<FTransformKey>* NewKeys, TArray<FTransformKey>* DefaultKeys, FTransformData CurrentTransform, ESequencerKeyMode KeyMode )
{
	TArray<UObject*> ObjectsToKey;
	ObjectsToKey.Add(ObjectToKey);
	
	return AddKeysToObjects(
		ObjectsToKey,
		Time,
		*NewKeys,
		*DefaultKeys,
		KeyMode,
		UMovieScene3DTransformTrack::StaticClass(),
		TransformPropertyName,
		[this](UMovieScene3DTransformTrack* NewTrack) {
			NewTrack->SetPropertyNameAndPath(TransformPropertyName, TransformPropertyName.ToString());
		}
	);
}


void F3DTransformTrackEditor::ImportAnimSequenceTransforms(const FAssetData& Asset, TSharedRef<ISequencer> Sequencer, UMovieScene3DTransformTrack* TransformTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Asset.GetAsset());

	// find object binding to recover any component transforms we need to incorporate (for characters)
	FTransform InvComponentTransform;
	UMovieSceneSequence* MovieSceneSequence = Sequencer->GetFocusedMovieSceneSequence();
	if(MovieSceneSequence)
	{
		UMovieScene* MovieScene = MovieSceneSequence->GetMovieScene();
		if(MovieScene)
		{
			FGuid ObjectBinding;
			if(MovieScene->FindTrackBinding(*TransformTrack, ObjectBinding))
			{
				const UClass* ObjectClass = nullptr;
				if(FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding))
				{
					ObjectClass = Spawnable->GetObjectTemplate()->GetClass();
				}
				else if(FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding))
				{
					ObjectClass = Possessable->GetPossessedObjectClass();
				}

				if(ObjectClass)
				{
					const ACharacter* Character = Cast<const ACharacter>(ObjectClass->ClassDefaultObject);
					if(Character)
					{
						const USkeletalMeshComponent* SkeletalMeshComponent = Character->GetMesh();
						FTransform MeshRelativeTransform = SkeletalMeshComponent->GetRelativeTransform();
						InvComponentTransform = MeshRelativeTransform.GetRelativeTransform(SkeletalMeshComponent->GetOwner()->GetTransform()).Inverse();
					}
				}
			}
		}
	}

	if(AnimSequence && AnimSequence->GetRawAnimationData().Num() > 0)
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "ImportAnimSequenceTransforms", "Import Anim Sequence Transforms" ) );

		TransformTrack->Modify();

		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
		Section->GetScaleCurve(EAxis::X).SetDefaultValue(1);
		Section->GetScaleCurve(EAxis::Y).SetDefaultValue(1);
		Section->GetScaleCurve(EAxis::Z).SetDefaultValue(1);
		TransformTrack->AddSection(*Section);

		if (Section->TryModify())
		{
			float SectionMin = Section->GetStartTime();
			float SectionMax = Section->GetEndTime();

			struct FTempTransformKey
			{
				FTransform Transform;
				FRotator WoundRotation;
				float Time;
			};

			TArray<FTempTransformKey> TempKeys;

			FRawAnimSequenceTrack& RawTrack = AnimSequence->GetRawAnimationTrack(0);
			const int32 KeyCount = FMath::Max(FMath::Max(RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num()), RawTrack.ScaleKeys.Num());
			for(int32 KeyIndex = 0; KeyIndex < KeyCount; KeyIndex++)
			{
				FTempTransformKey TempKey;
				TempKey.Time = AnimSequence->GetTimeAtFrame(KeyIndex);

				if(RawTrack.PosKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetTranslation(RawTrack.PosKeys[KeyIndex]);
				}
				else if(RawTrack.PosKeys.Num() > 0)
				{
					TempKey.Transform.SetTranslation(RawTrack.PosKeys[0]);
				}
				
				if(RawTrack.RotKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetRotation(RawTrack.RotKeys[KeyIndex]);
				}
				else if(RawTrack.RotKeys.Num() > 0)
				{
					TempKey.Transform.SetRotation(RawTrack.RotKeys[0]);
				}

				if(RawTrack.ScaleKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetScale3D(RawTrack.ScaleKeys[KeyIndex]);
				}
				else if(RawTrack.ScaleKeys.Num() > 0)
				{
					TempKey.Transform.SetScale3D(RawTrack.ScaleKeys[0]);
				}

				// apply component transform if any
				TempKey.Transform = InvComponentTransform * TempKey.Transform;

				TempKey.WoundRotation = TempKey.Transform.GetRotation().Rotator();

				TempKeys.Add(TempKey);
			}

			int32 TransformCount = TempKeys.Num();
			for(int32 TransformIndex = 0; TransformIndex < TransformCount - 1; TransformIndex++)
			{
				FRotator& Rotator = TempKeys[TransformIndex].WoundRotation;
				FRotator& NextRotator = TempKeys[TransformIndex + 1].WoundRotation;

				FMath::WindRelativeAnglesDegrees(Rotator.Pitch, NextRotator.Pitch);
				FMath::WindRelativeAnglesDegrees(Rotator.Yaw, NextRotator.Yaw);
				FMath::WindRelativeAnglesDegrees(Rotator.Roll, NextRotator.Roll);
			}

			const bool bUnwindRotation = false;
			for(const FTempTransformKey& TempKey : TempKeys)
			{
				SectionMin = FMath::Min( SectionMin, TempKey.Time );
				SectionMax = FMath::Max( SectionMax, TempKey.Time );

				Section->SetStartTime( SectionMin );
				Section->SetEndTime( SectionMax );

				const FVector Translation = TempKey.Transform.GetTranslation();
				const FVector Rotation = TempKey.WoundRotation.Euler();
				const FVector Scale = TempKey.Transform.GetScale3D();

				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Translation, EAxis::X, Translation.X, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Translation, EAxis::Y, Translation.Y, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Translation, EAxis::Z, Translation.Z, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);

				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Rotation, EAxis::X, Rotation.X, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Y, Rotation.Y, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Z, Rotation.Z, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);

				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Scale, EAxis::X, Scale.X, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Scale, EAxis::Y, Scale.Y, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Scale, EAxis::Z, Scale.Z, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
			}

			Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
		}
	}
}

#undef LOCTEXT_NAMESPACE
