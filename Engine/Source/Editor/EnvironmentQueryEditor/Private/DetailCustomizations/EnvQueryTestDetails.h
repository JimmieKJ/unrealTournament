// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"
#include "../STestFunctionWidget.h"

class FEnvQueryTestDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

protected:

	/** cached name, value pairs of enums that may change available options based on other options. */
	struct FStringIntPair
	{
		FString Str;
		int32 Int;

		FStringIntPair() {}
		FStringIntPair(FString InStr, int32 InInt) : Str(InStr), Int(InInt) {}
	};

	TSharedPtr<IPropertyHandle> FilterTypeHandle;
	TSharedPtr<IPropertyHandle> ScoreEquationHandle;
	TSharedPtr<IPropertyHandle> TestPurposeHandle;
	TSharedPtr<IPropertyHandle> ClampMinTypeHandle;
	TSharedPtr<IPropertyHandle> ClampMaxTypeHandle;
	TSharedPtr<IPropertyHandle> ScoreClampMinHandle;
	TSharedPtr<IPropertyHandle> FloatValueMinHandle;
	TSharedPtr<IPropertyHandle> ScoreClampMaxHandle;
	TSharedPtr<IPropertyHandle> FloatValueMaxHandle;
	TSharedPtr<IPropertyHandle> ScoreHandle;

	bool IsFiltering() const;
	bool IsScoring() const;

	bool UsesFilterMin() const;
	bool UsesFilterMax() const;

	FString GetCurrentFilterTestDesc() const;
	FString GetScoreEquationInfo() const;

 	EVisibility GetScoreVisibility() const;
	EVisibility GetClampingVisibility() const;

	TSharedRef<SWidget> OnGetFilterTestContent();
	void BuildFilterTestValues();

	void BuildScoreEquationValues();
	TSharedRef<SWidget> OnGetEquationValuesContent();
	FString GetEquationValuesDesc() const;
	void OnScoreEquationChange(int32 Index);

	void OnFilterTestChange(int32 Index);
	void OnClampMinTestChange(int32 Index);
	void OnClampMaxTestChange(int32 Index);

	TSharedRef<SWidget> OnGetClampMaxTypeContent();
	FString GetClampMaxTypeDesc() const;

	TSharedRef<SWidget> OnGetClampMinTypeContent();
	FString GetClampMinTypeDesc() const;

	bool IsMatchingBoolValue() const;

	// Is this a float test at all?
	EVisibility GetFloatTestVisibility() const;
	
	// Is this a float test that is filtering?
	EVisibility GetFloatFilterVisibility() const;

	// Is this a float test that is scoring?
	EVisibility GetFloatScoreVisibility() const;
	
	EVisibility GetTestPreviewVisibility() const;
	EVisibility GetVisibilityOfFloatValueMin() const;
	EVisibility GetVisibilityOfFloatValueMax() const;
	EVisibility GetVisibilityOfValueMinForScoreClamping() const;
	EVisibility GetVisibilityOfValueMaxForScoreClamping() const;
	EVisibility GetBoolValueVisibilityForScoring() const;
	EVisibility GetBoolValueVisibility() const;
	EVisibility GetVisibilityOfScoreClampMinimum() const;
	EVisibility GetVisibilityOfScoreClampMaximum() const;

	void BuildScoreClampingTypeValues(bool bBuildMinValues, TArray<FStringIntPair>& ClampTypeValues) const;
	
	void UpdateTestFunctionPreview() const;
	void FillEquationSamples(uint8 EquationType, bool bInversed, TArray<float>& Samples) const;
	TSharedPtr<STestFunctionWidget> PreviewWidget;


	TArray<FStringIntPair> FilterTestValues;

	TArray<FStringIntPair> ClampMinTypeValues;
	TArray<FStringIntPair> ClampMaxTypeValues;

	TArray<FStringIntPair> ScoreEquationValues;

	TWeakObjectPtr<UObject> MyTest;

	FORCEINLINE bool AllowWritingToFiltersFromScore() const { return false; }
	TAttribute<bool> AllowWriting;
};
