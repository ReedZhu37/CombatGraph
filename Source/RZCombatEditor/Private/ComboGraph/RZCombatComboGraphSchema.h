#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "RZCombatComboGraphSchema.generated.h"

struct RZCOMBATEDITOR_API FRZCombatComboGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	UClass* NodeClass = nullptr;

	FRZCombatComboGraphSchemaAction_NewNode() = default;
	FRZCombatComboGraphSchemaAction_NewNode(
		const FText& InNodeCategory,
		const FText& InMenuDesc,
		const FText& InToolTip,
		int32 InGrouping,
		UClass* InNodeClass
	);

	virtual UEdGraphNode* PerformAction(
		UEdGraph* ParentGraph,
		UEdGraphPin* FromPin,
		const FVector2D Location,
		bool bSelectNewNode = true
	) override;
};

UCLASS()
class RZCOMBATEDITOR_API URZCombatComboGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual bool CreateAutomaticConversionNodeAndConnections(UEdGraphPin* A, UEdGraphPin* B) const override;
};
