// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// Hides the mesh sections the notify names for the length of the window, and shows them again
// after. This is how the game hides a weapon, a cape or a body part mid-animation.
//
// RESOLVING A QUERY. FMaterialQuery can address a slot three ways -- by index, by slot NAME, or
// by a tag on the material. Index and name are resolvable from the mesh alone and are handled.
// The TAG form reads a value out of the game's own material data, which a stock editor has no
// way to look up, so a tag query resolves to nothing here rather than to slot 0.

#include "AnimNotifyState_TimedHideMaterial.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshRenderData.h"

// What this notify actually hid, per component: the (LOD, section, materialIndex) triples it
// changed. NotifyEnd un-hides exactly these. Re-deriving the set from the live properties at end
// time -- which is what this file used to do -- strands slots hidden forever the moment an author
// edits the query while the playhead sits inside the window.
struct FHiddenSection
{
	int32 Lod = 0;
	int32 Section = 0;
	int32 MaterialIndex = 0;
};
static TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FHiddenSection>> GHiddenSections;
static TMap<TWeakObjectPtr<USkeletalMeshComponent>, bool> GPriorComponentVisibility;

// Sections of LOD 0 that use a given material slot. Hiding is a SECTION operation and the notify
// names a MATERIAL, so the mapping has to be made explicitly.
static void SectionsForMaterial(const USkeletalMeshComponent* MeshComp, int32 MaterialIndex,
	TArray<FIntPoint>& OutSections)
{
	const USkeletalMesh* Mesh = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshRenderData* Render = Mesh ? Mesh->GetResourceForRendering() : nullptr;
	if (!Render || Render->LODRenderData.Num() == 0)
	{
		return;
	}
	// EVERY LOD, not just LOD 0. ShowMaterialSection records the hidden section per LOD and the
	// renderer consults the list for the LOD it is actually drawing, so a LOD-0-only hide silently
	// stops working the moment the viewport picks another LOD -- and LOD selection is automatic by
	// default. The out array is (LODIndex, SectionIndex) pairs for that reason.
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
	// READ THE DISCRIMINATOR. FMaterialQuery carries EMaterialQueryType, which STATES which of its
	// three addressing fields is the live one. Guessing the mode from whichever sibling field looked
	// non-empty meant a tag query -- whose SlotName is legitimately None -- fell through to
	// SlotIndex, and SlotIndex is 0 by default, so it silently hid material slot 0: a slot the
	// author never named, and the exact outcome this file's header promised could not happen.
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
		// A tag query reads the game's own material data, which a stock editor cannot look up.
		// Resolving to nothing is the honest answer; resolving to slot 0 was a wrong one.
		return INDEX_NONE;
	}
}

static void HideNow(USkeletalMeshComponent* MeshComp, const UAnimNotifyState_TimedHideMaterial* N)
{
	if (!MeshComp)
	{
		return;
	}

	if (N->bHideAllSections)
	{
		// NO PROPAGATION. "Hide all sections" is about THIS mesh's material sections; propagating
		// blanked every attached component -- props, effects, anything parented to the preview mesh
		// -- and the end of the window then force-showed them all, wiping state this notify never
		// owned and never saved.
		if (!GPriorComponentVisibility.Contains(MeshComp))
		{
			GPriorComponentVisibility.Add(MeshComp, MeshComp->IsVisible());
		}
		MeshComp->SetVisibility(false, /*bPropagateToChildren=*/false);
		return;
	}

	TArray<int32> Materials = N->HideMaterialIDArray;
	for (const FMaterialQuery& Query : N->HideMaterialQuery)
	{
		const int32 Index = ResolveQuery(MeshComp, Query);
		if (Index != INDEX_NONE)
		{
			Materials.AddUnique(Index);
		}
	}

	for (const FName& Suffix : N->HideMaterialSlotSuffixes)
	{
		if (Suffix.IsNone())
		{
			continue;
		}
		const USkeletalMesh* Mesh = MeshComp->GetSkeletalMeshAsset();
		if (!Mesh)
		{
			continue;
		}
		const TArray<FSkeletalMaterial>& Slots = Mesh->GetMaterials();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].MaterialSlotName.ToString().EndsWith(Suffix.ToString()))
			{
				Materials.AddUnique(i);
			}
		}
	}

	TArray<FHiddenSection>& Record = GHiddenSections.FindOrAdd(MeshComp);
	for (int32 MaterialIndex : Materials)
	{
		TArray<FIntPoint> Sections;
		SectionsForMaterial(MeshComp, MaterialIndex, Sections);
		for (const FIntPoint& LodSection : Sections)
		{
			MeshComp->ShowMaterialSection(MaterialIndex, LodSection.Y, false, LodSection.X);
			Record.Add({ LodSection.X, LodSection.Y, MaterialIndex });
		}
	}
}

static void RestoreNow(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return;
	}
	if (const bool* Prior = GPriorComponentVisibility.Find(MeshComp))
	{
		MeshComp->SetVisibility(*Prior, /*bPropagateToChildren=*/false);
		GPriorComponentVisibility.Remove(MeshComp);
	}
	if (const TArray<FHiddenSection>* Record = GHiddenSections.Find(MeshComp))
	{
		for (const FHiddenSection& H : *Record)
		{
			MeshComp->ShowMaterialSection(H.MaterialIndex, H.Section, true, H.Lod);
		}
	}
	GHiddenSections.Remove(MeshComp);
}

void UAnimNotifyState_TimedHideMaterial::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	HideNow(MeshComp, this);
}

void UAnimNotifyState_TimedHideMaterial::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_TimedHideMaterial::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	RestoreNow(MeshComp);
}
