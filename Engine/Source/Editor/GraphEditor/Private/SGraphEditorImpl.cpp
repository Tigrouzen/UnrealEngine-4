// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphEditorImpl.h"
#include "GraphEditorModule.h"

#define LOCTEXT_NAMESPACE "GraphEditorModule"

/////////////////////////////////////////////////////
// SGraphEditorImpl

FVector2D SGraphEditorImpl::GetPasteLocation() const
{
	return GraphPanel->GetPastePosition();
}

bool SGraphEditorImpl::IsNodeTitleVisible( const UEdGraphNode* Node, bool bEnsureVisible )
{
	return GraphPanel->IsNodeTitleVisible(Node, bEnsureVisible);
}

void SGraphEditorImpl::JumpToNode( const UEdGraphNode* JumpToMe, bool bRequestRename )
{
	GraphPanel->JumpToNode(JumpToMe, bRequestRename);
	FocusLockedEditorHere();
}


void SGraphEditorImpl::JumpToPin( const UEdGraphPin* JumpToMe )
{
	GraphPanel->JumpToPin(JumpToMe);
	FocusLockedEditorHere();
}


bool SGraphEditorImpl::SupportsKeyboardFocus() const
{
	return true;
}

FReply SGraphEditorImpl::OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	OnFocused.ExecuteIfBound(SharedThis(this));
	return FReply::Handled();
}

FReply SGraphEditorImpl::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if(MouseEvent.IsMouseButtonDown(EKeys::ThumbMouseButton))
	{
		OnNavigateHistoryBack.ExecuteIfBound();
	}
	else if(MouseEvent.IsMouseButtonDown(EKeys::ThumbMouseButton2))
	{
		OnNavigateHistoryForward.ExecuteIfBound();
	}
	return FReply::Handled().SetKeyboardFocus( SharedThis(this), EKeyboardFocusCause::Mouse );
}

FReply SGraphEditorImpl::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	int32 NumNodes = GetCurrentGraph()->Nodes.Num();
	if (Commands->ProcessCommandBindings( InKeyboardEvent ) )
	{
		bool bPasteOperation = InKeyboardEvent.IsControlDown() && InKeyboardEvent.GetKey() == EKeys::V;

		if(	!bPasteOperation && GetCurrentGraph()->Nodes.Num() > NumNodes )
		{
			OnNodeSpawnedByKeymap.ExecuteIfBound();
		}
		return FReply::Handled();
	}
	else
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyboardEvent);
	}
}

void SGraphEditorImpl::NotifyGraphChanged()
{
	FEdGraphEditAction DefaultAction;
	OnGraphChanged(DefaultAction);
}

void SGraphEditorImpl::OnGraphChanged(const FEdGraphEditAction& InAction)
{
	if (!bNeedsRefresh)
	{
		// Remove the old user interface nodes
		GraphPanel->PurgeVisualRepresentation();
	}

	bNeedsRefresh = true;
}

void SGraphEditorImpl::GraphEd_OnPanelUpdated()
{
	bNeedsRefresh = false;
}

const TSet<class UObject*>& SGraphEditorImpl::GetSelectedNodes() const
{
	return GraphPanel->SelectionManager.GetSelectedNodes();
}

void SGraphEditorImpl::ClearSelectionSet()
{
	GraphPanel->SelectionManager.ClearSelectionSet();
}

void SGraphEditorImpl::SetNodeSelection(UEdGraphNode* Node, bool bSelect)
{
	GraphPanel->SelectionManager.SetNodeSelection(Node, bSelect);
}

void SGraphEditorImpl::SelectAllNodes()
{
	FGraphPanelSelectionSet NewSet;
	for (int32 NodeIndex = 0; NodeIndex < EdGraphObj->Nodes.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = EdGraphObj->Nodes[NodeIndex];
		ensureMsg(Node->IsValidLowLevel(), TEXT("Node is invalid"));
		NewSet.Add(Node);
	}
	GraphPanel->SelectionManager.SetSelectionSet(NewSet);
}

UEdGraphPin* SGraphEditorImpl::GetGraphPinForMenu()
{
	return GraphPinForMenu;
}

void SGraphEditorImpl::ZoomToFit(bool bOnlySelection)
{
	GraphPanel->ZoomToFit(bOnlySelection);
}
bool SGraphEditorImpl::GetBoundsForSelectedNodes( class FSlateRect& Rect, float Padding )
{
	return GraphPanel->GetBoundsForSelectedNodes(Rect, Padding);
}

void SGraphEditorImpl::Construct( const FArguments& InArgs )
{
	Commands = MakeShareable( new FUICommandList() );
	IsEditable = InArgs._IsEditable;
	Appearance = InArgs._Appearance;
	TitleBarEnabledOnly = InArgs._TitleBarEnabledOnly;
	TitleBar	= InArgs._TitleBar;
	bAutoExpandActionMenu = InArgs._AutoExpandActionMenu;
	bShowPIENotification = InArgs._ShowPIENotification;

	OnNavigateHistoryBack = InArgs._OnNavigateHistoryBack;
	OnNavigateHistoryForward = InArgs._OnNavigateHistoryForward;
	OnNodeSpawnedByKeymap = InArgs._GraphEvents.OnNodeSpawnedByKeymap;

	// Make sure that the editor knows about what kinds
	// of commands GraphEditor can do.
	FGraphEditorCommands::Register();

	// Tell GraphEditor how to handle all the known commands
	{
		Commands->MapAction( FGraphEditorCommands::Get().ReconstructNodes,
			FExecuteAction::CreateSP( this, &SGraphEditorImpl::ReconstructNodes ),
			FCanExecuteAction::CreateSP( this, &SGraphEditorImpl::CanReconstructNodes )
		);

		Commands->MapAction( FGraphEditorCommands::Get().BreakNodeLinks,
			FExecuteAction::CreateSP( this, &SGraphEditorImpl::BreakNodeLinks ),
			FCanExecuteAction::CreateSP( this, &SGraphEditorImpl::CanBreakNodeLinks )
		);

		Commands->MapAction( FGraphEditorCommands::Get().BreakPinLinks,
			FExecuteAction::CreateSP( this, &SGraphEditorImpl::BreakPinLinks, true),
			FCanExecuteAction::CreateSP( this, &SGraphEditorImpl::CanBreakPinLinks )
		);

		// Append any additional commands that a consumer of GraphEditor wants us to be aware of.
		const TSharedPtr<FUICommandList>& AdditionalCommands = InArgs._AdditionalCommands;
		if ( AdditionalCommands.IsValid() )
		{
			Commands->Append( AdditionalCommands.ToSharedRef() );
		}

	}

	GraphPinForMenu = NULL;
	EdGraphObj = InArgs._GraphToEdit;
	bNeedsRefresh = false;
		
	OnFocused = InArgs._GraphEvents.OnFocused;
	OnCreateActionMenu = InArgs._GraphEvents.OnCreateActionMenu;
	
	struct Local
	{
		static FString GetPIENotifyText(TAttribute<FGraphAppearanceInfo> Appearance, FString DefaultText)
		{
			FString OverrideText = Appearance.Get().PIENotifyText;
			return OverrideText.Len() ? OverrideText : DefaultText;
		}
	};
	
	FString DefaultPIENotify(TEXT("SIMULATING"));
	TAttribute<FString> PIENotifyText = Appearance.IsBound() ?
		TAttribute<FString>::Create( TAttribute<FString>::FGetter::CreateStatic(&Local::GetPIENotifyText, Appearance, DefaultPIENotify) ) :
		TAttribute<FString>(DefaultPIENotify);

	TSharedPtr<SOverlay> OverlayWidget;

	this->ChildSlot
	[
		SAssignNew(OverlayWidget, SOverlay)

		// The graph panel
		+SOverlay::Slot()
		.Expose(GraphPanelSlot)
		[
			SAssignNew(GraphPanel, SGraphPanel)
			.GraphObj( EdGraphObj )
			.GraphObjToDiff( InArgs._GraphToDiff)
			.OnGetContextMenuFor( this, &SGraphEditorImpl::GraphEd_OnGetContextMenuFor )
			.OnSelectionChanged( InArgs._GraphEvents.OnSelectionChanged )
			.OnNodeDoubleClicked( InArgs._GraphEvents.OnNodeDoubleClicked )
			.IsEditable( this->IsEditable )
			.OnDropActor( InArgs._GraphEvents.OnDropActor )
			.OnDropStreamingLevel( InArgs._GraphEvents.OnDropStreamingLevel )
			.IsEnabled(this, &SGraphEditorImpl::GraphEd_OnGetGraphEnabled)
			.OnVerifyTextCommit( InArgs._GraphEvents.OnVerifyTextCommit )
			.OnTextCommitted( InArgs._GraphEvents.OnTextCommitted )
			.OnSpawnNodeByShortcut( InArgs._GraphEvents.OnSpawnNodeByShortcut )
			.OnUpdateGraphPanel( this, &SGraphEditorImpl::GraphEd_OnPanelUpdated )
			.OnDisallowedPinConnection( InArgs._GraphEvents.OnDisallowedPinConnection )
			.ShowPIENotification( InArgs._ShowPIENotification )
		]

		// Indicator of current zoom level
		+SOverlay::Slot()
		.Padding(5)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), "Graph.ZoomText" )
			.Text( this, &SGraphEditorImpl::GetZoomString )
			.ColorAndOpacity( this, &SGraphEditorImpl::GetZoomTextColorAndOpacity )
		]

		// Title bar - optional
		+SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			InArgs._TitleBar.IsValid() ? InArgs._TitleBar.ToSharedRef() : (TSharedRef<SWidget>)SNullWidget::NullWidget
		]

		// Bottom-right corner text indicating the type of tool
		+SOverlay::Slot()
		.Padding(10)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Visibility( EVisibility::HitTestInvisible )
			.TextStyle( FEditorStyle::Get(), "Graph.CornerText" )
			.Text(Appearance.Get().CornerText)
		]

		// Top-right corner text indicating PIE is active
		+SOverlay::Slot()
		.Padding(20)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Visibility(this, &SGraphEditorImpl::PIENotification)
			.TextStyle( FEditorStyle::Get(), "Graph.SimulatingText" )
			.Text( PIENotifyText )
		]

		// Bottom-right corner text for notification list position
		+SOverlay::Slot()
		.Padding(15.f)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		[
			SAssignNew(NotificationListPtr, SNotificationList)
			.Visibility(EVisibility::HitTestInvisible)
		]
	];

	GraphPanel->RestoreViewSettings(FVector2D::ZeroVector, -1);

	NotifyGraphChanged();
}

EVisibility SGraphEditorImpl::PIENotification( ) const
{
	if (bShowPIENotification && (GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != NULL))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}
	
SGraphEditorImpl::~SGraphEditorImpl()
{
}

void SGraphEditorImpl::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;
		GraphPanel->Update();
	}	

	// If locked to another graph editor, and our panel has moved, synchronise the locked graph editor accordingly
	if ((EdGraphObj != NULL) && GraphPanel.IsValid())
	{
		if(GraphPanel->HasMoved() && LockedGraph.IsValid())
		{
			FocusLockedEditorHere();
		}
	}
}

void SGraphEditorImpl::OnClosedActionMenu()
{
	GraphPanel->OnStopMakingConnection(/*bForceStop=*/ true);
}

bool SGraphEditorImpl::GraphEd_OnGetGraphEnabled() const
{
	const bool bTitleBarOnly = TitleBarEnabledOnly.Get();
	return !bTitleBarOnly;
}

FActionMenuContent SGraphEditorImpl::GraphEd_OnGetContextMenuFor( const FVector2D& NodeAddPosition, UEdGraphNode* InGraphNode, UEdGraphPin* InGraphPin, const TArray<UEdGraphPin*>& InDragFromPins )
{
	if (EdGraphObj != NULL)
	{
		const UEdGraphSchema* Schema = EdGraphObj->GetSchema();
		check(Schema);
			
		// Cache the pin this menu is being brought up for
		GraphPinForMenu = InGraphPin;

		if ((InGraphPin != NULL) || (InGraphNode != NULL))
		{
			// Get all menu extenders for this context menu from the graph editor module
			FGraphEditorModule& GraphEditorModule = FModuleManager::GetModuleChecked<FGraphEditorModule>( TEXT("GraphEditor") );
			TArray<FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode> MenuExtenderDelegates = GraphEditorModule.GetAllGraphEditorContextMenuExtender();

			TArray<TSharedPtr<FExtender>> Extenders;
			for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
			{
				if (MenuExtenderDelegates[i].IsBound())
				{
					Extenders.Add(MenuExtenderDelegates[i].Execute(this->Commands.ToSharedRef(), EdGraphObj, InGraphNode, InGraphPin, !IsEditable.Get()));
				}
			}
			TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

			// Show the menu for the pin or node under the cursor
			const bool bShouldCloseAfterAction = true;
			FMenuBuilder MenuBuilder( bShouldCloseAfterAction, this->Commands, MenuExtender );
			Schema->GetContextMenuActions(EdGraphObj, InGraphNode, InGraphPin, &MenuBuilder, !IsEditable.Get());

			return FActionMenuContent(MenuBuilder.MakeWidget());
		}
		else if (IsEditable.Get())
		{
			if (EdGraphObj->GetSchema() != NULL )
			{
				FActionMenuContent Content;

				if(OnCreateActionMenu.IsBound())
				{
					Content = OnCreateActionMenu.Execute(
						EdGraphObj, 
						NodeAddPosition, 
						InDragFromPins, 
						bAutoExpandActionMenu, 
						SGraphEditor::FActionMenuClosed::CreateSP(this, &SGraphEditorImpl::OnClosedActionMenu)
						);
				}
				else
				{
					TSharedRef<SGraphEditorActionMenu> Menu =	
						SNew(SGraphEditorActionMenu)
						.GraphObj( EdGraphObj )
						.NewNodePosition( NodeAddPosition )
						.DraggedFromPins( InDragFromPins )
						.AutoExpandActionMenu(bAutoExpandActionMenu)
						.OnClosedCallback( SGraphEditor::FActionMenuClosed::CreateSP(this, &SGraphEditorImpl::OnClosedActionMenu)
						);

					Content = FActionMenuContent( Menu, Menu->GetFilterTextBox() );
				}

				if (InDragFromPins.Num() > 0)
				{
					GraphPanel->PreservePinPreviewUntilForced();
				}

				return Content;
			}

			return FActionMenuContent( SNew(STextBlock) .Text( NSLOCTEXT("GraphEditor", "NoNodes", "No Nodes").ToString() ) );
		}
		else
		{
			return FActionMenuContent( SNew(STextBlock)  .Text( NSLOCTEXT("GraphEditor", "CannotCreateWhileDebugging", "Cannot create new nodes while debugging").ToString() ) );
		}
	}
	else
	{
		return FActionMenuContent( SNew(STextBlock) .Text( NSLOCTEXT("GraphEditor", "GraphObjectIsNull", "Graph Object is Null").ToString() ) );
	}
		
}

bool SGraphEditorImpl::CanReconstructNodes() const
{
	return IsGraphEditable() && (GraphPanel->SelectionManager.AreAnyNodesSelected());
}

bool SGraphEditorImpl::CanBreakNodeLinks() const
{
	return IsGraphEditable() && (GraphPanel->SelectionManager.AreAnyNodesSelected());
}

bool SGraphEditorImpl::CanBreakPinLinks() const
{
	return IsGraphEditable() && (GraphPinForMenu != NULL);
}

void SGraphEditorImpl::ReconstructNodes()
{
	const UEdGraphSchema* Schema = this->EdGraphObj->GetSchema();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt( GraphPanel->SelectionManager.GetSelectedNodes() ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			Schema->ReconstructNode(*Node);
		}
	}
	NotifyGraphChanged();
}
	
void SGraphEditorImpl::BreakNodeLinks()
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakNodeLinks", "Break Node Links") );

	for (FGraphPanelSelectionSet::TConstIterator NodeIt( GraphPanel->SelectionManager.GetSelectedNodes() ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			const UEdGraphSchema* Schema = Node->GetSchema();
			Schema->BreakNodeLinks(*Node);
		}
	}
}
	
void SGraphEditorImpl::BreakPinLinks(bool bSendNodeNotification)
{
	const UEdGraphSchema* Schema = GraphPinForMenu->GetSchema();
	Schema->BreakPinLinks(*GraphPinForMenu, bSendNodeNotification);
}

FString SGraphEditorImpl::GetZoomString() const
{
	return GraphPanel->GetZoomString();
}

FSlateColor SGraphEditorImpl::GetZoomTextColorAndOpacity() const
{
	return GraphPanel->GetZoomTextColorAndOpacity();
}

bool SGraphEditorImpl::IsGraphEditable() const
{
	return (EdGraphObj != NULL) && (EdGraphObj->bEditable) && IsEditable.Get();
}

TSharedPtr<SWidget> SGraphEditorImpl::GetTitleBar() const
{
	return TitleBar;
}

void SGraphEditorImpl::SetViewLocation( const FVector2D& Location, float ZoomAmount ) 
{
	if( GraphPanel.IsValid() &&  EdGraphObj && (!LockedGraph.IsValid() || !GraphPanel->HasDeferredObjectFocus()))
	{
		GraphPanel->RestoreViewSettings(Location, ZoomAmount);
	}
}

void SGraphEditorImpl::GetViewLocation( FVector2D& Location, float& ZoomAmount ) 
{
	if( GraphPanel.IsValid() &&  EdGraphObj && (!LockedGraph.IsValid() || !GraphPanel->HasDeferredObjectFocus()))
	{
		Location = GraphPanel->GetViewOffset();
		ZoomAmount = GraphPanel->GetZoomAmount();
	}
}

void SGraphEditorImpl::LockToGraphEditor( TWeakPtr<SGraphEditor> Other ) 
{
	LockedGraph = Other;

	if (GraphPanel.IsValid())
	{
		FocusLockedEditorHere();
	}
}

void SGraphEditorImpl::AddNotification( FNotificationInfo& Info, bool bSuccess )
{
	// set up common notification properties
	Info.bUseLargeFont = true;

	TSharedPtr<SNotificationItem> Notification = NotificationListPtr->AddNotification(Info);
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail );
	}
}

void SGraphEditorImpl::FocusLockedEditorHere()
{
	if(SGraphEditor* Editor = LockedGraph.Pin().Get())
	{
		Editor->SetViewLocation(GraphPanel->GetViewOffset(), GraphPanel->GetZoomAmount());	
	}
}

void SGraphEditorImpl::SetPinVisibility( SGraphEditor::EPinVisibility Visibility ) 
{
	if( GraphPanel.IsValid())
	{
		SGraphEditor::EPinVisibility CachedVisibility = GraphPanel->GetPinVisibility();
		GraphPanel->SetPinVisibility(Visibility);
		if(CachedVisibility != Visibility)
		{
			NotifyGraphChanged();
		}
	}
}



/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE 
