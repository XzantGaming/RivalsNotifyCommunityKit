// EDITOR PREVIEW for a stub notify. Hand-written; the header beside it is generated.
//
// This is NOT a reimplementation of the game's notify. It spawns the effect the notify names, at
// the socket it names, with the offset it carries, so that scrubbing a montage in the editor
// shows you roughly where and when the effect appears. What the game actually does with this
// notify is decided by the game's code, which resolves the class by path at runtime and ignores
// everything here.
//
// The AO ("aim offset") half is deliberately NOT approximated: MinPitch/MaxPitch/LerpStart/LerpEnd
// drive the effect from the character's aim, which a montage preview has no aim to read. Guessing
// would put the effect somewhere the game never puts it, which is worse than putting it at the
// socket and saying so.

#include "AnimNotify_PlayFXWithAO.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"

// File-local, not a member: the generator owns the header and declares only the overrides it
// knows about, so a preview cannot add members to its own class. Duplicated in the one other
// preview that needs it rather than dragging a shared header through every module's Private/.
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

void UAnimNotify_PlayFXWithAO::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// A soft pointer in a notify is normally streamed in ahead of the montage. There is nothing
	// to stream against in a preview, so it is loaded synchronously -- acceptable in the editor
	// and nowhere else.
	const FVector Location = OffsetTransform.GetLocation();
	const FRotator Rotation = OffsetTransform.GetRotation().Rotator();
	const FVector Scale = OffsetTransform.GetScale3D();

	if (UNiagaraSystem* System = NiagaraParticle.LoadSynchronous())
	{
		UNiagaraComponent* Spawned = nullptr;
		if (bIsAttached)
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(System, MeshComp, BoneName,
				Location, Rotation, EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true);
		}
		else
		{
			const FTransform SocketTransform = MeshComp->GetSocketTransform(BoneName);
			const FTransform World = OffsetTransform * SocketTransform;
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(MeshComp,
				System, World.GetLocation(), World.GetRotation().Rotator());
		}

		if (Spawned)
		{
			Spawned->SetRelativeScale3D(Scale.IsNearlyZero() ? FVector::OneVector : Scale);
			Spawned->SetRenderCustomDepth(bRenderCustomDepthPass != 0);
			ApplyParameterConfig(Spawned, ParameterConfig);
		}
	}

	// Cascade is the legacy path and the game still uses it on some notifies. Previewing it costs
	// one call, so there is no reason to leave it out.
	if (UParticleSystem* Cascade = CascadeParticle.LoadSynchronous())
	{
		if (bIsAttached)
		{
			UGameplayStatics::SpawnEmitterAttached(Cascade, MeshComp, BoneName,
				Location, Rotation, Scale.IsNearlyZero() ? FVector::OneVector : Scale,
				EAttachLocation::KeepRelativeOffset);
		}
		else
		{
			const FTransform World = OffsetTransform * MeshComp->GetSocketTransform(BoneName);
			UGameplayStatics::SpawnEmitterAtLocation(MeshComp, Cascade, World.GetLocation(),
				World.GetRotation().Rotator(), World.GetScale3D());
		}
	}
}
