#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IAssetTools;
class IAssetTypeActions;
struct FGraphPanelNodeFactory;
struct FGraphPanelPinConnectionFactory;

class FRZCombatEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;
	TSharedPtr<FGraphPanelNodeFactory> ComboGraphNodeFactory;
	TSharedPtr<FGraphPanelPinConnectionFactory> ComboGraphConnectionFactory;
};
