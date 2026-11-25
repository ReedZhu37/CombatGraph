#include "RZCombatEditorModule.h"

#include "AssetToolsModule.h"
#include "AssetTypeActions/RZCombatComboTableAssetTypeActions.h"
#include "ComboGraph/RZCombatComboGraphVisuals.h"
#include "EdGraphUtilities.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"

void FRZCombatEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	RegisterAssetTypeAction(AssetTools, MakeShared<FRZCombatComboTableAssetTypeActions>());

	ComboGraphNodeFactory = MakeShared<FRZCombatComboGraphNodeFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(ComboGraphNodeFactory);

	ComboGraphConnectionFactory = MakeShared<FRZCombatComboGraphConnectionFactory>();
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(ComboGraphConnectionFactory);
}

void FRZCombatEditorModule::ShutdownModule()
{
	if (ComboGraphConnectionFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinConnectionFactory(ComboGraphConnectionFactory);
		ComboGraphConnectionFactory.Reset();
	}

	if (ComboGraphNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(ComboGraphNodeFactory);
		ComboGraphNodeFactory.Reset();
	}

	if (FAssetToolsModule::IsModuleLoaded())
	{
		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			if (Action.IsValid())
			{
				AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
			}
		}
	}

	RegisteredAssetTypeActions.Reset();
}

void FRZCombatEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

IMPLEMENT_MODULE(FRZCombatEditorModule, RZCombatEditor)
