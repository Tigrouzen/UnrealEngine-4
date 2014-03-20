// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CollisionAnalyzerPCH.h"

#define LOCTEXT_NAMESPACE "SCAQueryDetails"

/** Util to give written explanation for why we missed something */
FString GetReasonForMiss(const UPrimitiveComponent* MissedComp, const FCAQuery* Query)
{
	if(MissedComp != NULL && Query != NULL)
	{
		if(MissedComp->GetOwner() && !MissedComp->GetOwner()->GetActorEnableCollision())
		{
			return FString::Printf(TEXT("Owning Actor '%s' has all collision disabled (SetActorEnableCollision)"), *MissedComp->GetOwner()->GetName());
		}

		if(!MissedComp->IsCollisionEnabled())
		{
			return FString::Printf(TEXT("Component '%s' has CollisionEnabled == NoCollision"), *MissedComp->GetName());
		}

		if(MissedComp->GetCollisionResponseToChannel(Query->Channel) == ECR_Ignore)
		{
			return FString::Printf(TEXT("Component '%s' ignores this channel."), *MissedComp->GetName());
		}

		if(Query->ResponseParams.CollisionResponse.GetResponse(MissedComp->GetCollisionObjectType()) == ECR_Ignore)
		{
			return FString::Printf(TEXT("Query ignores Component '%s' movement channel."), *MissedComp->GetName());
		}
	}

	return TEXT("Unknown");
}

/** Implements a row widget for result list. */
class SHitResultRow : public SMultiColumnTableRow< TSharedPtr<FCAHitInfo> >
{
public:

	SLATE_BEGIN_ARGS(SHitResultRow) {}
		SLATE_ARGUMENT(TSharedPtr<FCAHitInfo>, Info)
		SLATE_ARGUMENT(TSharedPtr<SCAQueryDetails>, OwnerDetailsPtr)
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		Info = InArgs._Info;
		OwnerDetailsPtr = InArgs._OwnerDetailsPtr;
		SMultiColumnTableRow< TSharedPtr<FCAHitInfo> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) OVERRIDE
	{
		// Get info to apply to all columns (color and tooltip)
		FSlateColor ResultColor = FSlateColor::UseForeground();
		FString TooltipString;
		if(Info->bMiss)
		{
			ResultColor = FLinearColor(0.4f,0.4f,0.65f);
			TooltipString = LOCTEXT("MissPrefix", "Miss: ").ToString() + GetReasonForMiss(Info->Result.Component.Get(), OwnerDetailsPtr.Pin()->GetCurrentQuery());
		}
		else if(Info->Result.bBlockingHit && Info->Result.bStartPenetrating)
		{
			ResultColor = FLinearColor(1.f,0.25f,0.25f);
		}


		// Generate widget for column
		if (ColumnName == TEXT("Time"))
		{
			return	SNew(STextBlock)
					.ColorAndOpacity( ResultColor )
					.ToolTipText( TooltipString )
					.Text( FString::Printf(TEXT("%.3f"), Info->Result.Time) );
		}
		else if (ColumnName == TEXT("Type"))
		{
			FString TypeString;
			if(Info->bMiss)
			{
				TypeString = TEXT("Miss");
			}
			else if(Info->Result.bBlockingHit)
			{
				TypeString = TEXT("Block");
			}
			else
			{
				TypeString = TEXT("Touch");
			}

			return	SNew(STextBlock)
					.ColorAndOpacity( ResultColor )
					.ToolTipText( TooltipString )
					.Text( TypeString );
		}
		else if (ColumnName == TEXT("Component"))
		{
			FString LongName = TEXT("Invalid");
			if(Info->Result.Component.IsValid())
			{
				LongName = Info->Result.Component.Get()->GetReadableName();
			}

			return	SNew(STextBlock)
					.ColorAndOpacity( ResultColor )
					.ToolTipText( TooltipString )
					.Text( LongName );
		}
		else if (ColumnName == TEXT("Normal"))
		{
			return	SNew(STextBlock)
					.ColorAndOpacity( ResultColor )
					.ToolTipText( TooltipString )
					.Text( FString::Printf(TEXT("%s"), *Info->Result.Normal.ToString()) );
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	/** Result to display */
	TSharedPtr<FCAHitInfo> Info;
	/** Show details of  */
	TWeakPtr<SCAQueryDetails> OwnerDetailsPtr;
};


//////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCAQueryDetails::Construct(const FArguments& InArgs, TSharedPtr<SCollisionAnalyzer> OwningAnalyzerWidget)
{
	bDisplayQuery = false;
	bShowMisses = false;
	OwningAnalyzerWidgetPtr = OwningAnalyzerWidget;

	ChildSlot
	[
		SNew(SVerticalBox)
		// Top area is info on the trace
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush( "ToolBar.Background" ) )
			[
				SNew(SHorizontalBox)
				// Left is start/end locations
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SGridPanel)
					+SGridPanel::Slot(0,0)
					.Padding(2)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("QueryStart", "Start:").ToString())
					]
					+SGridPanel::Slot(1,0)
					.Padding(2)
					[
						SNew(STextBlock)
						.Text(this, &SCAQueryDetails::GetStartString)
					]
					+SGridPanel::Slot(0,1)
					.Padding(2)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("QueryEnd", "End:").ToString())
					]
					+SGridPanel::Slot(1,1)
					.Padding(2)
					[
						SNew(STextBlock)
						.Text(this, &SCAQueryDetails::GetEndString)
					]
				]
				// Right has controls
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.VAlign(VAlign_Top)
				.Padding(4,0)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SCAQueryDetails::OnToggleShowMisses)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ShowMisses", "Show Misses").ToString())
					]
				]
			]
		]
		// Bottom area is list of hits
		+SVerticalBox::Slot()
		.FillHeight(1) 
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
			.Padding(1.0)
			[
				SAssignNew(ResultListWidget, SListView< TSharedPtr<FCAHitInfo> >)
				.ItemHeight(20)
				.ListItemsSource(&ResultList)
				.SelectionMode(ESelectionMode::Single)
				.OnSelectionChanged(this, &SCAQueryDetails::ResultListSelectionChanged)
				.OnGenerateRow(this, &SCAQueryDetails::ResultListGenerateRow)
				.HeaderRow(
					SNew(SHeaderRow)
					+SHeaderRow::Column("Time").DefaultLabel(LOCTEXT("ResultListTimeHeader", "Time").ToString()).FillWidth(0.7)
					+SHeaderRow::Column("Type").DefaultLabel(LOCTEXT("ResultListTypeHeader", "Type").ToString()).FillWidth(0.7)
					+SHeaderRow::Column("Component").DefaultLabel(LOCTEXT("ResultListComponentHeader", "Component").ToString()).FillWidth(3)
					+SHeaderRow::Column("Normal").DefaultLabel(LOCTEXT("ResultListNormalHeader", "Normal").ToString()).FillWidth(1.8)
				)
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FString SCAQueryDetails::GetStartString() const
{
	return bDisplayQuery ? CurrentQuery.Start.ToString() : TEXT("");
}

FString SCAQueryDetails::GetEndString() const
{
	return bDisplayQuery ? CurrentQuery.End.ToString() : TEXT("");
}

TSharedRef<ITableRow> SCAQueryDetails::ResultListGenerateRow(TSharedPtr<FCAHitInfo> Info, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SHitResultRow, OwnerTable)
		.Info(Info)
		.OwnerDetailsPtr(SharedThis(this));
}

void SCAQueryDetails::UpdateDisplayedBox()
{
	FCollisionAnalyzer* Analyzer = OwningAnalyzerWidgetPtr.Pin()->Analyzer;
	Analyzer->DrawBox = FBox(0);

	if(bDisplayQuery)
	{
		TArray< TSharedPtr<FCAHitInfo> > SelectedInfos = ResultListWidget->GetSelectedItems();
		if(SelectedInfos.Num() > 0)
		{
			UPrimitiveComponent* HitComp = SelectedInfos[0]->Result.Component.Get();
			if(HitComp != NULL)
			{
				Analyzer->DrawBox = HitComp->Bounds.GetBox();
			}
		}
	}
}


void SCAQueryDetails::ResultListSelectionChanged(TSharedPtr<FCAHitInfo> SelectedInfos, ESelectInfo::Type SelectInfo)
{
	UpdateDisplayedBox();
}


void SCAQueryDetails::OnToggleShowMisses(ESlateCheckBoxState::Type InCheckboxState)
{
	bShowMisses = (InCheckboxState == ESlateCheckBoxState::Checked);
	UpdateResultList();
}

ESlateCheckBoxState::Type SCAQueryDetails::GetShowMissesState() const
{
	return bShowMisses ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}


/** See if an array of results contains a particular component */
static bool ResultsContainComponent(const TArray<FHitResult> Results, UPrimitiveComponent* Component)
{
	for(int32 i=0; i<Results.Num(); i++)
	{
		if(Results[i].Component.Get() == Component)
		{
			return true;
		}
	}
	return false;
}

void SCAQueryDetails::UpdateResultList()
{
	ResultList.Empty();
	UpdateDisplayedBox();

	if(bDisplayQuery)
	{
		// First add actual results
		for(int32 i=0; i<CurrentQuery.Results.Num(); i++)
		{
			ResultList.Add( FCAHitInfo::Make(CurrentQuery.Results[i], false) );
		}

		// If desired, look for results from our touching query that were not in the real results, and add them
		if(bShowMisses)
		{
			for(int32 i=0; i<CurrentQuery.TouchAllResults.Num(); i++)
			{
				FHitResult& MissResult = CurrentQuery.TouchAllResults[i];
				if(MissResult.Component.IsValid() && !ResultsContainComponent(CurrentQuery.Results, MissResult.Component.Get()))
				{
					ResultList.Add( FCAHitInfo::Make(MissResult, true) );
				}
			}
		}

		// Then sort
		struct FCompareFCAHitInfo
		{
			FORCEINLINE bool operator()( const TSharedPtr<FCAHitInfo> A, const TSharedPtr<FCAHitInfo> B ) const
			{
				check(A.IsValid());
				check(B.IsValid());
				return A->Result.Time < B->Result.Time;
			}
		};

		ResultList.Sort( FCompareFCAHitInfo() );
	}

	// Finally refresh display widget
	ResultListWidget->RequestListRefresh();
}

void SCAQueryDetails::SetCurrentQuery(const FCAQuery& NewQuery)
{
	bDisplayQuery = true;
	CurrentQuery = NewQuery;
	UpdateResultList();	
}

void SCAQueryDetails::ClearCurrentQuery()
{
	bDisplayQuery = false;
	ResultList.Empty();
	UpdateDisplayedBox();
}

FCAQuery* SCAQueryDetails::GetCurrentQuery()
{
	return bDisplayQuery ? &CurrentQuery : NULL;
}

#undef LOCTEXT_NAMESPACE
