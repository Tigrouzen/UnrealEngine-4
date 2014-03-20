// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2009 - 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
	Main implementation of FFbxImporter : import FBX data to Unreal
=============================================================================*/

#include "UnrealEd.h"
#include "FeedbackContextEditor.h"

#include "Factories.h"
#include "Engine.h"
#include "SkelImport.h"
#include "FbxImporter.h"
#include "FbxOptionWindow.h"
#include "Mainframe.h"

DEFINE_LOG_CATEGORY(LogFbx);

#define LOCTEXT_NAMESPACE "FbxMainImport"

namespace UnFbx
{

TSharedPtr<FFbxImporter> FFbxImporter::StaticInstance;



FBXImportOptions* GetImportOptions( UnFbx::FFbxImporter* FbxImporter, UFbxImportUI* ImportUI, bool & bShowOption, const FString& FullPath, bool& OutOperationCanceled, bool bForceImportType, EFBXImportType ImportType )
{
	OutOperationCanceled = false;

	if ( bShowOption )
	{
		UnFbx::FBXImportOptions* ImportOptions = FbxImporter->GetImportOptions();

		// if Skeleton was set by outside, please make sure copy back to UI
		if ( ImportOptions->SkeletonForAnimation )
		{
			ImportUI->Skeleton = ImportOptions->SkeletonForAnimation;
		}
		else
		{
			ImportUI->Skeleton = NULL;
		}

		if ( ImportOptions->PhysicsAsset )
		{
			ImportUI->PhysicsAsset = ImportOptions->PhysicsAsset;
		}
		else
		{
			ImportUI->PhysicsAsset = NULL;
		}

		TSharedPtr<SWindow> ParentWindow;
		// Check if the main frame is loaded.  When using the old main frame it may not be.
		if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
			ParentWindow = MainFrame.GetParentWindow();
		}

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(NSLOCTEXT("UnrealEd", "FBXImportOpionsTitle", "FBX Import Options"))
			.SizingRule( ESizingRule::Autosized );

		TSharedPtr<SFbxOptionWindow> FbxOptionWindow;
		Window->SetContent
		(
			SAssignNew(FbxOptionWindow, SFbxOptionWindow)
			.ImportUI(ImportUI)
			.WidgetWindow(Window)
			.FullPath(FullPath)
			.ForcedImportType( bForceImportType ? TOptional<EFBXImportType>( ImportType ) : TOptional<EFBXImportType>() )
		);

		// @todo: we can make this slow as showing progress bar later
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		ImportUI->SaveConfig();

		if( ImportUI->StaticMeshImportData )
		{
			ImportUI->StaticMeshImportData->SaveConfig();
		}

		if( ImportUI->SkeletalMeshImportData )
		{
			ImportUI->SkeletalMeshImportData->SaveConfig();
		}

		if( ImportUI->AnimSequenceImportData )
		{
			ImportUI->AnimSequenceImportData->SaveConfig();
		}

		if( ImportUI->TextureImportData )
		{
			ImportUI->TextureImportData->SaveConfig();
		}

		// after showing an option, we should turn this off, so it does not show for batch import
		bShowOption = false;

		if (FbxOptionWindow->ShouldImport())
		{
			// open dialog
			// see if it's canceled
			ApplyImportUIToImportOptions(ImportUI, *ImportOptions);

			return ImportOptions;
		}
		else
		{
			OutOperationCanceled = true;
		}
	}
	else
	{
		return FbxImporter->GetImportOptions();
	}

	return NULL;

}

void ApplyImportUIToImportOptions(UFbxImportUI* ImportUI, FBXImportOptions& InOutImportOptions)
{
	check(ImportUI);
	InOutImportOptions.bImportMaterials = ImportUI->bImportMaterials;
	InOutImportOptions.bInvertNormalMap = ImportUI->TextureImportData->bInvertNormalMaps;
	InOutImportOptions.bImportTextures = ImportUI->bImportTextures;
	InOutImportOptions.bUsedAsFullName = ImportUI->bOverrideFullName;
	InOutImportOptions.bImportAnimations = ImportUI->bImportAnimations;
	InOutImportOptions.SkeletonForAnimation = ImportUI->Skeleton;
	if ( ImportUI->MeshTypeToImport == FBXIT_StaticMesh )
	{
		InOutImportOptions.NormalImportMethod = ImportUI->StaticMeshImportData->NormalImportMethod;
	}
	else if ( ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh )
	{
		InOutImportOptions.NormalImportMethod = ImportUI->SkeletalMeshImportData->NormalImportMethod;
	}
	else
	{
		InOutImportOptions.NormalImportMethod = FBXNIM_ComputeNormals;
	}
	// only re-sample if they don't want to use default sample rate
	InOutImportOptions.bResample = ImportUI->bUseDefaultSampleRate==false;
	InOutImportOptions.bImportMorph = ImportUI->SkeletalMeshImportData->bImportMorphTargets;
	InOutImportOptions.bUpdateSkeletonReferencePose = ImportUI->SkeletalMeshImportData->bUpdateSkeletonReferencePose;
	InOutImportOptions.bImportRigidMesh = ImportUI->bImportRigidMesh;
	InOutImportOptions.bUseT0AsRefPose = ImportUI->SkeletalMeshImportData->bUseT0AsRefPose;
	InOutImportOptions.bPreserveSmoothingGroups = ImportUI->SkeletalMeshImportData->bPreserveSmoothingGroups;
	InOutImportOptions.bKeepOverlappingVertices = ImportUI->SkeletalMeshImportData->bKeepOverlappingVertices;
	InOutImportOptions.bCombineToSingle = ImportUI->bCombineMeshes;
	InOutImportOptions.bReplaceVertexColors = ImportUI->StaticMeshImportData->bReplaceVertexColors;
	InOutImportOptions.bRemoveDegenerates = ImportUI->StaticMeshImportData->bRemoveDegenerates;
	InOutImportOptions.bOneConvexHullPerUCX = ImportUI->StaticMeshImportData->bOneConvexHullPerUCX;
	InOutImportOptions.StaticMeshLODGroup = ImportUI->StaticMeshImportData->StaticMeshLODGroup;
	InOutImportOptions.bImportMeshesInBoneHierarchy = ImportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy;
	InOutImportOptions.bCreatePhysicsAsset = ImportUI->bCreatePhysicsAsset;
	InOutImportOptions.PhysicsAsset = ImportUI->PhysicsAsset;
	// animation options
	InOutImportOptions.AnimationLengthImportType = ImportUI->AnimSequenceImportData->AnimationLength;
	InOutImportOptions.AnimationRange.X = ImportUI->AnimSequenceImportData->StartFrame;
	InOutImportOptions.AnimationRange.Y = ImportUI->AnimSequenceImportData->EndFrame;
	InOutImportOptions.AnimationName = ImportUI->AnimationName;
	InOutImportOptions.bPreserveLocalTransform = ImportUI->bPreserveLocalTransform;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
FFbxImporter::FFbxImporter()
	: bFirstMesh(true)
	, Importer( NULL )
	, ImportOptions(NULL)
	, GeometryConverter(NULL)
	, Scene(NULL)
	, SdkManager(NULL)
	, Logger(NULL)
{
	// Create the SdkManager
	SdkManager = FbxManager::Create();
	
	// create an IOSettings object
	FbxIOSettings * ios = FbxIOSettings::Create(SdkManager, IOSROOT );
	SdkManager->SetIOSettings(ios);

	// Create the geometry converter
	GeometryConverter = new FbxGeometryConverter(SdkManager);
	Scene = NULL;
	
	ImportOptions = new FBXImportOptions();
	FMemory::MemZero(*ImportOptions);
	
	CurPhase = NOTSTARTED;
}
	
//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
FFbxImporter::~FFbxImporter()
{
	CleanUp();
}

const float FFbxImporter::SCALE_TOLERANCE = 0.000001;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
FFbxImporter* FFbxImporter::GetInstance()
{
	if (!StaticInstance.IsValid())
	{
		StaticInstance = MakeShareable( new FFbxImporter() );
	}
	return StaticInstance.Get();
}

void FFbxImporter::DeleteInstance()
{
	StaticInstance.Reset();
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void FFbxImporter::CleanUp()
{
	ClearTokenizedErrorMessages();
	ReleaseScene();
	
	delete GeometryConverter;
	GeometryConverter = NULL;
	delete ImportOptions;
	ImportOptions = NULL;

	if (SdkManager)
	{
		SdkManager->Destroy();
	}
	SdkManager = NULL;
	Logger = NULL;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void FFbxImporter::ReleaseScene()
{
	if (Importer)
	{
		Importer->Destroy();
		Importer = NULL;
	}
	
	if (Scene)
	{
		Scene->Destroy();
		Scene = NULL;
	}
	
	// reset
	CollisionModels.Clear();
	CurPhase = NOTSTARTED;
	bFirstMesh = true;
}

FBXImportOptions* UnFbx::FFbxImporter::GetImportOptions() const
{
	return ImportOptions;
}

int32 FFbxImporter::GetImportType(const FString& InFilename)
{
	int32 Result = -1; // Default to invalid
	FString Filename = InFilename;

	// Prioritized in the order of SkeletalMesh > StaticMesh > Animation (only if animation data is found)
	if (OpenFile(Filename, true))
	{
		FbxStatistics Statistics;
		Importer->GetStatistics(&Statistics);
		int32 ItemIndex;
		FbxString ItemName;
		int32 ItemCount;
		bool bHasAnimation = false;

		for ( ItemIndex = 0; ItemIndex < Statistics.GetNbItems(); ItemIndex++ )
		{
			Statistics.GetItemPair(ItemIndex, ItemName, ItemCount);
			const FString NameBuffer(ItemName.Buffer());
			UE_LOG(LogFbx, Log, TEXT("ItemName: %s, ItemCount : %d"), *NameBuffer, ItemCount);
		}

		for ( ItemIndex = 0; ItemIndex < Statistics.GetNbItems(); ItemIndex++ )
		{
			Statistics.GetItemPair(ItemIndex, ItemName, ItemCount);
			const char* NameBuffer = ItemName.Buffer();
			if ( ItemName == "Deformer" && ItemCount > 0 )
			{
				// if SkeletalMesh is found, just return
				Result = 1;
				break;
			}
			// if Geometry is found, sets it, but it can be overwritten by Deformer
			else if ( ItemName == "Geometry" && ItemCount > 0)
			{
				// let it still loop through even if Geometry is found
				// Deformer can overwrite this information
				Result = 0;
			}
			// Check for animation data. It can be overwritten by Geometry or Deformer
			else if ( (ItemName == "AnimationStack" || ItemName == "AnimationLayer" || ItemName == "AnimationCurve" || ItemName == "AnimationCurveNode") && ItemCount > 0 )
			{
				bHasAnimation = true;
			}
		}
		Importer->Destroy();
		Importer = NULL;
		CurPhase = NOTSTARTED;

		// In case no Geometry was found, check for animation (FBX can still contain mesh data though)
		if ( Result == -1 )
		{
			// If animation data is found, set the result to 2, otherwise default to static mesh
			Result = bHasAnimation ? 2 : 0;
		}
	}
	
	return Result; 
}

bool FFbxImporter::GetSceneInfo(FString Filename, FbxSceneInfo& SceneInfo)
{
	bool Result = true;
	FFeedbackContextEditor FbxImportWarn;
	FbxImportWarn.BeginSlowTask( NSLOCTEXT("FbxImporter", "BeginGetSceneInfoTask", "Parse FBX file to get scene info"), true );
	
	bool bSceneInfo = true;
	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile( Filename, false, bSceneInfo ))
		{
			Result = false;
			break;
		}
		FbxImportWarn.UpdateProgress( 40, 100 );
	case FILEOPENED:
		if (!ImportFile(Filename))
		{
			Result = false;
			break;
		}
		FbxImportWarn.UpdateProgress( 90, 100 );
	case IMPORTED:
	
	default:
		break;
	}
	
	if (Result)
	{
		FbxTimeSpan GlobalTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);
		
		TArray<FbxNode*> LinkNodes;
		
		SceneInfo.TotalMaterialNum = Scene->GetMaterialCount();
		SceneInfo.TotalTextureNum = Scene->GetTextureCount();
		SceneInfo.TotalGeometryNum = 0;
		SceneInfo.NonSkinnedMeshNum = 0;
		SceneInfo.SkinnedMeshNum = 0;
		for ( int32 GeometryIndex = 0; GeometryIndex < Scene->GetGeometryCount(); GeometryIndex++ )
		{
			FbxGeometry * Geometry = Scene->GetGeometry(GeometryIndex);
			
			if (Geometry->GetAttributeType() == FbxNodeAttribute::eMesh)
			{
				FbxNode* GeoNode = Geometry->GetNode();
				bool bIsLinkNode = false;

				// check if this geometry node is used as link
				for ( int32 i = 0; i < LinkNodes.Num(); i++ )
				{
					if ( GeoNode == LinkNodes[i])
					{
						bIsLinkNode = true;
						break;
					}
				}
				// if the geometry node is used as link, ignore it
				if (bIsLinkNode)
				{
					continue;
				}

				SceneInfo.TotalGeometryNum++;
				
				FbxMesh* Mesh = (FbxMesh*)Geometry;
				SceneInfo.MeshInfo.AddUninitialized();
				FbxMeshInfo& MeshInfo = SceneInfo.MeshInfo.Last();
				MeshInfo.Name = MakeName(GeoNode->GetName());
				MeshInfo.bTriangulated = Mesh->IsTriangleMesh();
				MeshInfo.MaterialNum = GeoNode->GetMaterialCount();
				MeshInfo.FaceNum = Mesh->GetPolygonCount();
				MeshInfo.VertexNum = Mesh->GetControlPointsCount();
				
				// LOD info
				MeshInfo.LODGroup = NULL;
				FbxNode* ParentNode = GeoNode->GetParent();
				if ( ParentNode->GetNodeAttribute() && ParentNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
				{
					FbxNodeAttribute* LODGroup = ParentNode->GetNodeAttribute();
					MeshInfo.LODGroup = MakeName(ParentNode->GetName());
					for (int32 LODIndex = 0; LODIndex < ParentNode->GetChildCount(); LODIndex++)
					{
						if ( GeoNode == ParentNode->GetChild(LODIndex))
						{
							MeshInfo.LODLevel = LODIndex;
							break;
						}
					}
				}
				
				// skeletal mesh
				if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
				{
					SceneInfo.SkinnedMeshNum++;
					MeshInfo.bIsSkelMesh = true;
					MeshInfo.MorphNum = Mesh->GetShapeCount();
					// skeleton root
					FbxSkin* Skin = (FbxSkin*)Mesh->GetDeformer(0, FbxDeformer::eSkin);
					FbxNode* Link = Skin->GetCluster(0)->GetLink();
					while (Link->GetParent() && Link->GetParent()->GetSkeleton())
					{
						Link = Link->GetParent();
					}
					MeshInfo.SkeletonRoot = MakeName(Link->GetName());
					MeshInfo.SkeletonElemNum = Link->GetChildCount(true);
					
					FbxTimeSpan AnimTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);
					Link->GetAnimationInterval(AnimTimeSpan);
					GlobalTimeSpan.UnionAssignment(AnimTimeSpan);
				}
				else
				{
					SceneInfo.NonSkinnedMeshNum++;
					MeshInfo.bIsSkelMesh = false;
					MeshInfo.SkeletonRoot = NULL;
				}
			}
		}
		
		// TODO: display multiple anim stack
		SceneInfo.TakeName = NULL;
		for( int32 AnimStackIndex = 0; AnimStackIndex < Scene->GetSrcObjectCount(FbxAnimStack::ClassId); AnimStackIndex++ )
		{
			FbxAnimStack* CurAnimStack = FbxCast<FbxAnimStack>(Scene->GetSrcObject(FbxAnimStack::ClassId, 0));
			// TODO: skip empty anim stack
			const char* AnimStackName = CurAnimStack->GetName();
			SceneInfo.TakeName = new char[FCStringAnsi::Strlen(AnimStackName) + 1];

			FCStringAnsi::Strcpy(SceneInfo.TakeName, FCStringAnsi::Strlen(AnimStackName) + 1, AnimStackName);
		}
		SceneInfo.FrameRate = FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode());
		
		if ( GlobalTimeSpan.GetDirection() == FBXSDK_TIME_FORWARD)
		{
			SceneInfo.TotalTime = (GlobalTimeSpan.GetDuration().GetMilliSeconds())/1000.f * SceneInfo.FrameRate;
		}
		else
		{
			SceneInfo.TotalTime = 0;
		}
	}
	
	FbxImportWarn.EndSlowTask();
	return Result;
}


bool FFbxImporter::OpenFile(FString Filename, bool bParseStatistics, bool bForSceneInfo )
{
	bool Result = true;
	
	if (CurPhase != NOTSTARTED)
	{
		// something went wrong
		return false;
	}

	int32 SDKMajor,  SDKMinor,  SDKRevision;

	// Create an importer.
	Importer = FbxImporter::Create(SdkManager,"");

	// Get the version number of the FBX files generated by the
	// version of FBX SDK that you are using.
	FbxManager::GetFileFormatVersion(SDKMajor, SDKMinor, SDKRevision);

	// Initialize the importer by providing a filename.
	if (bParseStatistics)
	{
		Importer->ParseForStatistics(true);
	}
	
	const bool bImportStatus = Importer->Initialize(TCHAR_TO_UTF8(*Filename));

	if( !bImportStatus )  // Problem with the file to be imported
	{
		UE_LOG(LogFbx, Error,TEXT("Call to KFbxImporter::Initialize() failed."));
		UE_LOG(LogFbx, Warning, TEXT("Error returned: %s"), ANSI_TO_TCHAR(Importer->GetLastErrorString()));

		if ((FbxIO::EErrorCode)Importer->GetLastErrorID() ==
			FbxIO::eFileVersionNotSupportedYet ||
			(FbxIO::EErrorCode)Importer->GetLastErrorID() ==
			FbxIO::eFileVersionNotSupportedAnymore)
		{
			UE_LOG(LogFbx, Warning, TEXT("FBX version number for this FBX SDK is %d.%d.%d"),
				SDKMajor, SDKMinor, SDKRevision);
		}

		return false;
	}

	// Skip the version check if we are just parsing for information or scene info.
	if( !bParseStatistics && !bForSceneInfo )
	{
		int32 FileMajor,  FileMinor,  FileRevision;
		Importer->GetFileVersion(FileMajor, FileMinor, FileRevision);

		int32 FileVersion = (FileMajor << 16 | FileMinor << 8 | FileRevision);
		int32 SDKVersion = (SDKMajor << 16 | SDKMinor << 8 | SDKRevision);

		if( FileVersion != SDKVersion )
		{

			// Appending the SDK version to the config key causes the warning to automatically reappear even if previously suppressed when the SDK version we use changes. 
			FString ConfigStr = FString::Printf( TEXT("Warning_OutOfDateFBX_%d"), SDKVersion );

			FString FileVerStr = FString::Printf( TEXT("%d.%d.%d"), FileMajor, FileMinor, FileRevision );
			FString SDKVerStr  = FString::Printf( TEXT("%d.%d.%d"), SDKMajor, SDKMinor, SDKRevision );

			const FText WarningText = FText::Format(
				NSLOCTEXT("UnrealEd", "Warning_OutOfDateFBX", "An out of date FBX has been detected.\nImporting different versions of FBX files than the SDK version can cause undesirable results.\n\nFile Version: {0}\nSDK Version: {1}" ),
				FText::FromString(FileVerStr), FText::FromString(SDKVerStr) );
			
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, WarningText));
		}
	}

	CurPhase = FILEOPENED;
	// Destroy the importer
	//Importer->Destroy();

	return Result;
}

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(SdkManager->GetIOSettings()))
#endif

bool FFbxImporter::ImportFile(FString Filename)
{
	bool Result = true;
	
	bool bStatus;
	
	FileBasePath = FPaths::GetPath(Filename);

	// Create the Scene
	Scene = FbxScene::Create(SdkManager,"");
	UE_LOG(LogFbx, Log, TEXT("Loading FBX Scene from %s"), *Filename);

	int32 FileMajor, FileMinor, FileRevision;

	IOS_REF.SetBoolProp(IMP_FBX_MATERIAL,		true);
	IOS_REF.SetBoolProp(IMP_FBX_TEXTURE,		 true);
	IOS_REF.SetBoolProp(IMP_FBX_LINK,			true);
	IOS_REF.SetBoolProp(IMP_FBX_SHAPE,		   true);
	IOS_REF.SetBoolProp(IMP_FBX_GOBO,			true);
	IOS_REF.SetBoolProp(IMP_FBX_ANIMATION,	   true);
	IOS_REF.SetBoolProp(IMP_SKINS,			   true);
	IOS_REF.SetBoolProp(IMP_DEFORMATION,		 true);
	IOS_REF.SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
	IOS_REF.SetBoolProp(IMP_TAKE,				true);

	// Import the scene.
	bStatus = Importer->Import(Scene);

	// Get the version number of the FBX file format.
	Importer->GetFileVersion(FileMajor, FileMinor, FileRevision);

	// output result
	if(bStatus)
	{
		UE_LOG(LogFbx, Log, TEXT("FBX Scene Loaded Succesfully"));
		CurPhase = IMPORTED;
	}
	else
	{
		ErrorMessage = ANSI_TO_TCHAR(Importer->GetLastErrorString());
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_TriangulatingFailed", "FBX Scene Loading Failed : '{0}'"), FText::FromString(ErrorMessage))));
		CleanUp();
		Result = false;
		CurPhase = NOTSTARTED;
	}
	
	Importer->Destroy();
	Importer = NULL;
	
	return Result;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
bool FFbxImporter::ImportFromFile(const TCHAR* Filename)
{
	bool Result = true;
	// Converts the FBX data to Z-up, X-forward, Y-left.  Unreal is the same except with Y-right, 
	// but the conversion to left-handed coordinates is not working properly
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd;
	const FbxAxisSystem UnrealZUp(FbxAxisSystem::eZAxis, FrontVector, FbxAxisSystem::eRightHanded);

	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile(FString(Filename), false))
		{
			Result = false;
			break;
		}
	case FILEOPENED:
		if (!ImportFile(FString(Filename)))
		{
			Result = false;
			CurPhase = NOTSTARTED;
			break;
		}
	case IMPORTED:
		// convert axis to Z-up
		FbxRootNodeUtility::RemoveAllFbxRoots( Scene );
		UnrealZUp.ConvertScene( Scene );

		// Convert the scene's units to what is used in this program, if needed.
		// The base unit used in both FBX and Unreal is centimeters.  So unless the units 
		// are already in centimeters (ie: scalefactor 1.0) then it needs to be converted
		//if( FbxScene->GetGlobalSettings().GetSystemUnit().GetScaleFactor() != 1.0 )
		//{
		//	KFbxSystemUnit::cm.ConvertScene( FbxScene );
		//}


		// convert name to unreal-supported format
		// actually, crashes...
		//KFbxSceneRenamer renamer(FbxScene);
		//renamer.ResolveNameClashing(false,false,true,true,true,KString(),"TestBen",false,false);
		
	default:
		break;
	}
	
	return Result;
}

ANSICHAR* FFbxImporter::MakeName(const ANSICHAR* Name)
{
	int SpecialChars[] = {'.', ',', '/', '`', '%'};

	int len = FCStringAnsi::Strlen(Name);
	ANSICHAR* TmpName = new ANSICHAR[len+1];
	
	FCStringAnsi::Strcpy(TmpName, len + 1, Name);

	for ( int32 i = 0; i < 5; i++ )
	{
		ANSICHAR* CharPtr = TmpName;
		while ( (CharPtr = FCStringAnsi::Strchr(CharPtr,SpecialChars[i])) != NULL )
		{
			CharPtr[0] = '_';
		}
	}

	// Remove namespaces
	ANSICHAR* NewName;
	NewName = FCStringAnsi::Strchr(TmpName, ':');
	  
	// there may be multiple namespace, so find the last ':'
	while (NewName && FCStringAnsi::Strchr(NewName + 1, ':'))
	{
		NewName = FCStringAnsi::Strchr(NewName + 1, ':');
	}

	if (NewName)
	{
		return NewName + 1;
	}

	return TmpName;
}

FName FFbxImporter::MakeNameForMesh(FString InName, FbxObject* FbxObject)
{
	FName OutputName;

	// "Name" field can't be empty
	if (ImportOptions->bUsedAsFullName && InName != FString("None"))
	{
		OutputName = *InName;
	}
	else
	{
		char Name[MAX_SPRINTF];
		int SpecialChars[] = {'.', ',', '/', '`', '%'};

		FCStringAnsi::Sprintf(Name, "%s", FbxObject->GetName());

		for ( int32 i = 0; i < 5; i++ )
		{
			char* CharPtr = Name;
			while ( (CharPtr = FCStringAnsi::Strchr(CharPtr,SpecialChars[i])) != NULL )
			{
				CharPtr[0] = '_';
			}
		}

		// for mesh, replace ':' with '_' because Unreal doesn't support ':' in mesh name
		char* NewName = NULL;
		NewName = FCStringAnsi::Strchr (Name, ':');

		if (NewName)
		{
			char* Tmp;
			Tmp = NewName;
			while (Tmp)
			{

				// Always remove namespaces
				NewName = Tmp + 1;
				
				// there may be multiple namespace, so find the last ':'
				Tmp = FCStringAnsi::Strchr(NewName + 1, ':');
			}
		}
		else
		{
			NewName = Name;
		}

		if ( InName == FString("None"))
		{
			OutputName = FName( *FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(NewName )) );
		}
		else
		{
			OutputName = FName( *FString::Printf(TEXT("%s_%s"), *InName,ANSI_TO_TCHAR(NewName)) );
		}
	}
	
	return OutputName;
}

FbxAMatrix FFbxImporter::ComputeTotalMatrix(FbxNode* Node)
{
	FbxAMatrix Geometry;
	FbxVector4 Translation, Rotation, Scaling;
	Translation = Node->GetGeometricTranslation(FbxNode::eSourcePivot);
	Rotation = Node->GetGeometricRotation(FbxNode::eSourcePivot);
	Scaling = Node->GetGeometricScaling(FbxNode::eSourcePivot);
	Geometry.SetT(Translation);
	Geometry.SetR(Rotation);
	Geometry.SetS(Scaling);

	//For Single Matrix situation, obtain transfrom matrix from eDESTINATION_SET, which include pivot offsets and pre/post rotations.
	FbxAMatrix& GlobalTransform = Scene->GetEvaluator()->GetNodeGlobalTransform(Node);
	
	FbxAMatrix TotalMatrix;
	TotalMatrix = GlobalTransform * Geometry;

	return TotalMatrix;
}

bool FFbxImporter::IsOddNegativeScale(FbxAMatrix& TotalMatrix)
{
	FbxVector4 Scale = TotalMatrix.GetS();
	int32 NegativeNum = 0;

	if (Scale[0] < 0) NegativeNum++;
	if (Scale[1] < 0) NegativeNum++;
	if (Scale[2] < 0) NegativeNum++;

	return NegativeNum == 1 || NegativeNum == 3;
}

/**
* Recursively get skeletal mesh count
*
* @param Node Root node to find skeletal meshes
* @return int32 skeletal mesh count
*/
int32 GetFbxSkeletalMeshCount(FbxNode* Node)
{
	int32 SkeletalMeshCount = 0;
	if (Node->GetMesh() && (Node->GetMesh()->GetDeformerCount(FbxDeformer::eSkin)>0))
	{
		SkeletalMeshCount = 1;
	}

	int32 ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		SkeletalMeshCount += GetFbxSkeletalMeshCount(Node->GetChild(ChildIndex));
	}

	return SkeletalMeshCount;
}

/**
* Get mesh count (including static mesh and skeletal mesh, except collision models) and find collision models
*
* @param Node Root node to find meshes
* @return int32 mesh count
*/
int32 FFbxImporter::GetFbxMeshCount( FbxNode* Node, bool bCountLODs, int32& OutNumLODGroups )
{
	// Is this node an LOD group
	bool bLODGroup = Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup;
	
	if( bLODGroup )
	{
		++OutNumLODGroups;
	}
	int32 MeshCount = 0;
	// Don't count LOD group nodes unless we are ignoring them
	if( !bLODGroup || bCountLODs )
	{
		if (Node->GetMesh())
		{
			if (!FillCollisionModelList(Node))
			{
				MeshCount = 1;
			}
		}

		int32 ChildIndex;
		for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			MeshCount += GetFbxMeshCount(Node->GetChild(ChildIndex),bCountLODs,OutNumLODGroups);
		}
	}
	else
	{
		// An LOD group should count as one mesh
		MeshCount = 1;
	}

	return MeshCount;
}

/**
* Get all Fbx mesh objects
*
* @param Node Root node to find meshes
* @param outMeshArray return Fbx meshes
*/
void FFbxImporter::FillFbxMeshArray(FbxNode* Node, TArray<FbxNode*>& outMeshArray, UnFbx::FFbxImporter* FFbxImporter)
{
	if (Node->GetMesh())
	{
		if (!FFbxImporter->FillCollisionModelList(Node))
		{ 
			outMeshArray.Add(Node);
		}
	}

	int32 ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FillFbxMeshArray(Node->GetChild(ChildIndex), outMeshArray, FFbxImporter);
	}
}

/**
* Get all Fbx skeletal mesh objects
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes
*/
void FillFbxSkelMeshArray(FbxNode* Node, TArray<FbxNode*>& outSkelMeshArray)
{
	if (Node->GetMesh() && Node->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) > 0 )
	{
		outSkelMeshArray.Add(Node);
	}

	int32 ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FillFbxSkelMeshArray(Node->GetChild(ChildIndex), outSkelMeshArray);
	}
}

void FFbxImporter::RecursiveFixSkeleton(FbxNode* Node, TArray<FbxNode*> &SkelMeshes, bool bImportNestedMeshes )
{
	for (int32 i = 0; i < Node->GetChildCount(); i++)
	{
		RecursiveFixSkeleton(Node->GetChild(i), SkelMeshes, bImportNestedMeshes );
	}

	FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	if ( Attr && (Attr->GetAttributeType() == FbxNodeAttribute::eMesh || Attr->GetAttributeType() == FbxNodeAttribute::eNull ) )
	{
		if( bImportNestedMeshes  && Attr->GetAttributeType() == FbxNodeAttribute::eMesh )
		{
			// for leaf mesh, keep them as mesh
			int32 ChildCount = Node->GetChildCount();
			int32 ChildIndex;
			for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++)
			{
				FbxNode* Child = Node->GetChild(ChildIndex);
				if (Child->GetMesh() == NULL)
				{
					break;
				}
			}

			if (ChildIndex != ChildCount)
			{
				// Remove from the mesh list it is no longer a mesh
				SkelMeshes.Remove(Node);

				//replace with skeleton
				FbxSkeleton* lSkeleton = FbxSkeleton::Create(SdkManager,"");
				Node->SetNodeAttribute(lSkeleton);
				lSkeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
			}
			else // this mesh may be not in skeleton mesh list. If not, add it.
			{
				if( !SkelMeshes.Contains( Node ) )
				{
					SkelMeshes.Add(Node);
				}
			}
		}
		else
		{
			// Remove from the mesh list it is no longer a mesh
			SkelMeshes.Remove(Node);
	
			//replace with skeleton
			FbxSkeleton* lSkeleton = FbxSkeleton::Create(SdkManager,"");
			Node->SetNodeAttribute(lSkeleton);
			lSkeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
		}
	}
}

FbxNode* FFbxImporter::GetRootSkeleton(FbxNode* Link)
{
	FbxNode* RootBone = Link;

	// get Unreal skeleton root
	// mesh and dummy are used as bone if they are in the skeleton hierarchy
	while (RootBone->GetParent())
	{
		FbxNodeAttribute* Attr = RootBone->GetParent()->GetNodeAttribute();
		if (Attr && 
			(Attr->GetAttributeType() == FbxNodeAttribute::eMesh || 
			 Attr->GetAttributeType() == FbxNodeAttribute::eNull ||
			 Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton) &&
			RootBone->GetParent() != Scene->GetRootNode())
		{
			// in some case, skeletal mesh can be ancestor of bones
			// this avoids this situation
			if (Attr->GetAttributeType() == FbxNodeAttribute::eMesh )
			{
				FbxMesh* Mesh = (FbxMesh*)Attr;
				if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
				{
					break;
				}
			}

			RootBone = RootBone->GetParent();
		}
		else
		{
			break;
		}
	}

	return RootBone;
}


void DumpFBXNode(FbxNode* Node)
{
	FbxMesh* Mesh = Node->GetMesh();
	const FString NodeName(Node->GetName());
	FbxNodeAttribute* NodeAttribute = Node->GetNodeAttribute();
	if(NodeAttribute)
	{
		FbxNodeAttribute::EType Type = NodeAttribute->GetAttributeType();
	}

	if(Mesh)
	{
		int DeformerCount = Mesh->GetDeformerCount();
		UE_LOG(LogFbx, Log,TEXT("Dumping Node [%s] : Total Deformer Count %d."), *NodeName, DeformerCount);
		for(int i=0; i<DeformerCount; i++)
		{
			FbxDeformer* Deformer = Mesh->GetDeformer(i);
			const FString DeformerName(Deformer->GetName());
			const FString DeformerTypeName(Deformer->GetTypeName());
			UE_LOG(LogFbx, Log,TEXT("\t[Node %d] %s (Type %s)."), i+1, *DeformerName, *DeformerTypeName);
		}
	}

	for(int ChildIdx=0; ChildIdx < Node->GetChildCount(); ChildIdx++)
	{
		FbxNode* ChildNode = Node->GetChild(ChildIdx);
		DumpFBXNode(ChildNode);
	}

}

/**
* Get all Fbx skeletal mesh objects which are grouped by skeleton they bind to
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
* @param SkeletonArray
* @param ExpandLOD flag of expanding LOD to get each mesh
*/
void FFbxImporter::RecursiveFindFbxSkelMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, bool ExpandLOD)
{
	FbxNode* SkelMeshNode = NULL;
	FbxNode* NodeToAdd = Node;

	DumpFBXNode(Node);

    if (Node->GetMesh() && Node->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) > 0 )
	{
		SkelMeshNode = Node;
	}
	else if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
	{
		// for LODgroup, add the LODgroup to OutSkelMeshArray according to the skeleton that the first child bind to
		SkelMeshNode = Node->GetChild(0);
		// check if the first child is skeletal mesh
		if (!(SkelMeshNode->GetMesh() && SkelMeshNode->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) > 0))
		{
			SkelMeshNode = NULL;
		}
		else if (ExpandLOD)
		{
			// if ExpandLOD is true, only add the first LODGroup level node
			NodeToAdd = SkelMeshNode;
		}		
		// else NodeToAdd = Node;
	}

	if (SkelMeshNode)
	{
		// find root skeleton

		check(SkelMeshNode->GetMesh() != NULL);
		const int32 fbxDeformerCount = SkelMeshNode->GetMesh()->GetDeformerCount();
		FbxSkin* Deformer = static_cast<FbxSkin*>( SkelMeshNode->GetMesh()->GetDeformer(0, FbxDeformer::eSkin) );
		
		if (Deformer != NULL)
		{
			FbxNode* Link = Deformer->GetCluster(0)->GetLink(); //Get the bone influences by this first cluster
			Link = GetRootSkeleton(Link); // Get the skeleton root itself

			int32 i;
			for (i = 0; i < SkeletonArray.Num(); i++)
			{
				if ( Link == SkeletonArray[i] )
				{
					// append to existed outSkelMeshArray element
					TArray<FbxNode*>* TempArray = outSkelMeshArray[i];
					TempArray->Add(NodeToAdd);
					break;
				}
			}

			// if there is no outSkelMeshArray element that is bind to this skeleton
			// create new element for outSkelMeshArray
			if ( i == SkeletonArray.Num() )
			{
				TArray<FbxNode*>* TempArray = new TArray<FbxNode*>();
				TempArray->Add(NodeToAdd);
				outSkelMeshArray.Add(TempArray);
				SkeletonArray.Add(Link);
			}
		}
	}
	else
	{
		int32 ChildIndex;
		for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			RecursiveFindFbxSkelMesh(Node->GetChild(ChildIndex), outSkelMeshArray, SkeletonArray, ExpandLOD);
		}
	}
}

void FFbxImporter::RecursiveFindRigidMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, bool ExpandLOD)
{
	bool RigidNodeFound = false;
	FbxNode* RigidMeshNode = NULL;

	if (Node->GetMesh())
	{
		// ignore skeletal mesh
		if (Node->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) == 0 )
		{
			RigidMeshNode = Node;
			RigidNodeFound = true;
		}
	}
	else if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
	{
		// for LODgroup, add the LODgroup to OutSkelMeshArray according to the skeleton that the first child bind to
		FbxNode* FirstLOD = Node->GetChild(0);
		// check if the first child is skeletal mesh
		if (FirstLOD->GetMesh())
		{
			if (FirstLOD->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) == 0 )
			{
				RigidNodeFound = true;
			}
		}

		if (RigidNodeFound)
		{
			if (ExpandLOD)
			{
				RigidMeshNode = FirstLOD;
			}
			else
			{
				RigidMeshNode = Node;
			}

		}
	}

	if (RigidMeshNode)
	{
		// find root skeleton
		FbxNode* Link = GetRootSkeleton(RigidMeshNode);

		int32 i;
		for (i = 0; i < SkeletonArray.Num(); i++)
		{
			if ( Link == SkeletonArray[i])
			{
				// append to existed outSkelMeshArray element
				TArray<FbxNode*>* TempArray = outSkelMeshArray[i];
				TempArray->Add(RigidMeshNode);
				break;
			}
		}

		// if there is no outSkelMeshArray element that is bind to this skeleton
		// create new element for outSkelMeshArray
		if ( i == SkeletonArray.Num() )
		{
			TArray<FbxNode*>* TempArray = new TArray<FbxNode*>();
			TempArray->Add(RigidMeshNode);
			outSkelMeshArray.Add(TempArray);
			SkeletonArray.Add(Link);
		}
	}

	// for LODGroup, we will not deep in.
	if (!(Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup))
	{
		int32 ChildIndex;
		for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			RecursiveFindRigidMesh(Node->GetChild(ChildIndex), outSkelMeshArray, SkeletonArray, ExpandLOD);
		}
	}
}

/**
* Get all Fbx skeletal mesh objects in the scene. these meshes are grouped by skeleton they bind to
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
*/
void FFbxImporter::FillFbxSkelMeshArrayInScene(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, bool ExpandLOD)
{
	TArray<FbxNode*> SkeletonArray;

	// a) find skeletal meshes
	
	RecursiveFindFbxSkelMesh(Node, outSkelMeshArray, SkeletonArray, ExpandLOD);
	// for skeletal mesh, we convert the skeleton system to skeleton
	// in less we recognize bone mesh as rigid mesh if they are textured
	for ( int32 SkelIndex = 0; SkelIndex < SkeletonArray.Num(); SkelIndex++)
	{
		RecursiveFixSkeleton(SkeletonArray[SkelIndex], *outSkelMeshArray[SkelIndex], ImportOptions->bImportMeshesInBoneHierarchy );
	}

	SkeletonArray.Empty();
	// b) find rigid mesh
	
	// for rigid meshes, we don't convert to bone
	if (ImportOptions->bImportRigidMesh)
	{
		RecursiveFindRigidMesh(Node, outSkelMeshArray, SkeletonArray, ExpandLOD);
	}
}

FbxNode* FFbxImporter::FindFBXMeshesByBone(const FName & RootBoneName, bool bExpandLOD, TArray<FbxNode*>& OutFBXMeshNodeArray)
{
	// get the root bone of Unreal skeletal mesh
	const FString BoneNameString = RootBoneName.ToString();

	// we do not need to check if the skeleton root node is a skeleton
	// because the animation may be a rigid animation
	FbxNode* SkeletonRoot = NULL;

	// find the FBX skeleton node according to name
	SkeletonRoot = Scene->FindNodeByName(TCHAR_TO_ANSI(*BoneNameString));

	// SinceFBX bone names are changed on import, it's possible that the 
	// bone name in the engine doesn't match that of the one in the FBX file and
	// would not be found by FindNodeByName().  So apply the same changes to the 
	// names of the nodes before checking them against the name of the Unreal bone
	if (!SkeletonRoot)
	{
		ANSICHAR TmpBoneName[64];

		for (int32 NodeIndex = 0; NodeIndex < Scene->GetNodeCount(); NodeIndex++)
		{
			FbxNode* FbxNode = Scene->GetNode(NodeIndex);

			FCStringAnsi::Strcpy(TmpBoneName, 64, MakeName(FbxNode->GetName()));
			FString FbxBoneName = FSkeletalMeshImportData::FixupBoneName(TmpBoneName);

			if (FbxBoneName == BoneNameString)
			{
				SkeletonRoot = FbxNode;
				break;
			}
		}
	}


	// return if do not find matched FBX skeleton
	if (!SkeletonRoot)
	{
		return NULL;
	}
	

	// Get Mesh nodes array that bind to the skeleton system
	// 1, get all skeltal meshes in the FBX file
	TArray< TArray<FbxNode*>* > SkelMeshArray;
	FillFbxSkelMeshArrayInScene(Scene->GetRootNode(), SkelMeshArray, false);

	// 2, then get skeletal meshes that bind to this skeleton
	for (int32 SkelMeshIndex = 0; SkelMeshIndex < SkelMeshArray.Num(); SkelMeshIndex++)
	{
		FbxNode* MeshNode = NULL;
		{
			FbxNode* Node = (*SkelMeshArray[SkelMeshIndex])[0];
			if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
			{
				MeshNode = Node->GetChild(0);
			}
			else
			{
				MeshNode = Node;
			}
		}
		
		if( !ensure( MeshNode && MeshNode->GetMesh() ) )
		{
			return NULL;
		}

		// 3, get the root bone that the mesh bind to
		FbxSkin* Deformer = (FbxSkin*)MeshNode->GetMesh()->GetDeformer(0, FbxDeformer::eSkin);
		// If there is no deformer this is likely rigid animation
		if( Deformer )
		{
			FbxNode* Link = Deformer->GetCluster(0)->GetLink();
			Link = GetRootSkeleton(Link);
			// 4, fill in the mesh node
			if (Link == SkeletonRoot)
			{
				// copy meshes
				if (bExpandLOD)
				{
					TArray<FbxNode*> SkelMeshes = 	*SkelMeshArray[SkelMeshIndex];
					for (int32 NodeIndex = 0; NodeIndex < SkelMeshes.Num(); NodeIndex++)
					{
						FbxNode* Node = SkelMeshes[NodeIndex];
						if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
						{
							OutFBXMeshNodeArray.Add(Node->GetChild(0));
						}
						else
						{
							OutFBXMeshNodeArray.Add(Node);
						}
					}
				}
				else
				{
					OutFBXMeshNodeArray.Append(*SkelMeshArray[SkelMeshIndex]);
				}
				break;
			}
		}
	}

	for (int32 i = 0; i < SkelMeshArray.Num(); i++)
	{
		delete SkelMeshArray[i];
	}

	return SkeletonRoot;
}

/**
* Get the first Fbx mesh node.
*
* @param Node Root node
* @param bIsSkelMesh if we want a skeletal mesh
* @return FbxNode* the node containing the first mesh
*/
FbxNode* GetFirstFbxMesh(FbxNode* Node, bool bIsSkelMesh)
{
	if (Node->GetMesh())
	{
		if (bIsSkelMesh)
		{
			if (Node->GetMesh()->GetDeformerCount(FbxDeformer::eSkin)>0)
			{
				return Node;
			}
		}
		else
		{
			return Node;
		}
	}

	int32 ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FbxNode* FirstMesh;
		FirstMesh = GetFirstFbxMesh(Node->GetChild(ChildIndex), bIsSkelMesh);

		if (FirstMesh)
		{
			return FirstMesh;
		}
	}

	return NULL;
}

void FFbxImporter::CheckSmoothingInfo(FbxMesh* FbxMesh)
{
	if (FbxMesh && bFirstMesh)
	{
		bFirstMesh = false;	 // don't check again
		
		FbxLayer* LayerSmoothing = FbxMesh->GetLayer(0, FbxLayerElement::eSmoothing);
		if (!LayerSmoothing)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("Prompt_NoSmoothgroupForFBXScene", "Warning: No smoothing group information was found in this FBX scene.  Please make sure to enable the 'Export Smoothing Groups' option in the FBX Exporter plug-in before exporting the file.  Even for tools that don't support smoothing groups, the FBX Exporter will generate appropriate smoothing data at export-time so that correct vertex normals can be inferred while importing.")));
		}
	}
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
FbxNode* FFbxImporter::RetrieveObjectFromName(const TCHAR* ObjectName, FbxNode* Root)
{
	FbxNode* Result = NULL;
	
	if ( Scene != NULL )
	{
		if (Root == NULL)
		{
			Root = Scene->GetRootNode();
		}

		for (int32 ChildIndex=0;ChildIndex<Root->GetChildCount() && !Result;++ChildIndex)
		{
			FbxNode* Node = Root->GetChild(ChildIndex);
			FbxMesh* FbxMesh = Node->GetMesh();
			if (FbxMesh && 0 == FCString::Strcmp(ObjectName,ANSI_TO_TCHAR(Node->GetName())))
			{
				Result = Node;
			}
			else
			{
				Result = RetrieveObjectFromName(ObjectName,Node);
			}
		}
	}
	return Result;
}

} // namespace UnFbx

#undef LOCTEXT_NAMESPACE