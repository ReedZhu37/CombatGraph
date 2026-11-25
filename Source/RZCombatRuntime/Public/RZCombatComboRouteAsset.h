#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RZCombatComboTypes.h"
#include "RZCombatComboRouteAsset.generated.h"

UCLASS(BlueprintType)
class RZCOMBATRUNTIME_API URZCombatComboRouteAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag RouteTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo", meta = (TitleProperty = "DisplayName"))
	TArray<FRZCombatComboEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo", meta = (TitleProperty = "DisplayName"))
	TArray<FRZCombatComboAbilityRoute> AbilityRoutes;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DesignerNote;
#endif

	const FRZCombatComboEntry* FindEntryByInputTag(FGameplayTag InputTag, const FGameplayTagContainer& ContextTags) const;
	const FRZCombatComboAbilityRoute* FindRouteBySourceAbilityTag(FGameplayTag SourceAbilityTag) const;
	const FRZCombatComboBranch* ResolveBranch(
		FGameplayTag SourceAbilityTag,
		FGameplayTag InputTag,
		const FGameplayTagContainer& ActiveWindowTags,
		const FGameplayTagContainer& ContextTags,
		bool bHasHitConfirm
	) const;
};
