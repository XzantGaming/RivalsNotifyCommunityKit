// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// Spawns the prop skeletal mesh the notify names, attaches it to the named socket with the
// authored offset, plays the authored animation on it, and tears it down when the window ends.
// This is the notify that puts weapons, banners and companion characters into an emote, so
// previewing it is most of what makes the pack usable for emote work.
//
// WHAT IS NOT PREVIEWED, and why: MeshSource has three values and only SkeletalMeshTemplate names
// an asset. CurrentChildBPMainMesh and ExistingTaggedMesh resolve against the character's own
// blueprint and its tagged components -- game state that a montage preview does not have. Those
// two spawn nothing here rather than spawning the wrong thing.

#include "AnimNotifyState_TimedSkeletonAnimation.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

static FName SpawnedComponentTag(const UObject* Notify)
{
	return Notify ? Notify->GetFName() : NAME_None;
}

// Every prop this notify spawned. Searching the owner's components OR the mesh's attach children
// covers most cases but not their gap: bDoNotAttach with an ownerless preview mesh leaves the prop
// reachable from neither, and it leaked into the preview world on every scrub -- the precise case
// the spawn path had just been taught to support. Holding the component directly cannot miss.
static TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<TWeakObjectPtr<USceneComponent>>> GSpawnedProps;

// Place the prop from the notify's CURRENT offsets.
//
// Called from NotifyBegin AND from every NotifyTick, deliberately. Two separate reasons:
//
//  1. Attaching with a SnapToTarget rule ZEROES RelativeLocation/RelativeRotation inside
//     AttachToComponent, so the authored offset is a post-hoc correction rather than the
//     component's authored relative transform. Anything that re-runs the attach snap afterwards
//     silently discards it, and nothing re-asserts it. Re-applying every frame makes that class of
//     failure impossible rather than merely unlikely.
//  2. A montage preview is an EDITING loop. Typing a new LocationOffset must move the prop now --
//     NotifyBegin already ran, so a spawn-time-only transform would not move until the window was
//     re-entered, which reads exactly like "the offsets do nothing".
static void PlaceProp(USceneComponent* Prop, USkeletalMeshComponent* MeshComp,
	bool bAttached, FName SocketName, const FVector& LocationOffset, const FRotator& RotationOffset)
{
	if (!Prop || !MeshComp)
	{
		return;
	}
	if (bAttached)
	{
		Prop->SetRelativeLocationAndRotation(LocationOffset, RotationOffset);
	}
	else
	{
		Prop->SetWorldTransform(FTransform(RotationOffset, LocationOffset)
			* MeshComp->GetSocketTransform(SocketName));
	}
}

void UAnimNotifyState_TimedSkeletonAnimation::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || MeshSource != ETimedSkeletonAnimationMeshSource::SkeletalMeshTemplate)
	{
		return;
	}

	USkeletalMesh* Mesh = SkeletalMeshTemplate.LoadSynchronous();
	if (!Mesh)
	{
		return;
	}

	// DO NOT REQUIRE AN ACTOR OWNER. An anim-editor preview scene does not always host the mesh on
	// an actor, and bailing out when GetOwner() is null would make this notify do nothing in the
	// one place it most needs to be seen. The mesh component itself is a valid outer, and the
	// component is registered against the world the previewed mesh is already in.
	AActor* Owner = MeshComp->GetOwner();
	UObject* Outer = Owner ? static_cast<UObject*>(Owner) : static_cast<UObject*>(MeshComp);
	USkeletalMeshComponent* Prop = NewObject<USkeletalMeshComponent>(Outer);
	if (!Prop)
	{
		return;
	}
	Prop->SetSkeletalMeshAsset(Mesh);
	Prop->ComponentTags.AddUnique(SpawnedComponentTag(this));
	for (const FName& Tag : AddComponentTags)
	{
		Prop->ComponentTags.AddUnique(Tag);
	}

	for (int32 i = 0; i < OverrideMaterials.Num(); ++i)
	{
		if (UMaterialInterface* Material = OverrideMaterials[i].LoadSynchronous())
		{
			Prop->SetMaterial(i, Material);
		}
	}

	Prop->SetRenderCustomDepth(bIsRenderCustomDepth);
	if (bUsedCustomStencil)
	{
		Prop->SetCustomDepthStencilValue(CustomStencilValue);
	}
	Prop->SetReceivesDecals(bReceiveDecal);
	Prop->VisibilityBasedAnimTickOption = VisibilityBasedAnimTickOption;
	if (bCustomLightingChannels)
	{
		Prop->LightingChannels = LightingChannels;
	}

	// Attach BEFORE registering: a component registered while unattached picks up the world
	// transform it does not have yet, which puts the first frame at the origin.
	if (!bDoNotAttach)
	{
		Prop->AttachToComponent(MeshComp,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	}
	Prop->RegisterComponentWithWorld(MeshComp->GetWorld());
	PlaceProp(Prop, MeshComp, !bDoNotAttach, SocketName, LocationOffset, RotationOffset);
	GSpawnedProps.FindOrAdd(MeshComp).Add(Prop);

	if (UAnimationAsset* Anim = AnimToPlay.LoadSynchronous())
	{
		Prop->PlayAnimation(Anim, bIsLoop);
		if (AnimStartPos > 0.0f)
		{
			Prop->SetPosition(AnimStartPos, /*bFireNotifies=*/false);
		}
	}

	// ALWAYS VISIBLE IN THE PREVIEW, deliberately not `bVisibleDuringAnimNotifyState`.
	//
	// These stubs have no constructor, so every bool on them defaults to FALSE -- verified against
	// the CDO, not assumed. Gating on that flag therefore spawned the prop and hid it in the same
	// frame, on every freshly placed notify: the mesh was there, correctly placed, and invisible,
	// which looks exactly like the notify doing nothing at all.
	//
	// The flag governs an in-game visibility rule this preview cannot model anyway (it interacts
	// with bFixVisibleDuringAnimNotifyState and bDelayHiddenOnPaused). Showing the mesh is the
	// only behaviour that makes the notify previewable, which is the whole point of being here.
	Prop->SetVisibility(true, /*bPropagateToChildren=*/true);
	Prop->SetHiddenInGame(false, /*bPropagateToChildren=*/true);
}

void UAnimNotifyState_TimedSkeletonAnimation::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// Re-assert the authored placement every frame -- see PlaceProp for why.
	if (!MeshComp)
	{
		return;
	}
	const FName Tag = SpawnedComponentTag(this);
	for (USceneComponent* Child : MeshComp->GetAttachChildren())
	{
		if (Child && Child->ComponentTags.Contains(Tag))
		{
			PlaceProp(Child, MeshComp, /*bAttached=*/true, SocketName, LocationOffset, RotationOffset);
		}
	}
}

void UAnimNotifyState_TimedSkeletonAnimation::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Destroy exactly what was spawned, from the record kept at spawn time.
	if (TArray<TWeakObjectPtr<USceneComponent>>* Props = GSpawnedProps.Find(MeshComp))
	{
		for (const TWeakObjectPtr<USceneComponent>& Weak : *Props)
		{
			if (USceneComponent* Prop = Weak.Get())
			{
				Prop->DestroyComponent();
			}
		}
	}
	GSpawnedProps.Remove(MeshComp);

	// Belt and braces: anything still carrying this notify's tag, in case a spawn predates the
	// record (a hot-reloaded module, an interrupted montage) and would otherwise never be reclaimed.
	const FName Tag = SpawnedComponentTag(this);
	TArray<USceneComponent*> Children = MeshComp->GetAttachChildren();
	for (USceneComponent* Child : Children)
	{
		if (Child && Child->ComponentTags.Contains(Tag))
		{
			Child->DestroyComponent();
		}
	}

	for (auto It = GSpawnedProps.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
