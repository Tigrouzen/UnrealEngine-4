// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "ObjectTools.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
#include "EngineTerrainClasses.h"
#include "EngineFoliageClasses.h"
#include "Landscape/LandscapeEdit.h"
#include "Landscape/LandscapeRender.h"
#include "Landscape/LandscapeDataAccess.h"
#include "Landscape/LandscapeSplineProxies.h"
#include "LandscapeEditorModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "LandscapeEdModeTools.h"

//
// FLandscapeToolErosionBase
//
class FLandscapeToolStrokeErosionBase
{
public:
	FLandscapeToolStrokeErosionBase(FEdModeLandscape* InEdMode, const FLandscapeToolTarget& InTarget)
	:	LandscapeInfo(InTarget.LandscapeInfo.Get())
	,	HeightCache(InTarget)
	,	WeightCache(InTarget)
	,	bWeightApplied(InTarget.TargetType != ELandscapeToolTargetType::Heightmap)
	{}

	virtual void Apply(FLevelEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolMousePosition>& MousePositions) = 0;
protected:
	class ULandscapeInfo* LandscapeInfo;
	FLandscapeHeightCache HeightCache;
	FLandscapeFullWeightCache WeightCache;
	bool bWeightApplied;
};

template<class TStrokeClass>
class FLandscapeToolErosionBase : public FLandscapeToolBase<TStrokeClass>
{
public:
	FLandscapeToolErosionBase(class FEdModeLandscape* InEdMode)
	:	FLandscapeToolBase<TStrokeClass>(InEdMode)
	{}

	virtual bool IsValidForTarget(const FLandscapeToolTarget& Target) OVERRIDE 
	{
		// Erosion is applied to all layers
		return true;
	}
};

//
// FLandscapeToolErosion
//

class FLandscapeToolStrokeErosion : public FLandscapeToolStrokeErosionBase
{
public:
	FLandscapeToolStrokeErosion(FEdModeLandscape* InEdMode, const FLandscapeToolTarget& InTarget)
	:	FLandscapeToolStrokeErosionBase(InEdMode, InTarget)
	{}

	virtual void Apply(FLevelEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolMousePosition>& MousePositions) OVERRIDE
	{
		if( !LandscapeInfo )
		{
			return;
		}

		// Get list of verts to update
		TMap<FIntPoint, float> BrushInfo;
		int32 X1, Y1, X2, Y2;
		if( !Brush->ApplyBrush(MousePositions, BrushInfo, X1, Y1, X2, Y2) || MousePositions.Num()==0 )
		{
			return;
		}

		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		const int32 NeighborNum = 4;
		const int32 Iteration = UISettings->ErodeIterationNum;
		const int32 Thickness = UISettings->ErodeSurfaceThickness;
		const int32 LayerNum = LandscapeInfo->Layers.Num();

		HeightCache.CacheData(X1,Y1,X2,Y2);
		TArray<uint16> HeightData;
		HeightCache.GetCachedData(X1,Y1,X2,Y2,HeightData);

		TArray<uint8> WeightDatas; // Weight*Layers...
		WeightCache.CacheData(X1,Y1,X2,Y2);
		WeightCache.GetCachedData(X1,Y1,X2,Y2, WeightDatas, LayerNum);	

		// Apply the brush	
		uint16 Thresh = UISettings->ErodeThresh;
		int32 WeightMoveThresh = FMath::Min<int32>(FMath::Max<int32>(Thickness >> 2, Thresh), Thickness >> 1);

		TArray<float> CenterWeight;
		CenterWeight.Empty(LayerNum);
		CenterWeight.AddUninitialized(LayerNum);
		TArray<float> NeighborWeight;
		NeighborWeight.Empty(NeighborNum*LayerNum);
		NeighborWeight.AddUninitialized(NeighborNum*LayerNum);

		bool bHasChanged = false;
		for (int32 i = 0; i < Iteration; i++)
		{
			bHasChanged = false;
			for( auto It = BrushInfo.CreateIterator(); It; ++It )
			{
				int32 X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f )
				{
					int32 Center = (X-X1) + (Y-Y1)*(1+X2-X1);
					int32 Neighbor[NeighborNum] = {(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)};
					uint32 SlopeTotal = 0;
					uint16 SlopeMax = Thresh;

					for (int32 Idx = 0; Idx < NeighborNum; Idx++)
					{
						if (HeightData[Center] > HeightData[Neighbor[Idx]])
						{
							uint16 Slope = HeightData[Center] - HeightData[Neighbor[Idx]];
							if (Slope*It.Value() > Thresh)
							{
								SlopeTotal += Slope;
								if (SlopeMax < Slope)
								{
									SlopeMax = Slope;
								}
							}
						}
					}

					if (SlopeTotal > 0)
					{
						float Softness = 1.f;
						{
							for (int32 Idx = 0; Idx < LayerNum; Idx++)
							{
								ULandscapeLayerInfoObject* LayerInfo = LandscapeInfo->Layers[Idx].LayerInfoObj;
								if (LayerInfo)
								{
									uint8 Weight = WeightDatas[Center*LayerNum + Idx];
									Softness -= (float)(Weight) / 255.f * LayerInfo->Hardness;
								}
							}
						}
						if (Softness > 0.f)
						{
							//Softness = FMath::Clamp<float>(Softness, 0.f, 1.f);
							float TotalHeightDiff = 0;
							int32 WeightTransfer = FMath::Min<int32>(WeightMoveThresh, SlopeMax - Thresh);
							for (int32 Idx = 0; Idx < NeighborNum; Idx++)
							{
								float TotalWeight = 0.f;
								if (HeightData[Center] > HeightData[Neighbor[Idx]])
								{
									uint16 Slope = HeightData[Center] - HeightData[Neighbor[Idx]];
									if (Slope > Thresh)
									{
										float WeightDiff = Softness * UISettings->ToolStrength * Pressure * ((float)Slope / SlopeTotal) * It.Value();
										//uint16 HeightDiff = (uint16)((SlopeMax - Thresh) * WeightDiff);
										float HeightDiff = (SlopeMax - Thresh) * WeightDiff;
										HeightData[Neighbor[Idx]] += HeightDiff;
										TotalHeightDiff += HeightDiff;

										if (bWeightApplied)
										{
											for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
											{
												float CenterWeight = (float)(WeightDatas[Center*LayerNum + LayerIdx]) / 255.f;
												float Weight = (float)(WeightDatas[Neighbor[Idx]*LayerNum + LayerIdx]) / 255.f;
												NeighborWeight[Idx*LayerNum + LayerIdx] = Weight*(float)Thickness + CenterWeight*WeightDiff*WeightTransfer; // transferred + original...
												TotalWeight += NeighborWeight[Idx*LayerNum + LayerIdx];
											}
											// Need to normalize weight...
											for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
											{
												WeightDatas[Neighbor[Idx]*LayerNum + LayerIdx] = (uint8)(255.f * NeighborWeight[Idx*LayerNum + LayerIdx] / TotalWeight);
											}
										}
									}
								}
							}

							HeightData[Center] -= TotalHeightDiff;

							if (bWeightApplied)
							{
								float TotalWeight = 0.f;
								float WeightDiff = Softness * UISettings->ToolStrength * Pressure * It.Value();

								for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
								{
									float Weight = (float)(WeightDatas[Center*LayerNum + LayerIdx]) / 255.f;
									CenterWeight[LayerIdx] = Weight*Thickness - Weight*WeightDiff*WeightTransfer;
									TotalWeight += CenterWeight[LayerIdx];
								}
								// Need to normalize weight...
								for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
								{
									WeightDatas[Center*LayerNum + LayerIdx] = (uint8)(255.f * CenterWeight[LayerIdx] / TotalWeight);
								}
							}

							bHasChanged = true;
						} // if Softness > 0.f
					} // if SlopeTotal > 0
				}
			}
			if (!bHasChanged)
			{
				break;
			}
		}

		float BrushSizeAdjust = 1.0f;
		if (UISettings->BrushRadius < UISettings->MaximumValueRadius)
		{
			BrushSizeAdjust = UISettings->BrushRadius / UISettings->MaximumValueRadius;
		}

		// Make some noise...
		for( auto It = BrushInfo.CreateIterator(); It; ++It )
		{
			int32 X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() > 0.f )
			{
				FNoiseParameter NoiseParam(0, UISettings->ErosionNoiseScale, It.Value() * Thresh * UISettings->ToolStrength * BrushSizeAdjust);
				float PaintAmount = ELandscapeToolNoiseMode::Conversion((ELandscapeToolNoiseMode::Type)UISettings->ErosionNoiseMode.GetValue(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
				HeightData[(X-X1) + (Y-Y1)*(1+X2-X1)] = FLandscapeHeightCache::ClampValue(HeightData[(X-X1) + (Y-Y1)*(1+X2-X1)] + PaintAmount);
			}
		}

		HeightCache.SetCachedData(X1,Y1,X2,Y2,HeightData);
		HeightCache.Flush();
		if (bWeightApplied)
		{
			WeightCache.SetCachedData(X1,Y1,X2,Y2,WeightDatas, LayerNum, ELandscapeLayerPaintingRestriction::None);
		}
		WeightCache.Flush();
	}
};

class FLandscapeToolErosion : public FLandscapeToolErosionBase<FLandscapeToolStrokeErosion>
{
public:
	FLandscapeToolErosion(class FEdModeLandscape* InEdMode)
	:	FLandscapeToolErosionBase(InEdMode)
	{}

	virtual const TCHAR* GetToolName() OVERRIDE { return TEXT("Erosion"); }
	virtual FText GetDisplayName() OVERRIDE { return NSLOCTEXT("UnrealEd", "LandscapeMode_Erosion", "Erosion"); };

};

//
// FLandscapeToolHydraErosion
//

class FLandscapeToolStrokeHydraErosion : public FLandscapeToolStrokeErosionBase
{
public:
	FLandscapeToolStrokeHydraErosion(FEdModeLandscape* InEdMode, const FLandscapeToolTarget& InTarget)
	:	FLandscapeToolStrokeErosionBase(InEdMode, InTarget)
	{}

	virtual void Apply(FLevelEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolMousePosition>& MousePositions) OVERRIDE
	{
		if( !LandscapeInfo )
		{
			return;
		}

		// Get list of verts to update
		TMap<FIntPoint, float> BrushInfo;
		int32 X1, Y1, X2, Y2;
		if( !Brush->ApplyBrush(MousePositions,BrushInfo, X1, Y1, X2, Y2) || MousePositions.Num()==0 )
		{
			return;
		}

		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		const int32 NeighborNum = 8;
		const int32 LayerNum = LandscapeInfo->Layers.Num();

		const int32 Iteration = UISettings->HErodeIterationNum;
		const uint16 RainAmount = UISettings->RainAmount;
		const float DissolvingRatio = 0.07 * UISettings->ToolStrength * Pressure;  //0.01;
		const float EvaporateRatio = 0.5;
		const float SedimentCapacity = 0.10 * UISettings->SedimentCapacity; //DissolvingRatio; //0.01;

		HeightCache.CacheData(X1,Y1,X2,Y2);
		TArray<uint16> HeightData;
		HeightCache.GetCachedData(X1,Y1,X2,Y2,HeightData);

		// Apply the brush
		TArray<uint16> WaterData;
		WaterData.Empty((1+X2-X1)*(1+Y2-Y1));
		WaterData.AddZeroed((1+X2-X1)*(1+Y2-Y1));
		TArray<uint16> SedimentData;
		SedimentData.Empty((1+X2-X1)*(1+Y2-Y1));
		SedimentData.AddZeroed((1+X2-X1)*(1+Y2-Y1));

		// Only initial raining works better...
		FNoiseParameter NoiseParam(0, UISettings->RainDistScale, RainAmount);
		for( auto It = BrushInfo.CreateIterator(); It; ++It )
		{
			int32 X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() >= 1.f )
			{
				float PaintAmount = ELandscapeToolNoiseMode::Conversion((ELandscapeToolNoiseMode::Type)UISettings->RainDistMode.GetValue(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
				if (PaintAmount > 0) // Raining only for positive region...
					WaterData[(X-X1) + (Y-Y1)*(1+X2-X1)] += PaintAmount;
			}
		}

		for (int32 i = 0; i < Iteration; i++)
		{
			bool bWaterExist = false;
			for( auto It = BrushInfo.CreateIterator(); It; ++It )
			{
				int32 X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f)
				{
					int32 Center = (X-X1) + (Y-Y1)*(1+X2-X1);

					int32 Neighbor[NeighborNum] = {
						(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)
						,(X-1-X1) + (Y-1-Y1)*(1+X2-X1), (X+1-X1) + (Y+1-Y1)*(1+X2-X1), (X+1-X1) + (Y-1-Y1)*(1+X2-X1), (X-1-X1) + (Y+1-Y1)*(1+X2-X1)
					};

					// Dissolving...				
					float DissolvedAmount = DissolvingRatio * WaterData[Center] * It.Value();
					if (DissolvedAmount > 0 && HeightData[Center] >= DissolvedAmount)
					{
						HeightData[Center] -= DissolvedAmount;
						SedimentData[Center] += DissolvedAmount;
					}

					uint32 TotalHeightDiff = 0;
					uint32 TotalAltitudeDiff = 0;
					uint32 AltitudeDiff[NeighborNum] = {0};
					uint32 TotalWaterDiff = 0;
					uint32 TotalSedimentDiff = 0;

					uint32 Altitude = HeightData[Center]+WaterData[Center];
					float AverageAltitude = 0;
					uint32 LowerNeighbor = 0;
					for (int32 Idx = 0; Idx < NeighborNum; Idx++)
					{
						uint32 NeighborAltitude = HeightData[Neighbor[Idx]]+WaterData[Neighbor[Idx]];
						if (Altitude > NeighborAltitude)
						{
							AltitudeDiff[Idx] = Altitude - NeighborAltitude;
							TotalAltitudeDiff += AltitudeDiff[Idx];
							LowerNeighbor++;
							AverageAltitude += NeighborAltitude;
							if (HeightData[Center] > HeightData[Neighbor[Idx]])
							{
								TotalHeightDiff += HeightData[Center] - HeightData[Neighbor[Idx]];
							}
						}
						else
						{
							AltitudeDiff[Idx] = 0;
						}
					}

					// Water Transfer
					if (LowerNeighbor > 0)
					{
						AverageAltitude /= (LowerNeighbor);
						// This is not mathematically correct, but makes good result
						if (TotalHeightDiff)
						{
							AverageAltitude *= (1.f - 0.1 * UISettings->ToolStrength * Pressure);
							//AverageAltitude -= 4000.f * UISettings->ToolStrength;
						}

						uint32 WaterTransfer = FMath::Min<uint32>(WaterData[Center], Altitude - (uint32)AverageAltitude) * It.Value();

						for (int32 Idx = 0; Idx < NeighborNum; Idx++)
						{
							if (AltitudeDiff[Idx] > 0)
							{
								uint32 WaterDiff = (uint32)(WaterTransfer * (float)AltitudeDiff[Idx] / TotalAltitudeDiff);
								WaterData[Neighbor[Idx]] += WaterDiff;
								TotalWaterDiff += WaterDiff;
								uint32 SedimentDiff = (uint32)(SedimentData[Center] * (float)WaterDiff / WaterData[Center]);
								SedimentData[Neighbor[Idx]] += SedimentDiff;
								TotalSedimentDiff += SedimentDiff;
							}
						}

						WaterData[Center] -= TotalWaterDiff;
						SedimentData[Center] -= TotalSedimentDiff;
					}

					// evaporation
					if (WaterData[Center] > 0)
					{
						bWaterExist = true;
						WaterData[Center] = (uint16)(WaterData[Center] * (1.f - EvaporateRatio));
						float SedimentCap = SedimentCapacity*WaterData[Center];
						float SedimentDiff = SedimentData[Center] - SedimentCap;
						if (SedimentDiff > 0)
						{
							SedimentData[Center] -= SedimentDiff;
							HeightData[Center] = FMath::Clamp<uint16>(HeightData[Center]+SedimentDiff, 0, 65535);
						}
					}
				}	
			}

			if (!bWaterExist) 
			{
				break;
			}
		}

		if (UISettings->bHErosionDetailSmooth)
		{
			//LowPassFilter<uint16>(X1, Y1, X2, Y2, BrushInfo, HeightData, UISettings->HErosionDetailScale, UISettings->ToolStrength * Pressure);
			LowPassFilter<uint16>(X1, Y1, X2, Y2, BrushInfo, HeightData, UISettings->HErosionDetailScale, 1.0f);
		}

		HeightCache.SetCachedData(X1,Y1,X2,Y2,HeightData);
		HeightCache.Flush();
	}
};

class FLandscapeToolHydraErosion : public FLandscapeToolErosionBase<FLandscapeToolStrokeHydraErosion>
{
public:
	FLandscapeToolHydraErosion(class FEdModeLandscape* InEdMode)
	:	FLandscapeToolErosionBase(InEdMode)
	{}

	virtual const TCHAR* GetToolName() OVERRIDE { return TEXT("HydraulicErosion"); }
	virtual FText GetDisplayName() OVERRIDE { return NSLOCTEXT("UnrealEd", "LandscapeMode_HydraErosion", "Hydraulic Erosion"); };

};

//
// Toolset initialization
//
void FEdModeLandscape::IntializeToolSet_Erosion()
{
	FLandscapeToolSet* ToolSet_Erosion = new(LandscapeToolSets) FLandscapeToolSet(TEXT("ToolSet_Erosion"));
	ToolSet_Erosion->AddTool(new FLandscapeToolErosion(this));

	ToolSet_Erosion->ValidBrushes.Add("BrushSet_Circle");
	ToolSet_Erosion->ValidBrushes.Add("BrushSet_Alpha");
	ToolSet_Erosion->ValidBrushes.Add("BrushSet_Pattern");
}

void FEdModeLandscape::IntializeToolSet_HydraErosion()
{
	FLandscapeToolSet* ToolSet_HydraErosion = new(LandscapeToolSets) FLandscapeToolSet(TEXT("ToolSet_HydraErosion"));
	ToolSet_HydraErosion->AddTool(new FLandscapeToolHydraErosion(this));

	ToolSet_HydraErosion->ValidBrushes.Add("BrushSet_Circle");
	ToolSet_HydraErosion->ValidBrushes.Add("BrushSet_Alpha");
	ToolSet_HydraErosion->ValidBrushes.Add("BrushSet_Pattern");
}
