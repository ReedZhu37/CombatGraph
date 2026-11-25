#include "RZCombatComboTableEditor.h"

#include "GraphEditor.h"
#include "IDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "RZCombatComboGraph.h"
#include "RZCombatComboGraphCompiler.h"
#include "RZCombatComboGraphNodes.h"
#include "RZCombatComboGraphRuntimeDebug.h"
#include "RZCombatComboComponent.h"
#include "RZCombatComboTableAsset.h"
#include "AbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphUtilities.h"
#include "GameFramework/Actor.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "Kismet2/DebuggerCommands.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Selection.h"
#include "SGraphPanel.h"
#include "ToolMenus.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "RZCombatComboTableEditor"

namespace
{
FString CompilerMessageToString(const FText& Message)
{
	return Message.ToString();
}

FString CompilerMessageToString(const FString& Message)
{
	return Message;
}

FString CompilerMessageToString(const FName& Message)
{
	return Message.ToString();
}

template <typename MessageType>
FString CompilerMessageToString(const MessageType& Message)
{
	return LexToString(Message);
}

template <typename MessageArrayType>
void AppendCompilerMessages(FString& OutMessage, const TCHAR* Heading, const MessageArrayType& Messages)
{
	if (Messages.Num() == 0)
	{
		return;
	}

	OutMessage += FString::Printf(TEXT("\n%s:\n"), Heading);
	for (const auto& Message : Messages)
	{
		OutMessage += FString::Printf(TEXT("- %s\n"), *CompilerMessageToString(Message));
	}
}

void ShowCompileNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
{
	FNotificationInfo Info(Message);
	Info.bFireAndForget = true;
	Info.ExpireDuration = 4.0f;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(CompletionState);
	}
}

bool CanDeleteComboGraphNode(const UEdGraphNode* Node)
{
	return Node && Node->CanUserDeleteNode() && !Node->IsA<URZCombatComboGraphNode_Entry>();
}

bool CanCopyComboGraphNode(const UEdGraphNode* Node)
{
	return Node
		&& (Node->IsA<URZCombatComboGraphNode_Combo>() || Node->IsA<UEdGraphNode_Comment>())
		&& Node->CanDuplicateNode();
}

bool ClipboardContainsCopyableComboGraphNode(const FString& ClipboardContent)
{
	return ClipboardContent.Contains(URZCombatComboGraphNode_Combo::StaticClass()->GetName())
		|| ClipboardContent.Contains(UEdGraphNode_Comment::StaticClass()->GetName());
}

FString DebugNodeName(const FRZCombatComboNodeId& NodeId)
{
	return !NodeId.IsValid() ? TEXT("Entry / None") : !NodeId.Name.IsNone()
		? NodeId.Name.ToString() : NodeId.Guid.ToString(EGuidFormats::Short);
}

const TCHAR* DebugEventName(ERZCombatComboDebugEventType Type)
{
	switch (Type)
	{
	case ERZCombatComboDebugEventType::Resolve: return TEXT("Resolve");
	case ERZCombatComboDebugEventType::ComboCommit: return TEXT("Combo Commit (state)");
	case ERZCombatComboDebugEventType::AbilityActivated: return TEXT("GA Activated");
	case ERZCombatComboDebugEventType::AbilityCommitted: return TEXT("GA Commit (observed)");
	case ERZCombatComboDebugEventType::AbilityFailed: return TEXT("GA Failed");
	case ERZCombatComboDebugEventType::AbilityEnded: return TEXT("GA Ended");
	case ERZCombatComboDebugEventType::AbilityCancelled: return TEXT("GA Cancelled");
	case ERZCombatComboDebugEventType::Reset: return TEXT("Reset");
	case ERZCombatComboDebugEventType::TableChanged: return TEXT("Table Changed");
	default: return TEXT("Unknown");
	}
}
}

FName RZCombatComboTableEditorCompileStatusOverlayName(ERZCombatComboTableEditorCompileStatus Status)
{
	switch (Status)
	{
	case ERZCombatComboTableEditorCompileStatus::Succeeded:
		return FName(TEXT("Blueprint.CompileStatus.Overlay.Good"));
	case ERZCombatComboTableEditorCompileStatus::Failed:
		return FName(TEXT("Blueprint.CompileStatus.Overlay.Error"));
	case ERZCombatComboTableEditorCompileStatus::Unknown:
	default:
		return FName(TEXT("Blueprint.CompileStatus.Overlay.Unknown"));
	}
}

const FName FRZCombatComboTableEditor::AppIdentifier(TEXT("RZCombatComboTableEditorApp"));
const FName FRZCombatComboTableEditor::GraphTabId(TEXT("RZCombatComboTableEditor_Graph"));
const FName FRZCombatComboTableEditor::DetailsTabId(TEXT("RZCombatComboTableEditor_Details"));
const FName FRZCombatComboTableEditor::RuntimeDebugTabId(TEXT("RZCombatComboTableEditor_RuntimeDebug"));

FRZCombatComboTableEditor::~FRZCombatComboTableEditor()
{
	UnbindRuntimeDebugDelegates();
	RuntimeDebugger.EndSession();
	if (ComboGraph)
	{
		ComboGraph->ClearRuntimeDebugSnapshot();
	}

	if (ComboGraph && GraphChangedDelegateHandle.IsValid())
	{
		ComboGraph->RemoveOnGraphChangedHandler(GraphChangedDelegateHandle);
		GraphChangedDelegateHandle.Reset();
	}

	GraphEditorWidget.Reset();
	DetailsView.Reset();
	ComboGraph = nullptr;
	ComboTable = nullptr;
}

void FRZCombatComboTableEditor::InitComboTableEditor(
	EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	URZCombatComboTableAsset* InComboTable
)
{
	check(InComboTable);

	ComboTable = InComboTable;
	ComboTable->SetFlags(RF_Transactional);
	ComboGraph = FRZCombatComboGraphCompiler::EnsureGraph(ComboTable);
	if (ComboGraph)
	{
		ComboGraph->SetFlags(RF_Transactional);
		GraphChangedDelegateHandle = ComboGraph->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateSP(this, &FRZCombatComboTableEditor::HandleGraphChanged)
		);
	}

	CreateDetailsView();
	CreateGraphEditorWidget();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("RZCombatComboTableEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.72f)
					->Split(FTabManager::NewStack()->SetSizeCoefficient(0.70f)->AddTab(GraphTabId, ETabState::OpenedTab))
					->Split(FTabManager::NewStack()->SetSizeCoefficient(0.30f)->AddTab(RuntimeDebugTabId, ETabState::OpenedTab))
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.28f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
			)
		);

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		AppIdentifier,
		StandaloneDefaultLayout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		ComboTable
	);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
	BindRuntimeDebugDelegates();
	RefreshRuntimeDebugSnapshot();
}

void FRZCombatComboTableEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_ComboTableEditor", "Combo Table Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FRZCombatComboTableEditor::SpawnGraphTab))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FRZCombatComboTableEditor::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(RuntimeDebugTabId, FOnSpawnTab::CreateSP(this, &FRZCombatComboTableEditor::SpawnRuntimeDebugTab))
		.SetDisplayName(LOCTEXT("RuntimeDebugTab", "Runtime Debug"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(GetRuntimeDebugToolbarIcon());
}

void FRZCombatComboTableEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(RuntimeDebugTabId);
}

FName FRZCombatComboTableEditor::GetToolkitFName() const
{
	return FName(TEXT("RZCombatComboTableEditor"));
}

FText FRZCombatComboTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Combo Table Editor");
}

FString FRZCombatComboTableEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("Combo Table");
}

FLinearColor FRZCombatComboTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.16f, 0.10f, 0.32f, 0.5f);
}

void FRZCombatComboTableEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ComboTable);
	Collector.AddReferencedObject(ComboGraph);
}

FString FRZCombatComboTableEditor::GetReferencerName() const
{
	return TEXT("FRZCombatComboTableEditor");
}

void FRZCombatComboTableEditor::CreateGraphEditorWidget()
{
	if (!ComboGraph)
	{
		GraphEditorWidget.Reset();
		return;
	}

	BindGraphCommands();

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("GraphCornerText", "Combo Graph");

	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FRZCombatComboTableEditor::HandleGraphSelectionChanged);

	SAssignNew(GraphEditorWidget, SGraphEditor)
		.AdditionalCommands(GetToolkitCommands())
		.Appearance(AppearanceInfo)
		.GraphToEdit(ComboGraph)
		.GraphEvents(GraphEvents)
		.ShowGraphStateOverlay(false);
}

void FRZCombatComboTableEditor::CreateDetailsView()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bUpdatesFromSelection = false;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(ComboTable);
	}
}

void FRZCombatComboTableEditor::ExtendToolbar()
{
	// Use the engine's actual play toolbar, command routing, mode menu and global PIE visibility rules.
	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(GetToolMenuToolbarName());
	if (!ToolbarMenu->FindSection("RZCombatPlay"))
	{
		FToolMenuSection& PlaySection = ToolbarMenu->AddSection("RZCombatPlay", FText::GetEmpty(), FToolMenuInsert("Asset", EToolMenuInsertType::After));
		FPlayWorldCommands::BuildToolbar(PlaySection);
	}

	TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FRZCombatComboTableEditor::FillToolbar)
	);

	AddToolbarExtender(ToolbarExtender);
}

void FRZCombatComboTableEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("RZCombatComboTable");
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CompileComboGraph),
				FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CanCompileComboGraph)
			),
			NAME_None,
			LOCTEXT("CompileButtonLabel", "Compile"),
			LOCTEXT("CompileButtonTooltip", "Compile the combo graph into runtime combo table data."),
			GetCompileToolbarIcon()
		);

		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::ToggleRuntimeDebug),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FRZCombatComboTableEditor::IsRuntimeDebugEnabled)
			),
			NAME_None,
			LOCTEXT("RuntimeDebugButtonLabel", "PIE Debug"),
			LOCTEXT("RuntimeDebugButtonTooltip", "Observe a locked PIE / Simulate instance using this table. Play selects automatically; Debug Object switches instances. Never changes gameplay policies."),
			GetRuntimeDebugToolbarIcon(),
			EUserInterfaceActionType::ToggleButton
		);

		ToolbarBuilder.AddWidget(
			SNew(SComboButton)
			.OnGetMenuContent(this, &FRZCombatComboTableEditor::GetDebugObjectMenu)
			.ToolTipText(this, &FRZCombatComboTableEditor::GetDebugObjectTooltipText)
			.ButtonContent()
			[
				SNew(SBox).MaxDesiredWidth(340.0f)
				[
					SNew(STextBlock)
					.Text(this, &FRZCombatComboTableEditor::GetDebugObjectText)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
			]
		);
	}
	ToolbarBuilder.EndSection();
}

FSlateIcon FRZCombatComboTableEditor::GetCompileToolbarIcon() const
{
	static const FName CompileBackground(TEXT("Blueprint.CompileStatus.Background"));
	return FSlateIcon(
		FAppStyle::GetAppStyleSetName(),
		CompileBackground,
		NAME_None,
		RZCombatComboTableEditorCompileStatusOverlayName(CompileStatus)
	);
}

FSlateIcon FRZCombatComboTableEditor::GetRuntimeDebugToolbarIcon() const
{
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Visible"));
}

void FRZCombatComboTableEditor::BindGraphCommands()
{
	const TSharedRef<FUICommandList> CommandList = GetToolkitCommands();
	const FGenericCommands& GenericCommands = FGenericCommands::Get();
	const FGraphEditorCommandsImpl& GraphEditorCommands = FGraphEditorCommands::Get();

	CommandList->MapAction(
		GenericCommands.Copy,
		FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CopySelectedNodes),
		FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CanCopySelectedNodes)
	);

	CommandList->MapAction(
		GenericCommands.Cut,
		FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CutSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CanCutSelectedNodes)
	);

	CommandList->MapAction(
		GenericCommands.Paste,
		FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::PasteNodes),
		FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CanPasteNodes)
	);

	CommandList->MapAction(
		GenericCommands.Duplicate,
		FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::DuplicateSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CanDuplicateSelectedNodes)
	);

	CommandList->MapAction(
		GenericCommands.Delete,
		FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CanDeleteSelectedNodes)
	);

	CommandList->MapAction(
		GraphEditorCommands.DeleteAndReconnectNodes,
		FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::CanDeleteSelectedNodes)
	);

	CommandList->MapAction(
		GenericCommands.SelectAll,
		FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::SelectAllNodes)
	);
}

void FRZCombatComboTableEditor::CompileComboGraph()
{
	if (!ComboTable)
	{
		return;
	}

	ComboTable->Modify();
	if (ComboGraph)
	{
		ComboGraph->Modify();
	}

	const auto CompileResult = FRZCombatComboGraphCompiler::Compile(ComboTable);
	if (CompileResult.bSucceeded)
	{
		SetCompileStatus(ERZCombatComboTableEditorCompileStatus::Succeeded);
		ComboTable->MarkPackageDirty();

		const int32 WarningCount = CompileResult.Warnings.Num();
		if (WarningCount > 0)
		{
			ShowCompileNotification(
				FText::Format(
					LOCTEXT("CompileSucceededWithWarnings", "Combo graph compiled with {0} warning(s)."),
					FText::AsNumber(WarningCount)
				),
				SNotificationItem::CS_Success
			);
		}
		else
		{
			ShowCompileNotification(LOCTEXT("CompileSucceeded", "Combo graph compiled successfully."), SNotificationItem::CS_Success);
		}

		return;
	}

	FString FailureMessage = LOCTEXT("CompileFailed", "Combo graph compile failed.").ToString();
	AppendCompilerMessages(FailureMessage, TEXT("Errors"), CompileResult.Errors);
	AppendCompilerMessages(FailureMessage, TEXT("Warnings"), CompileResult.Warnings);

	const FText FailureText = FText::FromString(FailureMessage);
	SetCompileStatus(ERZCombatComboTableEditorCompileStatus::Failed);
	ShowCompileNotification(LOCTEXT("CompileFailedNotification", "Combo graph compile failed."), SNotificationItem::CS_Fail);
	FMessageDialog::Open(EAppMsgType::Ok, FailureText);
}

bool FRZCombatComboTableEditor::CanCompileComboGraph() const
{
	return ComboTable != nullptr;
}

void FRZCombatComboTableEditor::ToggleRuntimeDebug()
{
	bRuntimeDebugEnabled = !bRuntimeDebugEnabled;
	RefreshRuntimeDebugSnapshot();

	RegenerateMenusAndToolbars();
}

bool FRZCombatComboTableEditor::IsRuntimeDebugEnabled() const
{
	return bRuntimeDebugEnabled;
}

void FRZCombatComboTableEditor::SetCompileStatus(ERZCombatComboTableEditorCompileStatus NewStatus)
{
	if (CompileStatus == NewStatus)
	{
		return;
	}

	CompileStatus = NewStatus;
	RegenerateMenusAndToolbars();
}

void FRZCombatComboTableEditor::HandleGraphChanged(const FEdGraphEditAction& EditAction)
{
	SetCompileStatus(ERZCombatComboTableEditorCompileStatus::Unknown);
}

void FRZCombatComboTableEditor::CopySelectedNodes()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	TSet<UObject*> NodesToCopy;

	for (UObject* SelectedObject : SelectedNodes)
	{
		UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(SelectedObject);
		if (CanCopyComboGraphNode(SelectedNode))
		{
			NodesToCopy.Add(SelectedNode);
		}
	}

	if (NodesToCopy.Num() == 0)
	{
		return;
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FRZCombatComboTableEditor::CanCopySelectedNodes() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	for (UObject* SelectedObject : SelectedNodes)
	{
		const UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(SelectedObject);
		if (CanCopyComboGraphNode(SelectedNode))
		{
			return true;
		}
	}

	return false;
}

void FRZCombatComboTableEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedNodes();
}

bool FRZCombatComboTableEditor::CanCutSelectedNodes() const
{
	return CanCopySelectedNodes() && CanDeleteSelectedNodes();
}

void FRZCombatComboTableEditor::PasteNodes()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	PasteNodesHere(GraphEditorWidget->GetPasteLocation2f());
}

void FRZCombatComboTableEditor::PasteNodesHere(const FVector2D& Location)
{
	if (!ComboGraph || !GraphEditorWidget.IsValid())
	{
		return;
	}

	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);
	if (TextToImport.IsEmpty())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	ComboGraph->Modify();
	if (ComboTable)
	{
		ComboTable->Modify();
	}

	GraphEditorWidget->ClearSelectionSet();

	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(ComboGraph, TextToImport, PastedNodes);

	if (PastedNodes.Num() == 0)
	{
		return;
	}

	FVector2D AverageNodePosition(0.0, 0.0);
	int32 AverageNodeCount = 0;
	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		if (CanCopyComboGraphNode(PastedNode))
		{
			AverageNodePosition.X += PastedNode->NodePosX;
			AverageNodePosition.Y += PastedNode->NodePosY;
			++AverageNodeCount;
		}
	}

	if (AverageNodeCount > 0)
	{
		const double InvAverageNodeCount = 1.0 / static_cast<double>(AverageNodeCount);
		AverageNodePosition.X *= InvAverageNodeCount;
		AverageNodePosition.Y *= InvAverageNodeCount;
	}

	bool bPastedAnyNode = false;
	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		if (!PastedNode)
		{
			continue;
		}

		if (!CanCopyComboGraphNode(PastedNode))
		{
			PastedNode->DestroyNode();
			continue;
		}

		PastedNode->SetFlags(RF_Transactional);
		PastedNode->Modify();

		const double NewNodePosX = (PastedNode->NodePosX - AverageNodePosition.X) + Location.X;
		const double NewNodePosY = (PastedNode->NodePosY - AverageNodePosition.Y) + Location.Y;
		PastedNode->NodePosX = static_cast<int32>(NewNodePosX);
		PastedNode->NodePosY = static_cast<int32>(NewNodePosY);
		PastedNode->SnapToGrid(16);

		PastedNode->CreateNewGuid();
		PastedNode->PostPasteNode();
		GraphEditorWidget->SetNodeSelection(PastedNode, true);
		bPastedAnyNode = true;
	}

	if (bPastedAnyNode)
	{
		ComboGraph->NotifyGraphChanged();
		if (ComboTable)
		{
			ComboTable->MarkPackageDirty();
		}
	}
}

bool FRZCombatComboTableEditor::CanPasteNodes() const
{
	if (!ComboGraph || !GraphEditorWidget.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return ClipboardContainsCopyableComboGraphNode(ClipboardContent)
		&& FEdGraphUtilities::CanImportNodesFromText(ComboGraph, ClipboardContent);
}

void FRZCombatComboTableEditor::DuplicateSelectedNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FRZCombatComboTableEditor::CanDuplicateSelectedNodes() const
{
	return CanCopySelectedNodes();
}

void FRZCombatComboTableEditor::DeleteSelectedNodes()
{
	if (!ComboGraph || !GraphEditorWidget.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteSelectedNodes", "Delete Combo Graph Node"));
	ComboGraph->Modify();
	GraphEditorWidget->ClearSelectionSet();

	bool bDeletedAnyNode = false;
	for (UObject* SelectedObject : SelectedNodes)
	{
		UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(SelectedObject);
		if (!CanDeleteComboGraphNode(SelectedNode))
		{
			continue;
		}

		SelectedNode->Modify();
		SelectedNode->DestroyNode();
		bDeletedAnyNode = true;
	}

	if (bDeletedAnyNode)
	{
		ComboGraph->NotifyGraphChanged();
		if (ComboTable)
		{
			ComboTable->MarkPackageDirty();
		}
	}
}

bool FRZCombatComboTableEditor::CanDeleteSelectedNodes() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	for (UObject* SelectedObject : SelectedNodes)
	{
		const UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(SelectedObject);
		if (CanDeleteComboGraphNode(SelectedNode))
		{
			return true;
		}
	}

	return false;
}

void FRZCombatComboTableEditor::SelectAllNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->SelectAllNodes();
	}
}

void FRZCombatComboTableEditor::HandleGraphSelectionChanged(const TSet<UObject*>& NewSelection)
{
	if (!DetailsView.IsValid())
	{
		return;
	}

	if (NewSelection.Num() == 0)
	{
		DetailsView->SetObject(ComboTable);
		return;
	}

	TArray<UObject*> SelectedObjects;
	SelectedObjects.Reserve(NewSelection.Num());
	for (UObject* SelectedObject : NewSelection)
	{
		if (SelectedObject)
		{
			SelectedObjects.Add(SelectedObject);
		}
	}

	if (SelectedObjects.Num() == 0)
	{
		DetailsView->SetObject(ComboTable);
	}
	else if (SelectedObjects.Num() == 1)
	{
		DetailsView->SetObject(SelectedObjects[0]);
	}
	else
	{
		DetailsView->SetObjects(SelectedObjects);
	}
}

void FRZCombatComboTableEditor::BindRuntimeDebugDelegates()
{
	if (!SelectionChangedDelegateHandle.IsValid())
	{
		SelectionChangedDelegateHandle = USelection::SelectionChangedEvent.AddRaw(
			this,
			&FRZCombatComboTableEditor::HandleEditorSelectionChanged
		);
	}

	if (!BeginPIEDelegateHandle.IsValid())
	{
		BeginPIEDelegateHandle = FEditorDelegates::BeginPIE.AddRaw(
			this,
			&FRZCombatComboTableEditor::HandleBeginPIE
		);
	}

	if (!EndPIEDelegateHandle.IsValid())
	{
		EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddRaw(
			this,
			&FRZCombatComboTableEditor::HandleEndPIE
		);
	}

	if (!RuntimeDebugTickerHandle.IsValid())
	{
		RuntimeDebugTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FRZCombatComboTableEditor::TickRuntimeDebug),
			0.10f
		);
	}
}

void FRZCombatComboTableEditor::UnbindRuntimeDebugDelegates()
{
	if (SelectionChangedDelegateHandle.IsValid())
	{
		USelection::SelectionChangedEvent.Remove(SelectionChangedDelegateHandle);
		SelectionChangedDelegateHandle.Reset();
	}

	if (BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(BeginPIEDelegateHandle);
		BeginPIEDelegateHandle.Reset();
	}

	if (EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
		EndPIEDelegateHandle.Reset();
	}

	if (RuntimeDebugTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RuntimeDebugTickerHandle);
		RuntimeDebugTickerHandle.Reset();
	}
}

void FRZCombatComboTableEditor::HandleEditorSelectionChanged(UObject* NewSelection)
{
	(void)NewSelection;
	RefreshRuntimeDebugSnapshot();
}

void FRZCombatComboTableEditor::HandleBeginPIE(bool bIsSimulating)
{
	(void)bIsSimulating;
	bPIEEnding = false;
	RuntimeDebugger.EndSession();
	RefreshRuntimeDebugSnapshot();
}

void FRZCombatComboTableEditor::HandleEndPIE(bool bIsSimulating)
{
	(void)bIsSimulating;
	// EndPIE is broadcast before the old worlds are fully torn down. Do not rediscover them on the next ticker.
	bPIEEnding = true;
	RuntimeDebugger.EndSession();
	RefreshRuntimeDebugSnapshot();
}

bool FRZCombatComboTableEditor::TickRuntimeDebug(float DeltaTime)
{
	(void)DeltaTime;
	RefreshRuntimeDebugSnapshot();
	return true;
}

void FRZCombatComboTableEditor::RefreshRuntimeDebugSnapshot()
{
	const TArray<UWorld*> PlayWorlds = bPIEEnding ? TArray<UWorld*>() : RZCombatComboDebugger::GetPlayWorlds();
	RuntimeDebugger.Update(ComboTable, PlayWorlds, RZCombatComboDebugger::GetEditorSelection(), bRuntimeDebugEnabled);
	const ERZCombatComboDebuggerState State = RuntimeDebugger.GetState();
	ApplyRuntimeDebugSnapshot(BuildRuntimeDebugSnapshot(
		State == ERZCombatComboDebuggerState::Live || State == ERZCombatComboDebuggerState::Paused ? RuntimeDebugger.GetTarget() : nullptr));
	RefreshRuntimeDebugText();
}

void FRZCombatComboTableEditor::ApplyRuntimeDebugSnapshot(const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebugSnapshot)
{
	const FString NewSignature = RZCombatComboGraphRuntimeDebug::MakeDebugSignature(RuntimeDebugSnapshot);
	if (RuntimeDebugSignature == NewSignature)
	{
		return;
	}

	RuntimeDebugSignature = NewSignature;
	if (ComboGraph)
	{
		if (RuntimeDebugSnapshot.bHasDebugTarget)
		{
			ComboGraph->SetRuntimeDebugSnapshot(RuntimeDebugSnapshot);
		}
		else
		{
			ComboGraph->ClearRuntimeDebugSnapshot();
		}
	}

	InvalidateGraphRuntimeDebugVisuals();
}

FRZCombatComboGraphRuntimeDebugSnapshot FRZCombatComboTableEditor::BuildRuntimeDebugSnapshot(URZCombatComboComponent* ComboComponent) const
{
	FRZCombatComboGraphRuntimeDebugSnapshot RuntimeDebugSnapshot;
	if (!IsValid(ComboComponent))
	{
		return RuntimeDebugSnapshot;
	}

	const FRZCombatComboDebugSnapshot ComponentSnapshot = ComboComponent->GetComboDebugSnapshot();
	RuntimeDebugSnapshot.bHasDebugTarget = true;
	RuntimeDebugSnapshot.ComboComponent = ComboComponent;
	RuntimeDebugSnapshot.ActiveTable = ComponentSnapshot.ActiveTable;
	RuntimeDebugSnapshot.CurrentNodeId = ComponentSnapshot.CurrentNodeId;
	RuntimeDebugSnapshot.PendingSourceNodeId = ComponentSnapshot.PendingSourceNodeId;
	RuntimeDebugSnapshot.PendingTargetNodeId = ComponentSnapshot.PendingTargetNodeId;
	RuntimeDebugSnapshot.LastSourceNodeId = ComponentSnapshot.LastSourceNodeId;
	RuntimeDebugSnapshot.LastTargetNodeId = ComponentSnapshot.LastTargetNodeId;
	RuntimeDebugSnapshot.AvailableTargetNodeIds = ComponentSnapshot.AvailableTargetNodeIds;
	RuntimeDebugSnapshot.LastTriggerTag = ComponentSnapshot.LastTriggerTag;
	RuntimeDebugSnapshot.LastResult = ComponentSnapshot.LastResult;
	RuntimeDebugSnapshot.ExecutionState = ComponentSnapshot.ExecutionState;
	RuntimeDebugSnapshot.bObservedAbilityWasCancelled = ComponentSnapshot.bObservedAbilityWasCancelled;

	RuntimeDebugSnapshot.DebugTargetLabel = RuntimeDebugger.GetTargetLabel();
	return RuntimeDebugSnapshot;
}

TSharedRef<SWidget> FRZCombatComboTableEditor::GetDebugObjectMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DebugObjectAuto", "Auto — reselect instance"),
		LOCTEXT("DebugObjectAutoTip", "Select the matching selected character/component, then a local player, then the first stable-sorted matching instance. Stay locked until reselected."),
		FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::ReturnToAutomaticDebugObject)));
	MenuBuilder.BeginSection("DebugObjects", LOCTEXT("DebugObjects", "Instances using this Combo Table"));
	const TArray<UWorld*> Worlds = bPIEEnding ? TArray<UWorld*>() : RZCombatComboDebugger::GetPlayWorlds();
	const TArray<FRZCombatComboDebugCandidate> Candidates = RZCombatComboDebugger::FindCandidates(
		ComboTable, Worlds, RZCombatComboDebugger::GetEditorSelection());
	for (const FRZCombatComboDebugCandidate& Candidate : Candidates)
	{
		const TWeakObjectPtr<URZCombatComboComponent> Component = Candidate.Component;
		MenuBuilder.AddMenuEntry(FText::FromString(Candidate.Label), FText::FromString(Candidate.SortKey), FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::SelectDebugObject, Component),
				FCanExecuteAction::CreateSP(this, &FRZCombatComboTableEditor::IsRuntimeDebugEnabled),
				FIsActionChecked::CreateLambda([WeakThis = TWeakPtr<FRZCombatComboTableEditor>(SharedThis(this)), Component]()
				{
					const TSharedPtr<FRZCombatComboTableEditor> Editor = WeakThis.Pin();
					return Editor && Component.IsValid() && Editor->RuntimeDebugger.GetTarget() == Component.Get();
				})), NAME_None, EUserInterfaceActionType::RadioButton);
	}
	if (Candidates.IsEmpty())
	{
		MenuBuilder.AddWidget(SNew(STextBlock).Text(LOCTEXT("NoDebugObjects", "No matching PIE / Simulate instance. Play the current level.")), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

FText FRZCombatComboTableEditor::GetDebugObjectText() const
{
	return RuntimeDebugger.HasChosenTarget() ? FText::FromString(RuntimeDebugger.GetTargetLabel())
		: LOCTEXT("DebugObjectWaitingLabel", "Debug Object: Auto (waiting for Play / instance)");
}

FText FRZCombatComboTableEditor::GetDebugObjectTooltipText() const
{
	return FText::Format(
		LOCTEXT("DebugObjectIdentityTooltip", "Debug Object ({0})\n{1}\n{2}\nSwitch the observed instance or return to Auto using this menu."),
		RuntimeDebugger.IsManual() ? LOCTEXT("DebugObjectManualMode", "Manual") : LOCTEXT("DebugObjectAutoMode", "Auto"),
		RuntimeDebugger.HasChosenTarget() ? FText::FromString(RuntimeDebugger.GetTargetLabel()) : LOCTEXT("DebugObjectNoTarget", "No target selected"),
		RuntimeDebugger.GetStateText());
}

void FRZCombatComboTableEditor::SelectDebugObject(TWeakObjectPtr<URZCombatComboComponent> Component)
{
	if (bRuntimeDebugEnabled && !bPIEEnding)
	{
		RuntimeDebugger.SelectTarget(Component.Get(), ComboTable, RZCombatComboDebugger::GetPlayWorlds());
		RefreshRuntimeDebugSnapshot();
	}
}

void FRZCombatComboTableEditor::ReturnToAutomaticDebugObject()
{
	RuntimeDebugger.ReturnToAuto();
	RefreshRuntimeDebugSnapshot();
}

void FRZCombatComboTableEditor::RefreshRuntimeDebugText()
{
	RuntimeDebugStatusText = RuntimeDebugger.GetStateText();
	FString Details = FString::Printf(TEXT("Debug Object (%s): %s"),
		RuntimeDebugger.IsManual() ? TEXT("Manual, locked") : TEXT("Auto, locked after selection"),
		RuntimeDebugger.HasChosenTarget() ? *RuntimeDebugger.GetTargetLabel() : TEXT("None"));
	if (URZCombatComboComponent* Component = bRuntimeDebugEnabled ? RuntimeDebugger.GetTarget() : nullptr)
	{
		const FRZCombatComboDebugSnapshot Snapshot = Component->GetComboDebugSnapshot();
		Details += FString::Printf(TEXT("\nActive Table: %s\nCurrent: %s | Pending: %s | State: %s\nLast Trigger: %s | Result: %s"),
			*GetPathNameSafe(Component->GetActiveComboTable()), *DebugNodeName(Snapshot.CurrentNodeId),
			*DebugNodeName(Snapshot.PendingTargetNodeId), *RZCombatComboGraphRuntimeDebug::GetExecutionStateText(Snapshot.ExecutionState).ToString(),
			*Snapshot.LastTriggerTag.ToString(), *StaticEnum<ERZCombatComboRequestResultType>()->GetNameStringByValue(static_cast<int64>(Snapshot.LastResult)));
		Details += FString::Printf(TEXT("\nGAS Observer: %s | ASC: %s | Bound: %s\nResolved / Observed GA: %s | Spec: %s | Failure Tags: %s"),
			Component->bObserveAbilitySystemLifecycle ? TEXT("Enabled") : TEXT("Disabled"), *GetPathNameSafe(Component->GetBoundAbilitySystemComponent()),
			Component->IsBoundToAbilitySystemLifecycle() ? TEXT("Yes") : TEXT("No"),
			*GetNameSafe(Snapshot.ObservedAbilityClass.Get()), *Snapshot.ObservedAbilitySpecHandle.ToString(), *Snapshot.ObservedFailureTags.ToStringSimple());
	}
	Details += TEXT("\nRead-only observation. Candidate highlights are structural only; Checkers have NOT been evaluated by the debugger.");
	RuntimeDebugDetailsText = FText::FromString(Details);

	FString History;
	const TArray<FRZCombatComboDebugEvent>& Events = RuntimeDebugger.GetRecentEvents();
	for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
	{
		const FRZCombatComboDebugEvent& Event = Events[Index];
		History += FString::Printf(TEXT("#%llu | frame %llu | %.3fs | %s | last %s -> %s | current %s | trigger %s | last result %s\n    Table: %s%s%s | GA: %s | Spec: %s%s%s\n"),
			Event.Sequence, Event.Frame, Event.WorldTime, DebugEventName(Event.Type), *DebugNodeName(Event.SourceNodeId), *DebugNodeName(Event.TargetNodeId),
			*DebugNodeName(Event.CurrentNodeId), *Event.TriggerTag.ToString(),
			*StaticEnum<ERZCombatComboRequestResultType>()->GetNameStringByValue(static_cast<int64>(Event.Result)),
			*Event.TablePath, Event.PreviousTablePath.IsEmpty() ? TEXT("") : TEXT(" | previous: "), *Event.PreviousTablePath,
			*Event.AbilityClassPath, *Event.AbilitySpecHandle.ToString(), Event.FailureTags.IsEmpty() ? TEXT("") : TEXT(" | failure: "), *Event.FailureTags.ToStringSimple());
	}
	RuntimeDebugHistoryText = Events.IsEmpty()
		? LOCTEXT("NoDebugEvents", "No recorded events for this target yet.") : FText::FromString(History);
}

void FRZCombatComboTableEditor::InvalidateGraphRuntimeDebugVisuals() const
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->Invalidate(EInvalidateWidgetReason::Paint);
		if (SGraphPanel* GraphPanel = GraphEditorWidget->GetGraphPanel())
		{
			GraphPanel->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
}

TSharedRef<SDockTab> FRZCombatComboTableEditor::SpawnGraphTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == GraphTabId);

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.Label(LOCTEXT("GraphTabTitle", "Graph"))
		.TabColorScale(GetTabColorScale());

	if (GraphEditorWidget.IsValid())
	{
		DockTab->SetContent(
			SNew(SBorder)
			.Padding(2.0f)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				GraphEditorWidget.ToSharedRef()
			]
		);
	}
	else
	{
		DockTab->SetContent(
			SNew(SBorder)
			.Padding(8.0f)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MissingGraphText", "Combo graph could not be created."))
			]
		);
	}

	return DockTab;
}

TSharedRef<SDockTab> FRZCombatComboTableEditor::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == DetailsTabId);

	const TSharedRef<SWidget> DetailsContent = DetailsView.IsValid()
		? StaticCastSharedRef<SWidget>(DetailsView.ToSharedRef())
		: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("MissingDetailsText", "Details view could not be created.")));

	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabTitle", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2.0f)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				DetailsContent
			]
		];
}

TSharedRef<SDockTab> FRZCombatComboTableEditor::SpawnRuntimeDebugTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == RuntimeDebugTabId);
	return SNew(SDockTab)
		.Label(LOCTEXT("RuntimeDebugTabTitle", "Runtime Debug"))
		[
			SNew(SBorder).Padding(8.0f).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[
					SNew(STextBlock).Text(this, &FRZCombatComboTableEditor::GetRuntimeDebugStatusText)
					.Font(FAppStyle::GetFontStyle("BoldFont")).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SSplitter)
					+ SSplitter::Slot().Value(0.42f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot().Padding(0, 0, 8, 0)
						[
							SNew(STextBlock).Text(this, &FRZCombatComboTableEditor::GetRuntimeDebugDetailsText).AutoWrapText(true)
						]
					]
					+ SSplitter::Slot().Value(0.58f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(LOCTEXT("DebugHistoryHeading", "Recent events — newest first, maximum 128 per instance (oldest events expire). Combo Commit is NOT a GAS Commit."))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 4, 0, 0)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(STextBlock).Text(this, &FRZCombatComboTableEditor::GetRuntimeDebugHistoryText).AutoWrapText(true)
							]
						]
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
