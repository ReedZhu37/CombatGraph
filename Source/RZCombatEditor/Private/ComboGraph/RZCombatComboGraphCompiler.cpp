#include "ComboGraph/RZCombatComboGraphCompiler.h"

#include "ComboGraph/RZCombatComboGraph.h"
#include "ComboGraph/RZCombatComboGraphNodes.h"
#include "ComboGraph/RZCombatComboGraphSchema.h"
#include "RZCombatComboPayload.h"
#include "RZCombatComboTableAsset.h"
#include "RZCombatComboTransitionChecker.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RZCombatComboGraphCompiler"

namespace RZCombatComboGraphCompiler
{
	struct FResolvedTransition
	{
		URZCombatComboGraphNode_Entry* EntrySource = nullptr;
		URZCombatComboGraphNode_Combo* ComboSource = nullptr;
		URZCombatComboGraphNode_Combo* Target = nullptr;

		bool HasSource() const
		{
			return EntrySource || ComboSource;
		}
	};

	struct FPriorityTieGroup
	{
		int32 HighestPriority = MIN_int32;
		TArray<const URZCombatComboGraphNode_Transition*> HighestPriorityTransitions;
	};

	static FString DescribeNode(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("<null>");
		}

		return FString::Printf(TEXT("%s (%s)"), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	}

	static void AddError(FRZCombatComboGraphCompileResult& Result, const FText& Error)
	{
		Result.Errors.Add(Error);
	}

	static FObjectPropertyBase* FindEditorGraphProperty(URZCombatComboTableAsset* ComboTable)
	{
		if (!ComboTable)
		{
			return nullptr;
		}

		FProperty* Property = ComboTable->GetClass()->FindPropertyByName(TEXT("EditorGraph"));
		return CastField<FObjectPropertyBase>(Property);
	}

	static UEdGraph* GetEditorGraph(URZCombatComboTableAsset* ComboTable)
	{
		FObjectPropertyBase* EditorGraphProperty = FindEditorGraphProperty(ComboTable);
		if (!EditorGraphProperty)
		{
			return nullptr;
		}

		void* ValuePtr = EditorGraphProperty->ContainerPtrToValuePtr<void>(ComboTable);
		return Cast<UEdGraph>(EditorGraphProperty->GetObjectPropertyValue(ValuePtr));
	}

	static void SetEditorGraph(URZCombatComboTableAsset* ComboTable, UEdGraph* Graph)
	{
		FObjectPropertyBase* EditorGraphProperty = FindEditorGraphProperty(ComboTable);
		if (!EditorGraphProperty)
		{
			return;
		}

		void* ValuePtr = EditorGraphProperty->ContainerPtrToValuePtr<void>(ComboTable);
		EditorGraphProperty->SetObjectPropertyValue(ValuePtr, Graph);
	}

	static FString MakeNameSafe(const FString& Source)
	{
		FString Trimmed = Source;
		Trimmed.TrimStartAndEndInline();

		FString Result;
		Result.Reserve(Trimmed.Len());

		for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR Character = Trimmed[Index];
			if (FChar::IsAlnum(Character))
			{
				Result.AppendChar(Character);
			}
			else if (Character == TEXT('_') || Character == TEXT('-') || FChar::IsWhitespace(Character))
			{
				Result.AppendChar(TEXT('_'));
			}
		}

		return Result.IsEmpty() ? TEXT("Combo") : Result;
	}

	static FName MakeNodeNameFromDisplayName(const URZCombatComboGraphNode_Combo* ComboNode, int32 Index)
	{
		const FString DisplayNameString = ComboNode ? ComboNode->DisplayName.ToString() : FString();
		FString BaseName = MakeNameSafe(DisplayNameString);

		if (BaseName == TEXT("Combo") && ComboNode && ComboNode->NodeId.Guid.IsValid())
		{
			BaseName = FString::Printf(TEXT("Combo_%s"), *ComboNode->NodeId.Guid.ToString(EGuidFormats::Digits).Left(8));
		}
		else if (BaseName == TEXT("Combo"))
		{
			BaseName = FString::Printf(TEXT("Combo_%d"), Index + 1);
		}

		return FName(*BaseName);
	}

	static void EnsureComboNodeIds(const TArray<URZCombatComboGraphNode_Combo*>& ComboNodes)
	{
		for (int32 Index = 0; Index < ComboNodes.Num(); ++Index)
		{
			URZCombatComboGraphNode_Combo* ComboNode = ComboNodes[Index];
			if (!ComboNode)
			{
				continue;
			}

			ComboNode->EnsureNodeId();

			const FName RuntimeNodeName = MakeNodeNameFromDisplayName(ComboNode, Index);
			if (ComboNode->NodeId.Name != RuntimeNodeName)
			{
				ComboNode->Modify();
				ComboNode->NodeId.Name = RuntimeNodeName;
			}
		}
	}

	static FResolvedTransition ResolveTransitionEndpoints(
		const URZCombatComboGraphNode_Transition* TransitionNode,
		FRZCombatComboGraphCompileResult& Result
	)
	{
		FResolvedTransition Resolved;
		if (!TransitionNode)
		{
			return Resolved;
		}

		const UEdGraphPin* InputPin = TransitionNode->GetInputPin();
		const UEdGraphPin* OutputPin = TransitionNode->GetOutputPin();

		if (!InputPin || InputPin->LinkedTo.Num() != 1)
		{
			AddError(Result, FText::Format(
				LOCTEXT("TransitionSourceCountError", "Transition '{0}' must have exactly one source link."),
				FText::FromString(DescribeNode(TransitionNode))
			));
		}
		else
		{
			UEdGraphNode* SourceNode = InputPin->LinkedTo[0] ? InputPin->LinkedTo[0]->GetOwningNode() : nullptr;
			Resolved.EntrySource = Cast<URZCombatComboGraphNode_Entry>(SourceNode);
			Resolved.ComboSource = Cast<URZCombatComboGraphNode_Combo>(SourceNode);

			if (!Resolved.HasSource())
			{
				AddError(Result, FText::Format(
					LOCTEXT("TransitionInvalidSourceError", "Transition '{0}' source must be Entry or Combo."),
					FText::FromString(DescribeNode(TransitionNode))
				));
			}
		}

		if (!OutputPin || OutputPin->LinkedTo.Num() != 1)
		{
			AddError(Result, FText::Format(
				LOCTEXT("TransitionTargetCountError", "Transition '{0}' must have exactly one target combo link."),
				FText::FromString(DescribeNode(TransitionNode))
			));
		}
		else
		{
			UEdGraphNode* TargetNode = OutputPin->LinkedTo[0] ? OutputPin->LinkedTo[0]->GetOwningNode() : nullptr;
			Resolved.Target = Cast<URZCombatComboGraphNode_Combo>(TargetNode);

			if (!Resolved.Target)
			{
				AddError(Result, FText::Format(
					LOCTEXT("TransitionInvalidTargetError", "Transition '{0}' target must be a Combo node."),
					FText::FromString(DescribeNode(TransitionNode))
				));
			}
		}

		return Resolved;
	}

	static FString MakeSourceKey(const FResolvedTransition& Transition)
	{
		if (Transition.EntrySource)
		{
			return TEXT("Entry");
		}

		if (Transition.ComboSource && Transition.ComboSource->NodeId.Guid.IsValid())
		{
			return FString::Printf(TEXT("Combo:%s"), *Transition.ComboSource->NodeId.Guid.ToString(EGuidFormats::Digits));
		}

		if (Transition.ComboSource && !Transition.ComboSource->NodeId.Name.IsNone())
		{
			return FString::Printf(TEXT("ComboName:%s"), *Transition.ComboSource->NodeId.Name.ToString());
		}

		return FString();
	}

	static void TrackPriorityTie(
		TMap<FString, FPriorityTieGroup>& PriorityGroups,
		const FString& SourceKey,
		const URZCombatComboGraphNode_Transition* TransitionNode
	)
	{
		if (SourceKey.IsEmpty() || !TransitionNode || !TransitionNode->TriggerTag.IsValid())
		{
			return;
		}

		const FString GroupKey = FString::Printf(TEXT("%s|%s"), *SourceKey, *TransitionNode->TriggerTag.GetTagName().ToString());
		FPriorityTieGroup& Group = PriorityGroups.FindOrAdd(GroupKey);

		const int32 GeneratedPriority = RZCombatComboGraphNodes::CalculateGeneratedTransitionPriority(TransitionNode);
		if (GeneratedPriority > Group.HighestPriority)
		{
			Group.HighestPriority = GeneratedPriority;
			Group.HighestPriorityTransitions.Reset();
			Group.HighestPriorityTransitions.Add(TransitionNode);
			return;
		}

		if (GeneratedPriority == Group.HighestPriority)
		{
			Group.HighestPriorityTransitions.Add(TransitionNode);
		}
	}

	static FRZCombatComboTransition MakeRuntimeTransition(
		URZCombatComboTableAsset* ComboTable,
		const URZCombatComboGraphNode_Transition* TransitionNode,
		const URZCombatComboGraphNode_Combo* TargetNode
	)
	{
		FRZCombatComboTransition RuntimeTransition;
		RuntimeTransition.bEnabled = TransitionNode->bEnabled;
		RuntimeTransition.DisplayName = TransitionNode->DisplayName;
		RuntimeTransition.TriggerTag = TransitionNode->TriggerTag;
		RuntimeTransition.TargetNodeId = TargetNode->NodeId;
		RuntimeTransition.Priority = RZCombatComboGraphNodes::CalculateGeneratedTransitionPriority(TransitionNode);
		RuntimeTransition.bConsumeTrigger = TransitionNode->bConsumeTrigger;
		RuntimeTransition.ConditionKey = TransitionNode->ConditionKey;
		if (TransitionNode->Payload)
		{
			const FName PayloadName = MakeUniqueObjectName(ComboTable, TransitionNode->Payload->GetClass(), TEXT("ComboPayload"));
			RuntimeTransition.Payload = DuplicateObject<URZCombatComboPayload>(TransitionNode->Payload, ComboTable, PayloadName);
		}
		if (TransitionNode->Checker)
		{
			const FName CheckerName = MakeUniqueObjectName(ComboTable, TransitionNode->Checker->GetClass(), TEXT("ComboChecker"));
			RuntimeTransition.Checker = DuplicateObject<URZCombatComboTransitionChecker>(TransitionNode->Checker, ComboTable, CheckerName);
		}
		RuntimeTransition.DesignerNote = TransitionNode->DesignerNote;
		return RuntimeTransition;
	}

	static URZCombatComboGraphNode_Combo* CreateComboNodeFromRuntime(
		URZCombatComboGraph* ComboGraph,
		const FRZCombatComboNode& RuntimeNode,
		int32 Index
	)
	{
		if (!ComboGraph)
		{
			return nullptr;
		}

		URZCombatComboGraphNode_Combo* ComboNode = NewObject<URZCombatComboGraphNode_Combo>(ComboGraph, NAME_None, RF_Transactional);
		if (RuntimeNode.NodeId.Guid.IsValid())
		{
			ComboNode->NodeGuid = RuntimeNode.NodeId.Guid;
		}
		else
		{
			ComboNode->CreateNewGuid();
		}

		ComboNode->NodePosX = 400 + Index * 320;
		ComboNode->NodePosY = 0;
		ComboNode->bEnabled = RuntimeNode.bEnabled;
		ComboNode->DisplayName = RuntimeNode.DisplayName;
		ComboNode->NodeId = RuntimeNode.NodeId;
		ComboNode->AbilityClass = RuntimeNode.AbilityClass;
		ComboNode->DesignerNote = RuntimeNode.DesignerNote;
		ComboNode->AllocateDefaultPins();
		ComboNode->EnsureNodeId();
		ComboGraph->AddNode(ComboNode, false, false);
		return ComboNode;
	}

	static URZCombatComboGraphNode_Combo* FindComboNodeById(
		const TArray<URZCombatComboGraphNode_Combo*>& ComboNodes,
		const FRZCombatComboNodeId& NodeId
	)
	{
		for (URZCombatComboGraphNode_Combo* ComboNode : ComboNodes)
		{
			if (ComboNode && ComboNode->NodeId.Matches(NodeId))
			{
				return ComboNode;
			}
		}

		return nullptr;
	}

	static void LinkPins(UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
	{
		if (OutputPin && InputPin)
		{
			OutputPin->MakeLinkTo(InputPin);
		}
	}

	static void CreateTransitionNodeFromRuntime(
		URZCombatComboGraph* ComboGraph,
		const FRZCombatComboTransition& RuntimeTransition,
		UEdGraphPin* SourceOutputPin,
		const TArray<URZCombatComboGraphNode_Combo*>& ComboNodes,
		int32 Index,
		int32 SourceY
	)
	{
		if (!ComboGraph)
		{
			return;
		}

		URZCombatComboGraphNode_Combo* TargetNode = FindComboNodeById(ComboNodes, RuntimeTransition.TargetNodeId);
		if (!SourceOutputPin || !TargetNode)
		{
			return;
		}

		URZCombatComboGraphNode_Transition* TransitionNode = NewObject<URZCombatComboGraphNode_Transition>(ComboGraph, NAME_None, RF_Transactional);
		TransitionNode->CreateNewGuid();
		TransitionNode->NodePosX = 200 + Index * 160;
		TransitionNode->NodePosY = SourceY + 140 + Index * 80;
		TransitionNode->bEnabled = RuntimeTransition.bEnabled;
		TransitionNode->DisplayName = RuntimeTransition.DisplayName;
		TransitionNode->TriggerTag = RuntimeTransition.TriggerTag;
		TransitionNode->Priority = RuntimeTransition.Priority;
		TransitionNode->bConsumeTrigger = RuntimeTransition.bConsumeTrigger;
		TransitionNode->ConditionKey = RuntimeTransition.ConditionKey;
		if (RuntimeTransition.Payload)
		{
			const FName PayloadName = MakeUniqueObjectName(TransitionNode, RuntimeTransition.Payload->GetClass(), TEXT("ComboPayload"));
			TransitionNode->Payload = DuplicateObject<URZCombatComboPayload>(RuntimeTransition.Payload, TransitionNode, PayloadName);
		}
		if (RuntimeTransition.Checker)
		{
			const FName CheckerName = MakeUniqueObjectName(TransitionNode, RuntimeTransition.Checker->GetClass(), TEXT("ComboChecker"));
			TransitionNode->Checker = DuplicateObject<URZCombatComboTransitionChecker>(RuntimeTransition.Checker, TransitionNode, CheckerName);
		}
		TransitionNode->DesignerNote = RuntimeTransition.DesignerNote;
		TransitionNode->AllocateDefaultPins();
		ComboGraph->AddNode(TransitionNode, false, false);

		LinkPins(SourceOutputPin, TransitionNode->GetInputPin());
		LinkPins(TransitionNode->GetOutputPin(), TargetNode->GetInputPin());
	}

	static void PopulateGraphFromRuntimeTable(
		URZCombatComboGraph* ComboGraph,
		URZCombatComboTableAsset* ComboTable,
		URZCombatComboGraphNode_Entry* EntryNode
	)
	{
		if (!ComboGraph || !ComboTable || !EntryNode)
		{
			return;
		}

		TArray<URZCombatComboGraphNode_Combo*> ComboNodes;
		ComboNodes.Reserve(ComboTable->Nodes.Num());
		for (int32 Index = 0; Index < ComboTable->Nodes.Num(); ++Index)
		{
			if (URZCombatComboGraphNode_Combo* ComboNode = CreateComboNodeFromRuntime(ComboGraph, ComboTable->Nodes[Index], Index))
			{
				ComboNodes.Add(ComboNode);
			}
		}

		for (int32 Index = 0; Index < ComboTable->EntryTransitions.Num(); ++Index)
		{
			CreateTransitionNodeFromRuntime(
				ComboGraph,
				ComboTable->EntryTransitions[Index],
				EntryNode->GetOutputPin(),
				ComboNodes,
				Index,
				EntryNode->NodePosY
			);
		}

		for (int32 SourceIndex = 0; SourceIndex < ComboTable->Nodes.Num(); ++SourceIndex)
		{
			URZCombatComboGraphNode_Combo* SourceNode = SourceIndex < ComboNodes.Num() ? ComboNodes[SourceIndex] : nullptr;
			if (!SourceNode)
			{
				continue;
			}

			const FRZCombatComboNode& RuntimeNode = ComboTable->Nodes[SourceIndex];
			for (int32 TransitionIndex = 0; TransitionIndex < RuntimeNode.Transitions.Num(); ++TransitionIndex)
			{
				CreateTransitionNodeFromRuntime(
					ComboGraph,
					RuntimeNode.Transitions[TransitionIndex],
					SourceNode->GetOutputPin(),
					ComboNodes,
					TransitionIndex,
					SourceNode->NodePosY
				);
			}
		}
	}
}

URZCombatComboGraph* FRZCombatComboGraphCompiler::EnsureGraph(URZCombatComboTableAsset* ComboTable)
{
	if (!ComboTable)
	{
		return nullptr;
	}

	using namespace RZCombatComboGraphCompiler;

	if (!FindEditorGraphProperty(ComboTable))
	{
		return nullptr;
	}

	bool bCreatedGraph = false;
	URZCombatComboGraph* ComboGraph = Cast<URZCombatComboGraph>(GetEditorGraph(ComboTable));
	if (!ComboGraph)
	{
		ComboTable->Modify();
		const FName GraphName = MakeUniqueObjectName(ComboTable, URZCombatComboGraph::StaticClass(), TEXT("ComboGraph"));
		ComboGraph = NewObject<URZCombatComboGraph>(ComboTable, GraphName, RF_Transactional);
		ComboGraph->Schema = URZCombatComboGraphSchema::StaticClass();
		SetEditorGraph(ComboTable, ComboGraph);
		bCreatedGraph = true;
	}

	ComboGraph->InitializeGraph(ComboTable);
	if (bCreatedGraph)
	{
		URZCombatComboGraphNode_Entry* EntryNode = ComboGraph->EnsureEntryNode();
		if (ComboTable->EntryTransitions.Num() > 0 || ComboTable->Nodes.Num() > 0)
		{
			PopulateGraphFromRuntimeTable(ComboGraph, ComboTable, EntryNode);
		}
	}

	return ComboGraph;
}

FRZCombatComboGraphCompileResult FRZCombatComboGraphCompiler::Compile(URZCombatComboTableAsset* ComboTable)
{
	using namespace RZCombatComboGraphCompiler;

	FRZCombatComboGraphCompileResult Result;
	if (!ComboTable)
	{
		AddError(Result, LOCTEXT("MissingComboTable", "Cannot compile a null combo table."));
		return Result;
	}

	URZCombatComboGraph* ComboGraph = EnsureGraph(ComboTable);
	if (!ComboGraph)
	{
		AddError(Result, LOCTEXT("MissingEditorGraphStorage", "Combo table does not expose an editor-only EditorGraph property."));
		return Result;
	}

	TArray<URZCombatComboGraphNode_Entry*> EntryNodes;
	TArray<URZCombatComboGraphNode_Combo*> ComboNodes;
	TArray<URZCombatComboGraphNode_Transition*> TransitionNodes;

	for (UEdGraphNode* GraphNode : ComboGraph->Nodes)
	{
		if (URZCombatComboGraphNode_Entry* EntryNode = Cast<URZCombatComboGraphNode_Entry>(GraphNode))
		{
			EntryNodes.Add(EntryNode);
			continue;
		}

		if (URZCombatComboGraphNode_Combo* ComboNode = Cast<URZCombatComboGraphNode_Combo>(GraphNode))
		{
			ComboNodes.Add(ComboNode);
			continue;
		}

		if (URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(GraphNode))
		{
			TransitionNodes.Add(TransitionNode);
		}
	}

	if (EntryNodes.Num() != 1)
	{
		AddError(Result, FText::Format(
			LOCTEXT("EntryNodeCountError", "Combo graph must contain exactly one Entry node, but found {0}."),
			FText::AsNumber(EntryNodes.Num())
		));
	}

	EnsureComboNodeIds(ComboNodes);
	RZCombatComboGraphNodes::RefreshGeneratedTransitionPriorities(ComboGraph);

	TMap<FGuid, const URZCombatComboGraphNode_Combo*> GuidOwners;
	TMap<FName, const URZCombatComboGraphNode_Combo*> NameOwners;
	TMap<const URZCombatComboGraphNode_Combo*, int32> RuntimeNodeIndices;
	TArray<FRZCombatComboNode> RuntimeNodes;
	RuntimeNodes.Reserve(ComboNodes.Num());

	for (URZCombatComboGraphNode_Combo* ComboNode : ComboNodes)
	{
		if (!ComboNode || !ComboNode->bEnabled)
		{
			continue;
		}

		if (!ComboNode->NodeId.Guid.IsValid())
		{
			AddError(Result, FText::Format(
				LOCTEXT("InvalidComboGuidError", "Combo node '{0}' has no valid GUID."),
				FText::FromString(DescribeNode(ComboNode))
			));
		}
		else if (const URZCombatComboGraphNode_Combo* const* ExistingOwner = GuidOwners.Find(ComboNode->NodeId.Guid))
		{
			AddError(Result, FText::Format(
				LOCTEXT("DuplicateComboGuidError", "Combo nodes '{0}' and '{1}' share the same GUID."),
				FText::FromString(DescribeNode(*ExistingOwner)),
				FText::FromString(DescribeNode(ComboNode))
			));
		}
		else
		{
			GuidOwners.Add(ComboNode->NodeId.Guid, ComboNode);
		}

		if (ComboNode->NodeId.Name.IsNone())
		{
			AddError(Result, FText::Format(
				LOCTEXT("InvalidComboNameError", "Combo node '{0}' has no valid node name."),
				FText::FromString(DescribeNode(ComboNode))
			));
		}
		else if (const URZCombatComboGraphNode_Combo* const* ExistingOwner = NameOwners.Find(ComboNode->NodeId.Name))
		{
			AddError(Result, FText::Format(
				LOCTEXT("DuplicateComboNameError", "Combo nodes '{0}' and '{1}' share node name '{2}'."),
				FText::FromString(DescribeNode(*ExistingOwner)),
				FText::FromString(DescribeNode(ComboNode)),
				FText::FromName(ComboNode->NodeId.Name)
			));
		}
		else
		{
			NameOwners.Add(ComboNode->NodeId.Name, ComboNode);
		}

		if (!ComboNode->AbilityClass)
		{
			AddError(Result, FText::Format(
				LOCTEXT("MissingComboAbilityError", "Combo node '{0}' must have an AbilityClass."),
				FText::FromString(DescribeNode(ComboNode))
			));
		}

		FRZCombatComboNode RuntimeNode;
		RuntimeNode.bEnabled = ComboNode->bEnabled;
		RuntimeNode.DisplayName = ComboNode->DisplayName;
		RuntimeNode.NodeId = ComboNode->NodeId;
		RuntimeNode.AbilityClass = ComboNode->AbilityClass;
		RuntimeNode.DesignerNote = ComboNode->DesignerNote;

		RuntimeNodeIndices.Add(ComboNode, RuntimeNodes.Num());
		RuntimeNodes.Add(MoveTemp(RuntimeNode));
	}

	TArray<FRZCombatComboTransition> RuntimeEntryTransitions;
	TMap<FString, FPriorityTieGroup> PriorityGroups;

	for (URZCombatComboGraphNode_Transition* TransitionNode : TransitionNodes)
	{
		if (!TransitionNode || !TransitionNode->bEnabled)
		{
			continue;
		}

		if (!TransitionNode->TriggerTag.IsValid())
		{
			AddError(Result, FText::Format(
				LOCTEXT("InvalidTriggerTagError", "Transition '{0}' must have a valid TriggerTag."),
				FText::FromString(DescribeNode(TransitionNode))
			));
		}

		const FResolvedTransition Resolved = ResolveTransitionEndpoints(TransitionNode, Result);
		const FString SourceKey = MakeSourceKey(Resolved);
		TrackPriorityTie(PriorityGroups, SourceKey, TransitionNode);

		if (!Resolved.HasSource() || !Resolved.Target)
		{
			continue;
		}

		if (Resolved.ComboSource && !RuntimeNodeIndices.Contains(Resolved.ComboSource))
		{
			AddError(Result, FText::Format(
				LOCTEXT("DisabledSourceComboError", "Transition '{0}' source combo is disabled or not compiled."),
				FText::FromString(DescribeNode(TransitionNode))
			));
			continue;
		}

		const int32* TargetIndex = RuntimeNodeIndices.Find(Resolved.Target);
		if (!TargetIndex)
		{
			AddError(Result, FText::Format(
				LOCTEXT("DisabledTargetComboError", "Transition '{0}' target combo is disabled or not compiled."),
				FText::FromString(DescribeNode(TransitionNode))
			));
			continue;
		}

		const FRZCombatComboTransition RuntimeTransition = MakeRuntimeTransition(ComboTable, TransitionNode, Resolved.Target);
		if (Resolved.EntrySource)
		{
			RuntimeEntryTransitions.Add(RuntimeTransition);
		}
		else if (const int32* SourceIndex = RuntimeNodeIndices.Find(Resolved.ComboSource))
		{
			RuntimeNodes[*SourceIndex].Transitions.Add(RuntimeTransition);
		}
	}

	for (const TPair<FString, FPriorityTieGroup>& Pair : PriorityGroups)
	{
		if (Pair.Value.HighestPriorityTransitions.Num() > 1)
		{
			AddError(Result, FText::Format(
				LOCTEXT("PriorityTieError", "Source/trigger group '{0}' has {1} highest-priority transitions tied at priority {2}."),
				FText::FromString(Pair.Key),
				FText::AsNumber(Pair.Value.HighestPriorityTransitions.Num()),
				FText::AsNumber(Pair.Value.HighestPriority)
			));
		}
	}

	Result.bSucceeded = !Result.HasErrors();
	if (!Result.bSucceeded)
	{
		return Result;
	}

	ComboTable->Modify();
	ComboTable->EntryTransitions = MoveTemp(RuntimeEntryTransitions);
	ComboTable->Nodes = MoveTemp(RuntimeNodes);
	ComboTable->MarkPackageDirty();

	return Result;
}

#undef LOCTEXT_NAMESPACE
