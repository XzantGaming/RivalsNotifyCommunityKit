# Engine extensions -- what this pack cannot give you

Marvel Rivals runs a MODIFIED Unreal Engine. A few of its notify classes, and a few
properties on classes it inherits, live inside modules stock UE 5.3.2 already owns.
A plugin cannot add to those modules, so this folder records exactly what is missing
rather than shipping a stub that would look right in the editor and resolve to
nothing in game.

Everything here needs a SOURCE BUILD of Unreal Engine to act on. If you are not
doing that, treat this file as the list of fields you cannot set from the editor.

## Notify classes declared inside stock modules

An asset stores a notify by its full path, so these need the class to exist in
that exact module. The headers beside this file are ready to drop into a source
build of the module named in their folder.

- `/Script/NiagaraAnimNotifies.AnimNotifyState_TimedNiagaraEffectWithSpeed` -> `EngineExtensions/NiagaraAnimNotifies/AnimNotifyState_TimedNiagaraEffectWithSpeed.h`

## Properties the game added to stock classes

These classes exist in stock UE 5.3.2, and the game's copies declare MORE than
stock does. Inheriting the stock class gets you everything except the fields
listed here.

### `UAnimNotify` -- Engine, `Animation/AnimNotifies/AnimNotify.h`

- `NotifyCategory` : `EMarvelNotifyCategory`

### `UAnimNotifyState` -- Engine, `Animation/AnimNotifies/AnimNotifyState.h`

- `NotifyCategory` : `EMarvelNotifyCategory`

### `UAnimNotifyState_TimedNiagaraEffect` -- NiagaraAnimNotifies, `AnimNotifyState_TimedNiagaraEffect.h`

- `Scale3D` : `struct FVector`
- `ComponentTimeScale` : `float`
- `ComponentTags` : `TArray<class FName>`
- `bUsedTranslucencySortPriority` : `uint8`
- `TranslucencySortPriority` : `int32`
- `bUsedTranslucencySortDistanceOffset` : `uint8`
- `TranslucencySortDistanceOffset` : `float`

### `UAnimNotify_PlayNiagaraEffect` -- NiagaraAnimNotifies, `AnimNotify_PlayNiagaraEffect.h`

- `bUseCombineEffect` : `bool`
- `TemplateCombineEffect` : `TSoftObjectPtr<class UNiagaraSystem>`
- `bDeactivateOnMontageEnded` : `bool`
- `EffectLastTime` : `float`
- `ComponentTimeScale` : `float`
- `FloatUserParameterValues` : `TMap<class FName, float>`
- `VectorUserParameterValues` : `TMap<class FName, struct FVector>`
- `ColorUserParameterValues` : `TMap<class FName, struct FLinearColor>`
- `bUsedCustomStencil` : `uint8`
- `CustomStencilValue` : `int32`
- `bUsedTranslucencySortPriority` : `uint8`
- `TranslucencySortPriority` : `int32`
- `bUsedTranslucencySortDistanceOffset` : `uint8`
- `TranslucencySortDistanceOffset` : `float`
