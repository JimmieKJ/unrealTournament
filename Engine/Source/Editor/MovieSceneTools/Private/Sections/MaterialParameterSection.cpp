// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MaterialParameterSection.h"
#include "MovieSceneMaterialParameterSection.h"

#define LOCTEXT_NAMESPACE "MaterialParameterSection"

DECLARE_DELEGATE(FLayoutParameterDelegate);

struct FIndexAndLayoutParameterDelegate
{
	int32 Index;
	FLayoutParameterDelegate LayoutParameterDelegate;

	bool operator<(FIndexAndLayoutParameterDelegate const& Other) const
	{
		return Index < Other.Index;
	}
};

void LayoutScalarParameter( ISectionLayoutBuilder* LayoutBuilder, FScalarParameterNameAndCurve* ScalarParameterNameAndCurve, UMovieSceneMaterialParameterSection* ParameterSection )
{
	LayoutBuilder->AddKeyArea(
		ScalarParameterNameAndCurve->ParameterName,
		FText::FromName( ScalarParameterNameAndCurve->ParameterName ),
		MakeShareable( new FFloatCurveKeyArea( &ScalarParameterNameAndCurve->ParameterCurve, ParameterSection ) ) );
}

void LayoutVectorParameter( ISectionLayoutBuilder* LayoutBuilder, FVectorParameterNameAndCurves* VectorParameterNameAndCurves, UMovieSceneMaterialParameterSection* ParameterSection )
{
	LayoutBuilder->PushCategory( VectorParameterNameAndCurves->ParameterName, FText::FromName( VectorParameterNameAndCurves->ParameterName ) );
	{
		LayoutBuilder->AddKeyArea( "R", NSLOCTEXT( "MaterialParameterSection", "RedArea", "Red" ),
			MakeShareable( new FFloatCurveKeyArea( &VectorParameterNameAndCurves->RedCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "G", NSLOCTEXT( "MaterialParameterSection", "GreenArea", "Green" ), 
			MakeShareable( new FFloatCurveKeyArea( &VectorParameterNameAndCurves->GreenCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "B", NSLOCTEXT( "MaterialParameterSection", "BlueArea", "Blue" ), 
			MakeShareable( new FFloatCurveKeyArea( &VectorParameterNameAndCurves->BlueCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "A", NSLOCTEXT( "MaterialParameterSection", "OpacityArea", "Opacity" ), 
			MakeShareable( new FFloatCurveKeyArea( &VectorParameterNameAndCurves->AlphaCurve, ParameterSection ) ) );
	}
	LayoutBuilder->PopCategory();
}

void FMaterialParameterSection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	// TODO - This seems a bit convoluted here to have to use delegates to get the layout to happen in the right order based on the indices.
	UMovieSceneMaterialParameterSection* ParameterSection = Cast<UMovieSceneMaterialParameterSection>( &SectionObject );
	TArray<FIndexAndLayoutParameterDelegate> IndexAndLayoutDelegates;

	// Collect scalar parameters.
	TArray<FScalarParameterNameAndCurve>* ScalarParameterNamesAndCurves = ParameterSection->GetScalarParameterNamesAndCurves();
	for ( int32 i = 0; i < ScalarParameterNamesAndCurves->Num(); i++)
	{
		FIndexAndLayoutParameterDelegate IndexAndLayoutDelegate;
		IndexAndLayoutDelegate.Index = (*ScalarParameterNamesAndCurves)[i].Index;
		IndexAndLayoutDelegate.LayoutParameterDelegate.BindStatic(&LayoutScalarParameter, &LayoutBuilder, &(*ScalarParameterNamesAndCurves)[i], ParameterSection);
		IndexAndLayoutDelegates.Add(IndexAndLayoutDelegate);
	}

	// Collect vector parameters
	TArray<FVectorParameterNameAndCurves>* VectorParameterNamesAndCurves = ParameterSection->GetVectorParameterNamesAndCurves();
	for ( int32 i = 0; i < VectorParameterNamesAndCurves->Num(); i++ )
	{
		FIndexAndLayoutParameterDelegate IndexAndLayoutDelegate;
		IndexAndLayoutDelegate.Index = (*VectorParameterNamesAndCurves)[i].Index;
		IndexAndLayoutDelegate.LayoutParameterDelegate.BindStatic( &LayoutVectorParameter, &LayoutBuilder, &(*VectorParameterNamesAndCurves)[i], ParameterSection );
		IndexAndLayoutDelegates.Add( IndexAndLayoutDelegate );
	}

	// Sort and perform layout
	IndexAndLayoutDelegates.Sort();
	for ( FIndexAndLayoutParameterDelegate& IndexAndLayoutDelegate : IndexAndLayoutDelegates )
	{
		IndexAndLayoutDelegate.LayoutParameterDelegate.Execute();
	}
}

bool FMaterialParameterSection::RequestDeleteCategory( const TArray<FName>& CategoryNamePath )
{
	const FScopedTransaction Transaction( LOCTEXT( "DeleteVectorParameter", "Delete vector parameter" ) );
	UMovieSceneMaterialParameterSection* ParameterSection = Cast<UMovieSceneMaterialParameterSection>( &SectionObject );
	ParameterSection->Modify();
	return ParameterSection->RemoveVectorParameter( CategoryNamePath[0] );
}

bool FMaterialParameterSection::RequestDeleteKeyArea( const TArray<FName>& KeyAreaNamePath )
{
	// Only handle paths with a single name, in all other cases the user is deleting a component of a vector parameter.
	if ( KeyAreaNamePath.Num() == 1)
	{
		const FScopedTransaction Transaction( LOCTEXT( "DeleteScalarParameter", "Delete scalar parameter" ) );
		UMovieSceneMaterialParameterSection* ParameterSection = Cast<UMovieSceneMaterialParameterSection>( &SectionObject );
		ParameterSection->Modify();
		return ParameterSection->RemoveScalarParameter( KeyAreaNamePath[0] );
	}
	return false;
}

#undef LOCTEXT_NAMESPACE