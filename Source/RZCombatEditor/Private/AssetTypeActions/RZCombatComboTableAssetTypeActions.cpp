#include "AssetTypeActions/RZCombatComboTableAssetTypeActions.h"

#include "ComboGraph/RZCombatComboTableEditor.h"
#include "RZCombatComboTableAsset.h"
#include "Toolkits/IToolkit.h"

#define LOCTEXT_NAMESPACE "RZCombatComboTableAssetTypeActions"

FText FRZCombatComboTableAssetTypeActions::GetName() const
{
	return LOCTEXT("RZCombatComboTableAssetName", "RZ Combat Combo Table");
}

FColor FRZCombatComboTableAssetTypeActions::GetTypeColor() const
{
	return FColor(180, 80, 40);
}

UClass* FRZCombatComboTableAssetTypeActions::GetSupportedClass() const
{
	return URZCombatComboTableAsset::StaticClass();
}

uint32 FRZCombatComboTableAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Gameplay;
}

void FRZCombatComboTableAssetTypeActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor
)
{
	const EToolkitMode::Type ToolkitMode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (URZCombatComboTableAsset* ComboTable = Cast<URZCombatComboTableAsset>(Object))
		{
			TSharedRef<FRZCombatComboTableEditor> Editor = MakeShared<FRZCombatComboTableEditor>();
			Editor->InitComboTableEditor(ToolkitMode, EditWithinLevelEditor, ComboTable);
		}
	}
}

#undef LOCTEXT_NAMESPACE
