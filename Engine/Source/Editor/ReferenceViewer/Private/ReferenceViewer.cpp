// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "ReferenceViewer.h"
#include "ReferenceViewer.generated.inl"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"
//DEFINE_LOG_CATEGORY(LogReferenceViewer);

class FGraphPanelNodeFactory_ReferenceViewer : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const OVERRIDE
	{
		if ( UEdGraphNode_Reference* DependencyNode = Cast<UEdGraphNode_Reference>(Node) )
		{
			return SNew(SReferenceNode, DependencyNode);
		}

		return NULL;
	}
};

class FReferenceViewerModule : public IReferenceViewerModule
{
public:
	FReferenceViewerModule()
		: ReferenceViewerTabId("ReferenceViewer")
	{
		
	}

	virtual void StartupModule() OVERRIDE
	{
		GraphPanelNodeFactory = MakeShareable( new FGraphPanelNodeFactory_ReferenceViewer() );
		FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ReferenceViewerTabId, FOnSpawnTab::CreateRaw( this, &FReferenceViewerModule::SpawnReferenceViewerTab ) )
			.SetDisplayName( LOCTEXT("ReferenceViewerTitle", "Reference Viewer") )
			.SetMenuType( ETabSpawnerMenuType::Hide );
	}

	virtual void ShutdownModule() OVERRIDE
	{
		if ( GraphPanelNodeFactory.IsValid() )
		{
			FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory);
			GraphPanelNodeFactory.Reset();
		}

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ReferenceViewerTabId);
	}

	virtual void InvokeReferenceViewerTab(const TArray<FName>& GraphRootPackageNames) OVERRIDE
	{
		TSharedRef<SDockTab> NewTab = FGlobalTabmanager::Get()->InvokeTab( ReferenceViewerTabId );
		TSharedRef<SReferenceViewer> ReferenceViewer = StaticCastSharedRef<SReferenceViewer>( NewTab->GetContent() );
		ReferenceViewer->SetGraphRootPackageNames(GraphRootPackageNames);
	}

	virtual TSharedRef<SWidget> CreateReferenceViewer(const TArray<FName>& GraphRootPackageNames) OVERRIDE
	{
		TSharedRef<SReferenceViewer> ReferenceViewer = SNew(SReferenceViewer);
		ReferenceViewer->SetGraphRootPackageNames(GraphRootPackageNames);
		return ReferenceViewer;
	}

private:
	TSharedRef<SDockTab> SpawnReferenceViewerTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		NewTab->SetContent( SNew(SReferenceViewer) );

		return NewTab;
	}

private:
	TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory;
	FName ReferenceViewerTabId;
};

IMPLEMENT_MODULE( FReferenceViewerModule, ReferenceViewer )

#undef LOCTEXT_NAMESPACE