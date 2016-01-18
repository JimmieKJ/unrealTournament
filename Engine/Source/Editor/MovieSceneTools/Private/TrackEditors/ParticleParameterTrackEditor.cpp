// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ParticleParameterTrackEditor.h"
#include "MovieSceneParticleParameterTrack.h"
#include "ParameterSection.h"
#include "MovieSceneParameterSection.h"


#define LOCTEXT_NAMESPACE "ParticleParameterTrackEditor"

FName FParticleParameterTrackEditor::TrackName( "ParticleParameter" );

FParticleParameterTrackEditor::FParticleParameterTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer )
{
}


TSharedRef<ISequencerTrackEditor> FParticleParameterTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable( new FParticleParameterTrackEditor( OwningSequencer ) );
}


TSharedRef<ISequencerSection> FParticleParameterTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>(&SectionObject);
	checkf( ParameterSection != nullptr, TEXT("Unsupported section type.") );

	return MakeShareable(new FParameterSection( *ParameterSection, FText::FromName(ParameterSection->GetFName())));
}


TSharedPtr<SWidget> FParticleParameterTrackEditor::BuildOutlinerEditWidget( const FGuid& ObjectBinding, UMovieSceneTrack* Track )
{
	UMovieSceneParticleParameterTrack* ParticleParameterTrack = Cast<UMovieSceneParticleParameterTrack>(Track);
	return
		SNew( SComboButton )
		.ButtonStyle( FEditorStyle::Get(), "FlatButton.Light" )
		.OnGetMenuContent( this, &FParticleParameterTrackEditor::OnGetAddParameterMenuContent, ObjectBinding, ParticleParameterTrack )
		.ContentPadding( FMargin( 2, 0 ) )
		.ButtonContent()
		[
			SNew( SHorizontalBox )

			+ SHorizontalBox::Slot()
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SNew( STextBlock )
				.Font( FEditorStyle::Get().GetFontStyle( "FontAwesome.8" ) )
				.Text( FText::FromString( FString( TEXT( "\xf067" ) ) ) /*fa-plus*/ )
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 4, 0, 0, 0 )
			[
				SNew( STextBlock )
				.Font( FEditorStyle::GetFontStyle( "Sequencer.AnimationOutliner.RegularFont" ) )
				.Text( LOCTEXT( "AddParameterButton", "Parameter" ) )
			]
		];
}


void FParticleParameterTrackEditor::BuildObjectBindingTrackMenu( FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass )
{
	if ( ObjectClass->IsChildOf( AEmitter::StaticClass() ) )
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		MenuBuilder.AddMenuEntry(
			LOCTEXT( "AddParticleParameterTrack", "Particle Parameter Track" ),
			LOCTEXT( "AddParticleParameterTrackTooltip", "Adds a track for controlling particle parameter values." ),
			FSlateIcon(),
			FUIAction
			(
				FExecuteAction::CreateSP( this, &FParticleParameterTrackEditor::AddParticleParameterTrack, ObjectBinding ),
				FCanExecuteAction::CreateSP( this, &FParticleParameterTrackEditor::CanAddParticleParameterTrack, ObjectBinding )
			));
	}
}


bool FParticleParameterTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneParticleParameterTrack::StaticClass();
}


TSharedRef<SWidget> FParticleParameterTrackEditor::OnGetAddParameterMenuContent( FGuid ObjectBinding, UMovieSceneParticleParameterTrack* ParticleParameterTrack )
{
	FMenuBuilder AddParameterMenuBuilder( true, nullptr );

	AEmitter* Emitter = Cast<AEmitter>( GetSequencer()->GetFocusedMovieSceneSequenceInstance()->FindObject( ObjectBinding, *GetSequencer() ) );
	if ( Emitter != nullptr )
	{
		UParticleSystemComponent* ParticleSystemComponent = Emitter->GetParticleSystemComponent();
		if ( ParticleSystemComponent != nullptr )
		{
			TArray<FParameterNameAndAction> ParameterNamesAndActions;
			const TArray<FParticleSysParam> InstanceParameters = ParticleSystemComponent->GetAsyncInstanceParameters();
			for ( const FParticleSysParam& ParticleSystemParameter : InstanceParameters )
			{
				switch ( ParticleSystemParameter.ParamType )
				{
				case PSPT_Scalar:
				{
					FUIAction AddParameterMenuAction( FExecuteAction::CreateSP( this, &FParticleParameterTrackEditor::AddScalarParameter, ObjectBinding, ParticleParameterTrack, ParticleSystemParameter.Name ) );
					FParameterNameAndAction NameAndAction( ParticleSystemParameter.Name, AddParameterMenuAction );
					ParameterNamesAndActions.Add( NameAndAction );
					break;
				}
				case PSPT_Vector:
				{
					FUIAction AddParameterMenuAction( FExecuteAction::CreateSP( this, &FParticleParameterTrackEditor::AddVectorParameter, ObjectBinding, ParticleParameterTrack, ParticleSystemParameter.Name ) );
					FParameterNameAndAction NameAndAction( ParticleSystemParameter.Name, AddParameterMenuAction );
					ParameterNamesAndActions.Add( NameAndAction );
					break;
				}
				case PSPT_Color:
				{
					FUIAction AddParameterMenuAction( FExecuteAction::CreateSP( this, &FParticleParameterTrackEditor::AddColorParameter, ObjectBinding, ParticleParameterTrack, ParticleSystemParameter.Name ) );
					FParameterNameAndAction NameAndAction( ParticleSystemParameter.Name, AddParameterMenuAction );
					ParameterNamesAndActions.Add( NameAndAction );
					break;
				}
				}
			}

			// Sort and generate menu.
			ParameterNamesAndActions.Sort();
			for ( FParameterNameAndAction NameAndAction : ParameterNamesAndActions )
			{
				AddParameterMenuBuilder.AddMenuEntry( FText::FromName( NameAndAction.ParameterName ), FText(), FSlateIcon(), NameAndAction.Action );
			}
		}
	}

	return AddParameterMenuBuilder.MakeWidget();
}


bool FParticleParameterTrackEditor::CanAddParticleParameterTrack( FGuid ObjectBinding )
{
	return GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack( UMovieSceneParticleParameterTrack::StaticClass(), ObjectBinding, TrackName ) == nullptr;
}


void FParticleParameterTrackEditor::AddParticleParameterTrack( FGuid ObjectBinding )
{
	FindOrCreateTrackForObject( ObjectBinding, UMovieSceneParticleParameterTrack::StaticClass(), TrackName, true);
	NotifyMovieSceneDataChanged();
}


void FParticleParameterTrackEditor::AddScalarParameter( FGuid ObjectBinding, UMovieSceneParticleParameterTrack* ParticleParameterTrack, FName ParameterName )
{
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	float KeyTime = GetTimeForKey( MovieSceneSequence );

	UObject* Object = GetSequencer()->GetFocusedMovieSceneSequenceInstance()->FindObject( ObjectBinding, *GetSequencer() );
	AEmitter* Emitter = Cast<AEmitter>(Object);
	if ( Emitter != nullptr )
	{
		UParticleSystemComponent* ParticleSystemComponent = Emitter->GetParticleSystemComponent();
		if ( ParticleSystemComponent != nullptr )
		{
			float Value;
			ParticleSystemComponent->GetFloatParameter( ParameterName, Value );

			const FScopedTransaction Transaction( LOCTEXT( "AddScalarParameter", "Add scalar parameter" ) );
			ParticleParameterTrack->Modify();
			ParticleParameterTrack->AddScalarParameterKey( ParameterName, KeyTime, Value );
		}
	}
	NotifyMovieSceneDataChanged();
}


void FParticleParameterTrackEditor::AddVectorParameter( FGuid ObjectBinding, UMovieSceneParticleParameterTrack* ParticleParameterTrack, FName ParameterName )
{
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	float KeyTime = GetTimeForKey( MovieSceneSequence );

	UObject* Object = GetSequencer()->GetFocusedMovieSceneSequenceInstance()->FindObject( ObjectBinding, *GetSequencer() );
	AEmitter* Emitter = Cast<AEmitter>( Object );
	if ( Emitter != nullptr )
	{
		UParticleSystemComponent* ParticleSystemComponent = Emitter->GetParticleSystemComponent();
		if ( ParticleSystemComponent != nullptr )
		{
			FVector Value;
			ParticleSystemComponent->GetVectorParameter( ParameterName, Value );

			const FScopedTransaction Transaction( LOCTEXT( "AddVectorParameter", "Add vector parameter" ) );
			ParticleParameterTrack->Modify();
			ParticleParameterTrack->AddVectorParameterKey( ParameterName, KeyTime, Value );
		}
	}
	NotifyMovieSceneDataChanged();
}


void FParticleParameterTrackEditor::AddColorParameter( FGuid ObjectBinding, UMovieSceneParticleParameterTrack* ParticleParameterTrack, FName ParameterName )
{
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	float KeyTime = GetTimeForKey( MovieSceneSequence );

	UObject* Object = GetSequencer()->GetFocusedMovieSceneSequenceInstance()->FindObject( ObjectBinding, *GetSequencer() );
	AEmitter* Emitter = Cast<AEmitter>( Object );
	if ( Emitter != nullptr )
	{
		UParticleSystemComponent* ParticleSystemComponent = Emitter->GetParticleSystemComponent();
		if ( ParticleSystemComponent != nullptr )
		{
			FLinearColor Value;
			ParticleSystemComponent->GetColorParameter( ParameterName, Value );

			const FScopedTransaction Transaction( LOCTEXT( "AddColorParameter", "Add color parameter" ) );
			ParticleParameterTrack->Modify();
			ParticleParameterTrack->AddColorParameterKey( ParameterName, KeyTime, Value );
		}
	}
	NotifyMovieSceneDataChanged();
}


#undef LOCTEXT_NAMESPACE
