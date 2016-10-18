//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SEditorViewport.h"
#include "AssetThumbnail.h"

#include "SubstanceCoreTypedefs.h"
#include "SubstanceGraphInstance.h"
#include "SubstanceFGraph.h"
#include "SubstanceCoreHelpers.h"

/*-----------------------------------------------------------------------------
   SSubstanceEditorPanel
-----------------------------------------------------------------------------*/

class SSubstanceEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSubstanceEditorPanel )
		{}

		SLATE_ARGUMENT(TWeakPtr<ISubstanceEditor>, SubstanceEditor)
	SLATE_END_ARGS()

	/** Destructor */
	~SSubstanceEditorPanel();

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs);

	TWeakPtr<ISubstanceEditor> GetSubstanceEditor() const;

	void OnUndo();
	void OnRedo();

private:
	template<typename T> struct FInputValue
	{
		T							NewValue;
		TSharedPtr<input_inst_t>	Input;
		int32						Index;
	};

	USubstanceGraphInstance* Graph;
	TAttribute<FLinearColor> Color;
	
	TMap<TSharedPtr<FAssetThumbnail>, Substance::FImageInputInstance*> ThumbnailInputs;

	void ConstructDescription();
	void ConstructOutputs();
	void ConstructInputs();

	bool GetInputsEnabled() const { return !Graph->bFreezed; }
	void OnFreezeGraphValueChanged(ECheckBoxState InNewState);
	ECheckBoxState GetFreezeGraphValue() const;

	TSharedRef<SWidget> GetInputWidget(TSharedPtr<input_inst_t> Input);
	TSharedRef<SWidget> GetImageInputWidget(TSharedPtr<input_inst_t> Input);
	FString GetImageInputPath(TSharedPtr< input_inst_t > Input);

	template< typename T > void SetValue(T NewValue, TSharedPtr<input_inst_t> Input, int32 Index);
	template< typename T > void SetValues(uint32 NumInputValues, FInputValue<T>* Inputs);
	template< typename T > TOptional< T > GetInputValue(TSharedPtr<input_inst_t> Input, int32 Index) const;
	FText GetInputValue(TSharedPtr<input_inst_t> Input) const;

	void OnToggleValueChanged(ECheckBoxState InNewState, TSharedPtr<input_inst_t> Input);
	ECheckBoxState GetToggleValue(TSharedPtr<input_inst_t> Input) const;

	void OnToggleOutput(ECheckBoxState InNewState, output_inst_t* Input);
	ECheckBoxState GetOutputState(output_inst_t*  Input) const;
	void OnBrowseTexture(output_inst_t* Texture);

	FReply PickColor(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TSharedPtr<input_inst_t> Input);
	FReply RandomizeSeed(TSharedPtr<input_inst_t> Input);

	FLinearColor GetColor(TSharedPtr<input_inst_t> Input) const;
	void UpdateColor(FLinearColor NewColor, TSharedPtr<input_inst_t> Input);
	void CancelColor(FLinearColor OldColor, TSharedPtr<input_inst_t> Input);

	void OnComboboxSelectionChanged( TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo, TSharedPtr< input_inst_t > Input);
	TSharedRef<SWidget> MakeInputComboWidget( TSharedPtr<FString> InItem );

	bool OnAssetDraggedOver(const UObject* InObject) const;
	void OnAssetDropped(UObject* InObject, TSharedPtr<input_inst_t> Input, TSharedPtr<FAssetThumbnail> Thumbnail);
	void OnSetImageInput(const UObject* InObject, TSharedPtr<input_inst_t> Input, TSharedPtr<FAssetThumbnail> Thumbnail);

	FReply OnResetImageInput(TSharedPtr<input_inst_t> Input, TSharedPtr<FAssetThumbnail> Thumbnail);
	FReply OnResetInput(TSharedPtr< input_inst_t > Input);
	void OnBrowseImageInput(img_input_inst_t* ImageInput);

	void OnUseSelectedImageInput(TSharedPtr<input_inst_t> Input, TSharedPtr<FAssetThumbnail> Thumbnail);
	void OnGetClassesForAssetPicker(TArray<const UClass*>& OutClasses);
	void OnAssetSelected(const FAssetData& AssetData, TSharedPtr<input_inst_t> Input, TSharedPtr<FAssetThumbnail> Thumbnail);

	TSharedRef<SWidget> MakeInputSizeComboWidget( TSharedPtr<FString> InItem );
	void OnSizeComboboxSelectionChanged( TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo, TSharedPtr< input_inst_t > Input, int idx);
	FText GetOutputSizeValue(TSharedPtr<input_inst_t> Input, int32 Idx) const;
	ECheckBoxState GetLockRatioValue() const;
	void OnLockRatioValueChanged(ECheckBoxState InNewState);

	void BeginSliderMovement(TSharedPtr<input_inst_t> Input);

	TSharedRef<SHorizontalBox> GetBaseInputWidget(TSharedPtr< input_inst_t > Input, FString InputLabel = TEXT(""));
	
	template< typename T > TSharedRef<SHorizontalBox> GetInputWidgetSlider(TSharedPtr<input_inst_t> Input);
	template< typename T > TSharedRef<SNumericEntryBox<T>> GetInputWidgetSlider_internal(TSharedPtr<input_inst_t> Input, const int32 SliderIndex);

	TSharedRef<SWidget> GetInputWidgetAngle(TSharedPtr<input_inst_t> Input);
	TSharedRef<SWidget> GetInputWidgetCombobox(TSharedPtr<input_inst_t> Input);
	TSharedRef<SWidget> GetInputWidgetTogglebutton(TSharedPtr<input_inst_t> Input);
	TSharedRef<SWidget> GetInputWidgetSizePow2(TSharedPtr<input_inst_t> Input);
	TSharedRef<SWidget> GetInputWidgetRandomSeed(TSharedPtr<input_inst_t> Input);

	/** Pointer back to the Substance editor tool that owns us */
	TWeakPtr<ISubstanceEditor> SubstanceEditorPtr;

	TSharedPtr<SExpandableArea> DescArea;
	TSharedPtr<SExpandableArea> OutputsArea;
	TSharedPtr<SExpandableArea> InputsArea;
	TSharedPtr<SExpandableArea> ImageInputsArea;

	typedef TArray< TSharedPtr< FString > > SharedFStringArray;
	TArray< TSharedPtr< SharedFStringArray > > ComboBoxLabels;
	TAttribute<bool> RatioLocked;
	TSharedPtr< FAssetThumbnailPool > ThumbnailPool;

	int OutputSizePow2Min;
	int OutputSizePow2Max;
};


template< typename T > void GetMinMaxValues(input_desc_t* Desc, const int32 i, T& Min, T& Max, bool& Clamped);

template< > TSharedRef< SNumericEntryBox< float > > SSubstanceEditorPanel::GetInputWidgetSlider_internal<float>(TSharedPtr<input_inst_t> Input, const int32 SliderIndex);

template< > TSharedRef< SNumericEntryBox< int32 > > SSubstanceEditorPanel::GetInputWidgetSlider_internal<int32>(TSharedPtr<input_inst_t> Input, const int32 SliderIndex);
