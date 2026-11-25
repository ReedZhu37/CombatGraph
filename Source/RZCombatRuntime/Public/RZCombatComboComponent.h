#pragma once

#include "Components/ActorComponent.h"
#include "RZCombatComboTableTypes.h"
#include "RZCombatComboDebugEvent.h"
#include "RZCombatComboComponent.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;
class URZCombatComboTableAsset;
struct FAbilityEndedData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRZCombatComboRequestResolvedSignature, const FRZCombatComboRequestResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRZCombatComboStateChangedSignature, const FRZCombatComboDebugSnapshot&, Snapshot);

UCLASS(Blueprintable, BlueprintType, ClassGroup = (RZCombat), meta = (BlueprintSpawnableComponent))
class RZCOMBATRUNTIME_API URZCombatComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URZCombatComboComponent();

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	void AddComboTable(URZCombatComboTableAsset* Table);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	void RemoveComboTable(URZCombatComboTableAsset* Table);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	void SetActiveComboTable(URZCombatComboTableAsset* Table);

	UFUNCTION(BlueprintPure, Category = "RZCombat|Combo")
	URZCombatComboTableAsset* GetActiveComboTable() const;

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	void ResetCombo();

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	FRZCombatComboRequestResult RequestCombo(FGameplayTag TriggerTag);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	FRZCombatComboRequestResult RequestComboWithContext(const FRZCombatComboRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	bool CommitResolvedCombo(const FRZCombatComboRequestResult& ResolvedResult);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo|GAS")
	bool BindToAbilitySystemLifecycle();

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo|GAS")
	void UnbindFromAbilitySystemLifecycle();

	UFUNCTION(BlueprintPure, Category = "RZCombat|Combo|GAS")
	bool IsBoundToAbilitySystemLifecycle() const;

	UFUNCTION(BlueprintPure, Category = "RZCombat|Combo|GAS")
	UAbilitySystemComponent* GetBoundAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "RZCombat|Combo")
	FRZCombatComboRequestResult GetPendingResolvedCombo() const;

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	bool RecordResolvedAbilityActivated(
		const FRZCombatComboRequestResult& ResolvedResult,
		FGameplayAbilitySpecHandle AbilitySpecHandle,
		TSubclassOf<UGameplayAbility> AbilityClass
	);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	bool RecordResolvedAbilityCommitted(
		const FRZCombatComboRequestResult& ResolvedResult,
		FGameplayAbilitySpecHandle AbilitySpecHandle,
		TSubclassOf<UGameplayAbility> AbilityClass
	);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	bool RecordResolvedAbilityFailed(
		const FRZCombatComboRequestResult& ResolvedResult,
		FGameplayAbilitySpecHandle AbilitySpecHandle,
		TSubclassOf<UGameplayAbility> AbilityClass,
		const FGameplayTagContainer& FailureTags
	);

	UFUNCTION(BlueprintCallable, Category = "RZCombat|Combo")
	bool RecordResolvedAbilityEnded(
		const FRZCombatComboRequestResult& ResolvedResult,
		FGameplayAbilitySpecHandle AbilitySpecHandle,
		TSubclassOf<UGameplayAbility> AbilityClass,
		bool bWasCancelled,
		bool bResetComboState
	);

	UFUNCTION(BlueprintPure, Category = "RZCombat|Combo")
	FRZCombatComboNodeId GetCurrentNodeId() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "RZCombat|Combo")
	bool CanUseComboTransition(
		const FRZCombatComboRequest& Request,
		const FRZCombatComboNode& SourceNode,
		const FRZCombatComboTransition& Transition,
		const FRZCombatComboNode& TargetNode
	) const;

	UFUNCTION(BlueprintPure, Category = "RZCombat|Combo")
	FRZCombatComboDebugSnapshot GetComboDebugSnapshot() const;

	/** Oldest first, captured at mutation sites (including events occurring in the same frame).
	 * Non-shipping diagnostics only; reading never evaluates a transition or changes gameplay policy.
	 */
	const TArray<FRZCombatComboDebugEvent>& GetRecentComboDebugEvents() const { return RecentDebugEvents; }
	static constexpr int32 MaxRecentComboDebugEvents = 128;

	UPROPERTY(BlueprintAssignable, Category = "RZCombat|Combo")
	FRZCombatComboRequestResolvedSignature OnComboRequestResolved;

	UPROPERTY(BlueprintAssignable, Category = "RZCombat|Combo")
	FRZCombatComboStateChangedSignature OnComboStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TArray<TObjectPtr<URZCombatComboTableAsset>> ComboTables;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTableAsset> ActiveComboTable = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponentOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo|GAS")
	bool bObserveAbilitySystemLifecycle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo|GAS")
	bool bAutoBindAbilitySystemOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo|GAS")
	bool bCommitComboOnAbilityActivated = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo|GAS")
	bool bResetComboOnAbilityCancelled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RZCombat|Combo|GAS")
	bool bResetComboOnAbilityEnded = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNodeId CurrentNodeId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TSubclassOf<UGameplayAbility> CurrentAbilityClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboRequestResult LastRequestResult;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboRequestResult PendingResolvedResult;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	ERZCombatComboExecutionState ExecutionState = ERZCombatComboExecutionState::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TSubclassOf<UGameplayAbility> ObservedAbilityClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayAbilitySpecHandle ObservedAbilitySpecHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTagContainer ObservedFailureTags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bObservedAbilityWasCancelled = false;

private:
	void ResetComboState(bool bResetExecutionDebug);
	void RecordDebugEvent(ERZCombatComboDebugEventType Type, const FString& PreviousTablePath = FString());
	void ClearObservedExecutionDebug();
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	bool CachePendingAbilitySystemSpecForResult(const FRZCombatComboRequestResult& Result);
	FGameplayAbilitySpecHandle FindSpecHandleForAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const;
	bool DoesLifecycleAbilityMatchPending(
		const UGameplayAbility* Ability,
		FGameplayAbilitySpecHandle AbilitySpecHandle,
		bool bRequireSpecHandle
	) const;
	TSubclassOf<UGameplayAbility> ResolveObservedAbilityClass(const UGameplayAbility* Ability) const;
	FGameplayAbilitySpecHandle ResolveObservedSpecHandle(const UGameplayAbility* Ability) const;
	void ClearPendingAbilitySystemObservation();
	void HandleAbilityActivated(UGameplayAbility* Ability);
	void HandleAbilityCommitted(UGameplayAbility* Ability);
	void HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags);
	void HandleAbilityEnded(const FAbilityEndedData& AbilityEndedData);
	bool DoesResolvedResultMatchPending(const FRZCombatComboRequestResult& ResolvedResult) const;
	bool DoesObservedAbilityMatchPending(
		const FRZCombatComboRequestResult& ResolvedResult,
		FGameplayAbilitySpecHandle AbilitySpecHandle,
		TSubclassOf<UGameplayAbility> AbilityClass
	) const;
	void RecordObservedExecution(
		ERZCombatComboExecutionState NewExecutionState,
		FGameplayAbilitySpecHandle AbilitySpecHandle,
		TSubclassOf<UGameplayAbility> AbilityClass,
		const FGameplayTagContainer* FailureTags,
		bool bWasCancelled
	);

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	FGameplayAbilitySpecHandle PendingAbilitySpecHandle;
	FDelegateHandle AbilityActivatedDelegateHandle;
	FDelegateHandle AbilityCommittedDelegateHandle;
	FDelegateHandle AbilityFailedDelegateHandle;
	FDelegateHandle AbilityEndedDelegateHandle;
	bool bPendingComboCommitted = false;
	bool bAutomaticAbilitySystemBindingSuppressed = false;
	uint64 DebugEventSequence = 0;
	TArray<FRZCombatComboDebugEvent> RecentDebugEvents;
};
