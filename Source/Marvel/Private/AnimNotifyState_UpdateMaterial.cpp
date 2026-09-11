// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// Drives a scalar material parameter from the authored curve across the notify window, so a
// dissolve, a glow ramp or a mask sweep can be timed against the animation. At the end it puts the
// parameter back exactly as it found it.
//
// THREE THINGS THIS FILE IS CAREFUL ABOUT, ALL OF THEM PREVIOUSLY WRONG HERE:
//
//  1. IT ALWAYS RESTORES. The restore used to be gated on bRecoverToCustomValue /
//     bRecoverDefaultValue, both FALSE on a freshly placed notify (these stubs have no
//     constructor) -- so by default the preview drove a parameter and never put it back, leaving
//     the preview mesh permanently altered for the rest of the session. A preview must not do that.
//     The authored recovery values are still honoured when set; "neither is set" now means
//     "restore what was there", not "leave it".
//
//  2. IT RESTORES ONE SCALAR, NOT EVERYTHING. The old default path called ClearParameterValues(),
//     which erases EVERY scalar, vector, texture and font override on the material instance -- not
//     the single parameter this notify touched. On a character material that is catastrophic and
//     nothing puts the rest back.
//
//  3. IT ONLY TOUCHES MATERIALS WHEN IT HAS SOMETHING TO DRIVE. Creating a dynamic instance on
//     every slot the moment the window opens permanently swaps the component's materials even when
//     no Curve is set and the notify can never draw anything.
//
// NOT PREVIEWED: SingleMaterialCurve, the per-slot array of (mesh, slot, query, curve) entries.
// Its FMaterialQuery resolves slots the same way the hide notifies do, but its MeshName addresses a
// mesh by name within the character's own blueprint -- state a montage preview does not have. The
// single ParameterName/Curve pair on the class is what previews; an author using only
// SingleMaterialCurve will correctly see nothing here.

#include "AnimNotifyState_UpdateMaterial.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialInstanceDynamic.h"

// The value each dynamic instance held before this notify drove it. Keyed weakly so a destroyed
// material cannot hold an entry alive. File-local because the generated header declares only the
// overrides, and shared because a notify object is shared too.
static TMap<TWeakObjectPtr<UMaterialInstanceDynamic>, float> GPriorScalar;

void UAnimNotifyState_UpdateMaterial::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// Nothing to drive means nothing to touch -- see note 3 above.
	if (!MeshComp || ParameterName.IsNone() || !Curve)
	{
		return;
	}

	for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
	{
		UMaterialInstanceDynamic* Dynamic = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
		if (!Dynamic || GPriorScalar.Contains(Dynamic))
		{
			continue;
		}
		// GetScalarParameterValue reports the effective value, which is what "put it back" means.
		float Prior = 0.0f;
		Dynamic->GetScalarParameterValue(ParameterName, Prior);
		GPriorScalar.Add(Dynamic, Prior);
	}
}

void UAnimNotifyState_UpdateMaterial::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !Curve || ParameterName.IsNone())
	{
		return;
	}

	// The curve is sampled at the montage's position WITHIN this window, so a window starting at
	// 2.0s samples the curve from 0 -- what an author means by "this curve, over this window".
	// There is no per-instance elapsed time on a shared notify object, so it is read from the one
	// place that actually has it.
	float Elapsed = 0.0f;
	if (const FAnimNotifyEvent* Event = EventReference.GetNotify())
	{
		if (const UAnimMontage* Montage = Cast<UAnimMontage>(Animation))
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				if (const FAnimMontageInstance* Instance =
					AnimInstance->GetActiveInstanceForMontage(Montage))
				{
					Elapsed = Instance->GetPosition() - Event->GetTriggerTime();
				}
			}
		}
	}

	const float Value = Curve->GetFloatValue(FMath::Max(Elapsed, 0.0f));
	for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* Dynamic =
			Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(i)))
		{
			Dynamic->SetScalarParameterValue(ParameterName, Value);
		}
	}
}

void UAnimNotifyState_UpdateMaterial::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (ParameterName.IsNone())
	{
		return;
	}

	// Iterate what was CAPTURED, not what the live properties currently match: editing
	// ParameterName while the playhead is inside the window would otherwise strand the old
	// parameter at whatever the curve last wrote, with nothing able to find it again.
	for (auto It = GPriorScalar.CreateIterator(); It; ++It)
	{
		UMaterialInstanceDynamic* Dynamic = It.Key().Get();
		if (!Dynamic)
		{
			It.RemoveCurrent();
			continue;
		}
		if (bRecoverToCustomValue)
		{
			Dynamic->SetScalarParameterValue(ParameterName, CustomValue);
		}
		else
		{
			// Covers bRecoverDefaultValue AND the both-false default. One parameter, put back to
			// what it was -- never ClearParameterValues, which would wipe every other override the
			// material carries.
			Dynamic->SetScalarParameterValue(ParameterName, It.Value());
		}
		It.RemoveCurrent();
	}
}
