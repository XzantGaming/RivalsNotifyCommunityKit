// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// THIS PREVIEW EXISTS BECAUSE FLATTENING TOOK THE INHERITED ONE AWAY. In the game this class
// derives from AnimNotifyState_TimedNiagaraEffect, which stock UE implements -- so before the
// MinimalAPI re-parenting it spawned its system in the editor for free. Re-parenting to
// UAnimNotifyState fixed the link error and silently cost the preview on the single most used
// notify in the game. A fix that quietly removes behaviour is not finished until the behaviour
// is put back.
//
// Spawns Template at SocketName for the length of the window, with the authored offsets, scale,
// user parameters and render flags.

#include "AnimNotifyState_TimedNiagaraEffectEx.h"

#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

static FName SpawnedTag(const UObject* Notify)
{
	return Notify ? Notify->GetFName() : NAME_None;
}

void UAnimNotifyState_TimedNiagaraEffectEx::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UNiagaraSystem* System = Template.LoadSynchronous();
	if (!MeshComp || !System)
	{
		return;
	}

	UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(System, MeshComp,
		SocketName, LocationOffset, RotationOffset, EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/true);
	if (!Spawned)
	{
		return;
	}

	Spawned->ComponentTags.AddUnique(SpawnedTag(this));
	for (const FName& Tag : ComponentTags)
	{
		Spawned->ComponentTags.AddUnique(Tag);
	}

	if (!Scale3D.IsNearlyZero())
	{
		Spawned->SetRelativeScale3D(Scale3D);
	}
	Spawned->SetCastShadow(bCastShadow != 0);
	Spawned->SetRenderCustomDepth(bRenderCustomDepthPass != 0);
	if (bUsedCustomStencil)
	{
		Spawned->SetCustomDepthStencilValue(CustomStencilValue);
	}
	if (bUsedTranslucencySortPriority)
	{
		Spawned->SetTranslucentSortPriority(TranslucencySortPriority);
	}
	// The identically shaped distance-offset pair sits right beside the priority pair in the class
	// and was being ignored while its twin was honoured.
	if (bUsedTranslucencySortDistanceOffset)
	{
		Spawned->SetTranslucencySortDistanceOffset(TranslucencySortDistanceOffset);
	}
	Spawned->LightingChannels = LightingChannels;

	for (const TPair<FName, float>& Pair : FloatUserParameterValues)
	{
		Spawned->SetVariableFloat(Pair.Key, Pair.Value);
	}
	for (const TPair<FName, FVector>& Pair : VectorUserParameterValues)
	{
		Spawned->SetVariableVec3(Pair.Key, Pair.Value);
	}
	// The colour map is declared on this class and applied by every sibling preview; omitting it
	// here silently dropped authored colour overrides while the header claimed "user parameters".
	for (const TPair<FName, FLinearColor>& Pair : ColorUserParameterValues)
	{
		Spawned->SetVariableLinearColor(Pair.Key, Pair.Value);
	}
}

void UAnimNotifyState_TimedNiagaraEffectEx::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_TimedNiagaraEffectEx::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	const FName Tag = SpawnedTag(this);
	for (USceneComponent* Child : MeshComp->GetAttachChildren())
	{
		UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Child);
		if (!Niagara || !Niagara->ComponentTags.Contains(Tag))
		{
			continue;
		}
		// AUTO-DESTROY FIRST, DEACTIVATE SECOND. OnSystemComplete only destroys under
		// `else if (bAutoDestroy)`, and DeactivateImmediate completes the system SYNCHRONOUSLY --
		// so setting the flag afterwards misses the one completion event that would have reclaimed
		// the component, and it survives forever, one per scrub.
		Niagara->SetAutoDestroy(true);
		// bDestroyAtEnd false means the author wants the system to finish on its own after the
		// window closes -- a real choice, honoured here. It still gets reclaimed on completion.
		if (bDestroyAtEnd)
		{
			Niagara->DeactivateImmediate();
		}
		else
		{
			Niagara->Deactivate();
		}
	}
}
