// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "EngineVersion.h"

FPackageFileSummary::FPackageFileSummary()
{
	FMemory::Memzero( this, sizeof(*this) );
}

FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum )
{
	Ar << Sum.Tag;
	// only keep loading if we match the magic
	if( Sum.Tag == PACKAGE_FILE_TAG || Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
	{
		// The package has been stored in a separate endianness than the linker expected so we need to force
		// endian conversion. Latent handling allows the PC version to retrieve information about cooked packages.
		if( Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
		{
			// Set proper tag.
			Sum.Tag = PACKAGE_FILE_TAG;
			// Toggle forced byte swapping.
			if( Ar.ForceByteSwapping() )
			{
				Ar.SetByteSwapping( false );
			}
			else
			{
				Ar.SetByteSwapping( true );
			}
		}
		/**
		 * The package file version number when this package was saved.
		 *
		 * Lower 16 bits stores the UE3 engine version
		 * Upper 16 bits stores the UE4/licensee version
		 * For newer packages this is -3 (-2 indicates presence of enum-based custom versions, -3 indicates guid-based custom versions).
		 */
		int32 LegacyFileVersion = -3;
		Ar << LegacyFileVersion;

		if (Ar.IsLoading())
		{
			if (LegacyFileVersion < 0) // means we have modern version numbers
			{
				check(!Sum.bHadLegacyVersionNumbers); // should have been set by the constructor
				Ar << Sum.FileVersionUE3;
				Ar << Sum.FileVersionUE4;
				Ar << Sum.FileVersionLicenseeUE4;
				if ((Sum.GetFileVersionUE4() >= VER_UE4_READD_COOKER) &&
					(Sum.GetFileVersionUE4() < VER_UE4_COOKED_PACKAGE_VERSION_IS_PACKAGE_VERSION))
				{
					int32 DummyValue;
					Ar << DummyValue;	// was Sum.PackageCookedVersion;
					Ar << DummyValue;	// was Sum.PackageCookedLicenseeVersion;
				}

				if (LegacyFileVersion <= -2)
				{
					Sum.CustomVersionContainer.Serialize(Ar, (LegacyFileVersion == -2) ? ECustomVersionSerializationFormat::Enums : ECustomVersionSerializationFormat::Guids);
				}
			}
			else
			{
				Sum.bHadLegacyVersionNumbers = true;
				Sum.FileVersionUE3 = (LegacyFileVersion & 0xffff);

				Sum.FileVersionUE4 = 0;
				Sum.FileVersionLicenseeUE4 = 0;
				static const bool AllEpicUE4PackagesHaveBeenResavedWithNewVersionScheme = true;
				if (AllEpicUE4PackagesHaveBeenResavedWithNewVersionScheme)
				{
					Sum.FileVersionLicenseeUE4 = ((LegacyFileVersion >> 16) & 0xffff);
				}
				else
				{
					Sum.FileVersionUE4 = ((LegacyFileVersion >> 16) & 0xffff);
				}
			}
			if (!Sum.FileVersionUE3 && !Sum.FileVersionUE4 && !Sum.FileVersionLicenseeUE4)
			{
				// this file is unversioned, remember that, then use current versions
				Sum.bUnversioned = true;
				Sum.FileVersionUE3 = VER_LAST_ENGINE_UE3;
				Sum.FileVersionUE4 = GPackageFileUE4Version;
				Sum.FileVersionLicenseeUE4 = GPackageFileLicenseeUE4Version;

				Sum.CustomVersionContainer = FCustomVersionContainer::GetRegistered();
			}
		}
		else
		{
			if (Sum.bUnversioned)
			{
				int32 Zero = 0;
				Ar << Zero;
				Ar << Zero;
				Ar << Zero;

				FCustomVersionContainer NoCustomVersions;
				NoCustomVersions.Serialize(Ar);
			}
			else
			{
				Ar << Sum.FileVersionUE3;
				Ar << Sum.FileVersionUE4;
				Ar << Sum.FileVersionLicenseeUE4;

				// Serialise custom version map.
				Sum.CustomVersionContainer.Serialize(Ar);
			}
		}
		Ar << Sum.TotalHeaderSize;
		Ar << Sum.FolderName;
		Ar << Sum.PackageFlags;
		if( Sum.PackageFlags & PKG_FilterEditorOnly )
		{
			Ar.SetFilterEditorOnly(true);
		}
		Ar << Sum.NameCount     << Sum.NameOffset;
		Ar << Sum.ExportCount   << Sum.ExportOffset;
		Ar << Sum.ImportCount   << Sum.ImportOffset;
		Ar << Sum.DependsOffset;
		if (Ar.IsLoading() && (Sum.FileVersionUE3 < VER_MIN_ENGINE_UE3 || Sum.FileVersionUE4 < VER_UE4_OLDEST_LOADABLE_PACKAGE || Sum.FileVersionUE4 > GPackageFileUE4Version))
		{
			return Ar; // we can't safely load more than this because the below was different in older files.
		}

		Ar << Sum.ThumbnailTableOffset;

		int32 GenerationCount = Sum.Generations.Num();
		Ar << Sum.Guid << GenerationCount;
		if( Ar.IsLoading() && GenerationCount > 0 )
		{
			Sum.Generations.Empty( 1 );
			Sum.Generations.AddUninitialized( GenerationCount );
		}
		for( int32 i=0; i<GenerationCount; i++ )
		{
			Sum.Generations[i].Serialize(Ar, Sum);
		}

		if( Sum.GetFileVersionUE4() >= VER_UE4_ENGINE_VERSION_OBJECT )
		{
			if(Ar.IsCooking() || (Ar.IsSaving() && !GEngineVersion.IsPromotedBuild()))
			{
				FEngineVersion EmptyEngineVersion;
				Ar << EmptyEngineVersion;
			}
			else
			{
				Ar << Sum.EngineVersion;
			}
		}
		else
		{
			int32 EngineChangelist = 0;
			Ar << EngineChangelist;

			if(Ar.IsLoading() && EngineChangelist != 0)
			{
				Sum.EngineVersion.Set(4, 0, 0, EngineChangelist, TEXT(""));
			}
		}

		Ar << Sum.CompressionFlags;
		Ar << Sum.CompressedChunks;
		Ar << Sum.PackageSource;

		Ar << Sum.AdditionalPackagesToCook;

#if WITH_ENGINE
		//@todo legacy
		Ar << Sum.TextureAllocations;
#else
		check(!"this can't serialize successfully");
#endif		// WITH_ENGINE

		if( Sum.GetFileVersionUE4() >= VER_UE4_ASSET_REGISTRY_TAGS )
		{
			Ar << Sum.AssetRegistryDataOffset;
		}

		if ( Sum.GetFileVersionUE4() >= VER_UE4_SUMMARY_HAS_BULKDATA_OFFSET)
		{
			Ar << Sum.BulkDataStartOffset;
		}
		else
		{
			Sum.BulkDataStartOffset = 0;
		}
		
		if (Sum.GetFileVersionUE4() >= VER_UE4_WORLD_LEVEL_INFO)
		{
			Ar << Sum.WorldTileInfoDataOffset;
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS)
		{
			Ar << Sum.ChunkIDs;
		}
		else if (Sum.GetFileVersionUE4() >= VER_UE4_ADDED_CHUNKID_TO_ASSETDATA_AND_UPACKAGE)
		{
			// handle conversion of single ChunkID to an array of ChunkIDs
			if (Ar.IsLoading())
			{
				int ChunkID = -1;
				Ar << ChunkID;

				// don't load <0 entries since an empty array represents the same thing now
				if (ChunkID >= 0)
				{
					Sum.ChunkIDs.Add( ChunkID );
				}
			}
		}
	}

	return Ar;
}
