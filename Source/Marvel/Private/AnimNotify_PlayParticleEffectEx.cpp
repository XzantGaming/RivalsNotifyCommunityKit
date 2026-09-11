// EDITOR PREVIEW for a stub notify. Hand-written; the header beside it is generated.
//
// Restores the preview flattening removed: in the game this derives from
// AnimNotify_PlayParticleEffect, which stock UE implements. Spawns PSTemplate at SocketName with
// the authored offset, rotation and scale.
//
// bAttachToLocalCamera and bRettachToCharacterRootComponent (the game's spelling) are not
// previewed -- both re-parent the effect onto something a montage preview does not have.

#include "AnimNotify_PlayParticleEffectEx.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

void UAnimNotify_PlayParticleEffectEx::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UParticleSystem* System = PSTemplate.LoadSynchronous();
	if (!MeshComp || !System)
	{
		return;
	}
	// LOOPING SYSTEMS ARE REFUSED, exactly as the stock engine notify refuses them
	// (AnimNotify_PlayParticleEffect.cpp:107 -- "Spawning suppressed"). A one-shot notify has no end
	// event, so a looping system's component can never auto-destroy: it would leak one component into
	// the persistent Persona preview world on every single pass over the notify.
	if (System->IsLooping())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: '%s' loops; a one-shot notify cannot reclaim it, so the "
			"preview suppresses it."), *GetName(), *System->GetName());
		return;
	}

	const FVector UseScale = Scale.IsNearlyZero() ? FVector::OneVector : Scale;
	if (Attached)
	{
		UGameplayStatics::SpawnEmitterAttached(System, MeshComp, SocketName, LocationOffset,
			RotationOffset, UseScale, EAttachLocation::KeepRelativeOffset);
	}
	else
	{
		const FTransform World = FTransform(RotationOffset, LocationOffset)
			* MeshComp->GetSocketTransform(SocketName);
		UGameplayStatics::SpawnEmitterAtLocation(MeshComp, System, World.GetLocation(),
			World.GetRotation().Rotator(), UseScale);
	}
}
