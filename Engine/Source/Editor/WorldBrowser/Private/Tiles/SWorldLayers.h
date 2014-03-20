// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class FWorldTileCollectionModel;

//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
class SWorldLayerButton 
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldLayerButton)
	{}
		/** Data for the asset this item represents */
		SLATE_ARGUMENT(FWorldTileLayer, WorldLayer)
		SLATE_ARGUMENT(TSharedPtr<FWorldTileCollectionModel>, InWorldModel)
	SLATE_END_ARGS()

	~SWorldLayerButton();
	void Construct(const FArguments& InArgs);
	void OnCheckStateChanged(ESlateCheckBoxState::Type NewState);
	ESlateCheckBoxState::Type IsChecked() const;
	FReply OnDoubleClicked();
	FReply OnCtrlClicked();
	TSharedRef<SWidget> GetRightClickMenu();
			
private:
	/** The data for this item */
	TSharedPtr<FWorldTileCollectionModel>	WorldModel;
	FWorldTileLayer							WorldLayer;
};


class SNewLayerPopup 
	: public SBorder
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnCreateLayer, const FWorldTileLayer&)
	
	SLATE_BEGIN_ARGS(SNewLayerPopup)
	{}
	SLATE_EVENT(FOnCreateLayer, OnCreateLayer)
	SLATE_ARGUMENT(FString, DefaultName)
	SLATE_END_ARGS()
		
	void Construct(const FArguments& InArgs);

	TOptional<int32> GetStreamingDistance() const
	{
		return LayerData.StreamingDistance;
	}
	
	ESlateCheckBoxState::Type GetDistanceStreamingState() const
	{
		return IsDistanceStreamingEnabled() ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	bool IsDistanceStreamingEnabled() const
	{
		return LayerData.DistanceStreamingEnabled;
	}
	
	void OnDistanceStreamingStateChanged(ESlateCheckBoxState::Type NewState)
	{
		SetDistanceStreamingState(NewState == ESlateCheckBoxState::Checked);
	}

	FText GetLayerName() const
	{
		return FText::FromString(LayerData.Name);
	}
	
private:
	FReply OnClickedCreate();

	void SetLayerName(const FText& InText)
	{
		LayerData.Name = InText.ToString();
	}

	void SetStreamingDistance(int32 InValue)
	{
		LayerData.StreamingDistance = InValue;
	}

	void SetDistanceStreamingState(bool bIsEnabled)
	{
		LayerData.DistanceStreamingEnabled = bIsEnabled;
	}

private:
	/** The delegate to execute when the create button is clicked */
	FOnCreateLayer			OnCreateLayer;
	FWorldTileLayer			LayerData;
};
