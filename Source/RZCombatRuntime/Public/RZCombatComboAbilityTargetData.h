#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "RZCombatComboTableTypes.h"
#include "RZCombatComboAbilityTargetData.generated.h"

class UPackageMap;

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboActivationContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboRequestResult RequestResult;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<const UObject> SourceObject = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<const UObject> RequestPayloadObject = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboPayload> Payload = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTagContainer ContextTags;
};

USTRUCT()
struct RZCOMBATRUNTIME_API FRZCombatComboAbilityTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FRZCombatComboActivationContext ActivationContext;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	virtual FString ToString() const override;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FRZCombatComboAbilityTargetData> : public TStructOpsTypeTraitsBase2<FRZCombatComboAbilityTargetData>
{
	enum
	{
		WithNetSerializer = true
	};
};

namespace RZCombatComboTargetData
{
	RZCOMBATRUNTIME_API FGameplayAbilityTargetDataHandle MakeTargetDataHandle(const FRZCombatComboActivationContext& Context);
	RZCOMBATRUNTIME_API const FRZCombatComboAbilityTargetData* FindTargetData(const FGameplayAbilityTargetDataHandle& TargetData);
	RZCOMBATRUNTIME_API const FRZCombatComboActivationContext* FindActivationContext(const FGameplayAbilityTargetDataHandle& TargetData);
}
