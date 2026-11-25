#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "RZCombatComboTableTypes.h"
#include "RZCombatComboGraphNodes.generated.h"

class UEdGraph;
class URZCombatComboTransitionChecker;

UCLASS()
class RZCOMBATEDITOR_API URZCombatComboGraphNode_Entry : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName OutputPinName;

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;

	UEdGraphPin* GetOutputPin() const;
};

UCLASS()
class RZCOMBATEDITOR_API URZCombatComboGraphNode_Combo : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName InputPinName;
	static const FName OutputPinName;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, Category = "RZCombat|Combo", meta = (DisplayName = "Generated Node Id"))
	FRZCombatComboNodeId NodeId;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo", meta = (MultiLine = "true"))
	FText DesignerNote;

	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool ShowPaletteIconOnNode() const override;

	void EnsureNodeId();
	UEdGraphPin* GetInputPin() const;
	UEdGraphPin* GetOutputPin() const;
};

UCLASS()
class RZCOMBATEDITOR_API URZCombatComboGraphNode_Transition : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName InputPinName;
	static const FName OutputPinName;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	FGameplayTag TriggerTag;

	UPROPERTY(VisibleAnywhere, Category = "RZCombat|Combo", meta = (DisplayName = "Generated Priority", ToolTip = "Generated from outgoing target horizontal order. Leftmost target has the highest priority."))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	bool bConsumeTrigger = true;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo")
	FName ConditionKey = NAME_None;

	UPROPERTY(EditAnywhere, Instanced, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboPayload> Payload = nullptr;

	UPROPERTY(EditAnywhere, Instanced, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTransitionChecker> Checker = nullptr;

	UPROPERTY(EditAnywhere, Category = "RZCombat|Combo", meta = (MultiLine = "true"))
	FText DesignerNote;

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;

	UEdGraphPin* GetInputPin() const;
	UEdGraphPin* GetOutputPin() const;
	UEdGraphNode* GetPreviousNode() const;
	URZCombatComboGraphNode_Combo* GetNextComboNode() const;
	void CreateConnections(UEdGraphNode* PreviousNode, URZCombatComboGraphNode_Combo* NextNode);
};

namespace RZCombatComboGraphNodes
{
	struct FIncomingTransitionGroup
	{
		UEdGraphNode* SourceNode = nullptr;
		TArray<URZCombatComboGraphNode_Transition*> Transitions;
	};

	RZCOMBATEDITOR_API void GatherIncomingTransitionGroups(
		const URZCombatComboGraphNode_Combo* TargetNode,
		TArray<FIncomingTransitionGroup>& OutGroups
	);

	RZCOMBATEDITOR_API int32 CalculateGeneratedTransitionPriority(const URZCombatComboGraphNode_Transition* TransitionNode);
	RZCOMBATEDITOR_API void RefreshGeneratedTransitionPriorities(UEdGraph* Graph, bool bModifyNodes = true);
	RZCOMBATEDITOR_API FText GetSourceNodeDisplayText(const UEdGraphNode* SourceNode);
	RZCOMBATEDITOR_API FText GetTransitionDisplayText(const URZCombatComboGraphNode_Transition* TransitionNode);
}
