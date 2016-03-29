
#include "SequencerPrivatePCH.h"
#include "SectionLayout.h"

FSectionLayoutElement FSectionLayoutElement::FromGroup(const TSharedRef<FSequencerDisplayNode>& InNode, const TSharedRef<FGroupedKeyArea>& InKeyAreaGroup, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Group;
	Tmp.KeyArea = InKeyAreaGroup;
	Tmp.LocalOffset = InOffset;
	Tmp.Height = InNode->GetNodeHeight();
	Tmp.DisplayNode = InNode;
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::FromKeyAreaNode(const TSharedRef<FSequencerSectionKeyAreaNode>& InKeyAreaNode, int32 SectionIndex, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.KeyArea = InKeyAreaNode->GetKeyArea(SectionIndex);
	Tmp.LocalOffset = InOffset;
	Tmp.DisplayNode = InKeyAreaNode;
	Tmp.Height = InKeyAreaNode->GetNodeHeight();
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::FromTrack(const TSharedRef<FSequencerTrackNode>& InTrackNode, int32 SectionIndex, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.KeyArea = InTrackNode->GetTopLevelKeyNode()->GetKeyArea(SectionIndex);
	Tmp.LocalOffset = InOffset;
	Tmp.DisplayNode = InTrackNode;
	Tmp.Height = InTrackNode->GetNodeHeight();
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::EmptySpace(const TSharedRef<FSequencerDisplayNode>& InNode, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.LocalOffset = InOffset;
	Tmp.DisplayNode = InNode;
	Tmp.Height = InNode->GetNodeHeight();
	return Tmp;
}

FSectionLayoutElement::EType FSectionLayoutElement::GetType() const
{
	return Type;
}

float FSectionLayoutElement::GetOffset() const
{
	return LocalOffset;
}

float FSectionLayoutElement::GetHeight() const
{
	return Height;
}

TSharedPtr<IKeyArea> FSectionLayoutElement::GetKeyArea() const
{
	return KeyArea;
}

TSharedPtr<FSequencerDisplayNode> FSectionLayoutElement::GetDisplayNode() const
{
	return DisplayNode;
}

FSectionLayout::FSectionLayout(FSequencerDisplayNode& InNode, int32 InSectionIndex)
{
	float VerticalOffset = 0.f;

	auto SetupKeyArea = [&](FSequencerDisplayNode& Node){

		if (Node.GetType() == ESequencerNode::KeyArea)
		{
			Elements.Add(FSectionLayoutElement::FromKeyAreaNode(
				StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node.AsShared()),
				InSectionIndex,
				VerticalOffset
			));
		}
		else if (Node.GetType() == ESequencerNode::Track && static_cast<FSequencerTrackNode&>(Node).GetTopLevelKeyNode().IsValid())
		{
			Elements.Add(FSectionLayoutElement::FromTrack(
				StaticCastSharedRef<FSequencerTrackNode>(Node.AsShared()),
				InSectionIndex,
				VerticalOffset
			));
		}
		else if (!Node.IsExpanded())
		{
			Elements.Add(FSectionLayoutElement::FromGroup(
				Node.AsShared(),
				Node.UpdateKeyGrouping(InSectionIndex),
				VerticalOffset
			));
		}
		else
		{
			// It's benign space
			Elements.Add(FSectionLayoutElement::EmptySpace(
				Node.AsShared(),
				VerticalOffset
			));
		}

	};

	// First, layout the parent
	{
		VerticalOffset += InNode.GetNodePadding().Top;

		SetupKeyArea(InNode);

		VerticalOffset += InNode.GetNodeHeight() + InNode.GetNodePadding().Bottom;
	}

	// Then any children
	InNode.TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node){
		
		VerticalOffset += Node.GetNodePadding().Top;

		SetupKeyArea(Node);

		VerticalOffset += Node.GetNodeHeight() + Node.GetNodePadding().Bottom;
		return true;

	}, false);
}

const TArray<FSectionLayoutElement>& FSectionLayout::GetElements() const
{
	return Elements;
}

float FSectionLayout::GetTotalHeight() const
{
	if (Elements.Num())
	{
		return Elements.Last().GetOffset() + Elements.Last().GetDisplayNode()->GetNodePadding().Combined() + Elements.Last().GetHeight();
	}
	return 0.f;
}