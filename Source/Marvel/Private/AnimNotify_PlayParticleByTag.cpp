// EDITOR PREVIEW for a stub notify. Hand-written; the header beside it is generated.
//
// Restores the preview flattening removed. Spawns a Cascade system at SocketName.
//
// WHICH OF THE THREE TEMPLATES. The class names PSTemplate, DefaultPSTemplate and TagPSTemplate,
// and in game the choice depends on whether the character carries `Tag`. A montage preview has no
// character and no gameplay tags, so it shows the DEFAULT branch: PSTemplate if set, otherwise
// DefaultPSTemplate. TagPSTemplate is the tagged case and is deliberately not shown -- picking it
// would mean claiming a tag state the editor cannot know.

#include "AnimNotify_PlayParticleByTag.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

void UAnimNotify_PlayParticleByTag::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	UParticleSystem* System = PSTemplate.LoadSynchronous();
	if (!System)
	{
		System = DefaultPSTemplate.LoadSynchronous();
	}
	// LOOPING SYSTEMS ARE REFUSED, exactly as the stock engine notify refuses them
	// (AnimNotify_PlayParticleEffect.cpp:107 -- "Spawning suppressed"). A one-shot notify has no end
	// event, so a looping system's component can never auto-destroy: it would leak one component into
	// the persistent Persona preview world on every single pass over the notify.
	if (!System || System->IsLooping())
	{
		if (System)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: '%s' loops; suppressed in preview."),
				*GetName(), *System->GetName());
		}
		return;
	}

	const FVector UseScale = Scale.IsNearlyZero() ? FVector::OneVector : Scale;
	UParticleSystemComponent* Spawned = nullptr;
	if (Attached)
	{
		Spawned = UGameplayStatics::SpawnEmitterAttached(System, MeshComp, SocketName,
			LocationOffset, RotationOffset, UseScale, EAttachLocation::KeepRelativeOffset);
	}
	else
	{
		const FTransform World = FTransform(RotationOffset, LocationOffset)
			* MeshComp->GetSocketTransform(SocketName);
		Spawned = UGameplayStatics::SpawnEmitterAtLocation(MeshComp, System, World.GetLocation(),
			World.GetRotation().Rotator(), UseScale);
	}

	if (Spawned)
	{
		Spawned->SetCastShadow(bCastShadow != 0);
		Spawned->SetRenderCustomDepth(bRenderCustomDepthPass != 0);
	}
}
