// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// The windowed twin of AnimNotify_PlayParticleOnWeapon: spawns PSTemplate at SocketName for the
// length of the window. See that file for why the "on weapon" half cannot be previewed -- EquipID
// resolves against character equipment the editor does not have, so the effect rides the previewed
// mesh's socket of that name instead.

#include "AnimNotifyState_TimedParticleOnWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

static FName SpawnedTag(const UObject* Notify)
{
	return Notify ? Notify->GetFName() : NAME_None;
}

void UAnimNotifyState_TimedParticleOnWeapon::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UParticleSystem* System = PSTemplate.LoadSynchronous();
	if (!MeshComp || !System)
	{
		return;
	}

	const FVector UseScale = Scale.IsNearlyZero() ? FVector::OneVector : Scale;

	// Honour bAttached, the class's own discriminator. The one-shot twin of this notify branches on
	// exactly that field; this one always attached, silently, with no comment saying why.
	UParticleSystemComponent* Spawned = nullptr;
	if (bAttached)
	{
		Spawned = UGameplayStatics::SpawnEmitterAttached(System, MeshComp,
			SocketName, LocationOffset, RotationOffset, UseScale,
			EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true);
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
		Spawned->ComponentTags.AddUnique(SpawnedTag(this));
	}
}

void UAnimNotifyState_TimedParticleOnWeapon::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_TimedParticleOnWeapon::NotifyEnd(USkeletalMeshComponent* MeshComp,
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
		UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Child);
		if (!PSC || !PSC->ComponentTags.Contains(Tag))
		{
			continue;
		}
		// bDestroyAtEnd false lets the system finish on its own after the window closes.
		// Auto-destroy was set at SPAWN time, which is the only point early enough to catch a
		// system that completes before the window ends.
		if (bDestroyAtEnd)
		{
			PSC->KillParticlesForced();
		}
		PSC->DeactivateSystem();
	}
}
