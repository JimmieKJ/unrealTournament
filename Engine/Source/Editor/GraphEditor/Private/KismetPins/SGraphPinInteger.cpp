// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SGraphPin.h"
#include "SGraphPinString.h"
#include "SGraphPinNum.h"
#include "SGraphPinInteger.h"
#include "DefaultValueHelper.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"

void SGraphPinInteger::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPinNum::Construct(SGraphPinNum::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinInteger::GetDefaultValueWidget()
{
	check(GraphPinObj);

	if (const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(GraphPinObj->GetSchema()))
	{
		FString PinSubCategory = GraphPinObj->PinType.PinSubCategory;
		const UEnum* BitmaskEnum = Cast<UEnum>(GraphPinObj->PinType.PinSubCategoryObject.Get());

		if (PinSubCategory == K2Schema->PSC_Bitmask)
		{
			struct BitmaskFlagInfo
			{
				int32 Value;
				FText DisplayName;
				FText ToolTipText;
			};

			const int32 BitmaskBitCount = sizeof(int32) << 3;

			TArray<BitmaskFlagInfo> BitmaskFlags;
			BitmaskFlags.Reserve(BitmaskBitCount);

			if(BitmaskEnum)
			{
				for(int32 BitmaskEnumIndex = 0; BitmaskEnumIndex < BitmaskEnum->NumEnums() - 1; ++BitmaskEnumIndex)
				{
					int8 BitmaskFlagIndex = BitmaskEnum->GetValueByIndex(BitmaskEnumIndex);
					if(BitmaskFlagIndex >= 0 && BitmaskFlagIndex < BitmaskBitCount)
					{
						BitmaskFlagInfo* BitmaskFlag = new(BitmaskFlags) BitmaskFlagInfo();

						BitmaskFlag->Value = static_cast<int32>(1 << BitmaskFlagIndex);

						BitmaskFlag->DisplayName = BitmaskEnum->GetDisplayNameText(BitmaskEnumIndex);
						if(BitmaskFlag->DisplayName.IsEmpty())
						{
							BitmaskFlag->DisplayName = FText::FromString(BitmaskEnum->GetEnumName(BitmaskEnumIndex));
						}

						BitmaskFlag->ToolTipText = BitmaskEnum->GetToolTipText(BitmaskEnumIndex);
						if(BitmaskFlag->ToolTipText.IsEmpty())
						{
							BitmaskFlag->ToolTipText = FText::Format(NSLOCTEXT("GraphEditor", "BitmaskDefaultFlagToolTipText", "Toggle {0} on/off"), BitmaskFlag->DisplayName);
						}
					}
				}
			}
			else
			{
				for(int BitmaskFlagIndex = 0; BitmaskFlagIndex < BitmaskBitCount; ++BitmaskFlagIndex)
				{
					BitmaskFlagInfo* BitmaskFlag = new(BitmaskFlags) BitmaskFlagInfo();

					BitmaskFlag->Value = static_cast<int32>(1 << BitmaskFlagIndex);
					BitmaskFlag->DisplayName = FText::Format(NSLOCTEXT("GraphEditor", "BitmaskDefaultFlagDisplayName", "Flag {0}"), FText::AsNumber(BitmaskFlagIndex + 1));
					BitmaskFlag->ToolTipText = FText::Format(NSLOCTEXT("GraphEditor", "BitmaskDefaultFlagToolTipText", "Toggle {0} on/off"), BitmaskFlag->DisplayName);
				}
			}

			const auto& GetComboButtonText = [this, BitmaskFlags]() -> FText
			{
				int32 BitmaskValue = FCString::Atoi(*GraphPinObj->GetDefaultAsString());
				if(BitmaskValue != 0)
				{
					if(BitmaskValue & (BitmaskValue - 1))
					{
						return NSLOCTEXT("GraphEditor", "BitmaskButtonContentMultipleBitsSet", "(Multiple)");
					}
					else
					{
						for(int i = 0; i < BitmaskFlags.Num(); ++i)
						{
							if(BitmaskValue & BitmaskFlags[i].Value)
							{
								return BitmaskFlags[i].DisplayName;
							}
						}
					}
				}

				return NSLOCTEXT("GraphEditor", "BitmaskButtonContentNoFlagsSet", "(No Flags)");
			};

			return SNew(SComboButton)
				.ContentPadding(3)
				.MenuPlacement(MenuPlacement_BelowAnchor)
				.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
				.ButtonContent()
				[
					// Wrap in configurable box to restrain height/width of menu
					SNew(SBox)
					.MinDesiredWidth(84.0f)
					[
						SNew(STextBlock)
						.Text_Lambda(GetComboButtonText)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
				.OnGetMenuContent_Lambda([this, BitmaskFlags]()
				{
					FMenuBuilder MenuBuilder(false, nullptr);

					for(int i = 0; i < BitmaskFlags.Num(); ++i)
					{
						MenuBuilder.AddMenuEntry(
							BitmaskFlags[i].DisplayName,
							BitmaskFlags[i].ToolTipText,
							FSlateIcon(),
							FUIAction
							(
								FExecuteAction::CreateLambda([=]()
								{
									const int32 CurValue = FCString::Atoi(*GraphPinObj->GetDefaultAsString());
									GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FString::FromInt(CurValue ^ BitmaskFlags[i].Value));
								}),
								FCanExecuteAction(),
								FIsActionChecked::CreateLambda([=]() -> bool
								{
									const int32 CurValue = FCString::Atoi(*GraphPinObj->GetDefaultAsString());
									return (CurValue & BitmaskFlags[i].Value) != 0;
								})
							),
							NAME_None,
							EUserInterfaceActionType::Check);
					}

					return MenuBuilder.MakeWidget();
				});
		}
	}
	
	return SGraphPinNum::GetDefaultValueWidget();
}

void SGraphPinInteger::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	const FString TypeValueString = NewTypeInValue.ToString();
	if (FDefaultValueHelper::IsStringValidFloat(TypeValueString) || FDefaultValueHelper::IsStringValidInteger(TypeValueString))
	{
		if (GraphPinObj->GetDefaultAsString() != TypeValueString)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangeNumberPinValue", "Change Number Pin Value"));
			GraphPinObj->Modify();

			// Round-tripped here to allow floating point values to be pasted and truncated rather than failing to be set at all
			const int32 IntValue = FCString::Atoi(*TypeValueString);
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FString::FromInt(IntValue));
		}
	}
}
