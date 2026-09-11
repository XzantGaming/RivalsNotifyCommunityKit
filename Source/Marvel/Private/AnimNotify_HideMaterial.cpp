// EDITOR PREVIEW for a stub notify. Hand-written; the header beside it is generated.
//
// The instant twin of TimedHideMaterial: at the notify's moment it hides one set of material
// slots and shows another, and unlike the state version it never puts them back. That asymmetry
// is the point of the class -- it is how the game swaps a mesh's visible parts partway through a
// montage -- so the preview keeps it.
//
// See UAnimNotifyState_TimedHideMaterial.cpp for why a TAG query resolves to nothing here.

#include "AnimNotify_HideMaterial.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshRenderData.h"

static void SectionsForMaterial(const USkeletalMeshComponent* MeshComp, int32 MaterialIndex,
	TArray<FIntPoint>& OutSections)
{
	const USkeletalMesh* Mesh = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshRenderData* Render = Mesh ? Mesh->GetResourceForRendering() : nullptr;
	if (!Render || Render->LODRenderData.Num() == 0)
	{
		return;
	}
	// EVERY LOD -- see the note in UAnimNotifyState_TimedHideMaterial.cpp. Pairs are (LOD, section).
	for (int32 Lod = 0; Lod < Render->LODRenderData.Num(); ++Lod)
	{
		const FSkeletalMeshLODRenderData& LOD = Render->LODRenderData[Lod];
		for (int32 i = 0; i < LOD.RenderSections.Num(); ++i)
		{
			if (LOD.RenderSections[i].MaterialIndex == MaterialIndex)
			{
				OutSections.Add(FIntPoint(Lod, i));
			}
		}
	}
}

static int32 ResolveQuery(const USkeletalMeshComponent* MeshComp, const FMaterialQuery& Query)
{
	if (!MeshComp)
	{
		return INDEX_NONE;
	}
	// READ THE DISCRIMINATOR -- see UAnimNotifyState_TimedHideMaterial.cpp. Guessing the addressing
	// mode from which sibling field looked filled made a tag query fall through to SlotIndex, which
	// is 0 by default, silently hiding a slot the author never named.
	switch (Query.QueryType)
	{
	case EMaterialQueryType::QueryBySlotIndex:
		return Query.SlotIndex;

	case EMaterialQueryType::QueryBySlotName:
	{
		const USkeletalMesh* Mesh = MeshComp->GetSkeletalMeshAsset();
		if (!Mesh)
		{
			return INDEX_NONE;
		}
		const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			if (Materials[i].MaterialSlotName == Query.SlotName)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	case EMaterialQueryType::QueryBySlotTag:
	default:
		return INDEX_NONE;
	}
}

static void Apply(USkeletalMeshComponent* MeshComp, const TArray<int32>& Ids,
	const TArray<FMaterialQuery>& Queries, bool bShow)
{
	if (!MeshComp)
	{
		return;
	}
	TArray<int32> Materials = Ids;
	for (const FMaterialQuery& Query : Queries)
	{
		const int32 Index = ResolveQuery(MeshComp, Query);
		if (Index != INDEX_NONE)
		{
			Materials.AddUnique(Index);
		}
	}
	for (int32 MaterialIndex : Materials)
	{
		TArray<FIntPoint> Sections;
		SectionsForMaterial(MeshComp, MaterialIndex, Sections);
		for (const FIntPoint& LodSection : Sections)
		{
			MeshComp->ShowMaterialSection(MaterialIndex, LodSection.Y, bShow, LodSection.X);
		}
	}
}

void UAnimNotify_HideMaterial::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Hide first, then show: a slot named in both lists ends up VISIBLE, which is the reading that
	// lets an author write "hide everything, then bring these back".
	Apply(MeshComp, HideMaterialIDArray, HideMaterialQueries, /*bShow=*/false);
	Apply(MeshComp, ShowMaterialIDArray, ShowMaterialQueries, /*bShow=*/true);
}
