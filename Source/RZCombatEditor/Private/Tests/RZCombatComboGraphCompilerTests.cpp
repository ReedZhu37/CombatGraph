#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA

#include "ComboGraph/RZCombatComboGraph.h"
#include "ComboGraph/RZCombatComboGraphCompiler.h"
#include "ComboGraph/RZCombatComboGraphNodes.h"
#include "ComboGraph/RZCombatComboGraphRuntimeDebug.h"
#include "ComboGraph/RZCombatComboGraphSchema.h"
#include "ComboGraph/RZCombatComboTableEditor.h"
#include "RZCombatComboPayload.h"
#include "RZCombatComboTableAsset.h"
#include "RZCombatComboTransitionChecker.h"

#include "Abilities/GameplayAbility.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#include <initializer_list>

namespace
{
	FGameplayTag MakeTestTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}

	URZCombatComboTableAsset* MakeTable()
	{
		return NewObject<URZCombatComboTableAsset>();
	}

	bool ResolveLeafProperty(
		const UObject* Object,
		const std::initializer_list<FName>& Path,
		FProperty*& OutProperty,
		void*& OutContainer
	)
	{
		if (!Object || Path.size() == 0)
		{
			return false;
		}

		UStruct* OwnerStruct = Object->GetClass();
		void* Container = const_cast<UObject*>(Object);
		int32 SegmentIndex = 0;

		for (const FName Segment : Path)
		{
			FProperty* Property = OwnerStruct->FindPropertyByName(Segment);
			if (!Property)
			{
				return false;
			}

			++SegmentIndex;
			if (SegmentIndex == Path.size())
			{
				OutProperty = Property;
				OutContainer = Container;
				return true;
			}

			FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (!StructProperty)
			{
				return false;
			}

			Container = StructProperty->ContainerPtrToValuePtr<void>(Container);
			OwnerStruct = StructProperty->Struct;
		}

		return false;
	}

	bool ResolvePropertyByPossiblePaths(
		const UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		FProperty*& OutProperty,
		void*& OutContainer
	)
	{
		for (const std::initializer_list<FName>& Path : PossiblePaths)
		{
			if (ResolveLeafProperty(Object, Path, OutProperty, OutContainer))
			{
				return true;
			}
		}

		return false;
	}

	template <typename PropertyType, typename ApplyFuncType>
	bool SetPropertyByPossiblePaths(
		FAutomationTestBase& Test,
		UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		const TCHAR* Description,
		ApplyFuncType&& ApplyFunc
	)
	{
		FProperty* Property = nullptr;
		void* Container = nullptr;
		if (!ResolvePropertyByPossiblePaths(Object, PossiblePaths, Property, Container))
		{
			Test.AddError(FString::Printf(
				TEXT("%s is missing an expected %s property."),
				Object ? *Object->GetClass()->GetName() : TEXT("<null>"),
				Description
			));
			return false;
		}

		PropertyType* TypedProperty = CastField<PropertyType>(Property);
		if (!TypedProperty)
		{
			Test.AddError(FString::Printf(
				TEXT("%s resolved %s to an unexpected property type."),
				Object ? *Object->GetClass()->GetName() : TEXT("<null>"),
				Description
			));
			return false;
		}

		return ApplyFunc(TypedProperty, Container);
	}

	template <typename StructType>
	bool SetStructPropertyByPossiblePaths(
		FAutomationTestBase& Test,
		UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		UScriptStruct* ExpectedStruct,
		const TCHAR* Description,
		const StructType& Value
	)
	{
		return SetPropertyByPossiblePaths<FStructProperty>(
			Test,
			Object,
			PossiblePaths,
			Description,
			[&Test, Object, ExpectedStruct, Description, &Value](FStructProperty* Property, void* Container)
			{
				if (Property->Struct != ExpectedStruct)
				{
					Test.AddError(FString::Printf(
						TEXT("%s resolved %s to %s instead of the expected struct."),
						Object ? *Object->GetClass()->GetName() : TEXT("<null>"),
						Description,
						Property->Struct ? *Property->Struct->GetName() : TEXT("<null>")
					));
					return false;
				}

				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
				Property->Struct->CopyScriptStruct(ValuePtr, &Value);
				return true;
			}
		);
	}

	template <typename StructType>
	bool GetStructPropertyByPossiblePaths(
		FAutomationTestBase& Test,
		const UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		UScriptStruct* ExpectedStruct,
		const TCHAR* Description,
		StructType& OutValue
	)
	{
		FProperty* Property = nullptr;
		void* Container = nullptr;
		if (!ResolvePropertyByPossiblePaths(Object, PossiblePaths, Property, Container))
		{
			Test.AddError(FString::Printf(
				TEXT("%s is missing an expected %s property."),
				Object ? *Object->GetClass()->GetName() : TEXT("<null>"),
				Description
			));
			return false;
		}

		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty)
		{
			Test.AddError(FString::Printf(
				TEXT("%s resolved %s to an unexpected property type."),
				Object ? *Object->GetClass()->GetName() : TEXT("<null>"),
				Description
			));
			return false;
		}

		if (StructProperty->Struct != ExpectedStruct)
		{
			Test.AddError(FString::Printf(
				TEXT("%s resolved %s to %s instead of the expected struct."),
				Object ? *Object->GetClass()->GetName() : TEXT("<null>"),
				Description,
				StructProperty->Struct ? *StructProperty->Struct->GetName() : TEXT("<null>")
			));
			return false;
		}

		const void* ValuePtr = StructProperty->ContainerPtrToValuePtr<void>(Container);
		StructProperty->Struct->CopyScriptStruct(&OutValue, ValuePtr);
		return true;
	}

	bool SetTextPropertyByPossiblePaths(
		FAutomationTestBase& Test,
		UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		const TCHAR* Description,
		const FText& Value
	)
	{
		return SetPropertyByPossiblePaths<FTextProperty>(
			Test,
			Object,
			PossiblePaths,
			Description,
			[&Value](FTextProperty* Property, void* Container)
			{
				Property->SetPropertyValue_InContainer(Container, Value);
				return true;
			}
		);
	}

	bool SetBoolPropertyByPossiblePaths(
		FAutomationTestBase& Test,
		UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		const TCHAR* Description,
		const bool bValue
	)
	{
		return SetPropertyByPossiblePaths<FBoolProperty>(
			Test,
			Object,
			PossiblePaths,
			Description,
			[bValue](FBoolProperty* Property, void* Container)
			{
				Property->SetPropertyValue_InContainer(Container, bValue);
				return true;
			}
		);
	}

	bool SetNamePropertyByPossiblePaths(
		FAutomationTestBase& Test,
		UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		const TCHAR* Description,
		const FName Value
	)
	{
		return SetPropertyByPossiblePaths<FNameProperty>(
			Test,
			Object,
			PossiblePaths,
			Description,
			[Value](FNameProperty* Property, void* Container)
			{
				Property->SetPropertyValue_InContainer(Container, Value);
				return true;
			}
		);
	}

	bool SetIntPropertyByPossiblePaths(
		FAutomationTestBase& Test,
		UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		const TCHAR* Description,
		const int32 Value
	)
	{
		return SetPropertyByPossiblePaths<FIntProperty>(
			Test,
			Object,
			PossiblePaths,
			Description,
			[Value](FIntProperty* Property, void* Container)
			{
				Property->SetPropertyValue_InContainer(Container, Value);
				return true;
			}
		);
	}

	bool SetClassPropertyByPossiblePaths(
		FAutomationTestBase& Test,
		UObject* Object,
		const std::initializer_list<std::initializer_list<FName>>& PossiblePaths,
		const TCHAR* Description,
		UClass* Value
	)
	{
		return SetPropertyByPossiblePaths<FClassProperty>(
			Test,
			Object,
			PossiblePaths,
			Description,
			[Value](FClassProperty* Property, void* Container)
			{
				Property->SetPropertyValue_InContainer(Container, Value);
				return true;
			}
		);
	}

	URZCombatComboGraph* EnsureGraph(FAutomationTestBase& Test, URZCombatComboTableAsset* Table)
	{
		FRZCombatComboGraphCompiler::EnsureGraph(Table);

		FProperty* Property = nullptr;
		void* Container = nullptr;
		if (!ResolvePropertyByPossiblePaths(Table, { { TEXT("EditorGraph") } }, Property, Container))
		{
			Test.AddError(TEXT("URZCombatComboTableAsset is missing the expected EditorGraph property."));
			return nullptr;
		}

		FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
		if (!ObjectProperty)
		{
			Test.AddError(TEXT("URZCombatComboTableAsset.EditorGraph resolved to an unexpected property type."));
			return nullptr;
		}

		URZCombatComboGraph* Graph = Cast<URZCombatComboGraph>(ObjectProperty->GetObjectPropertyValue_InContainer(Container));
		Test.TestNotNull(TEXT("EnsureGraph should create an editor graph."), Graph);
		return Graph;
	}

	template <typename NodeType>
	NodeType* AddGraphNode(URZCombatComboGraph* Graph)
	{
		FGraphNodeCreator<NodeType> NodeCreator(*Graph);
		NodeType* Node = NodeCreator.CreateNode();
		NodeCreator.Finalize();
		return Node;
	}

	template <typename NodeType>
	NodeType* FindSingleNodeOfType(FAutomationTestBase& Test, URZCombatComboGraph* Graph, const TCHAR* Description)
	{
		NodeType* FoundNode = nullptr;
		int32 MatchCount = 0;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (NodeType* TypedNode = Cast<NodeType>(Node))
			{
				FoundNode = TypedNode;
				++MatchCount;
			}
		}

		Test.TestEqual(FString::Printf(TEXT("%s should exist exactly once."), Description), MatchCount, 1);
		return FoundNode;
	}

	template <typename NodeType>
	int32 CountNodesOfType(const URZCombatComboGraph* Graph)
	{
		int32 MatchCount = 0;
		if (!Graph)
		{
			return MatchCount;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->IsA<NodeType>())
			{
				++MatchCount;
			}
		}

		return MatchCount;
	}

	UEdGraphPin* FindSinglePinByDirection(FAutomationTestBase& Test, UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		UEdGraphPin* FoundPin = nullptr;
		int32 MatchCount = 0;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction)
			{
				FoundPin = Pin;
				++MatchCount;
			}
		}

		const TCHAR* DirectionText = Direction == EGPD_Input ? TEXT("input") : TEXT("output");
		Test.TestEqual(
			FString::Printf(TEXT("%s should expose exactly one %s pin."), *Node->GetClass()->GetName(), DirectionText),
			MatchCount,
			1
		);
		return FoundPin;
	}

	bool ConnectNodes(FAutomationTestBase& Test, URZCombatComboGraph* Graph, UEdGraphNode* SourceNode, UEdGraphNode* TargetNode)
	{
		UEdGraphPin* OutputPin = FindSinglePinByDirection(Test, SourceNode, EGPD_Output);
		UEdGraphPin* InputPin = FindSinglePinByDirection(Test, TargetNode, EGPD_Input);
		if (!OutputPin || !InputPin)
		{
			return false;
		}

		const UEdGraphSchema* Schema = Graph->GetSchema();
		Test.TestNotNull(TEXT("Graph should expose a schema."), Schema);
		if (!Schema)
		{
			return false;
		}

		const bool bConnected = Schema->TryCreateConnection(OutputPin, InputPin);
		Test.TestTrue(TEXT("Valid combo graph connections should be accepted by the schema."), bConnected);
		return bConnected;
	}

	bool ConfigureComboNode(
		FAutomationTestBase& Test,
		URZCombatComboGraphNode_Combo* ComboNode,
		const TCHAR* NodeName,
		const bool bWithAbility = true,
		const FGuid NodeGuid = FGuid()
	)
	{
		FRZCombatComboNodeId NodeId;
		NodeId.Guid = NodeGuid;
		NodeId.Name = FName(NodeName);

		if (NodeGuid.IsValid() && ComboNode)
		{
			ComboNode->NodeGuid = NodeGuid;
		}

		bool bSuccess = true;
		bSuccess &= SetStructPropertyByPossiblePaths(
			Test,
			ComboNode,
			{ { TEXT("NodeId") }, { TEXT("NodeData"), TEXT("NodeId") }, { TEXT("Node"), TEXT("NodeId") } },
			FRZCombatComboNodeId::StaticStruct(),
			TEXT("combo node id"),
			NodeId
		);
		bSuccess &= SetTextPropertyByPossiblePaths(
			Test,
			ComboNode,
			{ { TEXT("DisplayName") }, { TEXT("NodeData"), TEXT("DisplayName") }, { TEXT("Node"), TEXT("DisplayName") } },
			TEXT("combo display name"),
			FText::FromName(NodeId.Name)
		);
		bSuccess &= SetBoolPropertyByPossiblePaths(
			Test,
			ComboNode,
			{ { TEXT("bEnabled") }, { TEXT("NodeData"), TEXT("bEnabled") }, { TEXT("Node"), TEXT("bEnabled") } },
			TEXT("combo enabled flag"),
			true
		);

		if (bWithAbility)
		{
			bSuccess &= SetClassPropertyByPossiblePaths(
				Test,
				ComboNode,
				{ { TEXT("AbilityClass") }, { TEXT("NodeData"), TEXT("AbilityClass") }, { TEXT("Node"), TEXT("AbilityClass") } },
				TEXT("combo ability class"),
				UGameplayAbility::StaticClass()
			);
		}

		return bSuccess;
	}

	bool ConfigureTransitionNode(
		FAutomationTestBase& Test,
		URZCombatComboGraphNode_Transition* TransitionNode,
		const FGameplayTag TriggerTag,
		const int32 Priority = 0,
		const bool bConsumeTrigger = true,
		const FName ConditionKey = NAME_None
	)
	{
		bool bSuccess = true;
		bSuccess &= SetBoolPropertyByPossiblePaths(
			Test,
			TransitionNode,
			{ { TEXT("bEnabled") }, { TEXT("TransitionData"), TEXT("bEnabled") }, { TEXT("Transition"), TEXT("bEnabled") } },
			TEXT("transition enabled flag"),
			true
		);
		bSuccess &= SetTextPropertyByPossiblePaths(
			Test,
			TransitionNode,
			{ { TEXT("DisplayName") }, { TEXT("TransitionData"), TEXT("DisplayName") }, { TEXT("Transition"), TEXT("DisplayName") } },
			TEXT("transition display name"),
			FText::FromName(TriggerTag.GetTagName())
		);
		bSuccess &= SetStructPropertyByPossiblePaths(
			Test,
			TransitionNode,
			{ { TEXT("TriggerTag") }, { TEXT("TransitionData"), TEXT("TriggerTag") }, { TEXT("Transition"), TEXT("TriggerTag") } },
			FGameplayTag::StaticStruct(),
			TEXT("transition trigger tag"),
			TriggerTag
		);
		bSuccess &= SetIntPropertyByPossiblePaths(
			Test,
			TransitionNode,
			{ { TEXT("Priority") }, { TEXT("TransitionData"), TEXT("Priority") }, { TEXT("Transition"), TEXT("Priority") } },
			TEXT("transition priority"),
			Priority
		);
		bSuccess &= SetBoolPropertyByPossiblePaths(
			Test,
			TransitionNode,
			{ { TEXT("bConsumeTrigger") }, { TEXT("TransitionData"), TEXT("bConsumeTrigger") }, { TEXT("Transition"), TEXT("bConsumeTrigger") } },
			TEXT("transition consume-trigger flag"),
			bConsumeTrigger
		);
		bSuccess &= SetNamePropertyByPossiblePaths(
			Test,
			TransitionNode,
			{ { TEXT("ConditionKey") }, { TEXT("TransitionData"), TEXT("ConditionKey") }, { TEXT("Transition"), TEXT("ConditionKey") } },
			TEXT("transition condition key"),
			ConditionKey
		);
		return bSuccess;
	}

	const FRZCombatComboNode* FindCompiledNodeByName(const URZCombatComboTableAsset* Table, const TCHAR* NodeName)
	{
		FRZCombatComboNodeId Query;
		Query.Name = FName(NodeName);
		return Table->FindNode(Query);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboTableEditorCompileStatusIconTest,
	"RZCombat.Combo.Graph.CompileToolbarStatusUsesResultOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboTableEditorCompileStatusIconTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(
		TEXT("Unknown compile status should use the uncompiled overlay."),
		RZCombatComboTableEditorCompileStatusOverlayName(ERZCombatComboTableEditorCompileStatus::Unknown),
		FName(TEXT("Blueprint.CompileStatus.Overlay.Unknown"))
	);

	TestEqual(
		TEXT("Succeeded compile status should use the good overlay."),
		RZCombatComboTableEditorCompileStatusOverlayName(ERZCombatComboTableEditorCompileStatus::Succeeded),
		FName(TEXT("Blueprint.CompileStatus.Overlay.Good"))
	);

	TestEqual(
		TEXT("Failed compile status should use the error overlay."),
		RZCombatComboTableEditorCompileStatusOverlayName(ERZCombatComboTableEditorCompileStatus::Failed),
		FName(TEXT("Blueprint.CompileStatus.Overlay.Error"))
	);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphRuntimeDebugNodeStateTest,
	"RZCombat.Combo.Graph.RuntimeDebugMapsExecutionStateToNodeState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphRuntimeDebugNodeStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();

	FRZCombatComboNodeId CurrentNodeId;
	CurrentNodeId.Guid = FGuid(1, 2, 3, 4);
	CurrentNodeId.Name = TEXT("Light_1");

	FRZCombatComboNodeId PendingNodeId;
	PendingNodeId.Guid = FGuid(5, 6, 7, 8);
	PendingNodeId.Name = TEXT("Light_2");

	FRZCombatComboNodeId OtherNodeId;
	OtherNodeId.Guid = FGuid(9, 10, 11, 12);
	OtherNodeId.Name = TEXT("Heavy_1");

	const FGameplayTag PendingTriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag OtherTriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Heavy"));

	FRZCombatComboGraphRuntimeDebugSnapshot RuntimeDebug;
	RuntimeDebug.bHasDebugTarget = true;
	RuntimeDebug.ActiveTable = Table;
	RuntimeDebug.CurrentNodeId = CurrentNodeId;
	RuntimeDebug.PendingSourceNodeId = CurrentNodeId;
	RuntimeDebug.PendingTargetNodeId = PendingNodeId;
	RuntimeDebug.LastSourceNodeId = CurrentNodeId;
	RuntimeDebug.LastTargetNodeId = PendingNodeId;
	RuntimeDebug.LastTriggerTag = PendingTriggerTag;
	RuntimeDebug.AvailableTargetNodeIds.Add(OtherNodeId);
	RuntimeDebug.ExecutionState = ERZCombatComboExecutionState::ResolvedPendingActivation;

	TestEqual(
		TEXT("The current node should keep a current marker while another node waits for activation."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, CurrentNodeId),
		ERZCombatComboGraphRuntimeNodeState::Current
	);
	TestEqual(
		TEXT("The pending target should be marked as pending activation."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, PendingNodeId),
		ERZCombatComboGraphRuntimeNodeState::PendingActivation
	);
	TestEqual(
		TEXT("Available follow-up nodes should get a lighter candidate state."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, OtherNodeId),
		ERZCombatComboGraphRuntimeNodeState::Available
	);
	TestEqual(
		TEXT("The matching pending transition should be highlighted by source, target, and trigger."),
		RZCombatComboGraphRuntimeDebug::ResolveTransitionState(Table, RuntimeDebug, CurrentNodeId, false, PendingNodeId, PendingTriggerTag),
		ERZCombatComboGraphRuntimeNodeState::PendingActivation
	);
	TestEqual(
		TEXT("A transition to the same target from a different source should not be highlighted."),
		RZCombatComboGraphRuntimeDebug::ResolveTransitionState(Table, RuntimeDebug, OtherNodeId, false, PendingNodeId, PendingTriggerTag),
		ERZCombatComboGraphRuntimeNodeState::None
	);
	TestEqual(
		TEXT("A transition with the wrong trigger should not be highlighted as the runtime path."),
		RZCombatComboGraphRuntimeDebug::ResolveTransitionState(Table, RuntimeDebug, CurrentNodeId, false, PendingNodeId, OtherTriggerTag),
		ERZCombatComboGraphRuntimeNodeState::None
	);
	TestEqual(
		TEXT("Available transitions from the current node should receive the available wire state."),
		RZCombatComboGraphRuntimeDebug::ResolveTransitionState(Table, RuntimeDebug, CurrentNodeId, false, OtherNodeId, OtherTriggerTag),
		ERZCombatComboGraphRuntimeNodeState::Available
	);

	RuntimeDebug.ExecutionState = ERZCombatComboExecutionState::AbilityCommitted;
	TestEqual(TEXT("GAS commit on a pending target must not label the previous current source as GA committed."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, CurrentNodeId),
		ERZCombatComboGraphRuntimeNodeState::Current);
	RuntimeDebug.CurrentNodeId = PendingNodeId;
	TestEqual(
		TEXT("Committed execution should mark the current target as committed."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, PendingNodeId),
		ERZCombatComboGraphRuntimeNodeState::Committed
	);
	RuntimeDebug.ExecutionState = ERZCombatComboExecutionState::ComboCommitted;
	TestEqual(TEXT("Combo state commit is distinct from GAS commit observation."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, PendingNodeId),
		ERZCombatComboGraphRuntimeNodeState::ComboCommitted);

	RuntimeDebug.ExecutionState = ERZCombatComboExecutionState::AbilityFailed;
	RuntimeDebug.CurrentNodeId = CurrentNodeId;
	RuntimeDebug.PendingSourceNodeId = FRZCombatComboNodeId();
	RuntimeDebug.PendingTargetNodeId = FRZCombatComboNodeId();
	TestEqual(
		TEXT("Failed execution should use the last resolved target after pending data is cleared."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, PendingNodeId),
		ERZCombatComboGraphRuntimeNodeState::Failed
	);
	TestEqual(
		TEXT("Failed execution should still highlight only the matching last source-to-target transition."),
		RZCombatComboGraphRuntimeDebug::ResolveTransitionState(Table, RuntimeDebug, CurrentNodeId, false, PendingNodeId, PendingTriggerTag),
		ERZCombatComboGraphRuntimeNodeState::Failed
	);

	RuntimeDebug.ExecutionState = ERZCombatComboExecutionState::AbilityCancelled;
	TestEqual(
		TEXT("Cancelled execution should use the last resolved target after combo reset clears current state."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(Table, RuntimeDebug, PendingNodeId),
		ERZCombatComboGraphRuntimeNodeState::Cancelled
	);

	RuntimeDebug.ExecutionState = ERZCombatComboExecutionState::ResolvedPendingActivation;
	RuntimeDebug.PendingSourceNodeId = FRZCombatComboNodeId();
	RuntimeDebug.LastSourceNodeId = FRZCombatComboNodeId();
	RuntimeDebug.PendingTargetNodeId = PendingNodeId;
	TestEqual(
		TEXT("Entry-source transitions should match an empty runtime source only when the graph source is Entry."),
		RZCombatComboGraphRuntimeDebug::ResolveTransitionState(Table, RuntimeDebug, FRZCombatComboNodeId(), true, PendingNodeId, PendingTriggerTag),
		ERZCombatComboGraphRuntimeNodeState::PendingActivation
	);
	TestEqual(
		TEXT("A non-entry graph source should not match an empty entry-source runtime path."),
		RZCombatComboGraphRuntimeDebug::ResolveTransitionState(Table, RuntimeDebug, FRZCombatComboNodeId(), false, PendingNodeId, PendingTriggerTag),
		ERZCombatComboGraphRuntimeNodeState::None
	);

	TestEqual(
		TEXT("Snapshots from a different table should not highlight this graph."),
		RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(MakeTable(), RuntimeDebug, PendingNodeId),
		ERZCombatComboGraphRuntimeNodeState::None
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphComboNodeTitleFallbackTest,
	"RZCombat.Combo.Graph.ComboNodeTitleUsesDisplayNameAbilityNameOrInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphComboNodeTitleFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboGraphNode_Combo* ComboNode = NewObject<URZCombatComboGraphNode_Combo>();
	ComboNode->CreateNewGuid();
	ComboNode->NodeId.Guid = ComboNode->NodeGuid;
	ComboNode->NodeId.Name = FName(TEXT("Combo_12345678"));

	TestEqual(
		TEXT("A Combo node with no display name or ability should show Invalid instead of a generated id."),
		ComboNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
		FString(TEXT("Invalid"))
	);

	ComboNode->AbilityClass = UGameplayAbility::StaticClass();
	TestEqual(
		TEXT("A Combo node with no display name should show its ability class name."),
		ComboNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
		FString(TEXT("GameplayAbility"))
	);

	ComboNode->DisplayName = FText::FromString(TEXT("Light Attack"));
	TestEqual(
		TEXT("DisplayName should remain the highest priority Combo node title."),
		ComboNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
		FString(TEXT("Light Attack"))
	);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphDirectSourceToComboConnectionCreatesTransitionEdgeTest,
	"RZCombat.Combo.Graph.DirectSourceToComboConnectionCreatesTransitionEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphDirectSourceToComboConnectionCreatesTransitionEdgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* FirstComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* SecondComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	if (!EntryNode
		|| !ConfigureComboNode(*this, FirstComboNode, TEXT("Light_1"))
		|| !ConfigureComboNode(*this, SecondComboNode, TEXT("Light_2")))
	{
		return false;
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	TestNotNull(TEXT("Graph should expose a schema."), Schema);
	if (!Schema)
	{
		return false;
	}

	auto TestDirectConnection = [this, Graph, Schema](UEdGraphNode* SourceNode, URZCombatComboGraphNode_Combo* TargetNode, const TCHAR* Description)
	{
		UEdGraphPin* SourceOutputPin = FindSinglePinByDirection(*this, SourceNode, EGPD_Output);
		UEdGraphPin* TargetInputPin = TargetNode ? TargetNode->GetInputPin() : nullptr;
		TestNotNull(FString::Printf(TEXT("%s target should expose a combo input pin."), Description), TargetInputPin);
		if (!SourceOutputPin || !TargetInputPin)
		{
			return false;
		}

		const int32 TransitionCountBefore = CountNodesOfType<URZCombatComboGraphNode_Transition>(Graph);
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourceOutputPin, TargetInputPin);
		TestEqual(
			FString::Printf(TEXT("%s should use the schema conversion-node path."), Description),
			static_cast<int32>(Response.Response),
			static_cast<int32>(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE)
		);

		const bool bConnected = Schema->TryCreateConnection(SourceOutputPin, TargetInputPin);
		TestTrue(FString::Printf(TEXT("%s should create a connection."), Description), bConnected);
		TestEqual(
			FString::Printf(TEXT("%s should insert one transition node."), Description),
			CountNodesOfType<URZCombatComboGraphNode_Transition>(Graph),
			TransitionCountBefore + 1
		);
		TestFalse(FString::Printf(TEXT("%s should not leave a direct source-to-combo link."), Description), SourceOutputPin->LinkedTo.Contains(TargetInputPin));

		URZCombatComboGraphNode_Transition* InsertedTransition = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(Node);
			if (!TransitionNode)
			{
				continue;
			}

			UEdGraphPin* TransitionInputPin = TransitionNode->GetInputPin();
			UEdGraphPin* TransitionOutputPin = TransitionNode->GetOutputPin();
			if (TransitionInputPin
				&& TransitionOutputPin
				&& TransitionInputPin->LinkedTo.Contains(SourceOutputPin)
				&& TransitionOutputPin->LinkedTo.Contains(TargetInputPin))
			{
				InsertedTransition = TransitionNode;
				break;
			}
		}

		TestNotNull(FString::Printf(TEXT("%s should route through the inserted transition."), Description), InsertedTransition);
		if (InsertedTransition)
		{
			UEdGraphPin* TransitionInputPin = InsertedTransition->GetInputPin();
			UEdGraphPin* TransitionOutputPin = InsertedTransition->GetOutputPin();
			TestTrue(FString::Printf(TEXT("%s transition input pin should be hidden like an edge object."), Description), TransitionInputPin && TransitionInputPin->bHidden);
			TestTrue(FString::Printf(TEXT("%s transition output pin should be hidden like an edge object."), Description), TransitionOutputPin && TransitionOutputPin->bHidden);
			TestEqual(FString::Printf(TEXT("%s transition should resolve its previous node."), Description), InsertedTransition->GetPreviousNode(), SourceNode);
			TestEqual(FString::Printf(TEXT("%s transition should resolve its next combo node."), Description), InsertedTransition->GetNextComboNode(), TargetNode);
			TestFalse(FString::Printf(TEXT("%s transition edge should not be duplicatable as a loose node."), Description), InsertedTransition->CanDuplicateNode());
		}
		return InsertedTransition != nullptr;
	};

	bool bSuccess = true;
	bSuccess &= TestDirectConnection(EntryNode, FirstComboNode, TEXT("Entry -> Combo"));
	bSuccess &= TestDirectConnection(FirstComboNode, SecondComboNode, TEXT("Combo -> Combo"));
	return bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphIncomingFromGroupsSortBySourceAndPriorityTest,
	"RZCombat.Combo.Graph.IncomingFromGroupsSortBySourceAndGeneratedPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphIncomingFromGroupsSortBySourceAndPriorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Combo* SourceA = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* SourceC = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* TargetB = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	if (!ConfigureComboNode(*this, SourceA, TEXT("A"))
		|| !ConfigureComboNode(*this, SourceC, TEXT("C"))
		|| !ConfigureComboNode(*this, TargetB, TEXT("B")))
	{
		return false;
	}

	SourceA->NodePosX = 100;
	SourceA->NodePosY = 100;
	SourceC->NodePosX = 100;
	SourceC->NodePosY = 300;

	URZCombatComboGraphNode_Transition* LowPriorityFromA = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	URZCombatComboGraphNode_Transition* HighPriorityFromA = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	URZCombatComboGraphNode_Transition* FromC = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	if (!LowPriorityFromA || !HighPriorityFromA || !FromC)
	{
		AddError(TEXT("Expected transition nodes to be created."));
		return false;
	}

	LowPriorityFromA->CreateConnections(SourceA, TargetB);
	HighPriorityFromA->CreateConnections(SourceA, TargetB);
	FromC->CreateConnections(SourceC, TargetB);

	if (!ConfigureTransitionNode(*this, LowPriorityFromA, MakeTestTag(TEXT("Input.Action.Attack.Light")), 1)
		|| !ConfigureTransitionNode(*this, HighPriorityFromA, MakeTestTag(TEXT("Input.Action.Attack.Heavy")), 10)
		|| !ConfigureTransitionNode(*this, FromC, MakeTestTag(TEXT("Input.Action.Attack.Light")), 5))
	{
		return false;
	}

	TArray<RZCombatComboGraphNodes::FIncomingTransitionGroup> IncomingGroups;
	RZCombatComboGraphNodes::GatherIncomingTransitionGroups(TargetB, IncomingGroups);

	TestEqual(TEXT("Target B should have two incoming source groups."), IncomingGroups.Num(), 2);
	if (IncomingGroups.Num() != 2)
	{
		return false;
	}

	TestEqual(TEXT("The first source group should be the upper source node."), IncomingGroups[0].SourceNode, static_cast<UEdGraphNode*>(SourceA));
	TestEqual(TEXT("The second source group should be the lower source node."), IncomingGroups[1].SourceNode, static_cast<UEdGraphNode*>(SourceC));
	TestEqual(TEXT("Source A should expose two incoming transition chips."), IncomingGroups[0].Transitions.Num(), 2);
	if (IncomingGroups[0].Transitions.Num() == 2)
	{
		TestEqual(TEXT("Higher generated-priority transition should appear first inside the source group."), IncomingGroups[0].Transitions[0], HighPriorityFromA);
		TestEqual(TEXT("Lower generated-priority transition should appear second inside the source group."), IncomingGroups[0].Transitions[1], LowPriorityFromA);
	}
	TestEqual(TEXT("Source C should expose one incoming transition chip."), IncomingGroups[1].Transitions.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphIncomingFromGroupsExposeEditablePayloadTest,
	"RZCombat.Combo.Graph.IncomingFromGroupsExposeEditableTransitionPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphIncomingFromGroupsExposeEditablePayloadTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Combo* SourceNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* TargetNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	if (!SourceNode
		|| !TargetNode
		|| !TransitionNode
		|| !ConfigureComboNode(*this, SourceNode, TEXT("A"))
		|| !ConfigureComboNode(*this, TargetNode, TEXT("B"))
		|| !ConfigureTransitionNode(*this, TransitionNode, MakeTestTag(TEXT("Input.Action.Attack.Light")), 3))
	{
		return false;
	}

	TransitionNode->CreateConnections(SourceNode, TargetNode);
	URZCombatComboPayload* GraphPayload = NewObject<URZCombatComboPayload>(TransitionNode);
	TransitionNode->Payload = GraphPayload;

	TArray<RZCombatComboGraphNodes::FIncomingTransitionGroup> IncomingGroups;
	RZCombatComboGraphNodes::GatherIncomingTransitionGroups(TargetNode, IncomingGroups);

	TestEqual(TEXT("Target node should expose one incoming source group."), IncomingGroups.Num(), 1);
	if (IncomingGroups.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Incoming group should expose the actual transition graph node."), IncomingGroups[0].Transitions.Num(), 1);
	if (IncomingGroups[0].Transitions.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Incoming transition chip should target the same transition object that owns Payload."), IncomingGroups[0].Transitions[0], TransitionNode);
	TestEqual(TEXT("Graph payload should be owned by the selectable transition node."), GraphPayload->GetOuter(), static_cast<UObject*>(TransitionNode));

	FProperty* PayloadProperty = URZCombatComboGraphNode_Transition::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URZCombatComboGraphNode_Transition, Payload));
	TestNotNull(TEXT("Transition graph node should expose a Payload property."), PayloadProperty);
	if (!PayloadProperty)
	{
		return false;
	}

	FObjectPropertyBase* PayloadObjectProperty = CastField<FObjectPropertyBase>(PayloadProperty);
	TestNotNull(TEXT("Payload should be an object property editable by Details."), PayloadObjectProperty);
	if (!PayloadObjectProperty)
	{
		return false;
	}

	TestTrue(TEXT("Payload property should accept URZCombatComboPayload instances."), PayloadObjectProperty->PropertyClass->IsChildOf(URZCombatComboPayload::StaticClass()));
	TestTrue(TEXT("Payload property should be editable in Details."), PayloadProperty->HasAnyPropertyFlags(CPF_Edit));
	TestTrue(TEXT("Payload property should be instanced for inline payload authoring."), PayloadProperty->HasAnyPropertyFlags(CPF_InstancedReference));

	FProperty* CheckerProperty = URZCombatComboGraphNode_Transition::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URZCombatComboGraphNode_Transition, Checker));
	TestNotNull(TEXT("Transition graph node should expose a Checker property."), CheckerProperty);
	if (!CheckerProperty)
	{
		return false;
	}

	FObjectPropertyBase* CheckerObjectProperty = CastField<FObjectPropertyBase>(CheckerProperty);
	TestNotNull(TEXT("Checker should be an object property editable by Details."), CheckerObjectProperty);
	if (!CheckerObjectProperty)
	{
		return false;
	}

	TestTrue(TEXT("Checker property should accept URZCombatComboTransitionChecker instances."), CheckerObjectProperty->PropertyClass->IsChildOf(URZCombatComboTransitionChecker::StaticClass()));
	TestTrue(TEXT("Checker property should be editable in Details."), CheckerProperty->HasAnyPropertyFlags(CPF_Edit));
	TestTrue(TEXT("Checker property should be instanced for inline checker authoring."), CheckerProperty->HasAnyPropertyFlags(CPF_InstancedReference));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphEntryTransitionCompileTest,
	"RZCombat.Combo.Graph.EntryTransitionCompilesToTableEntryTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphEntryTransitionCompileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* ComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));

	if (!EntryNode || !ConfigureComboNode(*this, ComboNode, TEXT("Light_1")) || !ConfigureTransitionNode(*this, TransitionNode, TriggerTag, 7, false, TEXT("Condition.Entry")))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, ComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestTrue(TEXT("Compile should succeed for a valid Entry -> Transition -> Combo graph."), Result.bSucceeded);
	TestEqual(TEXT("Compile should not emit errors."), Result.Errors.Num(), 0);
	TestEqual(TEXT("Compile should not emit warnings for the basic valid graph."), Result.Warnings.Num(), 0);
	TestEqual(TEXT("Compile should emit exactly one entry transition."), Table->EntryTransitions.Num(), 1);
	TestEqual(TEXT("Compile should emit exactly one combo node."), Table->Nodes.Num(), 1);

	if (Table->EntryTransitions.Num() != 1 || Table->Nodes.Num() != 1)
	{
		return false;
	}

	const FRZCombatComboTransition& EntryTransition = Table->EntryTransitions[0];
	const FRZCombatComboNode& CompiledNode = Table->Nodes[0];

	TestTrue(TEXT("Entry transition should keep its trigger tag."), EntryTransition.TriggerTag.MatchesTagExact(TriggerTag));
	TestTrue(TEXT("Entry transition should target the compiled combo node id."), EntryTransition.TargetNodeId.Matches(CompiledNode.NodeId));
	TestEqual(TEXT("Entry transition should use generated priority."), EntryTransition.Priority, 1);
	TestFalse(TEXT("Entry transition should keep its consume-trigger flag."), EntryTransition.bConsumeTrigger);
	TestEqual(TEXT("Entry transition should keep its condition key."), EntryTransition.ConditionKey, FName(TEXT("Condition.Entry")));
	TestEqual(TEXT("Compiled combo node should keep its readable name."), CompiledNode.NodeId.Name, FName(TEXT("Light_1")));
	TestEqual(TEXT("Compiled combo node should keep its ability class."), CompiledNode.AbilityClass.Get(), UGameplayAbility::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphCommentActionCreatesCommentNodeTest,
	"RZCombat.Combo.Graph.CommentActionCreatesCommentNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphCommentActionCreatesCommentNodeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	FRZCombatComboGraphSchemaAction_NewNode Action(
		FText::FromString(TEXT("Combo Graph")),
		FText::FromString(TEXT("Comment")),
		FText::FromString(TEXT("Add a graph comment.")),
		1,
		UEdGraphNode_Comment::StaticClass()
	);

	UEdGraphNode* NewNode = Action.PerformAction(Graph, nullptr, FVector2D(320.0, 160.0), false);
	UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(NewNode);

	TestNotNull(TEXT("Comment action should create a comment node."), CommentNode);
	if (!CommentNode)
	{
		return false;
	}

	TestEqual(TEXT("Comment node should be placed at the requested X."), CommentNode->NodePosX, 320);
	TestEqual(TEXT("Comment node should be placed at the requested Y."), CommentNode->NodePosY, 160);
	TestEqual(TEXT("Comment node should get a useful default label."), CommentNode->NodeComment, FString(TEXT("Comment")));
	TestTrue(TEXT("Comment node should get a usable default width."), CommentNode->NodeWidth >= 300);
	TestTrue(TEXT("Comment node should get a usable default height."), CommentNode->NodeHeight >= 160);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphCommentNodeDoesNotAffectCompileTest,
	"RZCombat.Combo.Graph.CommentNodeDoesNotAffectCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphCommentNodeDoesNotAffectCompileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	UEdGraphNode_Comment* CommentNode = AddGraphNode<UEdGraphNode_Comment>(Graph);
	CommentNode->NodeComment = TEXT("Designer branch note");
	CommentNode->NodeWidth = 360;
	CommentNode->NodeHeight = 180;

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* ComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	if (!EntryNode
		|| !ConfigureComboNode(*this, ComboNode, TEXT("Light_1"))
		|| !ConfigureTransitionNode(*this, TransitionNode, TriggerTag))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, ComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);
	TestTrue(TEXT("Compile should ignore comment nodes and succeed for the valid combo flow."), Result.bSucceeded);
	TestEqual(TEXT("Comment node should not become a combo node."), Table->Nodes.Num(), 1);
	TestEqual(TEXT("Comment node should not become a transition."), Table->EntryTransitions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphNodeTransitionCompileTest,
	"RZCombat.Combo.Graph.NodeTransitionCompilesToSourceNodeTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphNodeTransitionCompileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* FirstComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* SecondComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* EntryTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	URZCombatComboGraphNode_Transition* NodeTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag EntryTriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag FollowupTriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Heavy"));

	if (!EntryNode
		|| !ConfigureComboNode(*this, FirstComboNode, TEXT("Light_1"))
		|| !ConfigureComboNode(*this, SecondComboNode, TEXT("Light_2"))
		|| !ConfigureTransitionNode(*this, EntryTransitionNode, EntryTriggerTag, 1)
		|| !ConfigureTransitionNode(*this, NodeTransitionNode, FollowupTriggerTag, 3, true, TEXT("Condition.Followup")))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, EntryTransitionNode)
		|| !ConnectNodes(*this, Graph, EntryTransitionNode, FirstComboNode)
		|| !ConnectNodes(*this, Graph, FirstComboNode, NodeTransitionNode)
		|| !ConnectNodes(*this, Graph, NodeTransitionNode, SecondComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestTrue(TEXT("Compile should succeed for a valid combo follow-up chain."), Result.bSucceeded);
	TestEqual(TEXT("Compile should not emit errors."), Result.Errors.Num(), 0);

	const FRZCombatComboNode* FirstCompiledNode = FindCompiledNodeByName(Table, TEXT("Light_1"));
	const FRZCombatComboNode* SecondCompiledNode = FindCompiledNodeByName(Table, TEXT("Light_2"));
	TestNotNull(TEXT("The first combo node should be compiled into the table."), FirstCompiledNode);
	TestNotNull(TEXT("The second combo node should be compiled into the table."), SecondCompiledNode);

	if (!FirstCompiledNode || !SecondCompiledNode || Table->EntryTransitions.Num() != 1)
	{
		return false;
	}

	TestTrue(TEXT("Entry transition should target the first combo node."), Table->EntryTransitions[0].TargetNodeId.Matches(FirstCompiledNode->NodeId));
	TestEqual(TEXT("The source combo node should keep exactly one compiled transition."), FirstCompiledNode->Transitions.Num(), 1);

	if (FirstCompiledNode->Transitions.Num() != 1)
	{
		return false;
	}

	const FRZCombatComboTransition& CompiledTransition = FirstCompiledNode->Transitions[0];
	TestTrue(TEXT("Node transition should keep its trigger tag."), CompiledTransition.TriggerTag.MatchesTagExact(FollowupTriggerTag));
	TestTrue(TEXT("Node transition should target the second combo node."), CompiledTransition.TargetNodeId.Matches(SecondCompiledNode->NodeId));
	TestEqual(TEXT("Node transition should use generated priority."), CompiledTransition.Priority, 1);
	TestEqual(TEXT("Node transition should keep its condition key."), CompiledTransition.ConditionKey, FName(TEXT("Condition.Followup")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphGuidPreservationTest,
	"RZCombat.Combo.Graph.CompilePreservesComboNodeGuidAcrossRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphGuidPreservationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* ComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));

	if (!EntryNode || !ConfigureComboNode(*this, ComboNode, TEXT("Light_1")) || !ConfigureTransitionNode(*this, TransitionNode, TriggerTag))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, ComboNode))
	{
		return false;
	}

	const auto FirstCompileResult = FRZCombatComboGraphCompiler::Compile(Table);
	TestTrue(TEXT("The first compile should succeed."), FirstCompileResult.bSucceeded);

	const FRZCombatComboNode* FirstCompiledNode = FindCompiledNodeByName(Table, TEXT("Light_1"));
	TestNotNull(TEXT("Compile should emit the combo node."), FirstCompiledNode);
	if (!FirstCompiledNode)
	{
		return false;
	}

	const FGuid FirstGuid = FirstCompiledNode->NodeId.Guid;
	TestTrue(TEXT("Compile should generate a valid combo node guid when one is missing."), FirstGuid.IsValid());

	FRZCombatComboNodeId GraphNodeIdAfterFirstCompile;
	if (!GetStructPropertyByPossiblePaths(
			*this,
			ComboNode,
			{ { TEXT("NodeId") }, { TEXT("NodeData"), TEXT("NodeId") }, { TEXT("Node"), TEXT("NodeId") } },
			FRZCombatComboNodeId::StaticStruct(),
			TEXT("combo node id"),
			GraphNodeIdAfterFirstCompile))
	{
		return false;
	}

	TestTrue(TEXT("The graph combo node should keep the generated guid after compile."), GraphNodeIdAfterFirstCompile.Guid == FirstGuid);

	const auto SecondCompileResult = FRZCombatComboGraphCompiler::Compile(Table);
	TestTrue(TEXT("The second compile should also succeed."), SecondCompileResult.bSucceeded);

	const FRZCombatComboNode* SecondCompiledNode = FindCompiledNodeByName(Table, TEXT("Light_1"));
	TestNotNull(TEXT("The combo node should still exist after recompiling."), SecondCompiledNode);
	if (!SecondCompiledNode)
	{
		return false;
	}

	TestTrue(TEXT("Repeated compile should preserve the combo node guid."), SecondCompiledNode->NodeId.Guid == FirstGuid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphDisplayNameNodeIdSyncTest,
	"RZCombat.Combo.Graph.DisplayNameSynchronizesRuntimeNodeName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphDisplayNameNodeIdSyncTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* ComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));

	if (!EntryNode
		|| !ConfigureComboNode(*this, ComboNode, TEXT("Stale_Manual_Name"))
		|| !ConfigureTransitionNode(*this, TransitionNode, TriggerTag)
		|| !SetTextPropertyByPossiblePaths(
			*this,
			ComboNode,
			{ { TEXT("DisplayName") }, { TEXT("NodeData"), TEXT("DisplayName") }, { TEXT("Node"), TEXT("DisplayName") } },
			TEXT("combo display name"),
			FText::FromString(TEXT("Light Attack One"))))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, ComboNode))
	{
		return false;
	}

	const auto CompileResult = FRZCombatComboGraphCompiler::Compile(Table);
	TestTrue(TEXT("Compile should succeed."), CompileResult.bSucceeded);

	const FRZCombatComboNode* CompiledNode = FindCompiledNodeByName(Table, TEXT("Light_Attack_One"));
	TestNotNull(TEXT("Compile should derive the runtime node name from DisplayName."), CompiledNode);
	if (!CompiledNode)
	{
		return false;
	}

	TestEqual(TEXT("Runtime NodeId.Name should be generated from DisplayName."), CompiledNode->NodeId.Name, FName(TEXT("Light_Attack_One")));
	TestTrue(TEXT("Runtime NodeId.Guid should stay valid."), CompiledNode->NodeId.Guid.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphMissingAbilityFailureTest,
	"RZCombat.Combo.Graph.MissingAbilityFailsAndDoesNotReplaceExistingTableData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphMissingAbilityFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();

	FRZCombatComboNode ExistingNode;
	ExistingNode.NodeId.Guid = FGuid(1, 2, 3, 4);
	ExistingNode.NodeId.Name = TEXT("Existing_Light");
	ExistingNode.AbilityClass = UGameplayAbility::StaticClass();
	Table->Nodes.Add(ExistingNode);

	FRZCombatComboTransition ExistingEntryTransition;
	ExistingEntryTransition.TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Heavy"));
	ExistingEntryTransition.TargetNodeId = ExistingNode.NodeId;
	ExistingEntryTransition.Priority = 11;
	Table->EntryTransitions.Add(ExistingEntryTransition);

	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* MissingAbilityNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));

	if (!EntryNode || !ConfigureComboNode(*this, MissingAbilityNode, TEXT("Light_1"), false) || !ConfigureTransitionNode(*this, TransitionNode, TriggerTag))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, MissingAbilityNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestFalse(TEXT("Compile should fail when a combo node is missing its ability class."), Result.bSucceeded);
	TestTrue(TEXT("Compile should report at least one error for the missing ability."), Result.Errors.Num() > 0);
	TestEqual(TEXT("Failed compile should preserve the existing node array."), Table->Nodes.Num(), 1);
	TestEqual(TEXT("Failed compile should preserve the existing entry transition array."), Table->EntryTransitions.Num(), 1);

	if (Table->Nodes.Num() != 1 || Table->EntryTransitions.Num() != 1)
	{
		return false;
	}

	TestTrue(TEXT("Failed compile should keep the previous node id."), Table->Nodes[0].NodeId.Matches(ExistingNode.NodeId));
	TestEqual(TEXT("Failed compile should keep the previous entry transition priority."), Table->EntryTransitions[0].Priority, 11);
	TestTrue(TEXT("Failed compile should keep the previous entry transition target."), Table->EntryTransitions[0].TargetNodeId.Matches(ExistingNode.NodeId));
	TestTrue(TEXT("Failed compile should keep the previous entry trigger."), Table->EntryTransitions[0].TriggerTag.MatchesTagExact(ExistingEntryTransition.TriggerTag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphExistingTableImportTest,
	"RZCombat.Combo.Graph.ExistingTableDataIsImportedBeforeCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphExistingTableImportTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();

	FRZCombatComboNode ExistingNode;
	ExistingNode.NodeId.Name = TEXT("Existing_Light");
	ExistingNode.DisplayName = FText::FromString(TEXT("Existing Light"));
	ExistingNode.AbilityClass = UGameplayAbility::StaticClass();
	Table->Nodes.Add(ExistingNode);

	FRZCombatComboTransition ExistingEntryTransition;
	ExistingEntryTransition.TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	ExistingEntryTransition.TargetNodeId = ExistingNode.NodeId;
	ExistingEntryTransition.Priority = 12;
	ExistingEntryTransition.ConditionKey = TEXT("Existing.Condition");
	Table->EntryTransitions.Add(ExistingEntryTransition);

	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	TestNotNull(TEXT("EnsureGraph should create a graph for an existing table."), Graph);
	if (!Graph)
	{
		return false;
	}

	TestNotNull(TEXT("Existing table import should create a combo node."), FindSingleNodeOfType<URZCombatComboGraphNode_Combo>(*this, Graph, TEXT("Imported combo node")));
	TestNotNull(TEXT("Existing table import should create a transition node."), FindSingleNodeOfType<URZCombatComboGraphNode_Transition>(*this, Graph, TEXT("Imported transition node")));

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);
	TestTrue(TEXT("Compile should succeed after importing existing table data into the graph."), Result.bSucceeded);
	TestEqual(TEXT("Compile should keep one entry transition."), Table->EntryTransitions.Num(), 1);
	TestEqual(TEXT("Compile should keep one combo node."), Table->Nodes.Num(), 1);

	if (Table->EntryTransitions.Num() != 1 || Table->Nodes.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Imported node should keep its name."), Table->Nodes[0].NodeId.Name, FName(TEXT("Existing_Light")));
	TestTrue(TEXT("Imported node should gain a stable graph guid."), Table->Nodes[0].NodeId.Guid.IsValid());
	TestTrue(TEXT("Imported transition should keep its trigger tag."), Table->EntryTransitions[0].TriggerTag.MatchesTagExact(ExistingEntryTransition.TriggerTag));
	TestTrue(TEXT("Imported transition should target the imported node."), Table->EntryTransitions[0].TargetNodeId.Matches(Table->Nodes[0].NodeId));
	TestEqual(TEXT("Imported transition should regenerate priority from graph order."), Table->EntryTransitions[0].Priority, 1);
	TestEqual(TEXT("Imported transition should keep condition key."), Table->EntryTransitions[0].ConditionKey, FName(TEXT("Existing.Condition")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphCompileDuplicatesTransitionPayloadTest,
	"RZCombat.Combo.Graph.CompileDuplicatesTransitionPayloadIntoTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphCompileDuplicatesTransitionPayloadTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* ComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	if (!EntryNode
		|| !ConfigureComboNode(*this, ComboNode, TEXT("Light_1"))
		|| !ConfigureTransitionNode(*this, TransitionNode, TriggerTag))
	{
		return false;
	}

	URZCombatComboPayload* GraphPayload = NewObject<URZCombatComboPayload>(TransitionNode);
	TransitionNode->Payload = GraphPayload;

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, ComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);
	TestTrue(TEXT("Compile should succeed."), Result.bSucceeded);
	TestEqual(TEXT("Compile should write one entry transition."), Table->EntryTransitions.Num(), 1);
	if (Table->EntryTransitions.Num() != 1)
	{
		return false;
	}

	URZCombatComboPayload* RuntimePayload = Table->EntryTransitions[0].Payload;
	TestNotNull(TEXT("Compiled transition should carry a runtime payload."), RuntimePayload);
	TestNotSame(TEXT("Compiled payload should be a duplicate, not the editor graph payload."), RuntimePayload, GraphPayload);
	TestEqual(TEXT("Compiled payload should be owned by the combo table asset."), RuntimePayload ? RuntimePayload->GetOuter() : nullptr, static_cast<UObject*>(Table));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphImportDuplicatesTransitionPayloadIntoGraphTest,
	"RZCombat.Combo.Graph.ImportDuplicatesTransitionPayloadIntoGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphImportDuplicatesTransitionPayloadIntoGraphTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode ExistingNode;
	ExistingNode.NodeId.Name = TEXT("Existing_Light");
	ExistingNode.DisplayName = FText::FromString(TEXT("Existing Light"));
	ExistingNode.AbilityClass = UGameplayAbility::StaticClass();
	Table->Nodes.Add(ExistingNode);

	FRZCombatComboTransition ExistingEntryTransition;
	ExistingEntryTransition.TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	ExistingEntryTransition.TargetNodeId = ExistingNode.NodeId;
	ExistingEntryTransition.Payload = NewObject<URZCombatComboPayload>(Table);
	Table->EntryTransitions.Add(ExistingEntryTransition);

	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Transition* TransitionNode = FindSingleNodeOfType<URZCombatComboGraphNode_Transition>(*this, Graph, TEXT("Imported transition node"));
	TestNotNull(TEXT("Imported transition should have a graph payload."), TransitionNode ? TransitionNode->Payload.Get() : nullptr);
	if (!TransitionNode || !TransitionNode->Payload)
	{
		return false;
	}

	TestNotSame(TEXT("Imported graph payload should not share the runtime table object."), TransitionNode->Payload.Get(), ExistingEntryTransition.Payload.Get());
	TestEqual(TEXT("Imported graph payload should be owned by the transition graph node."), TransitionNode->Payload->GetOuter(), static_cast<UObject*>(TransitionNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphCompileDuplicatesTransitionCheckerTest,
	"RZCombat.Combo.Graph.CompileDuplicatesTransitionCheckerIntoTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphCompileDuplicatesTransitionCheckerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* ComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	if (!EntryNode
		|| !ConfigureComboNode(*this, ComboNode, TEXT("Light_1"))
		|| !ConfigureTransitionNode(*this, TransitionNode, TriggerTag))
	{
		return false;
	}

	URZCombatComboAndChecker* GraphChecker = NewObject<URZCombatComboAndChecker>(TransitionNode);
	URZCombatComboTransitionChecker* GraphChildChecker = NewObject<URZCombatComboTransitionChecker>(GraphChecker);
	GraphChecker->Checkers.Add(GraphChildChecker);
	TransitionNode->Checker = GraphChecker;

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, ComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);
	TestTrue(TEXT("Compile should succeed."), Result.bSucceeded);
	TestEqual(TEXT("Compile should write one entry transition."), Table->EntryTransitions.Num(), 1);
	if (Table->EntryTransitions.Num() != 1)
	{
		return false;
	}

	URZCombatComboTransitionChecker* RuntimeChecker = Table->EntryTransitions[0].Checker;
	TestNotNull(TEXT("Compiled transition should carry a runtime checker."), RuntimeChecker);
	TestNotSame(
		TEXT("Compiled checker should be a duplicate, not the editor graph checker."),
		RuntimeChecker,
		static_cast<URZCombatComboTransitionChecker*>(GraphChecker)
	);
	TestEqual(TEXT("Compiled checker should be owned by the combo table asset."), RuntimeChecker ? RuntimeChecker->GetOuter() : nullptr, static_cast<UObject*>(Table));
	URZCombatComboAndChecker* RuntimeAndChecker = Cast<URZCombatComboAndChecker>(RuntimeChecker);
	TestNotNull(TEXT("Compiled checker should keep its composite checker class."), RuntimeAndChecker);
	TestEqual(TEXT("Compiled composite checker should keep child checker count."), RuntimeAndChecker ? RuntimeAndChecker->Checkers.Num() : 0, 1);
	if (RuntimeAndChecker && RuntimeAndChecker->Checkers.Num() == 1)
	{
		URZCombatComboTransitionChecker* RuntimeChildChecker = RuntimeAndChecker->Checkers[0].Get();
		TestNotNull(TEXT("Compiled composite checker should keep its child checker."), RuntimeChildChecker);
		TestNotSame(TEXT("Compiled child checker should be duplicated with the composite."), RuntimeChildChecker, GraphChildChecker);
		TestEqual(TEXT("Compiled child checker should be owned by the runtime composite checker."), RuntimeChildChecker ? RuntimeChildChecker->GetOuter() : nullptr, static_cast<UObject*>(RuntimeAndChecker));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphImportDuplicatesTransitionCheckerIntoGraphTest,
	"RZCombat.Combo.Graph.ImportDuplicatesTransitionCheckerIntoGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphImportDuplicatesTransitionCheckerIntoGraphTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode ExistingNode;
	ExistingNode.NodeId.Name = TEXT("Existing_Light");
	ExistingNode.DisplayName = FText::FromString(TEXT("Existing Light"));
	ExistingNode.AbilityClass = UGameplayAbility::StaticClass();
	Table->Nodes.Add(ExistingNode);

	FRZCombatComboTransition ExistingEntryTransition;
	ExistingEntryTransition.TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	ExistingEntryTransition.TargetNodeId = ExistingNode.NodeId;
	ExistingEntryTransition.Checker = NewObject<URZCombatComboTransitionChecker>(Table);
	Table->EntryTransitions.Add(ExistingEntryTransition);

	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Transition* TransitionNode = FindSingleNodeOfType<URZCombatComboGraphNode_Transition>(*this, Graph, TEXT("Imported transition node"));
	TestNotNull(TEXT("Imported transition should have a graph checker."), TransitionNode ? TransitionNode->Checker.Get() : nullptr);
	if (!TransitionNode || !TransitionNode->Checker)
	{
		return false;
	}

	TestNotSame(TEXT("Imported graph checker should not share the runtime table object."), TransitionNode->Checker.Get(), ExistingEntryTransition.Checker.Get());
	TestEqual(TEXT("Imported graph checker should be owned by the transition graph node."), TransitionNode->Checker->GetOuter(), static_cast<UObject*>(TransitionNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphDuplicateNodeNameFailureTest,
	"RZCombat.Combo.Graph.DuplicateNodeNameFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphDuplicateNodeNameFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* FirstComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* SecondComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* FirstTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	URZCombatComboGraphNode_Transition* SecondTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);

	if (!EntryNode
		|| !ConfigureComboNode(*this, FirstComboNode, TEXT("Duplicate"))
		|| !ConfigureComboNode(*this, SecondComboNode, TEXT("Duplicate"))
		|| !ConfigureTransitionNode(*this, FirstTransitionNode, MakeTestTag(TEXT("Input.Action.Attack.Light")), 1)
		|| !ConfigureTransitionNode(*this, SecondTransitionNode, MakeTestTag(TEXT("Input.Action.Attack.Heavy")), 2))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, FirstTransitionNode)
		|| !ConnectNodes(*this, Graph, FirstTransitionNode, FirstComboNode)
		|| !ConnectNodes(*this, Graph, EntryNode, SecondTransitionNode)
		|| !ConnectNodes(*this, Graph, SecondTransitionNode, SecondComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestFalse(TEXT("Compile should fail when two combo nodes share the same name."), Result.bSucceeded);
	TestTrue(TEXT("Compile should report an error for duplicate combo node names."), Result.Errors.Num() > 0);
	TestEqual(TEXT("Failed compile should not replace the table node array."), Table->Nodes.Num(), 0);
	TestEqual(TEXT("Failed compile should not replace the entry transition array."), Table->EntryTransitions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphDuplicateEntryFailureTest,
	"RZCombat.Combo.Graph.DuplicateEntryFailsAndPreservesTableData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphDuplicateEntryFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode ExistingNode;
	ExistingNode.NodeId.Name = TEXT("Existing_Light");
	ExistingNode.AbilityClass = UGameplayAbility::StaticClass();
	Table->Nodes.Add(ExistingNode);

	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	AddGraphNode<URZCombatComboGraphNode_Entry>(Graph);
	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestFalse(TEXT("Compile should fail with duplicate Entry nodes."), Result.bSucceeded);
	TestTrue(TEXT("Duplicate Entry compile should report errors."), Result.Errors.Num() > 0);
	TestEqual(TEXT("Failed duplicate Entry compile should preserve existing table data."), Table->Nodes.Num(), 1);
	TestEqual(TEXT("Existing node should remain after duplicate Entry compile failure."), Table->Nodes[0].NodeId.Name, FName(TEXT("Existing_Light")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphInvalidTriggerFailureTest,
	"RZCombat.Combo.Graph.InvalidTriggerFailsAndPreservesTableData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphInvalidTriggerFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode ExistingNode;
	ExistingNode.NodeId.Name = TEXT("Existing_Light");
	ExistingNode.AbilityClass = UGameplayAbility::StaticClass();
	Table->Nodes.Add(ExistingNode);

	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* ComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	if (!EntryNode || !ConfigureComboNode(*this, ComboNode, TEXT("Light_1")))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode) || !ConnectNodes(*this, Graph, TransitionNode, ComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestFalse(TEXT("Compile should fail when a transition trigger tag is invalid."), Result.bSucceeded);
	TestTrue(TEXT("Invalid trigger compile should report errors."), Result.Errors.Num() > 0);
	TestEqual(TEXT("Invalid trigger failure should preserve existing table data."), Table->Nodes.Num(), 1);
	TestEqual(TEXT("Existing node should remain after invalid trigger failure."), Table->Nodes[0].NodeId.Name, FName(TEXT("Existing_Light")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphDuplicateGuidFailureTest,
	"RZCombat.Combo.Graph.DuplicateGuidFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphDuplicateGuidFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* FirstComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* SecondComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* FirstTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	URZCombatComboGraphNode_Transition* SecondTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGuid DuplicateGuid(10, 20, 30, 40);

	if (!EntryNode
		|| !ConfigureComboNode(*this, FirstComboNode, TEXT("Light_1"), true, DuplicateGuid)
		|| !ConfigureComboNode(*this, SecondComboNode, TEXT("Light_2"), true, DuplicateGuid)
		|| !ConfigureTransitionNode(*this, FirstTransitionNode, MakeTestTag(TEXT("Input.Action.Attack.Light")), 1)
		|| !ConfigureTransitionNode(*this, SecondTransitionNode, MakeTestTag(TEXT("Input.Action.Attack.Heavy")), 2))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, FirstTransitionNode)
		|| !ConnectNodes(*this, Graph, FirstTransitionNode, FirstComboNode)
		|| !ConnectNodes(*this, Graph, EntryNode, SecondTransitionNode)
		|| !ConnectNodes(*this, Graph, SecondTransitionNode, SecondComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestFalse(TEXT("Compile should fail when two combo nodes share the same GUID."), Result.bSucceeded);
	TestTrue(TEXT("Duplicate GUID compile should report errors."), Result.Errors.Num() > 0);
	TestEqual(TEXT("Duplicate GUID failure should not write nodes."), Table->Nodes.Num(), 0);
	TestEqual(TEXT("Duplicate GUID failure should not write entry transitions."), Table->EntryTransitions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphMalformedTransitionFailureTest,
	"RZCombat.Combo.Graph.MalformedTransitionFailsAndPreservesTableData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphMalformedTransitionFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode ExistingNode;
	ExistingNode.NodeId.Name = TEXT("Existing_Light");
	ExistingNode.AbilityClass = UGameplayAbility::StaticClass();
	Table->Nodes.Add(ExistingNode);

	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Transition* TransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	if (!EntryNode || !ConfigureTransitionNode(*this, TransitionNode, MakeTestTag(TEXT("Input.Action.Attack.Light"))))
	{
		return false;
	}

	if (!ConnectNodes(*this, Graph, EntryNode, TransitionNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestFalse(TEXT("Compile should fail when a transition has no target."), Result.bSucceeded);
	TestTrue(TEXT("Malformed transition compile should report errors."), Result.Errors.Num() > 0);
	TestEqual(TEXT("Malformed transition failure should preserve existing table data."), Table->Nodes.Num(), 1);
	TestEqual(TEXT("Existing node should remain after malformed transition failure."), Table->Nodes[0].NodeId.Name, FName(TEXT("Existing_Light")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGraphGeneratedPriorityHorizontalOrderTest,
	"RZCombat.Combo.Graph.GeneratedPriorityUsesTargetHorizontalOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGraphGeneratedPriorityHorizontalOrderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboGraph* Graph = EnsureGraph(*this, Table);
	if (!Graph)
	{
		return false;
	}

	URZCombatComboGraphNode_Entry* EntryNode = FindSingleNodeOfType<URZCombatComboGraphNode_Entry>(*this, Graph, TEXT("Entry node"));
	URZCombatComboGraphNode_Combo* SourceComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* FirstTargetComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Combo* SecondTargetComboNode = AddGraphNode<URZCombatComboGraphNode_Combo>(Graph);
	URZCombatComboGraphNode_Transition* EntryTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	URZCombatComboGraphNode_Transition* FirstFollowupTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	URZCombatComboGraphNode_Transition* SecondFollowupTransitionNode = AddGraphNode<URZCombatComboGraphNode_Transition>(Graph);
	const FGameplayTag StartTriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag FollowupTriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Heavy"));

	if (!EntryNode
		|| !ConfigureComboNode(*this, SourceComboNode, TEXT("Light_1"))
		|| !ConfigureComboNode(*this, FirstTargetComboNode, TEXT("Light_2"))
		|| !ConfigureComboNode(*this, SecondTargetComboNode, TEXT("Heavy_1"))
		|| !ConfigureTransitionNode(*this, EntryTransitionNode, StartTriggerTag, 99)
		|| !ConfigureTransitionNode(*this, FirstFollowupTransitionNode, FollowupTriggerTag, 1)
		|| !ConfigureTransitionNode(*this, SecondFollowupTransitionNode, FollowupTriggerTag, 99))
	{
		return false;
	}

	FirstTargetComboNode->NodePosX = 100;
	FirstTargetComboNode->NodePosY = 200;
	SecondTargetComboNode->NodePosX = 500;
	SecondTargetComboNode->NodePosY = 200;

	if (!ConnectNodes(*this, Graph, EntryNode, EntryTransitionNode)
		|| !ConnectNodes(*this, Graph, EntryTransitionNode, SourceComboNode)
		|| !ConnectNodes(*this, Graph, SourceComboNode, FirstFollowupTransitionNode)
		|| !ConnectNodes(*this, Graph, FirstFollowupTransitionNode, FirstTargetComboNode)
		|| !ConnectNodes(*this, Graph, SourceComboNode, SecondFollowupTransitionNode)
		|| !ConnectNodes(*this, Graph, SecondFollowupTransitionNode, SecondTargetComboNode))
	{
		return false;
	}

	const auto Result = FRZCombatComboGraphCompiler::Compile(Table);

	TestTrue(TEXT("Compile should succeed because horizontal order generates unique priorities."), Result.bSucceeded);
	TestEqual(TEXT("Compile should write all three combo nodes."), Table->Nodes.Num(), 3);
	if (!Result.bSucceeded || Table->Nodes.Num() != 3)
	{
		return false;
	}

	const FRZCombatComboNode* SourceRuntimeNode = FindCompiledNodeByName(Table, TEXT("Light_1"));
	TestNotNull(TEXT("Compiled source node should exist."), SourceRuntimeNode);
	if (!SourceRuntimeNode)
	{
		return false;
	}

	TestEqual(TEXT("Source node should keep both followup transitions."), SourceRuntimeNode->Transitions.Num(), 2);
	if (SourceRuntimeNode->Transitions.Num() != 2)
	{
		return false;
	}

	const FRZCombatComboTransition* LeftRuntimeTransition = nullptr;
	const FRZCombatComboTransition* RightRuntimeTransition = nullptr;
	for (const FRZCombatComboTransition& RuntimeTransition : SourceRuntimeNode->Transitions)
	{
		if (RuntimeTransition.TargetNodeId.Matches(FirstTargetComboNode->NodeId))
		{
			LeftRuntimeTransition = &RuntimeTransition;
		}
		else if (RuntimeTransition.TargetNodeId.Matches(SecondTargetComboNode->NodeId))
		{
			RightRuntimeTransition = &RuntimeTransition;
		}
	}

	TestNotNull(TEXT("Left target transition should compile."), LeftRuntimeTransition);
	TestNotNull(TEXT("Right target transition should compile."), RightRuntimeTransition);
	if (!LeftRuntimeTransition || !RightRuntimeTransition)
	{
		return false;
	}

	TestTrue(TEXT("Left target should receive the higher generated priority."), LeftRuntimeTransition->Priority > RightRuntimeTransition->Priority);
	TestEqual(TEXT("Left graph transition should be updated to the generated priority."), FirstFollowupTransitionNode->Priority, LeftRuntimeTransition->Priority);
	TestEqual(TEXT("Right graph transition should be updated to the generated priority."), SecondFollowupTransitionNode->Priority, RightRuntimeTransition->Priority);

	FProperty* PriorityProperty = URZCombatComboGraphNode_Transition::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URZCombatComboGraphNode_Transition, Priority));
	TestNotNull(TEXT("Transition should expose generated Priority in Details."), PriorityProperty);
	if (!PriorityProperty)
	{
		return false;
	}

	TestTrue(TEXT("Generated Priority should be read-only in Details."), PriorityProperty->HasAnyPropertyFlags(CPF_EditConst));
	return true;
}

#endif
