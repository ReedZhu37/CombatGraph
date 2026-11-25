#include "ComboGraph/RZCombatComboGraphNodes.h"

#include "EdGraph/EdGraph.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "RZCombatComboGraphNodes"

namespace RZCombatComboGraphNodes
{
	static const FName PinCategory(TEXT("RZCombatCombo"));

	static UEdGraphPin* GetSourceOutputPin(UEdGraphNode* Node)
	{
		if (URZCombatComboGraphNode_Entry* EntryNode = Cast<URZCombatComboGraphNode_Entry>(Node))
		{
			return EntryNode->GetOutputPin();
		}

		if (URZCombatComboGraphNode_Combo* ComboNode = Cast<URZCombatComboGraphNode_Combo>(Node))
		{
			return ComboNode->GetOutputPin();
		}

		return nullptr;
	}

	static bool SortTransitionsByTargetHorizontalOrder(
		const URZCombatComboGraphNode_Transition& A,
		const URZCombatComboGraphNode_Transition& B
	)
	{
		const URZCombatComboGraphNode_Combo* TargetA = A.GetNextComboNode();
		const URZCombatComboGraphNode_Combo* TargetB = B.GetNextComboNode();
		if (!TargetA || !TargetB)
		{
			return TargetA != nullptr;
		}

		if (TargetA->NodePosX != TargetB->NodePosX)
		{
			return TargetA->NodePosX < TargetB->NodePosX;
		}

		if (TargetA->NodePosY != TargetB->NodePosY)
		{
			return TargetA->NodePosY < TargetB->NodePosY;
		}

		const FString TargetTitleA = GetSourceNodeDisplayText(TargetA).ToString();
		const FString TargetTitleB = GetSourceNodeDisplayText(TargetB).ToString();
		if (TargetTitleA != TargetTitleB)
		{
			return TargetTitleA < TargetTitleB;
		}

		const FString TransitionTitleA = GetTransitionDisplayText(&A).ToString();
		const FString TransitionTitleB = GetTransitionDisplayText(&B).ToString();
		if (TransitionTitleA != TransitionTitleB)
		{
			return TransitionTitleA < TransitionTitleB;
		}

		return A.NodeGuid.ToString(EGuidFormats::Digits) < B.NodeGuid.ToString(EGuidFormats::Digits);
	}

	static void GatherOutgoingTransitionsForSource(
		const UEdGraph* Graph,
		const UEdGraphNode* SourceNode,
		TArray<URZCombatComboGraphNode_Transition*>& OutTransitions
	)
	{
		OutTransitions.Reset();
		if (!Graph || !SourceNode)
		{
			return;
		}

		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(GraphNode);
			if (TransitionNode && TransitionNode->GetPreviousNode() == SourceNode && TransitionNode->GetNextComboNode())
			{
				OutTransitions.Add(TransitionNode);
			}
		}
	}

	static bool SortTransitionsByGeneratedPriorityThenTitle(
		const URZCombatComboGraphNode_Transition& A,
		const URZCombatComboGraphNode_Transition& B
	)
	{
		const int32 PriorityA = CalculateGeneratedTransitionPriority(&A);
		const int32 PriorityB = CalculateGeneratedTransitionPriority(&B);
		if (PriorityA != PriorityB)
		{
			return PriorityA > PriorityB;
		}

		return GetTransitionDisplayText(&A).ToString() < GetTransitionDisplayText(&B).ToString();
	}

	static bool SortIncomingGroupsBySourcePosition(
		const FIncomingTransitionGroup& A,
		const FIncomingTransitionGroup& B
	)
	{
		const UEdGraphNode* SourceA = A.SourceNode;
		const UEdGraphNode* SourceB = B.SourceNode;
		if (!SourceA || !SourceB)
		{
			return SourceA != nullptr;
		}

		if (SourceA->NodePosY != SourceB->NodePosY)
		{
			return SourceA->NodePosY < SourceB->NodePosY;
		}

		if (SourceA->NodePosX != SourceB->NodePosX)
		{
			return SourceA->NodePosX < SourceB->NodePosX;
		}

		return GetSourceNodeDisplayText(SourceA).ToString() < GetSourceNodeDisplayText(SourceB).ToString();
	}

	int32 CalculateGeneratedTransitionPriority(const URZCombatComboGraphNode_Transition* TransitionNode)
	{
		const UEdGraphNode* SourceNode = TransitionNode ? TransitionNode->GetPreviousNode() : nullptr;
		const UEdGraph* Graph = TransitionNode ? TransitionNode->GetGraph() : nullptr;
		if (!TransitionNode || !SourceNode || !Graph)
		{
			return TransitionNode ? TransitionNode->Priority : 0;
		}

		TArray<URZCombatComboGraphNode_Transition*> OutgoingTransitions;
		GatherOutgoingTransitionsForSource(Graph, SourceNode, OutgoingTransitions);
		OutgoingTransitions.Sort(
			[](const URZCombatComboGraphNode_Transition& A, const URZCombatComboGraphNode_Transition& B)
			{
				return SortTransitionsByTargetHorizontalOrder(A, B);
			}
		);

		for (int32 Index = 0; Index < OutgoingTransitions.Num(); ++Index)
		{
			if (OutgoingTransitions[Index] == TransitionNode)
			{
				return OutgoingTransitions.Num() - Index;
			}
		}

		return TransitionNode->Priority;
	}

	void RefreshGeneratedTransitionPriorities(UEdGraph* Graph, bool bModifyNodes)
	{
		if (!Graph)
		{
			return;
		}

		TMap<UEdGraphNode*, TArray<URZCombatComboGraphNode_Transition*>> TransitionsBySource;
		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(GraphNode);
			UEdGraphNode* SourceNode = TransitionNode ? TransitionNode->GetPreviousNode() : nullptr;
			if (TransitionNode && SourceNode && TransitionNode->GetNextComboNode())
			{
				TransitionsBySource.FindOrAdd(SourceNode).Add(TransitionNode);
			}
		}

		for (TPair<UEdGraphNode*, TArray<URZCombatComboGraphNode_Transition*>>& Pair : TransitionsBySource)
		{
			TArray<URZCombatComboGraphNode_Transition*>& OutgoingTransitions = Pair.Value;
			OutgoingTransitions.Sort(
				[](const URZCombatComboGraphNode_Transition& A, const URZCombatComboGraphNode_Transition& B)
				{
					return SortTransitionsByTargetHorizontalOrder(A, B);
				}
			);

			for (int32 Index = 0; Index < OutgoingTransitions.Num(); ++Index)
			{
				URZCombatComboGraphNode_Transition* TransitionNode = OutgoingTransitions[Index];
				const int32 GeneratedPriority = OutgoingTransitions.Num() - Index;
				if (TransitionNode && TransitionNode->Priority != GeneratedPriority)
				{
					if (bModifyNodes)
					{
						TransitionNode->Modify();
					}

					TransitionNode->Priority = GeneratedPriority;
				}
			}
		}
	}

	void GatherIncomingTransitionGroups(
		const URZCombatComboGraphNode_Combo* TargetNode,
		TArray<FIncomingTransitionGroup>& OutGroups
	)
	{
		OutGroups.Reset();

		const UEdGraphPin* TargetInputPin = TargetNode ? TargetNode->GetInputPin() : nullptr;
		if (!TargetInputPin)
		{
			return;
		}

		TMap<UEdGraphNode*, int32> GroupIndexBySource;
		for (const UEdGraphPin* LinkedPin : TargetInputPin->LinkedTo)
		{
			URZCombatComboGraphNode_Transition* TransitionNode = LinkedPin
				? Cast<URZCombatComboGraphNode_Transition>(LinkedPin->GetOwningNode())
				: nullptr;
			if (!TransitionNode || TransitionNode->GetNextComboNode() != TargetNode)
			{
				continue;
			}

			UEdGraphNode* SourceNode = TransitionNode->GetPreviousNode();
			if (!SourceNode)
			{
				continue;
			}

			int32* ExistingGroupIndex = GroupIndexBySource.Find(SourceNode);
			if (!ExistingGroupIndex)
			{
				ExistingGroupIndex = &GroupIndexBySource.Add(SourceNode, OutGroups.Num());
				FIncomingTransitionGroup& NewGroup = OutGroups.AddDefaulted_GetRef();
				NewGroup.SourceNode = SourceNode;
			}

			OutGroups[*ExistingGroupIndex].Transitions.Add(TransitionNode);
		}

		for (FIncomingTransitionGroup& Group : OutGroups)
		{
			Group.Transitions.Sort(
				[](const URZCombatComboGraphNode_Transition& A, const URZCombatComboGraphNode_Transition& B)
				{
					return SortTransitionsByGeneratedPriorityThenTitle(A, B);
				}
			);
		}

		OutGroups.Sort(SortIncomingGroupsBySourcePosition);
	}

	FText GetSourceNodeDisplayText(const UEdGraphNode* SourceNode)
	{
		if (!SourceNode)
		{
			return LOCTEXT("UnknownSourceNodeTitle", "Unknown");
		}

		return SourceNode->GetNodeTitle(ENodeTitleType::ListView);
	}

	FText GetTransitionDisplayText(const URZCombatComboGraphNode_Transition* TransitionNode)
	{
		if (!TransitionNode)
		{
			return LOCTEXT("UnknownTransitionTitle", "Unknown");
		}

		if (!TransitionNode->DisplayName.IsEmpty())
		{
			return TransitionNode->DisplayName;
		}

		if (TransitionNode->TriggerTag.IsValid())
		{
			return FText::FromName(TransitionNode->TriggerTag.GetTagName());
		}

		return LOCTEXT("TransitionNodeTitle", "Transition");
	}
}

const FName URZCombatComboGraphNode_Entry::OutputPinName(TEXT("Out"));

void URZCombatComboGraphNode_Entry::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, RZCombatComboGraphNodes::PinCategory, OutputPinName);
}

FText URZCombatComboGraphNode_Entry::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("EntryNodeTitle", "Entry");
}

FText URZCombatComboGraphNode_Entry::GetTooltipText() const
{
	return LOCTEXT("EntryNodeTooltip", "Combo start point. Outgoing transition nodes compile to EntryTransitions.");
}

bool URZCombatComboGraphNode_Entry::CanUserDeleteNode() const
{
	return false;
}

bool URZCombatComboGraphNode_Entry::CanDuplicateNode() const
{
	return false;
}

UEdGraphPin* URZCombatComboGraphNode_Entry::GetOutputPin() const
{
	return FindPin(OutputPinName, EGPD_Output);
}

const FName URZCombatComboGraphNode_Combo::InputPinName(TEXT("In"));
const FName URZCombatComboGraphNode_Combo::OutputPinName(TEXT("Out"));

void URZCombatComboGraphNode_Combo::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, RZCombatComboGraphNodes::PinCategory, InputPinName);
	CreatePin(EGPD_Output, RZCombatComboGraphNodes::PinCategory, OutputPinName);
}

void URZCombatComboGraphNode_Combo::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	EnsureNodeId();
}

void URZCombatComboGraphNode_Combo::PostPasteNode()
{
	Super::PostPasteNode();

	Modify();
	CreateNewGuid();
	NodeId.Guid = NodeGuid;
	NodeId.Name = NAME_None;
}

FText URZCombatComboGraphNode_Combo::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	if (AbilityClass)
	{
		return FText::FromString(AbilityClass->GetAuthoredName());
	}

	return LOCTEXT("InvalidComboNodeTitle", "Invalid");
}

FText URZCombatComboGraphNode_Combo::GetTooltipText() const
{
	return LOCTEXT("ComboNodeTooltip", "Combo segment. Compiles to one FRZCombatComboNode.");
}

bool URZCombatComboGraphNode_Combo::CanUserDeleteNode() const
{
	return true;
}

bool URZCombatComboGraphNode_Combo::CanDuplicateNode() const
{
	return true;
}

FSlateIcon URZCombatComboGraphNode_Combo::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor(0.95f, 0.62f, 0.20f);
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GraphEditor.Function_16x"));
}

bool URZCombatComboGraphNode_Combo::ShowPaletteIconOnNode() const
{
	return true;
}

void URZCombatComboGraphNode_Combo::EnsureNodeId()
{
	if (!NodeGuid.IsValid())
	{
		CreateNewGuid();
	}

	if (!NodeId.Guid.IsValid() || NodeId.Guid != NodeGuid)
	{
		Modify();
		NodeId.Guid = NodeGuid;
	}
}

UEdGraphPin* URZCombatComboGraphNode_Combo::GetInputPin() const
{
	return FindPin(InputPinName, EGPD_Input);
}

UEdGraphPin* URZCombatComboGraphNode_Combo::GetOutputPin() const
{
	return FindPin(OutputPinName, EGPD_Output);
}

const FName URZCombatComboGraphNode_Transition::InputPinName(TEXT("In"));
const FName URZCombatComboGraphNode_Transition::OutputPinName(TEXT("Out"));

void URZCombatComboGraphNode_Transition::AllocateDefaultPins()
{
	UEdGraphPin* InputPin = CreatePin(EGPD_Input, RZCombatComboGraphNodes::PinCategory, InputPinName);
	if (InputPin)
	{
		InputPin->bHidden = true;
	}

	UEdGraphPin* OutputPin = CreatePin(EGPD_Output, RZCombatComboGraphNodes::PinCategory, OutputPinName);
	if (OutputPin)
	{
		OutputPin->bHidden = true;
	}
}

FText URZCombatComboGraphNode_Transition::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return RZCombatComboGraphNodes::GetTransitionDisplayText(this);
}

FText URZCombatComboGraphNode_Transition::GetTooltipText() const
{
	return LOCTEXT("TransitionNodeTooltip", "Combo transition data bound to an Entry/Combo -> Combo connection.");
}

bool URZCombatComboGraphNode_Transition::CanUserDeleteNode() const
{
	return true;
}

bool URZCombatComboGraphNode_Transition::CanDuplicateNode() const
{
	return false;
}

UEdGraphPin* URZCombatComboGraphNode_Transition::GetInputPin() const
{
	return FindPin(InputPinName, EGPD_Input);
}

UEdGraphPin* URZCombatComboGraphNode_Transition::GetOutputPin() const
{
	return FindPin(OutputPinName, EGPD_Output);
}

UEdGraphNode* URZCombatComboGraphNode_Transition::GetPreviousNode() const
{
	const UEdGraphPin* InputPin = GetInputPin();
	if (InputPin && InputPin->LinkedTo.Num() > 0 && InputPin->LinkedTo[0])
	{
		return InputPin->LinkedTo[0]->GetOwningNode();
	}

	return nullptr;
}

URZCombatComboGraphNode_Combo* URZCombatComboGraphNode_Transition::GetNextComboNode() const
{
	const UEdGraphPin* OutputPin = GetOutputPin();
	if (OutputPin && OutputPin->LinkedTo.Num() > 0 && OutputPin->LinkedTo[0])
	{
		return Cast<URZCombatComboGraphNode_Combo>(OutputPin->LinkedTo[0]->GetOwningNode());
	}

	return nullptr;
}

void URZCombatComboGraphNode_Transition::CreateConnections(UEdGraphNode* PreviousNode, URZCombatComboGraphNode_Combo* NextNode)
{
	UEdGraphPin* SourceOutputPin = RZCombatComboGraphNodes::GetSourceOutputPin(PreviousNode);
	UEdGraphPin* TargetInputPin = NextNode ? NextNode->GetInputPin() : nullptr;
	UEdGraphPin* InputPin = GetInputPin();
	UEdGraphPin* OutputPin = GetOutputPin();

	if (!SourceOutputPin || !TargetInputPin || !InputPin || !OutputPin)
	{
		return;
	}

	Modify();
	InputPin->Modify();
	OutputPin->Modify();
	SourceOutputPin->Modify();
	TargetInputPin->Modify();

	InputPin->LinkedTo.Empty();
	OutputPin->LinkedTo.Empty();
	SourceOutputPin->MakeLinkTo(InputPin);
	OutputPin->MakeLinkTo(TargetInputPin);
}

#undef LOCTEXT_NAMESPACE
