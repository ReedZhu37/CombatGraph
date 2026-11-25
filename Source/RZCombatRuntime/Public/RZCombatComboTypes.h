#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "RZCombatComboTypes.generated.h"

UENUM(BlueprintType)
enum class ERZCombatComboBranchTargetType : uint8
{
	Ability,
	EndCombo,
	Custom
};

UENUM(BlueprintType)
enum class ERZCombatComboCommitPolicy : uint8
{
	Immediate,
	OnLinkPoint,
	OnWindowEnd,
	OnAbilityEnd
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag TargetAbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TSoftClassPtr<UObject> TargetAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTagQuery RequiredContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTagQuery BlockedContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DesignerNote;
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag RequiredInputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag RequiredWindowTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bRequireHitConfirm = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	ERZCombatComboBranchTargetType TargetType = ERZCombatComboBranchTargetType::Ability;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag TargetAbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TSoftClassPtr<UObject> TargetAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	ERZCombatComboCommitPolicy CommitPolicy = ERZCombatComboCommitPolicy::OnLinkPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bConsumeInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTagQuery RequiredContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTagQuery BlockedContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DesignerNote;
};

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboAbilityRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag SourceAbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TSoftClassPtr<UObject> SourceAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo", meta = (TitleProperty = "DisplayName"))
	TArray<FRZCombatComboBranch> Branches;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DesignerNote;
};
