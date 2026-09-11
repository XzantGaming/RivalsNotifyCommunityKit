// EDITOR PREVIEW for a stub notify. Hand-written; the header beside it is generated.
//
// Restores the preview that flattening removed -- see the note in
// UAnimNotifyState_TimedNiagaraEffectEx.cpp. In the game this derives from
// AnimNotify_PlayNiagaraEffect, whose stock implementation spawned the system for free.
//
// Spawns Template (and TemplateCombineEffect when bUseCombineEffect is set) at SocketName with
// the authored offsets, scale, user parameters and render flags.

#include "AnimNotify_PlayNiagaraEffectEx.h"

#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// The tag every component this notify spawns carries, so the next firing can reclaim the last.
static FName SpawnedTag(const UObject* Notify)
{
	return Notify ? FName(*(Notify->GetName() + TEXT("_PreviewSpawn"))) : NAME_None;
}

// A ONE-SHOT NOTIFY HAS NO END EVENT, so a Niagara system that never completes -- a looping emitter,
// or one bounded in game by EffectLastTime / bDeactivateOnMontageEnded, neither of which a preview
// can schedule -- would leave a component behind on every pass over the notify, forever, in a
// preview world that is never torn down. Sweeping this notify's previous spawns before making a new
// one bounds that at one live component instead of one per scrub. Systems that DO complete are
// reclaimed by bAutoDestroy as usual and this finds nothing.
static void SweepPreviousSpawns(const UAnimNotify_PlayNiagaraEffectEx* N,
	USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return;
	}
	const FName Tag = SpawnedTag(N);
	TArray<USceneComponent*> Children = MeshComp->GetAttachChildren();
	for (USceneComponent* Child : Children)
	{
		if (UNiagaraComponent* Old = Cast<UNiagaraComponent>(Child))
		{
			if (Old->ComponentTags.Contains(Tag))
			{
				Old->SetAutoDestroy(true);
				Old->DeactivateImmediate();
			}
		}
	}
}

static UNiagaraComponent* SpawnOne(const UAnimNotify_PlayNiagaraEffectEx* N,
	USkeletalMeshComponent* MeshComp, UNiagaraSystem* System)
{
	if (!System)
	{
		return nullptr;
	}

	UNiagaraComponent* Spawned = nullptr;
	if (N->Attached)
	{
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(System, MeshComp, N->SocketName,
			N->LocationOffset, N->RotationOffset, EAttachLocation::KeepRelativeOffset,
			/*bAutoDestroy=*/true);
	}
	else
	{
		const FTransform Socket = MeshComp->GetSocketTransform(N->SocketName);
		const FTransform Offset(N->RotationOffset, N->LocationOffset);
		const FTransform World = Offset * Socket;
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(MeshComp, System,
			World.GetLocation(), World.GetRotation().Rotator());
	}
	if (!Spawned)
	{
		return nullptr;
	}

	if (!N->Scale.IsNearlyZero())
	{
		// bAbsoluteScale means the authored scale ignores the parent's; the preview applies the
		// same number either way, which is right for an unscaled preview mesh and approximate for
		// a scaled one.
		Spawned->SetAbsolute(false, false, N->bAbsoluteScale);
		Spawned->SetRelativeScale3D(N->Scale);
	}
	Spawned->SetCastShadow(N->bCastShadow != 0);
	Spawned->SetRenderCustomDepth(N->bRenderCustomDepthPass != 0);
	if (N->bUsedCustomStencil)
	{
		Spawned->SetCustomDepthStencilValue(N->CustomStencilValue);
	}
	if (N->bUsedTranslucencySortPriority)
	{
		Spawned->SetTranslucentSortPriority(N->TranslucencySortPriority);
	}
	// Its identically shaped twin, previously honoured on one class and ignored on this one.
	if (N->bUsedTranslucencySortDistanceOffset)
	{
		Spawned->SetTranslucencySortDistanceOffset(N->TranslucencySortDistanceOffset);
	}
	Spawned->LightingChannels = N->LightingChannels;
	Spawned->SetOwnerNoSee(N->bOwnerNoSee != 0);
	Spawned->SetOnlyOwnerSee(N->bOnlyOwnerSee != 0);

	Spawned->ComponentTags.AddUnique(SpawnedTag(N));
	for (const FName& Tag : N->NiagaraTags)
	{
		Spawned->ComponentTags.AddUnique(Tag);
	}
	for (const TPair<FName, float>& Pair : N->FloatUserParameterValues)
	{
		Spawned->SetVariableFloat(Pair.Key, Pair.Value);
	}
	for (const TPair<FName, FVector>& Pair : N->VectorUserParameterValues)
	{
		Spawned->SetVariableVec3(Pair.Key, Pair.Value);
	}
	for (const TPair<FName, FLinearColor>& Pair : N->ColorUserParameterValues)
	{
		Spawned->SetVariableLinearColor(Pair.Key, Pair.Value);
	}
	return Spawned;
}

void UAnimNotify_PlayNiagaraEffectEx::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	SweepPreviousSpawns(this, MeshComp);
	SpawnOne(this, MeshComp, Template.LoadSynchronous());

	// The combine effect is a SECOND system this notify names -- the field that could not even be
	// described before the registry grew a second asset slot. It is spawned the same way; which of
	// the two the game picks is decided by CombineFXType and gameplay state the editor has none of,
	// so the preview shows both rather than guessing at one.
	if (bUseCombineEffect)
	{
		SpawnOne(this, MeshComp, TemplateCombineEffect.LoadSynchronous());
	}
}
