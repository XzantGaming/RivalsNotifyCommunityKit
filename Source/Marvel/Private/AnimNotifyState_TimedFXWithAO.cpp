// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// Spawns the named FX at the named socket for the duration of the notify window, so the montage
// timeline shows when the effect starts and stops. The game's own class decides what really
// happens; this exists so you can place the window without cooking a pak first.
//
// STATE LIVES ON THE SPAWNED COMPONENT, NOT ON THIS OBJECT. A UAnimNotifyState is a shared CDO --
// one instance serves every montage, every character and every concurrently playing window -- so
// a member variable holding "the component I spawned" is wrong the moment two windows overlap.
// The engine's own TimedNiagaraEffect solves this by TAGGING the component it spawns and looking
// the tag up again on end, and that is what this does.

#include "AnimNotifyState_TimedFXWithAO.h"

#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"

// Both helpers here are file-local rather than members: the generator owns this class's header
// and declares only the overrides, so a preview cannot add members to its own class.

// The tag that ties a spawned component back to the notify that spawned it.
static FName SpawnedComponentTag(const UObject* Notify)
{
	return Notify ? Notify->GetFName() : NAME_None;
}

// Duplicated from UAnimNotify_PlayFXWithAO.cpp on purpose: the generator copies one .cpp per class
// into its module's Private/, and a shared header would have to be copied alongside every one of
// them. Twenty lines twice is cheaper than that machinery.
static void ApplyParameterConfig(UNiagaraComponent* Component,
	const FAnimNotifyFXParameterConfig& Config)
{
	if (!Component || !Config.bUseCustomParameters)
	{
		return;
	}
	for (const TPair<FName, bool>& Pair : Config.BoolUserParameterValues)
	{
		Component->SetVariableBool(Pair.Key, Pair.Value);
	}
	for (const TPair<FName, float>& Pair : Config.FloatUserParameterValues)
	{
		Component->SetVariableFloat(Pair.Key, Pair.Value);
	}
	for (const TPair<FName, FVector>& Pair : Config.VectorUserParameterValues)
	{
		Component->SetVariableVec3(Pair.Key, Pair.Value);
	}
	for (const TPair<FName, FLinearColor>& Pair : Config.ColorUserParameterValues)
	{
		Component->SetVariableLinearColor(Pair.Key, Pair.Value);
	}
}

void UAnimNotifyState_TimedFXWithAO::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	UFXSystemAsset* Asset = FXSystemAsset.LoadSynchronous();
	if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset))
	{
		UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(System,
			MeshComp, SpawnLocationSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true);
		if (Spawned)
		{
			Spawned->ComponentTags.AddUnique(SpawnedComponentTag(this));
			Spawned->SetRenderCustomDepth(bRenderCustomDepthPass != 0);
			ApplyParameterConfig(Spawned, ParameterConfig);
		}
	}
	else if (UParticleSystem* Cascade = Cast<UParticleSystem>(Asset))
	{
		if (UParticleSystemComponent* Spawned = UGameplayStatics::SpawnEmitterAttached(Cascade,
			MeshComp, SpawnLocationSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector::OneVector, EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true))
		{
			Spawned->ComponentTags.AddUnique(SpawnedComponentTag(this));
		}
	}
}

void UAnimNotifyState_TimedFXWithAO::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	// Nothing per frame. bUpdateTransform / bLeanCompensate / the UpDown and LeftRight factors are
	// driven by the character's aim, which a montage preview does not have -- see the note in
	// UAnimNotify_PlayFXWithAO.cpp about not guessing at aim.
}

void UAnimNotifyState_TimedFXWithAO::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	const FName Tag = SpawnedComponentTag(this);
	for (USceneComponent* Child : MeshComp->GetAttachChildren())
	{
		UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child);
		if (!Prim || !Prim->ComponentTags.Contains(Tag))
		{
			continue;
		}

		if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Prim))
		{
			// AUTO-DESTROY FIRST, DEACTIVATE SECOND. UNiagaraComponent::OnSystemComplete destroys
			// the component only under `else if (bAutoDestroy)` (NiagaraComponent.cpp), and
			// DeactivateImmediate completes the system SYNCHRONOUSLY -- so setting the flag
			// afterwards means the one completion event that would have destroyed it has already
			// been and gone, reading bAutoDestroy == false. Every spawned component then survives
			// forever, one per scrub, and DeactivateImmediate is the enum's zero value, which is
			// what every freshly placed notify uses.
			Niagara->SetAutoDestroy(true);
			switch (NiagaraDeactivateTypeAtNotifyEnd)
			{
			case ENiagaraDeactivateTypeAtNotifyEnd::DeactivateImmediate:
				Niagara->DeactivateImmediate();
				break;
			case ENiagaraDeactivateTypeAtNotifyEnd::Ignore:
				// The author asked for the system to finish on its own rather than being cut off
				// at the window's end. Auto-destroy is still set above, so it is reclaimed when it
				// completes -- "let it finish" must not mean "leak it".
				break;
			case ENiagaraDeactivateTypeAtNotifyEnd::Deactivate:
			default:
				Niagara->Deactivate();
				break;
			}
		}
		else if (UParticleSystemComponent* Cascade = Cast<UParticleSystemComponent>(Prim))
		{
			// Same ordering rule, same reason.
			Cascade->bAutoDestroy = true;
			Cascade->DeactivateSystem();
		}
	}
}

