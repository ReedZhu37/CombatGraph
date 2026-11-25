#include "ComboGraph/RZCombatComboGraphSchema.h"

#include "ComboGraph/RZCombatComboGraph.h"
#include "ComboGraph/RZCombatComboGraphNodes.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Framework/Commands/GenericCommands.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "RZCombatComboGraphSchema"

FRZCombatComboGraphSchemaAction_NewNode::FRZCombatComboGraphSchemaAction_NewNode(
	const FText& InNodeCategory,
	const FText& InMenuDesc,
	const FText& InToolTip,
	int32 InGrouping,
	UClass* InNodeClass
)
	: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	, NodeClass(InNodeClass)
{
}

UEdGraphNode* FRZCombatComboGraphSchemaAction_NewNode::PerformAction(
	UEdGraph* ParentGraph,
	UEdGraphPin* FromPin,
	const FVector2D Location,
	bool bSelectNewNode
)
{
	if (!ParentGraph || !NodeClass)
	{
		return nullptr;
	}

	ParentGraph->Modify();

	UEdGraphNode* NewNode = NewObject<UEdGraphNode>(ParentGraph, NodeClass, NAME_None, RF_Transactional);
	NewNode->CreateNewGuid();
	NewNode->NodePosX = static_cast<int32>(Location.X);
	NewNode->NodePosY = static_cast<int32>(Location.Y);
	if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(NewNode))
	{
		CommentNode->NodeComment = TEXT("Comment");
		CommentNode->NodeWidth = 360;
		CommentNode->NodeHeight = 180;
	}
	NewNode->AllocateDefaultPins();
	ParentGraph->AddNode(NewNode, true, bSelectNewNode);
	NewNode->PostPlacedNewNode();
	NewNode->AutowireNewNode(FromPin);
	ParentGraph->NotifyGraphChanged();

	return NewNode;
}

namespace RZCombatComboGraphSchema
{
	static bool IsEntryOrComboOutputNode(const UEdGraphNode* Node)
	{
		return Node && (Node->IsA<URZCombatComboGraphNode_Entry>() || Node->IsA<URZCombatComboGraphNode_Combo>());
	}

	static bool IsTransitionNode(const UEdGraphNode* Node)
	{
		return Node && Node->IsA<URZCombatComboGraphNode_Transition>();
	}

	static bool IsComboNode(const UEdGraphNode* Node)
	{
		return Node && Node->IsA<URZCombatComboGraphNode_Combo>();
	}

	static bool IsTransitionPin(const UEdGraphPin* Pin)
	{
		return Pin && IsTransitionNode(Pin->GetOwningNode());
	}

	static bool IsDirectSourceToComboConnection(const UEdGraphNode* OutputNode, const UEdGraphNode* InputNode)
	{
		return IsEntryOrComboOutputNode(OutputNode) && IsComboNode(InputNode);
	}

	static ECanCreateConnectionResponse BreakOtherLinksOnTransitionPin(const UEdGraphPin* PinA, const UEdGraphPin* PinB)
	{
		if (IsTransitionPin(PinA))
		{
			return CONNECT_RESPONSE_BREAK_OTHERS_A;
		}

		if (IsTransitionPin(PinB))
		{
			return CONNECT_RESPONSE_BREAK_OTHERS_B;
		}

		return CONNECT_RESPONSE_MAKE;
	}
}

void URZCombatComboGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	if (URZCombatComboGraph* ComboGraph = Cast<URZCombatComboGraph>(&Graph))
	{
		ComboGraph->EnsureEntryNode();
	}
}

void URZCombatComboGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const FText Category = LOCTEXT("ComboGraphCategory", "Combo Graph");

	ContextMenuBuilder.AddAction(MakeShared<FRZCombatComboGraphSchemaAction_NewNode>(
		Category,
		LOCTEXT("AddComboNode", "Combo Node"),
		LOCTEXT("AddComboNodeTooltip", "Add a combo segment node."),
		0,
		URZCombatComboGraphNode_Combo::StaticClass()
	));

	ContextMenuBuilder.AddAction(MakeShared<FRZCombatComboGraphSchemaAction_NewNode>(
		Category,
		LOCTEXT("AddCommentNode", "Comment"),
		LOCTEXT("AddCommentNodeTooltip", "Add a graph comment."),
		1,
		UEdGraphNode_Comment::StaticClass()
	));
}

void URZCombatComboGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (Context && Context->Node)
	{
		FToolMenuSection& Section = Menu->AddSection("RZCombatComboGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		if (Context->Node->CanDuplicateNode())
		{
			Section.AddMenuEntry(FGenericCommands::Get().Copy);
			Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
		}

		if (!Context->Node->IsA<URZCombatComboGraphNode_Entry>())
		{
			Section.AddMenuEntry(FGenericCommands::Get().Delete);
		}
	}
	else if (Context && !Context->Pin)
	{
		FToolMenuSection& Section = Menu->AddSection("RZCombatComboGraphSchemaGraphActions", LOCTEXT("GraphActionsMenuHeader", "Graph Actions"));
		Section.AddMenuEntry(FGenericCommands::Get().Paste);
	}

	Super::GetContextMenuActions(Menu, Context);
}

const FPinConnectionResponse URZCombatComboGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if (!A || !B)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("MissingPin", "Missing pin."));
	}

	if (A == B || A->GetOwningNode() == B->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameNode", "Cannot connect a node to itself."));
	}

	if (A->Direction == B->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameDirection", "Pins must have opposite directions."));
	}

	const UEdGraphPin* OutputPin = A->Direction == EGPD_Output ? A : B;
	const UEdGraphPin* InputPin = A->Direction == EGPD_Input ? A : B;
	const UEdGraphNode* OutputNode = OutputPin->GetOwningNode();
	const UEdGraphNode* InputNode = InputPin->GetOwningNode();

	if (OutputNode && InputNode && OutputNode->GetGraph() != InputNode->GetGraph())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("DifferentGraphs", "Pins must belong to the same graph."));
	}

	if (RZCombatComboGraphSchema::IsEntryOrComboOutputNode(OutputNode) && RZCombatComboGraphSchema::IsTransitionNode(InputNode))
	{
		return FPinConnectionResponse(
			RZCombatComboGraphSchema::BreakOtherLinksOnTransitionPin(A, B),
			LOCTEXT("SourceToTransition", "Connect source to transition.")
		);
	}

	if (RZCombatComboGraphSchema::IsTransitionNode(OutputNode) && RZCombatComboGraphSchema::IsComboNode(InputNode))
	{
		return FPinConnectionResponse(
			RZCombatComboGraphSchema::BreakOtherLinksOnTransitionPin(A, B),
			LOCTEXT("TransitionToCombo", "Connect transition to combo.")
		);
	}

	if (RZCombatComboGraphSchema::IsDirectSourceToComboConnection(OutputNode, InputNode))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, LOCTEXT("SourceToComboTransitionEdge", "Create a transition edge."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("InvalidComboGraphConnection", "Use Entry/Combo -> Combo to create a transition edge."));
}

bool URZCombatComboGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	return Super::TryCreateConnection(A, B);
}

bool URZCombatComboGraphSchema::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* A, UEdGraphPin* B) const
{
	if (!A || !B || A->Direction == B->Direction)
	{
		return false;
	}

	UEdGraphPin* OutputPin = A->Direction == EGPD_Output ? A : B;
	UEdGraphPin* InputPin = A->Direction == EGPD_Input ? A : B;
	UEdGraphNode* SourceNode = OutputPin ? OutputPin->GetOwningNode() : nullptr;
	URZCombatComboGraphNode_Combo* TargetNode = InputPin ? Cast<URZCombatComboGraphNode_Combo>(InputPin->GetOwningNode()) : nullptr;
	if (!RZCombatComboGraphSchema::IsEntryOrComboOutputNode(SourceNode) || !TargetNode)
	{
		return false;
	}

	UEdGraph* Graph = SourceNode->GetGraph();
	if (!Graph || Graph != TargetNode->GetGraph())
	{
		return false;
	}

	Graph->Modify();
	SourceNode->Modify();
	TargetNode->Modify();

	FGraphNodeCreator<URZCombatComboGraphNode_Transition> NodeCreator(*Graph);
	URZCombatComboGraphNode_Transition* TransitionNode = NodeCreator.CreateNode(true);
	if (!TransitionNode)
	{
		return false;
	}

	TransitionNode->NodePosX = (SourceNode->NodePosX + TargetNode->NodePosX) / 2;
	TransitionNode->NodePosY = (SourceNode->NodePosY + TargetNode->NodePosY) / 2;
	NodeCreator.Finalize();
	TransitionNode->CreateConnections(SourceNode, TargetNode);
	Graph->NotifyGraphChanged();
	return true;
}

#undef LOCTEXT_NAMESPACE
