#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "ComboGraph/RZCombatComboDebugger.h"
#include "ComboGraph/RZCombatComboGraphRuntimeDebug.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"

class FSpawnTabArgs;
class FToolBarBuilder;
class IDetailsView;
class SGraphEditor;
class SDockTab;
struct FEdGraphEditAction;
class URZCombatComboGraph;
class URZCombatComboComponent;
class URZCombatComboTableAsset;

enum class ERZCombatComboTableEditorCompileStatus : uint8
{
	Unknown,
	Succeeded,
	Failed
};

FName RZCombatComboTableEditorCompileStatusOverlayName(ERZCombatComboTableEditorCompileStatus Status);

class FRZCombatComboTableEditor : public FAssetEditorToolkit, public FGCObject
{
public:
	virtual ~FRZCombatComboTableEditor() override;

	void InitComboTableEditor(
		EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		URZCombatComboTableAsset* InComboTable
	);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	void CreateGraphEditorWidget();
	void CreateDetailsView();
	void ExtendToolbar();
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	FSlateIcon GetCompileToolbarIcon() const;
	FSlateIcon GetRuntimeDebugToolbarIcon() const;
	void BindGraphCommands();
	void CompileComboGraph();
	bool CanCompileComboGraph() const;
	void ToggleRuntimeDebug();
	bool IsRuntimeDebugEnabled() const;
	void SetCompileStatus(ERZCombatComboTableEditorCompileStatus NewStatus);
	void HandleGraphChanged(const FEdGraphEditAction& EditAction);
	void CopySelectedNodes();
	bool CanCopySelectedNodes() const;
	void CutSelectedNodes();
	bool CanCutSelectedNodes() const;
	void PasteNodes();
	void PasteNodesHere(const FVector2D& Location);
	bool CanPasteNodes() const;
	void DuplicateSelectedNodes();
	bool CanDuplicateSelectedNodes() const;
	void DeleteSelectedNodes();
	bool CanDeleteSelectedNodes() const;
	void SelectAllNodes();
	void HandleGraphSelectionChanged(const TSet<UObject*>& NewSelection);
	void BindRuntimeDebugDelegates();
	void UnbindRuntimeDebugDelegates();
	void HandleEditorSelectionChanged(UObject* NewSelection);
	void HandleBeginPIE(bool bIsSimulating);
	void HandleEndPIE(bool bIsSimulating);
	bool TickRuntimeDebug(float DeltaTime);
	void RefreshRuntimeDebugSnapshot();
	void RefreshRuntimeDebugText();
	TSharedRef<SWidget> GetDebugObjectMenu();
	FText GetDebugObjectText() const;
	FText GetDebugObjectTooltipText() const;
	FText GetRuntimeDebugStatusText() const { return RuntimeDebugStatusText; }
	FText GetRuntimeDebugDetailsText() const { return RuntimeDebugDetailsText; }
	FText GetRuntimeDebugHistoryText() const { return RuntimeDebugHistoryText; }
	void SelectDebugObject(TWeakObjectPtr<URZCombatComboComponent> Component);
	void ReturnToAutomaticDebugObject();
	void ApplyRuntimeDebugSnapshot(const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebugSnapshot);
	FRZCombatComboGraphRuntimeDebugSnapshot BuildRuntimeDebugSnapshot(URZCombatComboComponent* ComboComponent) const;
	void InvalidateGraphRuntimeDebugVisuals() const;

	TSharedRef<SDockTab> SpawnGraphTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnRuntimeDebugTab(const FSpawnTabArgs& Args);

private:
	static const FName AppIdentifier;
	static const FName GraphTabId;
	static const FName DetailsTabId;
	static const FName RuntimeDebugTabId;

	TObjectPtr<URZCombatComboTableAsset> ComboTable = nullptr;
	TObjectPtr<URZCombatComboGraph> ComboGraph = nullptr;
	ERZCombatComboTableEditorCompileStatus CompileStatus = ERZCombatComboTableEditorCompileStatus::Unknown;
	FDelegateHandle GraphChangedDelegateHandle;
	FDelegateHandle SelectionChangedDelegateHandle;
	FDelegateHandle BeginPIEDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;
	FTSTicker::FDelegateHandle RuntimeDebugTickerHandle;
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<IDetailsView> DetailsView;
	bool bRuntimeDebugEnabled = true;
	bool bPIEEnding = false;
	FRZCombatComboDebuggerSession RuntimeDebugger;
	FText RuntimeDebugStatusText;
	FText RuntimeDebugDetailsText;
	FText RuntimeDebugHistoryText;
	FString RuntimeDebugSignature;
};
