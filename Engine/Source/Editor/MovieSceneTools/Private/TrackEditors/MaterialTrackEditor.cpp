// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MaterialTrackEditor.h"
#include "MovieSceneMaterialTrack.h"
#include "ParameterSection.h"
#include "MovieSceneParameterSection.h"


#define LOCTEXT_NAMESPACE "MaterialTrackEditor"


FMaterialTrackEditor::FMaterialTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer )
{
}


TSharedRef<ISequencerSection> FMaterialTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>(&SectionObject);
	checkf( ParameterSection != nullptr, TEXT("Unsupported section type.") );

	return MakeShareable(new FParameterSection( *ParameterSection, FText::FromName(ParameterSection->GetFName())));
}


TSharedPtr<SWidget> FMaterialTrackEditor::BuildOutlinerEditWidget( const FGuid& ObjectBinding, UMovieSceneTrack* Track )
{
	UMovieSceneMaterialTrack* MaterialTrack = Cast<UMovieSceneMaterialTrack>(Track);
	return
		SNew( SComboButton )
		.ButtonStyle( FEditorStyle::Get(), "FlatButton.Light" )
		.OnGetMenuContent( this, &FMaterialTrackEditor::OnGetAddParameterMenuContent, ObjectBinding, MaterialTrack )
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
				.Text( LOCTEXT( "AddTrackButton", "Parameter" ) )
			]
		];
}


struct FParameterNameAndAction
{
	FName ParameterName;
	FUIAction Action;

	FParameterNameAndAction( FName InParameterName, FUIAction InAction )
	{
		ParameterName = InParameterName;
		Action = InAction;
	}

	bool operator<(FParameterNameAndAction const& Other) const
	{
		return ParameterName < Other.ParameterName;
	}
};


TSharedRef<SWidget> FMaterialTrackEditor::OnGetAddParameterMenuContent( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	FMenuBuilder AddParameterMenuBuilder( true, nullptr );

	UMaterial* Material = GetMaterialForTrack( ObjectBinding, MaterialTrack );
	if ( Material != nullptr )
	{
		TArray<FParameterNameAndAction> ParameterNamesAndActions;

		// Collect scalar parameters.
		TArray<FName> ScalarParameterNames;
		TArray<FGuid> ScalarParmeterGuids;
		Material->GetAllScalarParameterNames( ScalarParameterNames, ScalarParmeterGuids );
		for ( const FName& ScalarParameterName : ScalarParameterNames )
		{
			FUIAction AddParameterMenuAction( FExecuteAction::CreateSP( this, &FMaterialTrackEditor::AddScalarParameter, ObjectBinding, MaterialTrack, ScalarParameterName ) );
			FParameterNameAndAction NameAndAction( ScalarParameterName, AddParameterMenuAction );
			ParameterNamesAndActions.Add(NameAndAction);
		}

		// Collect color parameters.
		TArray<FName> ColorParameterNames;
		TArray<FGuid> ColorParmeterGuids;
		Material->GetAllVectorParameterNames( ColorParameterNames, ColorParmeterGuids );
		for ( const FName& ColorParameterName : ColorParameterNames )
		{
			FUIAction AddParameterMenuAction( FExecuteAction::CreateSP( this, &FMaterialTrackEditor::AddColorParameter, ObjectBinding, MaterialTrack, ColorParameterName ) );
			FParameterNameAndAction NameAndAction( ColorParameterName, AddParameterMenuAction );
			ParameterNamesAndActions.Add( NameAndAction );
		}

		// Sort and generate menu.
		ParameterNamesAndActions.Sort();
		for ( FParameterNameAndAction NameAndAction : ParameterNamesAndActions )
		{
			AddParameterMenuBuilder.AddMenuEntry( FText::FromName( NameAndAction.ParameterName ), FText(), FSlateIcon(), NameAndAction.Action );
		}
	}

	return AddParameterMenuBuilder.MakeWidget();
}


UMaterial* FMaterialTrackEditor::GetMaterialForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	UMaterialInterface* MaterialInterface = GetMaterialInterfaceForTrack( ObjectBinding, MaterialTrack );
	if ( MaterialInterface != nullptr )
	{
		UMaterial* Material = Cast<UMaterial>( MaterialInterface );
		if ( Material != nullptr )
		{
			return Material;
		}
		else
		{
			UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( MaterialInterface );
			if ( MaterialInstance != nullptr )
			{
				return MaterialInstance->GetMaterial();
			}
		}
	}
	return nullptr;
}


void FMaterialTrackEditor::AddScalarParameter( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack, FName ParameterName )
{
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	float KeyTime = GetTimeForKey( MovieSceneSequence );

	UMaterial* Material = GetMaterialForTrack(ObjectBinding, MaterialTrack);
	if (Material != nullptr)
	{
		const FScopedTransaction Transaction( LOCTEXT( "AddScalarParameter", "Add scalar parameter" ) );
		float ParameterValue;
		Material->GetScalarParameterValue(ParameterName, ParameterValue);
		MaterialTrack->Modify();
		MaterialTrack->AddScalarParameterKey(ParameterName, KeyTime, ParameterValue);
	}
	NotifyMovieSceneDataChanged();
}


void FMaterialTrackEditor::AddColorParameter( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack, FName ParameterName )
{
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	float KeyTime = GetTimeForKey( MovieSceneSequence );

	UMaterial* Material = GetMaterialForTrack( ObjectBinding, MaterialTrack );
	if ( Material != nullptr )
	{
		const FScopedTransaction Transaction( LOCTEXT( "AddVectorParameter", "Add vector parameter" ) );
		FLinearColor ParameterValue;
		Material->GetVectorParameterValue( ParameterName, ParameterValue );
		MaterialTrack->Modify();
		MaterialTrack->AddColorParameterKey( ParameterName, KeyTime, ParameterValue );
	}
	NotifyMovieSceneDataChanged();
}


FComponentMaterialTrackEditor::FComponentMaterialTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMaterialTrackEditor( InSequencer )
{
}


TSharedRef<ISequencerTrackEditor> FComponentMaterialTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable( new FComponentMaterialTrackEditor( OwningSequencer ) );
}


bool FComponentMaterialTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneComponentMaterialTrack::StaticClass();
}


UMaterialInterface* FComponentMaterialTrackEditor::GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	UObject* ComponentObject = GetSequencer()->GetFocusedMovieSceneSequenceInstance()->FindObject( ObjectBinding, *GetSequencer() );
	UPrimitiveComponent* Component = Cast<UPrimitiveComponent>( ComponentObject );
	UMovieSceneComponentMaterialTrack* ComponentMaterialTrack = Cast<UMovieSceneComponentMaterialTrack>( MaterialTrack );
	if ( Component != nullptr && ComponentMaterialTrack != nullptr )
	{
		return Component->GetMaterial( ComponentMaterialTrack->GetMaterialIndex() );
	}
	return nullptr;
}


#undef LOCTEXT_NAMESPACE
