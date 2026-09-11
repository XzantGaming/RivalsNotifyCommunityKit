// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// Hides Niagara components attached to the previewed mesh for the length of the window, and puts
// them back at the end.
//
// HIDDEN, NOT DEACTIVATED, and that distinction is the whole point of the class. Deactivating a
// system destroys its particles, so a system hidden mid-flight and shown again would restart from
// nothing. Setting visibility leaves the simulation running underneath, which is what makes a
// window like this usable to blink an effect out and back.
//
// THREE PLACES THIS CLASS WOULD OTHERWISE DO NOTHING AT ALL. These stubs have no constructor, so on
// a freshly placed notify bHideWhenBegin is false, bRestoreWhenEnd is false, and
// RequiredComponentTags is empty -- three independent reasons for the preview to be inert, which is
// indistinguishable from it being broken. So:
//   - the hide is not gated on bHideWhenBegin; the window is the instruction
//   - an EMPTY tag list matches every attached Niagara component, which is what "all required tags
//     are present" means for an empty requirement, rather than matching nothing
//   - the restore ALWAYS runs, regardless of bRestoreWhenEnd
// That last one is deliberate and is not merely a default-value workaround: a preview must never
// leave the scene permanently mutated. Honouring bRestoreWhenEnd=false would hide a component this
// notify did not create and never put it back, for the rest of the editing session.

#include "AnimNotifyState_TimedHideAttachedNiagara.h"

#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"

// What was hidden, and what its visibility was first. Restoring from a capture rather than forcing
// visible means a component that was already hidden before the window stays hidden after it.
static TMap<TWeakObjectPtr<UNiagaraComponent>, bool> GPriorVisibility;

static bool Matches(const UNiagaraComponent* Niagara, const TArray<FName>& RequiredTags)
{
	// EVERY tag must be present -- the field is named RequiredComponentTags, and an "any" reading
	// would hide effects the author did not name. An empty requirement is satisfied by everything.
	for (const FName& Tag : RequiredTags)
	{
		if (Tag.IsNone() || !Niagara->ComponentTags.Contains(Tag))
		{
			return false;
		}
	}
	return true;
}

void UAnimNotifyState_TimedHideAttachedNiagara::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	for (USceneComponent* Child : MeshComp->GetAttachChildren())
	{
		UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Child);
		if (!Niagara || !Matches(Niagara, RequiredComponentTags))
		{
			continue;
		}
		if (!GPriorVisibility.Contains(Niagara))
		{
			GPriorVisibility.Add(Niagara, Niagara->IsVisible());
		}
		Niagara->SetVisibility(false, /*bPropagateToChildren=*/false);
	}
}

void UAnimNotifyState_TimedHideAttachedNiagara::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_TimedHideAttachedNiagara::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// Iterate what was actually hidden, NOT what the current properties would match. Editing the
	// tag list while the playhead sits inside the window would otherwise strand the old components
	// hidden with nothing left that could ever find them again.
	for (auto It = GPriorVisibility.CreateIterator(); It; ++It)
	{
		if (UNiagaraComponent* Niagara = It.Key().Get())
		{
			Niagara->SetVisibility(It.Value(), /*bPropagateToChildren=*/false);
		}
		It.RemoveCurrent();
	}
}
