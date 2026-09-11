// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// Hides the named bones for the length of the window and unhides them after. This is the one
// preview that is very close to what the game does, because "hide these bones" has exactly one
// reasonable meaning and the engine has an API for it.
//
// SCOPE NOTE, and it is the same trap the RX-Port renderer hit: a window like this addresses a
// SKELETON, and the skeleton it addresses is not always the character's. In game these notifies
// are also authored against a spawned prop's own clip. Here there is only the previewed mesh, so
// that distinction does not arise -- but do not read a working preview as proof the window will
// hit the same bones in game.

#include "AnimNotifyState_TimedHideBones.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"

// The bones this notify actually hid, per component. NotifyEnd un-hides exactly these -- it must
// not re-derive the set from HideBoneNames/bHideFirstNonRootBone, because editing either while the
// playhead is inside the window would then leave a bone hidden with nothing able to find it again.
static TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FName>> GHiddenBones;

void UAnimNotifyState_TimedHideBones::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	TArray<FName>& Hidden = GHiddenBones.FindOrAdd(MeshComp);

	for (const FName& Bone : HideBoneNames)
	{
		if (!Bone.IsNone())
		{
			MeshComp->HideBoneByName(Bone, PBO_None);
			Hidden.AddUnique(Bone);
		}
	}

	if (bHideFirstNonRootBone)
	{
		// Index 1 by definition: UE orders a reference skeleton parents-first, so index 0 is the
		// root and index 1 is the first bone below it.
		if (const USkinnedAsset* Asset = MeshComp->GetSkinnedAsset())
		{
			const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
			if (Ref.GetNum() > 1)
			{
				MeshComp->HideBoneByName(Ref.GetBoneName(1), PBO_None);
				Hidden.AddUnique(Ref.GetBoneName(1));
			}
		}
	}
}

void UAnimNotifyState_TimedHideBones::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_TimedHideBones::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (const TArray<FName>* Hidden = GHiddenBones.Find(MeshComp))
	{
		for (const FName& Bone : *Hidden)
		{
			MeshComp->UnHideBoneByName(Bone);
		}
	}
	GHiddenBones.Remove(MeshComp);

	// Drop entries whose component has gone, so a long editing session does not accumulate them.
	for (auto It = GHiddenBones.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
