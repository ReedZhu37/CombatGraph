#include "ComboGraph/RZCombatComboGraphVisuals.h"

#include "ComboGraph/RZCombatComboGraph.h"
#include "ComboGraph/RZCombatComboGraphNodes.h"
#include "ComboGraph/RZCombatComboGraphRuntimeDebug.h"
#include "ComboGraph/RZCombatComboGraphSchema.h"
#include "ConnectionDrawingPolicy.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/ArrangedWidget.h"
#include "Rendering/DrawElements.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"
#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RZCombatComboGraphVisuals"

class FRZCombatComboGraphStraightConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FRZCombatComboGraphStraightConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements
	)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	{
	}

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params) override
	{
		FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);
		Params.WireThickness = 3.25f;

		if (const URZCombatComboGraphNode_Transition* TransitionNode = InputPin ? Cast<URZCombatComboGraphNode_Transition>(InputPin->GetOwningNode()) : nullptr)
		{
			Params.WireColor = TransitionNode->bEnabled
				? FLinearColor::White
				: FLinearColor(0.38f, 0.38f, 0.38f, 0.75f);

			const URZCombatComboGraphNode_Combo* TargetNode = TransitionNode->GetNextComboNode();
			const URZCombatComboGraph* ComboGraph = TargetNode ? Cast<URZCombatComboGraph>(TargetNode->GetGraph()) : nullptr;
			const UEdGraphNode* SourceNode = TransitionNode->GetPreviousNode();
			const URZCombatComboGraphNode_Combo* SourceComboNode = Cast<URZCombatComboGraphNode_Combo>(SourceNode);
			const bool bSourceIsEntry = SourceNode && SourceNode->IsA<URZCombatComboGraphNode_Entry>();
			const FRZCombatComboNodeId SourceNodeId = SourceComboNode ? SourceComboNode->NodeId : FRZCombatComboNodeId();
			const ERZCombatComboGraphRuntimeNodeState TransitionState = ComboGraph && TransitionNode->bEnabled
				? ComboGraph->ResolveRuntimeDebugTransitionState(SourceNodeId, bSourceIsEntry, TargetNode->NodeId, TransitionNode->TriggerTag)
				: ERZCombatComboGraphRuntimeNodeState::None;
			if (TransitionState != ERZCombatComboGraphRuntimeNodeState::None)
			{
				Params.WireColor = RZCombatComboGraphRuntimeDebug::GetNodeBorderColor(TransitionState, false);
				Params.WireThickness = TransitionState == ERZCombatComboGraphRuntimeNodeState::Available ? 3.75f : 4.5f;
			}
		}
	}

	virtual void DetermineLinkGeometry(
		FArrangedChildren& ArrangedNodes,
		TSharedRef<SWidget>& OutputPinWidget,
		UEdGraphPin* OutputPin,
		UEdGraphPin* InputPin,
		FArrangedWidget*& StartWidgetGeometry,
		FArrangedWidget*& EndWidgetGeometry
	) override
	{
		if (const URZCombatComboGraphNode_Transition* TransitionNode = InputPin ? Cast<URZCombatComboGraphNode_Transition>(InputPin->GetOwningNode()) : nullptr)
		{
			UEdGraphNode* PreviousNode = TransitionNode->GetPreviousNode();
			URZCombatComboGraphNode_Combo* NextNode = TransitionNode->GetNextComboNode();
			int32* PreviousNodeIndex = PreviousNode ? NodeWidgetMap.Find(PreviousNode) : nullptr;
			int32* NextNodeIndex = NextNode ? NodeWidgetMap.Find(NextNode) : nullptr;
			FArrangedWidget* SourcePinGeometry = PinGeometries ? PinGeometries->Find(OutputPinWidget) : nullptr;
			FArrangedWidget* TargetPinGeometry = nullptr;
			if (UEdGraphPin* TargetInputPin = NextNode ? NextNode->GetInputPin() : nullptr)
			{
				if (const TSharedPtr<SGraphPin>* TargetPinWidget = PinToPinWidgetMap.Find(TargetInputPin))
				{
					TargetPinGeometry = TargetPinWidget->IsValid() && PinGeometries
						? PinGeometries->Find(TargetPinWidget->ToSharedRef())
						: nullptr;
				}
			}

			if (SourcePinGeometry && TargetPinGeometry)
			{
				StartWidgetGeometry = SourcePinGeometry;
				EndWidgetGeometry = TargetPinGeometry;
				return;
			}

			if (PreviousNodeIndex && NextNodeIndex)
			{
				StartWidgetGeometry = &ArrangedNodes[*PreviousNodeIndex];
				EndWidgetGeometry = &ArrangedNodes[*NextNodeIndex];
				return;
			}
		}

		FConnectionDrawingPolicy::DetermineLinkGeometry(
			ArrangedNodes,
			OutputPinWidget,
			OutputPin,
			InputPin,
			StartWidgetGeometry,
			EndWidgetGeometry
		);
	}

	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes) override
	{
		NodeWidgetMap.Reset();
		for (int32 NodeIndex = 0; NodeIndex < ArrangedNodes.Num(); ++NodeIndex)
		{
			const TSharedRef<SGraphNode> GraphNodeWidget = StaticCastSharedRef<SGraphNode>(ArrangedNodes[NodeIndex].Widget);
			if (UEdGraphNode* GraphNode = GraphNodeWidget->GetNodeObj())
			{
				NodeWidgetMap.Add(GraphNode, NodeIndex);
			}
		}

		FConnectionDrawingPolicy::Draw(InPinGeometries, ArrangedNodes);
	}

	virtual FVector2f ComputeSplineTangent(const FVector2f& Start, const FVector2f& End) const override
	{
		const FVector2f Tangent = End - Start;
		return Tangent.IsNearlyZero() ? FVector2f(1.0f, 0.0f) : Tangent;
	}

	virtual void DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params) override
	{
		const FVector2f StartPoint = FGeometryHelper::CenterOf(StartGeom);
		const FVector2f EndPoint = FGeometryHelper::CenterOf(EndGeom);
		DrawSplineWithArrow(StartPoint, EndPoint, Params);
	}

	virtual void DrawSplineWithArrow(const FVector2f& StartPoint, const FVector2f& EndPoint, const FConnectionParams& Params) override
	{
		DrawConnection(WireLayerID, StartPoint, EndPoint, Params);

		if (!ArrowImage)
		{
			return;
		}

		FVector2f Direction = EndPoint - StartPoint;
		if (Direction.IsNearlyZero())
		{
			Direction = FVector2f(1.0f, 0.0f);
		}
		else
		{
			Direction.Normalize();
		}

		const FVector2f ArrowCenter = EndPoint - (Direction * ArrowRadius.X);
		const FVector2f ArrowPoint = ArrowCenter - ArrowRadius;
		const float AngleInRadians = FMath::Atan2(Direction.Y, Direction.X);

		FSlateDrawElement::MakeRotatedBox(
			DrawElementsList,
			ArrowLayerID,
			FPaintGeometry(ArrowPoint, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
			ArrowImage,
			ESlateDrawEffect::None,
			AngleInRadians,
			TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement,
			Params.WireColor
		);
	}

private:
	TMap<UEdGraphNode*, int32> NodeWidgetMap;
};

class SRZCombatComboGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SRZCombatComboGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
	{
		SetCursor(EMouseCursor::Default);
		bShowLabel = false;
		GraphPinObj = InPin;
		check(GraphPinObj);

		SBorder::Construct(
			SBorder::FArguments()
			.BorderImage(this, &SRZCombatComboGraphPin::GetComboPinBorder)
			.BorderBackgroundColor(this, &SRZCombatComboGraphPin::GetPinColor)
			.OnMouseButtonDown(this, &SRZCombatComboGraphPin::OnPinMouseDown)
			.Cursor(this, &SRZCombatComboGraphPin::GetPinCursor)
			.Padding(FMargin(8.0f, 5.0f))
		);
	}

protected:
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override
	{
		return SNullWidget::NullWidget;
	}

	virtual FSlateColor GetPinColor() const override
	{
		return FSlateColor(IsHovered() ? FLinearColor(0.95f, 0.72f, 0.24f) : FLinearColor(0.05f, 0.05f, 0.06f));
	}

private:
	const FSlateBrush* GetComboPinBorder() const
	{
		return FAppStyle::GetBrush(TEXT("Graph.StateNode.Body"));
	}
};

class SRZCombatComboGraphNode_EntryOrCombo : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SRZCombatComboGraphNode_EntryOrCombo) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		UpdateGraphNode();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		RebuildIncomingFromRowsIfNeeded();
	}

	virtual void UpdateGraphNode() override
	{
		InputPins.Empty();
		OutputPins.Empty();
		RightNodeBox.Reset();
		LeftNodeBox.Reset();
		TopPinBox.Reset();
		BottomPinBox.Reset();
		IncomingFromBox.Reset();
		LastIncomingFromSignature.Reset();

		this->ContentScale.Bind(this, &SGraphNode::GetContentScale);
		this->GetOrAddSlot(ENodeZone::Center)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
				.BorderBackgroundColor(this, &SRZCombatComboGraphNode_EntryOrCombo::GetBorderColor)
				.Padding(0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.MinDesiredHeight(18.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SAssignNew(TopPinBox, SHorizontalBox)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(8.0f, 0.0f))
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("BTEditor.Graph.BTNode.Body"))
						.BorderBackgroundColor(this, &SRZCombatComboGraphNode_EntryOrCombo::GetBodyColor)
						.Padding(FMargin(10.0f, 7.0f))
						[
							SNew(SBox)
							.MinDesiredWidth(170.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
									[
										SNew(SImage)
										.Image(this, &SRZCombatComboGraphNode_EntryOrCombo::GetNodeIcon)
										.ColorAndOpacity(FLinearColor::White)
									]
									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(this, &SRZCombatComboGraphNode_EntryOrCombo::GetNodeTitle)
										.TextStyle(FAppStyle::Get(), "Graph.StateNode.NodeTitle")
										.ColorAndOpacity(FLinearColor::White)
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
								[
									SNew(STextBlock)
									.Text(this, &SRZCombatComboGraphNode_EntryOrCombo::GetRuntimeDebugLabel)
									.TextStyle(FAppStyle::Get(), "Graph.TransitionNode.TooltipRule")
									.ColorAndOpacity(this, &SRZCombatComboGraphNode_EntryOrCombo::GetRuntimeDebugTextColor)
									.Visibility(this, &SRZCombatComboGraphNode_EntryOrCombo::GetRuntimeDebugLabelVisibility)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SAssignNew(IncomingFromBox, SVerticalBox)
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.MinDesiredHeight(18.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SAssignNew(BottomPinBox, SHorizontalBox)
						]
					]
				]
			];

		CreatePinWidgets();
		RebuildIncomingFromRows();
	}

	virtual void CreatePinWidgets() override
	{
		if (!GraphNode)
		{
			return;
		}

		for (UEdGraphPin* Pin : GraphNode->Pins)
		{
			if (Pin && !Pin->bHidden)
			{
				AddPin(SNew(SRZCombatComboGraphPin, Pin));
			}
		}
	}

	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override
	{
		PinToAdd->SetOwner(SharedThis(this));

		const EEdGraphPinDirection Direction = PinToAdd->GetDirection();
		if (Direction == EGPD_Input)
		{
			TopPinBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					PinToAdd
				];
			InputPins.Add(PinToAdd);
			return;
		}

		BottomPinBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				PinToAdd
			];
		OutputPins.Add(PinToAdd);
	}

private:
	FString MakeIncomingFromSignature() const
	{
		const URZCombatComboGraphNode_Combo* ComboNode = Cast<URZCombatComboGraphNode_Combo>(GraphNode);
		if (!ComboNode)
		{
			return FString();
		}

		TArray<RZCombatComboGraphNodes::FIncomingTransitionGroup> IncomingGroups;
		RZCombatComboGraphNodes::GatherIncomingTransitionGroups(ComboNode, IncomingGroups);

		FString Signature;
		for (const RZCombatComboGraphNodes::FIncomingTransitionGroup& Group : IncomingGroups)
		{
			Signature += FString::Printf(
				TEXT("|S:%s@%d,%d"),
				Group.SourceNode ? *Group.SourceNode->NodeGuid.ToString(EGuidFormats::Digits) : TEXT("None"),
				Group.SourceNode ? Group.SourceNode->NodePosX : 0,
				Group.SourceNode ? Group.SourceNode->NodePosY : 0
			);

			for (const URZCombatComboGraphNode_Transition* TransitionNode : Group.Transitions)
			{
				const int32 GeneratedPriority = RZCombatComboGraphNodes::CalculateGeneratedTransitionPriority(TransitionNode);
				Signature += FString::Printf(
					TEXT(";T:%s:%d:%d:%s:%s"),
					TransitionNode ? *TransitionNode->NodeGuid.ToString(EGuidFormats::Digits) : TEXT("None"),
					GeneratedPriority,
					TransitionNode && TransitionNode->bEnabled ? 1 : 0,
					TransitionNode ? *TransitionNode->DisplayName.ToString() : TEXT(""),
					TransitionNode && TransitionNode->TriggerTag.IsValid() ? *TransitionNode->TriggerTag.GetTagName().ToString() : TEXT("")
				);
			}
		}

		return Signature;
	}

	void RebuildIncomingFromRowsIfNeeded()
	{
		const FString CurrentSignature = MakeIncomingFromSignature();
		if (CurrentSignature != LastIncomingFromSignature)
		{
			RebuildIncomingFromRows();
		}
	}

	void RebuildIncomingFromRows()
	{
		if (!IncomingFromBox.IsValid())
		{
			return;
		}

		IncomingFromBox->ClearChildren();

		const URZCombatComboGraphNode_Combo* ComboNode = Cast<URZCombatComboGraphNode_Combo>(GraphNode);
		if (!ComboNode)
		{
			LastIncomingFromSignature = FString();
			return;
		}

		TArray<RZCombatComboGraphNodes::FIncomingTransitionGroup> IncomingGroups;
		RZCombatComboGraphNodes::GatherIncomingTransitionGroups(ComboNode, IncomingGroups);
		LastIncomingFromSignature = MakeIncomingFromSignature();
		if (IncomingGroups.IsEmpty())
		{
			return;
		}

		IncomingFromBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 7.0f, 0.0f, 4.0f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.18f))
				.Padding(FMargin(0.0f, 1.0f))
			];

		for (const RZCombatComboGraphNodes::FIncomingTransitionGroup& Group : IncomingGroups)
		{
			TSharedRef<SHorizontalBox> TransitionChipBox = SNew(SHorizontalBox);
			URZCombatComboGraphNode_Transition* PrimaryTransitionNode = Group.Transitions.Num() > 0 ? Group.Transitions[0] : nullptr;
			const TWeakObjectPtr<URZCombatComboGraphNode_Transition> WeakPrimaryTransitionNode(PrimaryTransitionNode);

			for (URZCombatComboGraphNode_Transition* TransitionNode : Group.Transitions)
			{
				TransitionChipBox->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
					[
						MakeIncomingTransitionChip(TransitionNode)
					];
			}

			IncomingFromBox->AddSlot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(this, &SRZCombatComboGraphNode_EntryOrCombo::GetIncomingFromRowBorderColor, WeakPrimaryTransitionNode)
					.OnMouseButtonDown(this, &SRZCombatComboGraphNode_EntryOrCombo::SelectIncomingTransition, WeakPrimaryTransitionNode)
					.Cursor(EMouseCursor::Hand)
					.Padding(FMargin(1.0f))
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("BTEditor.Graph.BTNode.Body"))
						.BorderBackgroundColor(this, &SRZCombatComboGraphNode_EntryOrCombo::GetIncomingFromRowFillColor, WeakPrimaryTransitionNode)
						.Padding(FMargin(5.0f, 4.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.0f, 0.0f, 0.0f, 3.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush(TEXT("GraphEditor.Event_16x")))
									.ColorAndOpacity(FLinearColor(0.78f, 0.88f, 1.0f))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::Format(
										LOCTEXT("IncomingFromSourceLabel", "From {0}"),
										RZCombatComboGraphNodes::GetSourceNodeDisplayText(Group.SourceNode)
									))
									.ColorAndOpacity(FLinearColor(0.78f, 0.88f, 1.0f))
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								TransitionChipBox
							]
						]
					]
				];
		}
	}

	TSharedRef<SWidget> MakeIncomingTransitionChip(URZCombatComboGraphNode_Transition* TransitionNode)
	{
		const TWeakObjectPtr<URZCombatComboGraphNode_Transition> WeakTransitionNode(TransitionNode);
		const int32 Priority = RZCombatComboGraphNodes::CalculateGeneratedTransitionPriority(TransitionNode);

		return
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(this, &SRZCombatComboGraphNode_EntryOrCombo::GetIncomingTransitionChipBorderColor, WeakTransitionNode)
			.OnMouseButtonDown(this, &SRZCombatComboGraphNode_EntryOrCombo::SelectIncomingTransition, WeakTransitionNode)
			.Cursor(EMouseCursor::Hand)
			.ToolTipText(this, &SRZCombatComboGraphNode_EntryOrCombo::GetIncomingTransitionChipTooltip, WeakTransitionNode)
			.Padding(FMargin(1.0f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("BTEditor.Graph.BTNode.Body"))
				.BorderBackgroundColor(this, &SRZCombatComboGraphNode_EntryOrCombo::GetIncomingTransitionChipFillColor, WeakTransitionNode)
				.Padding(FMargin(5.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text(FText::Format(
						LOCTEXT("IncomingFromTransitionChipLabel", "P{0} {1}"),
						FText::AsNumber(Priority),
						RZCombatComboGraphNodes::GetTransitionDisplayText(TransitionNode)
					))
					.TextStyle(FAppStyle::Get(), "Graph.TransitionNode.TooltipRule")
					.ColorAndOpacity(FLinearColor::White)
				]
			];
	}

	FSlateColor GetIncomingFromRowBorderColor(TWeakObjectPtr<URZCombatComboGraphNode_Transition> TransitionNode) const
	{
		const URZCombatComboGraphNode_Transition* Transition = TransitionNode.Get();
		if (IsIncomingTransitionSelected(Transition))
		{
			return FSlateColor(FLinearColor(0.33f, 0.64f, 1.0f, 1.0f));
		}

		return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.08f));
	}

	FSlateColor GetIncomingFromRowFillColor(TWeakObjectPtr<URZCombatComboGraphNode_Transition> TransitionNode) const
	{
		const URZCombatComboGraphNode_Transition* Transition = TransitionNode.Get();
		if (IsIncomingTransitionSelected(Transition))
		{
			return FSlateColor(FLinearColor(0.12f, 0.17f, 0.24f, 0.97f));
		}

		return FSlateColor(FLinearColor(0.09f, 0.11f, 0.14f, 0.92f));
	}

	FSlateColor GetIncomingTransitionChipBorderColor(TWeakObjectPtr<URZCombatComboGraphNode_Transition> TransitionNode) const
	{
		const URZCombatComboGraphNode_Transition* Transition = TransitionNode.Get();
		if (IsIncomingTransitionSelected(Transition))
		{
			return FSlateColor(FLinearColor(0.96f, 0.72f, 0.30f, 1.0f));
		}

		return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.10f));
	}

	FSlateColor GetIncomingTransitionChipFillColor(TWeakObjectPtr<URZCombatComboGraphNode_Transition> TransitionNode) const
	{
		const URZCombatComboGraphNode_Transition* Transition = TransitionNode.Get();
		if (IsIncomingTransitionSelected(Transition))
		{
			return FSlateColor(FLinearColor(0.27f, 0.22f, 0.15f, 0.98f));
		}

		return FSlateColor(Transition && Transition->bEnabled
			? FLinearColor(0.19f, 0.28f, 0.38f, 0.95f)
			: FLinearColor(0.16f, 0.16f, 0.16f, 0.85f));
	}

	FText GetIncomingTransitionChipTooltip(TWeakObjectPtr<URZCombatComboGraphNode_Transition> TransitionNode) const
	{
		const URZCombatComboGraphNode_Transition* Transition = TransitionNode.Get();
		if (!Transition)
		{
			return LOCTEXT("IncomingTransitionMissingTooltip", "Transition no longer exists.");
		}

		return FText::Format(
			LOCTEXT("IncomingTransitionChipTooltip", "Select {0} to edit Trigger, generated Priority, Payload, Checker, and notes."),
			RZCombatComboGraphNodes::GetTransitionDisplayText(Transition)
		);
	}

	FReply SelectIncomingTransition(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakObjectPtr<URZCombatComboGraphNode_Transition> TransitionNode)
	{
		(void)MyGeometry;

		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		URZCombatComboGraphNode_Transition* Transition = TransitionNode.Get();
		const TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
		if (!Transition || !OwnerPanel.IsValid())
		{
			return FReply::Unhandled();
		}

		OwnerPanel->SelectionManager.ClickedOnNode(Transition, MouseEvent);
		return FReply::Handled();
	}

	bool IsIncomingTransitionSelected(const URZCombatComboGraphNode_Transition* TransitionNode) const
	{
		const TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
		return TransitionNode
			&& OwnerPanel.IsValid()
			&& OwnerPanel->SelectionManager.IsNodeSelected(const_cast<URZCombatComboGraphNode_Transition*>(TransitionNode));
	}

	ERZCombatComboGraphRuntimeNodeState GetRuntimeNodeState() const
	{
		const URZCombatComboGraphNode_Combo* ComboNode = Cast<URZCombatComboGraphNode_Combo>(GraphNode);
		const URZCombatComboGraph* ComboGraph = ComboNode ? Cast<URZCombatComboGraph>(ComboNode->GetGraph()) : nullptr;
		return ComboGraph
			? ComboGraph->ResolveRuntimeDebugNodeState(ComboNode->NodeId)
			: ERZCombatComboGraphRuntimeNodeState::None;
	}

	EVisibility GetRuntimeDebugLabelVisibility() const
	{
		const ERZCombatComboGraphRuntimeNodeState NodeState = GetRuntimeNodeState();
		return NodeState != ERZCombatComboGraphRuntimeNodeState::None && NodeState != ERZCombatComboGraphRuntimeNodeState::Available
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	FText GetRuntimeDebugLabel() const
	{
		const ERZCombatComboGraphRuntimeNodeState NodeState = GetRuntimeNodeState();
		switch (NodeState)
		{
		case ERZCombatComboGraphRuntimeNodeState::Current:
			return LOCTEXT("RuntimeDebugCurrentLabel", "Current");
		case ERZCombatComboGraphRuntimeNodeState::PendingActivation:
			return LOCTEXT("RuntimeDebugPendingLabel", "Pending");
		case ERZCombatComboGraphRuntimeNodeState::Activated:
			return LOCTEXT("RuntimeDebugActivatedLabel", "Activated");
		case ERZCombatComboGraphRuntimeNodeState::Committed:
			return LOCTEXT("RuntimeDebugCommittedLabel", "GA Commit");
		case ERZCombatComboGraphRuntimeNodeState::ComboCommitted:
			return LOCTEXT("RuntimeDebugComboCommittedLabel", "Combo Commit");
		case ERZCombatComboGraphRuntimeNodeState::Failed:
			return LOCTEXT("RuntimeDebugFailedLabel", "Failed");
		case ERZCombatComboGraphRuntimeNodeState::Ended:
			return LOCTEXT("RuntimeDebugEndedLabel", "Ended");
		case ERZCombatComboGraphRuntimeNodeState::Cancelled:
			return LOCTEXT("RuntimeDebugCancelledLabel", "Cancelled");
		case ERZCombatComboGraphRuntimeNodeState::Available:
		case ERZCombatComboGraphRuntimeNodeState::None:
		default:
			return FText::GetEmpty();
		}
	}

	FSlateColor GetRuntimeDebugTextColor() const
	{
		return FSlateColor(RZCombatComboGraphRuntimeDebug::GetNodeBorderColor(GetRuntimeNodeState(), false));
	}

	FText GetNodeTitle() const
	{
		return GraphNode ? GraphNode->GetNodeTitle(ENodeTitleType::FullTitle) : FText::GetEmpty();
	}

	FSlateColor GetBorderColor() const
	{
		return FSlateColor(RZCombatComboGraphRuntimeDebug::GetNodeBorderColor(GetRuntimeNodeState(), IsHovered()));
	}

	FSlateColor GetBodyColor() const
	{
		FLinearColor DefaultBodyColor;
		if (GraphNode && GraphNode->IsA<URZCombatComboGraphNode_Entry>())
		{
			DefaultBodyColor = FLinearColor(0.18f, 0.34f, 0.30f);
		}
		else
		{
			DefaultBodyColor = FLinearColor(0.32f, 0.22f, 0.12f);
		}

		return FSlateColor(RZCombatComboGraphRuntimeDebug::GetNodeBodyColor(GetRuntimeNodeState(), DefaultBodyColor, IsHovered()));
	}

	const FSlateBrush* GetNodeIcon() const
	{
		if (GraphNode && GraphNode->IsA<URZCombatComboGraphNode_Entry>())
		{
			return FAppStyle::GetBrush(TEXT("GraphEditor.Event_16x"));
		}

		return FAppStyle::GetBrush(TEXT("GraphEditor.Function_16x"));
	}

	TSharedPtr<SHorizontalBox> TopPinBox;
	TSharedPtr<SHorizontalBox> BottomPinBox;
	TSharedPtr<SVerticalBox> IncomingFromBox;
	FString LastIncomingFromSignature;
};

class SRZCombatComboGraphNode_Transition : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SRZCombatComboGraphNode_Transition) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, URZCombatComboGraphNode_Transition* InNode)
	{
		GraphNode = InNode;
		UpdateGraphNode();
	}

	virtual void MoveTo(const FVector2f& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override
	{
	}

	virtual bool RequiresSecondPassLayout() const override
	{
		return true;
	}

	virtual void PerformSecondPassLayout(const TMap<UObject*, TSharedRef<SNode>>& NodeToWidgetLookup) const override
	{
		const URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(GraphNode);
		if (!TransitionNode)
		{
			return;
		}

		UEdGraphNode* PreviousNode = TransitionNode->GetPreviousNode();
		URZCombatComboGraphNode_Combo* NextNode = TransitionNode->GetNextComboNode();
		const TSharedRef<SNode>* PreviousNodeWidget = PreviousNode ? NodeToWidgetLookup.Find(PreviousNode) : nullptr;
		const TSharedRef<SNode>* NextNodeWidget = NextNode ? NodeToWidgetLookup.Find(NextNode) : nullptr;
		if (!PreviousNode || !NextNode || !PreviousNodeWidget || !NextNodeWidget)
		{
			return;
		}

		const FGeometry StartGeometry(
			FVector2D(PreviousNode->NodePosX, PreviousNode->NodePosY),
			FVector2D::ZeroVector,
			PreviousNodeWidget->Get().GetDesiredSize(),
			1.0f
		);
		const FGeometry EndGeometry(
			FVector2D(NextNode->NodePosX, NextNode->NodePosY),
			FVector2D::ZeroVector,
			NextNodeWidget->Get().GetDesiredSize(),
			1.0f
		);

		int32 TransitionIndex = 0;
		int32 TransitionCount = 1;
		GetTransitionIndexForSameEndpoints(TransitionNode, PreviousNode, NextNode, TransitionIndex, TransitionCount);
		PositionBetweenTwoNodesWithOffset(StartGeometry, EndGeometry, TransitionIndex, TransitionCount);
	}

	virtual void UpdateGraphNode() override
	{
		InputPins.Empty();
		OutputPins.Empty();
		RightNodeBox.Reset();
		LeftNodeBox.Reset();

		this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

		TSharedRef<SOverlay> Capsule =
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Graph.TransitionNode.ColorSpill"))
				.BorderBackgroundColor(this, &SRZCombatComboGraphNode_Transition::GetTransitionColor)
				.Padding(FMargin(8.0f, 4.0f))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Graph.TransitionNode.Icon"))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SRZCombatComboGraphNode_Transition::GetTransitionLabel)
					.TextStyle(FAppStyle::Get(), "Graph.TransitionNode.TooltipRule")
					.ColorAndOpacity(FLinearColor::White)
				]
			];

		this->GetOrAddSlot(ENodeZone::Center)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				Capsule
			];
	}

	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override
	{
	}

private:
	static void GetTransitionIndexForSameEndpoints(
		const URZCombatComboGraphNode_Transition* TransitionNode,
		const UEdGraphNode* PreviousNode,
		const URZCombatComboGraphNode_Combo* NextNode,
		int32& OutTransitionIndex,
		int32& OutTransitionCount
	)
	{
		OutTransitionIndex = 0;
		OutTransitionCount = 1;

		const UEdGraphPin* SourceOutputPin = nullptr;
		if (const URZCombatComboGraphNode_Entry* EntryNode = Cast<URZCombatComboGraphNode_Entry>(PreviousNode))
		{
			SourceOutputPin = EntryNode->GetOutputPin();
		}
		else if (const URZCombatComboGraphNode_Combo* ComboNode = Cast<URZCombatComboGraphNode_Combo>(PreviousNode))
		{
			SourceOutputPin = ComboNode->GetOutputPin();
		}

		if (!SourceOutputPin || !NextNode)
		{
			return;
		}

		TArray<const URZCombatComboGraphNode_Transition*> MatchingTransitions;
		for (const UEdGraphPin* LinkedPin : SourceOutputPin->LinkedTo)
		{
			const URZCombatComboGraphNode_Transition* Candidate = LinkedPin ? Cast<URZCombatComboGraphNode_Transition>(LinkedPin->GetOwningNode()) : nullptr;
			if (Candidate && Candidate->GetNextComboNode() == NextNode)
			{
				MatchingTransitions.Add(Candidate);
			}
		}

		const int32 FoundIndex = MatchingTransitions.IndexOfByKey(TransitionNode);
		OutTransitionIndex = FoundIndex == INDEX_NONE ? 0 : FoundIndex;
		OutTransitionCount = FMath::Max(1, MatchingTransitions.Num());
	}

	void PositionBetweenTwoNodesWithOffset(const FGeometry& StartGeometry, const FGeometry& EndGeometry, int32 NodeIndex, int32 MaxNodes) const
	{
		const FVector2D StartCenter = FGeometryHelper::CenterOf(StartGeometry);
		const FVector2D EndCenter = FGeometryHelper::CenterOf(EndGeometry);
		const FVector2D SeedPoint = (StartCenter + EndCenter) * 0.5f;
		const FVector2D StartAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(StartGeometry, SeedPoint);
		const FVector2D EndAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(EndGeometry, SeedPoint);

		constexpr float Height = 30.0f;
		const FVector2D DesiredNodeSize = GetDesiredSize();
		FVector2D DeltaPosition = EndAnchorPoint - StartAnchorPoint;
		if (DeltaPosition.IsNearlyZero())
		{
			DeltaPosition = FVector2D(10.0f, 0.0f);
		}

		const FVector2D Normal = FVector2D(DeltaPosition.Y, -DeltaPosition.X).GetSafeNormal();
		const FVector2D DeltaNormal = DeltaPosition.GetSafeNormal();
		const FVector2D NewCenter = StartAnchorPoint + (0.5f * DeltaPosition) + (Height * Normal);

		constexpr float MultiNodeSpace = 0.2f;
		constexpr float MultiNodeStep = 1.0f + MultiNodeSpace;
		const float MultiNodeStart = -((MaxNodes - 1) * MultiNodeStep) / 2.0f;
		const float MultiNodeOffset = MultiNodeStart + (NodeIndex * MultiNodeStep);
		const FVector2D NewCorner = NewCenter - (0.5f * DesiredNodeSize) + (DeltaNormal * MultiNodeOffset * DesiredNodeSize.Size());

		GraphNode->NodePosX = static_cast<int32>(NewCorner.X);
		GraphNode->NodePosY = static_cast<int32>(NewCorner.Y);
	}

	FSlateColor GetTransitionColor() const
	{
		const URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(GraphNode);
		if (TransitionNode && !TransitionNode->bEnabled)
		{
			return FSlateColor(FLinearColor(0.18f, 0.18f, 0.18f, 0.85f));
		}

		return FSlateColor(IsHovered() ? FLinearColor(0.45f, 0.58f, 0.95f) : FLinearColor(0.24f, 0.32f, 0.56f));
	}

	FText GetTransitionLabel() const
	{
		if (const URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(GraphNode))
		{
			if (!TransitionNode->DisplayName.IsEmpty())
			{
				return TransitionNode->DisplayName;
			}

			if (TransitionNode->TriggerTag.IsValid())
			{
				return FText::FromName(TransitionNode->TriggerTag.GetTagName());
			}
		}

		return LOCTEXT("TransitionLabel", "Transition");
	}
};

TSharedPtr<SGraphNode> FRZCombatComboGraphNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (URZCombatComboGraphNode_Entry* EntryNode = Cast<URZCombatComboGraphNode_Entry>(Node))
	{
		return SNew(SRZCombatComboGraphNode_EntryOrCombo, EntryNode);
	}

	if (URZCombatComboGraphNode_Combo* ComboNode = Cast<URZCombatComboGraphNode_Combo>(Node))
	{
		return SNew(SRZCombatComboGraphNode_EntryOrCombo, ComboNode);
	}

	if (URZCombatComboGraphNode_Transition* TransitionNode = Cast<URZCombatComboGraphNode_Transition>(Node))
	{
		return SNew(SRZCombatComboGraphNode_Transition, TransitionNode);
	}

	return nullptr;
}

FConnectionDrawingPolicy* FRZCombatComboGraphConnectionFactory::CreateConnectionPolicy(
	const UEdGraphSchema* Schema,
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float ZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj
) const
{
	if (Schema && Schema->IsA<URZCombatComboGraphSchema>())
	{
		return new FRZCombatComboGraphStraightConnectionDrawingPolicy(
			InBackLayerID,
			InFrontLayerID,
			ZoomFactor,
			InClippingRect,
			InDrawElements
		);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
