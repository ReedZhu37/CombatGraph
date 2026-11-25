#include "RZCombatComboAbilityTargetData.h"

FString FRZCombatComboAbilityTargetData::ToString() const
{
	return FString::Printf(
		TEXT("RZCombatComboAbilityTargetData: Trigger=%s Target=%s Payload=%s"),
		*ActivationContext.RequestResult.TriggerTag.ToString(),
		*ActivationContext.RequestResult.TargetNodeId.Name.ToString(),
		ActivationContext.Payload ? *ActivationContext.Payload->GetName() : TEXT("None")
	);
}

bool FRZCombatComboAbilityTargetData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	(void)Ar;
	(void)Map;

	bOutSuccess = false;
	return false;
}

namespace RZCombatComboTargetData
{
	FGameplayAbilityTargetDataHandle MakeTargetDataHandle(const FRZCombatComboActivationContext& Context)
	{
		FRZCombatComboAbilityTargetData* TargetData = new FRZCombatComboAbilityTargetData();
		TargetData->ActivationContext = Context;
		return FGameplayAbilityTargetDataHandle(TargetData);
	}

	const FRZCombatComboAbilityTargetData* FindTargetData(const FGameplayAbilityTargetDataHandle& TargetData)
	{
		for (int32 Index = 0; Index < TargetData.Num(); ++Index)
		{
			const FGameplayAbilityTargetData* Data = TargetData.Get(Index);
			if (Data && Data->GetScriptStruct() == FRZCombatComboAbilityTargetData::StaticStruct())
			{
				return static_cast<const FRZCombatComboAbilityTargetData*>(Data);
			}
		}

		return nullptr;
	}

	const FRZCombatComboActivationContext* FindActivationContext(const FGameplayAbilityTargetDataHandle& TargetData)
	{
		const FRZCombatComboAbilityTargetData* ComboTargetData = FindTargetData(TargetData);
		return ComboTargetData ? &ComboTargetData->ActivationContext : nullptr;
	}
}
