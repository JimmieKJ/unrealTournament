// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PropertyEditorPrivatePCH.h"
#include "PropertyNode.h"
#include "CategoryPropertyNode.h"
#include "ItemPropertyNode.h"
#include "ObjectEditorUtils.h"
#include "StructurePropertyNode.h"

void FStructurePropertyNode::InitChildNodes()
{
	const bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const UStruct* Struct = StructData.IsValid() ? StructData->GetStruct() : NULL;

	for (TFieldIterator<UProperty> It(Struct); It; ++It)
	{
		UProperty* StructMember = *It;
		if (StructMember && (bShouldShowHiddenProperties || (StructMember->PropertyFlags & CPF_Edit)))
		{
			TSharedPtr<FItemPropertyNode> NewItemNode(new FItemPropertyNode);//;//CreatePropertyItem(StructMember,INDEX_NONE,this);

			FPropertyNodeInitParams InitParams;
			InitParams.ParentNode = SharedThis(this);
			InitParams.Property = StructMember;
			InitParams.ArrayOffset = 0;
			InitParams.ArrayIndex = INDEX_NONE;
			InitParams.bAllowChildren = true;
			InitParams.bForceHiddenPropertyVisibility = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
			InitParams.bCreateCategoryNodes = false;

			NewItemNode->InitNode(InitParams);
			AddChildNode(NewItemNode);
		}
	}
}