// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// For the length of the window: finds the components on the previewed actor carrying ComponentTag,
// applies the authored visibility change, and optionally re-attaches them to a socket. At the end
// it RESTORES what it changed.
//
// RESTORE MEANS RESTORE. An earlier version "reverted" by writing the opposite of what it had
// written at begin. That is not an undo: a component already visible, told ETV_SetVisibilityOn,
// was left permanently HIDDEN after the window -- a state the author never asked for and that
// nothing ever put back. Both the visibility and the attachment are now captured before they are
// touched and put back exactly.
//
// IT DRIVES SetVisibility EVEN FOR THE HiddenInGame MODES. bHiddenInGame is only consulted in a
// GAME world, so in an animation-editor preview two of the four ETimedVisibility values would do
// nothing visible at all. The authored distinction is real in game and meaningless here, so all
// four modes are mapped onto visibility -- the point of a preview is to show the change.

#include "AnimNotifyState_TimedAttachment.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

// What this notify changed, so the end of the window can put it back. Keyed weakly; file-local
// because the generated header declares only the overrides.
struct FTimedAttachmentPrior
{
	bool bVisible = true;
	TWeakObjectPtr<USceneComponent> AttachParent;
	FName AttachSocket = NAME_None;
	FTransform RelativeTransform = FTransform::Identity;
	bool bWasAttached = false;
};
static TMap<TWeakObjectPtr<USceneComponent>, FTimedAttachmentPrior> GPrior;

static void CollectTagged(AActor* Owner, FName Tag, bool bSearchAttachedActors,
	TArray<USceneComponent*>& Out)
{
	if (!Owner || Tag.IsNone())
	{
		return;
	}
	TArray<USceneComponent*> Components;
	Owner->GetComponents(Components);
	for (USceneComponent* Component : Components)
	{
		if (Component && Component->ComponentHasTag(Tag) && !Out.Contains(Component))
		{
			Out.Add(Component);
		}
	}
	if (bSearchAttachedActors)
	{
		TArray<AActor*> Attached;
		Owner->GetAttachedActors(Attached);
		for (AActor* Child : Attached)
		{
			CollectTagged(Child, Tag, /*bSearchAttachedActors=*/false, Out);
		}
	}
}

void UAnimNotifyState_TimedAttachment::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	TArray<USceneComponent*> Targets;
	CollectTagged(MeshComp->GetOwner(), ComponentTag, bSearchAttachedActors, Targets);

	for (USceneComponent* Target : Targets)
	{
		if (!GPrior.Contains(Target))
		{
			FTimedAttachmentPrior Prior;
			Prior.bVisible = Target->IsVisible();
			Prior.AttachParent = Target->GetAttachParent();
			Prior.AttachSocket = Target->GetAttachSocketName();
			Prior.RelativeTransform = Target->GetRelativeTransform();
			Prior.bWasAttached = Target->GetAttachParent() != nullptr;
			GPrior.Add(Target, Prior);
		}

		switch (TimedVisibility)
		{
		case ETimedVisibility::ETV_SetVisibilityOn:
		case ETimedVisibility::ETV_SetHiddenInGameOff:
			Target->SetVisibility(true, /*bPropagateToChildren=*/false);
			break;
		case ETimedVisibility::ETV_SetVisibilityOff:
		case ETimedVisibility::ETV_SetHiddenInGameOn:
			Target->SetVisibility(false, /*bPropagateToChildren=*/false);
			break;
		default:
			break;
		}

		if (TimedAttachment == ETimedAttachment::ETA_AttachToComponent)
		{
			const FAttachmentTransformRules Rules(AttachmentLocationRule, AttachmentRotationRule,
				AttachmentScaleRule, bWeldSimulatedBodies);
			Target->AttachToComponent(MeshComp, Rules, AttachmentSocketName);
		}
	}
}

void UAnimNotifyState_TimedAttachment::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_TimedAttachment::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Restore from what was CAPTURED, and iterate the captured set rather than re-deriving targets
	// from the notify's live properties: editing ComponentTag while the playhead is inside the
	// window would otherwise strand the old target in its changed state forever.
	for (auto It = GPrior.CreateIterator(); It; ++It)
	{
		USceneComponent* Target = It.Key().Get();
		if (!Target)
		{
			It.RemoveCurrent();
			continue;
		}

		const FTimedAttachmentPrior& Prior = It.Value();
		Target->SetVisibility(Prior.bVisible, /*bPropagateToChildren=*/false);

		// Put the component back where it hung before. The prior parent and socket are readable off
		// the live component, so "we cannot know where it came from" was simply false -- the old
		// code just never captured them, and left the preview actor's hierarchy permanently
		// rewritten every time a montage was scrubbed.
		if (TimedAttachment == ETimedAttachment::ETA_AttachToComponent)
		{
			if (Prior.bWasAttached && Prior.AttachParent.IsValid())
			{
				Target->AttachToComponent(Prior.AttachParent.Get(),
					FAttachmentTransformRules::KeepRelativeTransform, Prior.AttachSocket);
				Target->SetRelativeTransform(Prior.RelativeTransform);
			}
			else
			{
				Target->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
		}
		It.RemoveCurrent();
	}
}
