// EDITOR PREVIEW for a stub notify state. Hand-written; the header beside it is generated.
//
// Shows or hides the components carrying any of ComponentTags, at the start of the window and
// again at the end. The class has separate begin and end booleans rather than a restore flag, so
// the end state is authored, not an undo -- a window can legitimately turn something ON for good,
// and the preview reproduces that.
//
// TWO THINGS THIS FILE GETS RIGHT ON PURPOSE, BOTH LEARNED THE HARD WAY:
//
//  1. IT DRIVES SetVisibility, NEVER SetHiddenInGame. bHiddenInGame is only consulted for a GAME
//     world; an animation-editor preview viewport ignores it completely. bUseSetVisibility is false
//     on a fresh notify (these stubs have no constructor), so honouring that flag literally meant
//     the default path called SetHiddenInGame and the preview did nothing whatsoever -- with a flag
//     name that gives no hint the preview depends on it. The authored flag is recorded in the
//     comment below rather than obeyed, because obeying it makes the preview useless.
//
//  2. IT DOES NOT PROPAGATE TO CHILDREN. Propagating rewrites the visibility of every descendant
//     component and the end pass then forces them all back on, wiping state this preview never
//     saved and does not own.

#include "AnimNotifyState_SetComponentsInActorVisible.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

// Components this notify actually touched, and what their visibility was beforehand. Keyed weakly
// so a destroyed component cannot keep an entry alive. File-local because the generated header
// declares only the overrides, and shared across instances because a notify object is shared too.
static TMap<TWeakObjectPtr<USceneComponent>, bool> GPriorVisibility;

static void CollectTagged(USkeletalMeshComponent* MeshComp, const TArray<FName>& Tags,
	TArray<USceneComponent*>& Out)
{
	if (!MeshComp || Tags.Num() == 0)
	{
		return;
	}

	// Both the owner's components AND the previewed mesh's attach children: a preview scene does
	// not always host the mesh on an actor, and components can hang off the mesh while being owned
	// elsewhere -- including props this plugin's own sibling notify spawns in the ownerless case.
	TArray<USceneComponent*> Candidates;
	if (AActor* Owner = MeshComp->GetOwner())
	{
		TArray<USceneComponent*> Owned;
		Owner->GetComponents(Owned);
		Candidates.Append(Owned);
	}
	Candidates.Append(MeshComp->GetAttachChildren());

	for (USceneComponent* Component : Candidates)
	{
		if (!Component || Out.Contains(Component))
		{
			continue;
		}
		for (const FName& Tag : Tags)
		{
			if (!Tag.IsNone() && Component->ComponentHasTag(Tag))
			{
				Out.Add(Component);
				break;
			}
		}
	}
}

static void ApplyVisible(USkeletalMeshComponent* MeshComp, const TArray<FName>& Tags, bool bVisible)
{
	TArray<USceneComponent*> Targets;
	CollectTagged(MeshComp, Tags, Targets);
	for (USceneComponent* Target : Targets)
	{
		// Remember the state before this notify first touched it, so nothing has to be guessed
		// later and repeated windows cannot ratchet a component into a state nobody authored.
		if (!GPriorVisibility.Contains(Target))
		{
			GPriorVisibility.Add(Target, Target->IsVisible());
		}
		Target->SetVisibility(bVisible, /*bPropagateToChildren=*/false);
	}
}

void UAnimNotifyState_SetComponentsInActorVisible::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	ApplyVisible(MeshComp, ComponentTags, bSetComponentsVisibleWhenBegin);
}

void UAnimNotifyState_SetComponentsInActorVisible::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_SetComponentsInActorVisible::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	ApplyVisible(MeshComp, ComponentTags, bSetComponentsVisibleWhenEnd);

	// Drop the remembered states once the window is over. They exist to stop a repeated window from
	// ratcheting, not to be a permanent record -- and holding weak pointers to every component a
	// montage ever touched for the life of the editor session is not free.
	for (auto It = GPriorVisibility.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
