// EDITOR PREVIEW for a stub notify. Hand-written; the header beside it is generated.
//
// Restores the preview flattening removed. Spawns PSTemplate at SocketName.
//
// "ON WEAPON" IS THE PART THAT CANNOT BE PREVIEWED. EquipID names a weapon from the character's
// equipment, which a montage preview has no character to look up -- so the effect is spawned on
// the PREVIEWED MESH's socket of that name instead. If your preview mesh has the socket, the
// timing and rough placement are right; the parent is not. bUseExisting (reuse an already-spawned
// component found by PSComponentTag) is ignored for the same reason: there is nothing to reuse.

#include "AnimNotify_PlayParticleOnWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

void UAnimNotify_PlayParticleOnWeapon::Notify(USkeletalMeshComponent* MeshComp,
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
		UE_LOG(LogTemp, Warning, TEXT("%s: '%s' loops; suppressed in preview."),
			*GetName(), *System->GetName());
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
