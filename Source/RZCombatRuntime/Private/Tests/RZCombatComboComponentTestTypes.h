#pragma once

#include "Abilities/GameplayAbility.h"
#include "RZCombatComboTransitionChecker.h"

#include "RZCombatComboComponentTestTypes.generated.h"

class APawn;
class URZCombatComboComponent;

UCLASS()
class URZCombatComboStateChangedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 NotificationCount = 0;

	UPROPERTY(Transient)
	FRZCombatComboDebugSnapshot LastSnapshot;

	UFUNCTION()
	void HandleStateChanged(const FRZCombatComboDebugSnapshot& Snapshot)
	{
		++NotificationCount;
		LastSnapshot = Snapshot;
	}
};

UCLASS()
class URZCombatComboTransitionCheckerTestDouble : public URZCombatComboTransitionChecker
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bAllowTransition = true;

	static void ResetObservedContext();
	static bool HasObservedContext();
	static URZCombatComboComponent* GetObservedComboComponent();
	static APawn* GetObservedPawn();
	static FGameplayTag GetObservedTriggerTag();
	static FName GetObservedSourceNodeName();
	static FName GetObservedTargetNodeName();

	virtual bool CanUseTransition_Implementation(const FRZCombatComboTransitionCheckContext& Context) const override;

private:
	static bool bHasObservedContext;
	static TWeakObjectPtr<URZCombatComboComponent> ObservedComboComponent;
	static TWeakObjectPtr<APawn> ObservedPawn;
	static FGameplayTag ObservedTriggerTag;
	static FName ObservedSourceNodeName;
	static FName ObservedTargetNodeName;
};

UCLASS()
class URZCombatComboGASLifecycleTestAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URZCombatComboGASLifecycleTestAbility();

	void SetTestSpecHandle(FGameplayAbilitySpecHandle InHandle);
};

UCLASS()
class URZCombatComboGASLifecycleOtherAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URZCombatComboGASLifecycleOtherAbility();

	void SetTestSpecHandle(FGameplayAbilitySpecHandle InHandle);
};
