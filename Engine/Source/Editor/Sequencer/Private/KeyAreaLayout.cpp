
#include "SequencerPrivatePCH.h"
#include "KeyAreaLayout.h"

FKeyAreaLayoutElement FKeyAreaLayoutElement::FromGroup(const TSharedRef<FGroupedKeyArea>& InKeyAreaGroup, float InOffset, TOptional<float> InHeight)
{
	FKeyAreaLayoutElement Tmp;
	Tmp.Type = Group;
	Tmp.KeyArea = InKeyAreaGroup;
	Tmp.LocalOffset = InOffset;
	Tmp.Height = InHeight;
	return Tmp;
}

FKeyAreaLayoutElement FKeyAreaLayoutElement::FromKeyAreaNode(const TSharedRef<FSequencerSectionKeyAreaNode>& InKeyAreaNode, int32 SectionIndex, float InOffset)
{
	FKeyAreaLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.KeyArea = InKeyAreaNode->GetKeyArea(SectionIndex);
	Tmp.LocalOffset = InOffset;

	if (!InKeyAreaNode->IsTopLevel())
	{
		Tmp.Height = InKeyAreaNode->GetNodeHeight();
	}
	return Tmp;
}

FKeyAreaLayoutElement::EType FKeyAreaLayoutElement::GetType() const
{
	return Type;
}

float FKeyAreaLayoutElement::GetOffset() const
{
	return LocalOffset;
}

float FKeyAreaLayoutElement::GetHeight(const FGeometry& InParentGeometry) const
{
	return Height.IsSet() ? Height.GetValue() : InParentGeometry.GetDrawSize().Y;
}

TSharedRef<IKeyArea> FKeyAreaLayoutElement::GetKeyArea() const
{
	return KeyArea.ToSharedRef();
}

FKeyAreaLayout::FKeyAreaLayout(FSequencerDisplayNode& InNode, int32 InSectionIndex)
{
	TotalHeight = 0.f;

	float LastPadding = 0.f;

	auto SetupKeyArea = [&](FSequencerDisplayNode& Node){

		if (Node.GetType() == ESequencerNode::KeyArea)
		{
			Elements.Add(FKeyAreaLayoutElement::FromKeyAreaNode(StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node.AsShared()), InSectionIndex, TotalHeight));
		}
		else if (!Node.IsExpanded())
		{
			Elements.Add(FKeyAreaLayoutElement::FromGroup(Node.UpdateKeyGrouping(InSectionIndex), TotalHeight, Node.GetNodeHeight()));
		}

	};


	// First, layout the parent
	{
		FSequencerDisplayNode* LayoutNode = &InNode;
		if (InNode.GetType() == ESequencerNode::Track)
		{
			TSharedPtr<FSequencerDisplayNode> KeyAreaNode = static_cast<FSequencerTrackNode&>(InNode).GetTopLevelKeyNode();
			if (KeyAreaNode.IsValid())
			{
				LayoutNode = KeyAreaNode.Get();
			}
		}

		SetupKeyArea(*LayoutNode);

		LastPadding = LayoutNode->GetNodePadding().Bottom;
		TotalHeight += LayoutNode->GetNodeHeight() + LastPadding;
	}

	// Then any children
	InNode.TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node){
		
		TotalHeight += Node.GetNodePadding().Top;

		SetupKeyArea(Node);

		LastPadding = Node.GetNodePadding().Bottom;
		TotalHeight += Node.GetNodeHeight() + LastPadding;
		return true;

	}, false);

	// Remove the padding from the bottom
	TotalHeight -= LastPadding;
}

const TArray<FKeyAreaLayoutElement>& FKeyAreaLayout::GetElements() const
{
	return Elements;
}

float FKeyAreaLayout::GetTotalHeight() const
{
	return TotalHeight;
}