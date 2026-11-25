#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RZCombatComboTableTypes.h"
#include "RZCombatComboTableAsset.generated.h"

#if WITH_EDITORONLY_DATA
class UEdGraph;
#endif

UCLASS(BlueprintType)
class RZCOMBATRUNTIME_API URZCombatComboTableAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FName TableId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo", meta = (TitleProperty = "DisplayName"))
	TArray<FRZCombatComboTransition> EntryTransitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo", meta = (TitleProperty = "DisplayName"))
	TArray<FRZCombatComboNode> Nodes;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UEdGraph> EditorGraph;
#endif

	UFUNCTION(BlueprintPure, Category = "RZCombat|Combo")
	bool HasNode(const FRZCombatComboNodeId& NodeId) const;

	const FRZCombatComboNode* FindNode(const FRZCombatComboNodeId& NodeId) const;
	const FRZCombatComboTransition* ResolveBestTransition(
		const TArray<FRZCombatComboTransition>& Transitions,
		FGameplayTag TriggerTag,
		bool& bOutAmbiguous
	) const;
};
