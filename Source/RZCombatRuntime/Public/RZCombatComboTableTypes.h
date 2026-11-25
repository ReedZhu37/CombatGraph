#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "RZCombatComboPayload.h"
#include "RZCombatComboTableTypes.generated.h"

class URZCombatComboTableAsset;
class URZCombatComboTransitionChecker;

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboNodeId
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGuid Guid;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FName Name = NAME_None;

	bool IsValid() const
	{
		return Guid.IsValid() || !Name.IsNone();
	}

	bool Matches(const FRZCombatComboNodeId& Other) const
	{
		if (Guid.IsValid() && Other.Guid.IsValid())
		{
			return Guid == Other.Guid;
		}

		return !Name.IsNone() && Name == Other.Name;
	}
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag TriggerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId TargetNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bConsumeTrigger = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FName ConditionKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboPayload> Payload = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTransitionChecker> Checker = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DesignerNote;
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo", meta = (TitleProperty = "DisplayName"))
	TArray<FRZCombatComboTransition> Transitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DesignerNote;
};

UENUM(BlueprintType)
enum class ERZCombatComboRequestResultType : uint8
{
	Success,
	NoActiveTable,
	NoCurrentNode,
	NoEntryTransition,
	NoMatchingTrigger,
	ConditionFailed,
	TargetNodeMissing,
	TargetAbilityMissing,
	// Deprecated: request resolution no longer activates abilities, so this result is no longer produced.
	ActivationFailed,
	AmbiguousTransition
};

UENUM(BlueprintType)
enum class ERZCombatComboExecutionState : uint8
{
	None,
	ResolvedPendingActivation,
	AbilityActivated,
	AbilityCommitted,
	AbilityFailed,
	AbilityEnded,
	AbilityCancelled,
	// Appended for serialization compatibility. Combo state advancement is not a GAS CommitAbility observation.
	ComboCommitted
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo")
	FGameplayTag TriggerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo")
	TObjectPtr<UObject> SourceObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo")
	TObjectPtr<UObject> PayloadObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo")
	FGameplayTagContainer ContextTags;
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboRequestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	ERZCombatComboRequestResultType Result = ERZCombatComboRequestResultType::NoActiveTable;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTableAsset> SelectedTable = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId SourceNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId TargetNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag TriggerTag;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboPayload> Payload = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTransitionChecker> Checker = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bResolved = false;
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboDebugSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTableAsset> ActiveTable = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId CurrentNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TArray<FRZCombatComboNodeId> AvailableTargetNodeIds;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TSubclassOf<UGameplayAbility> CurrentAbilityClass;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag LastTriggerTag;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	ERZCombatComboRequestResultType LastResult = ERZCombatComboRequestResultType::NoActiveTable;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId LastSourceNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId LastTargetNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId PendingSourceNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId PendingTargetNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	ERZCombatComboExecutionState ExecutionState = ERZCombatComboExecutionState::None;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TSubclassOf<UGameplayAbility> ObservedAbilityClass;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayAbilitySpecHandle ObservedAbilitySpecHandle;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTagContainer ObservedFailureTags;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bObservedAbilityWasCancelled = false;
};
