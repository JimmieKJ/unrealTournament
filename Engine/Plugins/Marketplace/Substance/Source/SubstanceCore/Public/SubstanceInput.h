//! @file SubstanceInput.h
//! @brief Substance input definitions
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "framework/imageinput.h"
#include "substance_public.h"

class USubstanceImageInput;

namespace Substance
{
	//! @brief The list of input widgets available in Designer
	enum InputWidget
	{
		Input_NoWidget,         //!< Widget not defined
		Input_Slider,           //!< Simple widget slider 
		Input_Angle,            //!< Angle widget
		Input_Color,            //!< Color widget
		Input_Togglebutton,     //!< On/Off toggle button or checkbox
		Input_Combobox,         //!< Drop down list
		Input_SizePow2,         //!< Size Pow 2 list
		Input_Image,            //!< Image input widget
		Input_int32ERNAL_COUNT
	};


	//! @brief Substance input basic description
	struct FInputDescBase 
	{
        //! @brief Virtual destructor
        virtual ~FInputDescBase() {}
        
		//! @brief Return an instance of the Input
		TSharedPtr< input_inst_t > Instantiate();
		
		virtual const void* getRawDefault() const { return NULL; }

		SUBSTANCECORE_API bool IsNumerical() const;
		bool IsImage() const {return !IsNumerical();}

		//! @brief Internal identifier of the input
		FString			Identifier;

		//! @brief User-readable identifier of the input
		FString			Label;

		//! @brief User-readable identifier of the input
		FString			Group;

		//! @brief Preferred widget for edition
		InputWidget		Widget;

		//! @brief Substance UID of the reference input desc
		uint32			Uid;

		//! @brief Index of the input, used for display order
		uint32			Index;

		//! @brief Type of the input
		int32			Type;

		//! @brief list of output UIDSs altered by this input
		Substance::List< uint32 >  AlteredOutputUids;

		//! @brief Separate the inputs that cause a big change in the texture (random seed...)
		uint32		IsHeavyDuty:1;
	};

	SUBSTANCECORE_API TSharedPtr< FInputDescBase > FindInput(
		Substance::List< TSharedPtr<FInputDescBase> >& Inputs, 
		const uint32 uid);

	SUBSTANCECORE_API TSharedPtr< FInputDescBase > FindInput(
		Substance::List< TSharedPtr<FInputDescBase> >& Inputs, 
		const FString identifier);

	SUBSTANCECORE_API TSharedPtr< FInputDescBase > FindInput(
		const Substance::List< TSharedPtr<FInputDescBase> >& Inputs,
		const uint32 uid);

	//! @brief Numerical Substance input description
	template< typename T > struct FNumericalInputDesc : public FInputDescBase
	{
		FNumericalInputDesc():IsClamped(false)
		{
			FMemory::Memzero((void*)&DefaultValue, sizeof(T));
			FMemory::Memzero((void*)&Min, sizeof(T));
			FMemory::Memzero((void*)&Max, sizeof(T));
		}

		const void* getRawDefault() const { return &DefaultValue; }

		//! @brief whether the value should be clamped in the UI
		bool IsClamped;

		T DefaultValue;
		T Min;
		T Max;
	};


	//! @brief Special desc for combobox items
	//! @note Used to store value - text map
	struct FNumericalInputDescComboBox : public FNumericalInputDesc<int32>
	{
		SUBSTANCECORE_API FNumericalInputDescComboBox(FNumericalInputDesc< int32 >*);

		TMap< int32, FString > ValueText;
	};


	//! @brief Image Substance input description
	struct FImageInputDesc : public FInputDescBase
	{
		FString Label;

		FString Desc;
	};


	//! @brief Base Instance of a Substance input
	struct FInputInstanceBase
	{
		FInputInstanceBase(FInputDescBase* Input=NULL);
		virtual ~FInputInstanceBase(){}
		
		TSharedPtr< input_inst_t > Clone();

		SUBSTANCECORE_API bool IsNumerical() const;

		virtual void Reset() = 0;

		//	//! @brief Internal use only
		virtual bool isModified(const void*) const = 0;

		uint32	Uid;

		int32	Type;

		uint32	IsHeavyDuty:1;
		uint32	UseCache:1;

		input_desc_t* Desc;

		graph_inst_t* Parent;
	};


	SUBSTANCECORE_API TSharedPtr< FInputInstanceBase > FindInput(
		Substance::List< TSharedPtr< FInputInstanceBase > >& Inputs, 
		const uint32 uid);


	//! @brief Numerical Instance of a Substance input
	struct FNumericalInputInstanceBase : public FInputInstanceBase
	{
		FNumericalInputInstanceBase(FInputDescBase* Input=NULL):
			FInputInstanceBase(Input){}

		bool isModified(const void*) const;

		template < typename T > void GetValue(TArray< T >& OutValue);

		template < typename T > void SetValue(const TArray< T >& InValue);

		virtual const void* getRawData() const = 0;

		virtual SIZE_T getRawSize() const = 0;
	};


	//! @brief Numerical Instance of a Substance input
	template< typename T = int32 > struct FNumericalInputInstance : 
		public FNumericalInputInstanceBase
	{
		FNumericalInputInstance(FInputDescBase* Input=NULL):
			FNumericalInputInstanceBase(Input),LockRatio(true){}

		T Value;

		void Reset();

		uint32 LockRatio:1; //!< @brief used only for $outputsize inputs

		const void* getRawData() const { return &Value; }

		SIZE_T getRawSize() const { return sizeof(Value); }
	};

	//! @brief Image Instance of a Substance input
	struct FImageInputInstance : public FInputInstanceBase
	{
		FImageInputInstance(FInputDescBase* Input=NULL);
		FImageInputInstance(const FImageInputInstance&);
		FImageInputInstance& operator = (const FImageInputInstance&);
		virtual ~FImageInputInstance();


		void Reset()
		{
			SetImageInput(NULL, NULL);
		}

		bool isModified(const void*) const;

		//! @brief Set a new image content
		void SetImageInput(UObject*, FGraphInstance* Parent, bool unregisterOutput = true, bool isTransacting = false);

		//! @brief Accessor on current image content (can return NULL shared ptr)
		const std::shared_ptr< Substance::ImageInput >& GetImage() const { return ImageInput; }

		//! @brief Update this when the image is modified
		mutable bool PtrModified;

		std::shared_ptr< Substance::ImageInput > ImageInput;

		UObject* ImageSource;
	};

	SIZE_T getComponentsCount(SubstanceInputType type);

} // namespace Substance

// template impl
#include "SubstanceInput.inl"
