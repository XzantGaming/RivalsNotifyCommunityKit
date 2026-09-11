# Marvel Rivals Anim Notifies — a stub pack for stock UE 5.3.2

Author Marvel Rivals montages in a **stock** Unreal Engine 5.3.2 editor, with every anim notify
the game has, spelled the way the game spells it.

Drop this folder into `<YourProject>/Plugins/` and open the project. That is the whole install.

---

## What this is, and why it has to be C++

A montage does not store a notify's *settings* and leave the class implied. It stores the notify's
**class path** — a string like:

```
/Script/Marvel.AnimNotifyState_TimedNiagaraEffectEx
```

When the game loads your montage, it resolves that string against **its own** classes. So the only
thing your editor has to get right is the *name*: the module, the class, and the properties. The
code behind them is irrelevant — the game never runs it.

That is also why this could not be a Blueprint pack. A Blueprint notify lives at a content path
(`/Game/MyNotifies/ANS_Thing.ANS_Thing_C`), and the game has nothing at that path. It would look
correct in the editor and do **nothing** in game, with no error to tell you why. Only a native C++
module named exactly `Marvel` can produce `/Script/Marvel.…`.

So: **24 modules, named after the game's own modules, holding 179 notify classes plus the 27
structs and 36 enums their properties need.**

| module | what's in it |
|---|---|
| `Marvel` | 137 notifies — the bulk of the game |
| `Hero_1027` … `Hero_1061`, `HeroGAS_1023` | per-hero notifies (Wolverine, Angela, Peni Parker, …) |
| `MassMonster`, `MarvelLevel`, `MarvelGameplayCues` | mode- and level-specific notifies |
| `PyAbility_212`, `PySpaceVehicle`, … | notifies the game defines in Python |
| `KawaiiPhysics` | 2 notifies that live in a plugin, not the game — supplied by a [required dependency](#the-kawaii-physics-notifies) |
| `MarvelRivalsExternalStubs` | the few Wwise types the audio notifies reference |

Everything is generated from the game's SDK dump. Nothing is guessed from a name: a class is a
notify because its **ancestry** reaches `UAnimNotify`/`UAnimNotifyState`, its module is the one the
dump states, and its properties are the ones the dump declares — with the editor visibility
specifiers the dump records, so what you can edit matches what the game lets you edit.

---

## Using it

1. Drop the folder into `<YourProject>/Plugins/`. Because `Binaries/Win64/` is included, a
   Blueprint-only project works — you do not need Visual Studio or a C++ project.
2. Open the project. If the editor asks to rebuild, the binaries do not match your engine build;
   see **Building from source**.
3. Open an **Anim Montage** (or Anim Sequence) and find the **Notifies** track under the timeline.
4. **Right-click the track** at the time you want:
   - **Add Notify...** — an instant notify, fired at one moment (`AnimNotify_*`).
   - **Add Notify State...** — a notify with a start and an end you drag out (`AnimNotifyState_*`).
5. A class picker opens **with a search box**. Type the class name — `TimedNiagaraEffectEx`,
   `PlayFXWithAO`, `TimedSkeletonAnimation`. Every class is listed under its exact name as it
   appears in FModel and the SDK dump, so whatever you already know it as will find it.
6. Click the placed notify and fill in its properties in the **Details** panel.
7. Cook and pak as you normally would for a Marvel Rivals mod.

The two menus are separate lists and the split is by base class, not by name: an `AnimNotify_*`
only ever appears under *Add Notify*, an `AnimNotifyState_*` only under *Add Notify State*.

All 179 classes appear in those pickers. The editor filters the list on
`CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract` and on `CanBePlaced()`;
none of the stubs set those flags or override `CanBePlaced`, so none are hidden from you.

### Finding the right one

Per-hero notifies are named for their hero's numeric ID, not the hero's name —
`AnimNotify_PlayNiagaraEffectEx_105461` is in module `Hero_1054`. If you know the hero ID from
the pak paths, searching that number narrows the list fast.

### The Kawaii Physics notifies

The game's two Kawaii Physics notifies live in the `KawaiiPhysics` module, which belongs to a
plugin rather than to the game. This pack does **not** stub them. It declares a dependency on
[KawaiiPhysicsRivals](https://github.com/XzantGaming/KawaiiPhysicsRivals) instead, which
provides that module for real — same two classes, same three properties, same order.

Install both plugins into `Plugins/` and everything resolves on its own.

If `KawaiiPhysics` is missing, the editor says so plainly on startup:

```
This project requires the 'KawaiiPhysics' plugin. Install it and try again,
or remove it from the project's required plugin list.
```

**Why a dependency rather than a stub.** Only one module named `KawaiiPhysics` can exist in a
project — the name is the `/Script/` package path the game's assets reference, so neither side
can rename. An earlier version of this pack shipped its own 2-class stub, which collided with
the real plugin and made its editor module fail to load with `GetLastError=127`
(`ERROR_PROC_NOT_FOUND`): the imports bound to whichever `UnrealEditor-KawaiiPhysics.dll`
Windows found first. Declaring the dependency removes the collision instead of documenting a
workaround for it.

If you truly want notifies without the physics plugin, delete the `KawaiiPhysics` entry from
the `Plugins` array in `MarvelRivalsAnimNotifies.uplugin`. Those two notifies then will not
exist in your editor.

---

## Flattened classes

Eight stubs do not derive from the class the game derives them from. UE marks most notify classes
`MinimalAPI`; `UAnimNotify` and `UAnimNotifyState` get away with it because they export their
members individually, but `AnimNotify_PlayNiagaraEffect`, `AnimNotifyState_TimedNiagaraEffect`,
`AnimNotify_PlayMontageNotify` and friends do not — a subclass in another module fails at **link**
time on their virtuals. No Build.cs dependency fixes that; they were never meant to be subclassed
from outside.

Those eight derive from `UAnimNotify`/`UAnimNotifyState` instead and declare the skipped
ancestor's properties directly. Each says so in a comment at the top of its header.

**This changes nothing about the bytes.** A `.uasset` tags each property by NAME, not by the class
that declared it; on load the game looks the name up across its own class's whole chain. And since
the ancestor's fields come from the SDK dump rather than from stock UE, flattening actually gets
you *more*: the 15 fields the game added to `AnimNotify_PlayNiagaraEffect` and the 7 it added to
`AnimNotifyState_TimedNiagaraEffect` are authorable here, where inheriting the stock parent would
have hidden them.

---

## Editor preview

**16 notifies do something when you scrub the montage, and 38 of the 179 classes get a preview**
once inheritance is counted — a subclass of a previewed class inherits its behaviour.

| notify | what the preview does |
|---|---|
| `AnimNotifyState_TimedNiagaraEffectEx` | spawns the Niagara system at the socket for the window, with offsets, scale, user parameters and render flags |
| `AnimNotify_PlayNiagaraEffectEx` | instant Niagara spawn, including the combine effect when enabled |
| `AnimNotify_PlayFXWithAO` | spawns the Niagara/Cascade effect at the named bone with the authored offset |
| `AnimNotifyState_TimedFXWithAO` | same, held for the window, torn down per the authored deactivate mode |
| `AnimNotify_PlayParticleEffectEx` | instant Cascade spawn at the socket |
| `AnimNotify_PlayParticleByTag` | Cascade spawn, untagged branch |
| `AnimNotify_PlayParticleOnWeapon` | Cascade spawn at the socket |
| `AnimNotifyState_TimedParticleOnWeapon` | Cascade spawn held for the window |
| `AnimNotifyState_TimedSkeletonAnimation` | spawns the prop mesh, attaches it to the socket, plays its animation |
| `AnimNotifyState_TimedAttachment` | applies the visibility change and re-attach to the tagged component |
| `AnimNotifyState_SetComponentsInActorVisible` | shows/hides tagged components at begin and again at end |
| `AnimNotifyState_TimedHideBones` | hides the named bones for the window |
| `AnimNotifyState_TimedHideAttachedNiagara` | hides tagged attached Niagara without killing the simulation |
| `AnimNotify_HideMaterial` | hides/shows the named material slots at that instant |
| `AnimNotifyState_TimedHideMaterial` | hides the named slots for the window |
| `AnimNotifyState_UpdateMaterial` | drives the scalar parameter from the curve across the window |

**A preview is not a claim of fidelity.** It is close enough to time and place an effect; what the
game does is decided by the game's code. Where a faithful preview would need game state the editor
does not have, the preview does **nothing** rather than guessing, and says so in a comment next to
the field that would have driven it. The recurring cases:

- **aim** — the `WithAO` notifies lean their effect on the character's aim pitch/yaw.
- **equipment** — `OnWeapon` notifies resolve `EquipID` against the character's weapons; the
  effect rides the previewed mesh's socket of that name instead.
- **gameplay tags** — `PlayParticleByTag` picks its template from a tag the editor cannot know,
  so the untagged branch is shown.
- **hero blueprints** — `TimedSkeletonAnimation` previews only `MeshSource = SkeletalMeshTemplate`;
  the other two sources resolve against a hero's own blueprint.
- **material tags** — a `FMaterialQuery` addressing a slot by tag resolves to nothing; by index or
  slot name it resolves normally.

Every other notify compiles and exposes its full property set with an empty body. That is the
intended state, not an omission.

---

## Known limitation: property defaults

**These stubs have no constructor, so every property starts at C++ zero — every bool `false`,
every float `0`, every vector `(0,0,0)`.** The SDK dump describes a class's LAYOUT, not the
default values the game's own constructor assigns, and nothing in the paks carries them either.

That matters because of how Unreal saves: a property is written to the `.uasset` **only when it
differs from the class default**. So if the game's default for some flag is `true` and you want
`false`, you set `false` here, it matches this stub's default, nothing is written — and in game
the value falls back to the game's `true`. Your setting is silently dropped.

Where the game's default is KNOWN, the stub now reproduces it in a constructor and the problem goes
away for that property. Four classes carry such a constructor, copied from the stock parent whose
constructor the flattening removed: `AnimNotify_PlayNiagaraEffectEx`, `AnimNotify_PlayParticleEffectEx`,
`AnimNotify_PlayParticleByTag` and `AnimNotify_PlayParticleOnWeapon` all set `Attached = true` and
`Scale = (1,1,1)`. Before that, ticking `Attached` off wrote nothing — it matched the stub's default
— and the game attached the effect anyway.

Everywhere else the game's defaults are unknowable from the dump, so most UE defaults being zero is
what carries it. If a setting appears not to take effect in game, this is the first thing to suspect.

The same gap bit the editor previews: `AnimNotifyState_TimedSkeletonAnimation` gates its prop on
`bVisibleDuringAnimNotifyState`, which defaults to `false` here, so the preview spawned the mesh
and hid it in the same frame. The preview now ignores that flag deliberately — see the comment in
its source.

---

## What this pack cannot give you

Marvel Rivals runs a **modified** engine. A few notify classes, and a few properties on classes
you inherit, live inside modules stock UE 5.3.2 already owns — and a plugin cannot add to those.

`EngineExtensions/README.md` lists every one of them, including the seven extra fields the game
added to `AnimNotifyState_TimedNiagaraEffect` and the fifteen it added to
`AnimNotify_PlayNiagaraEffect`. If you build UE from source you can apply them; if you don't,
that file is your list of fields you cannot set from the editor.

One notify, `ANS_HideBone_BP_C`, is a **Blueprint** class in the game's content, not C++. No
module can reproduce its path, so it is not included.

---

## Rebuilding it for a new game patch

The pack is generated. When the game updates and a new SDK dump exists:

```
python tools/gen_notify_stubs.py --sdk <path to CppSDK/SDK> --engine <path to UE 5.3>
```

It re-reads the dump, re-indexes the engine, and rewrites `Source/`. **It does not rebuild
`Binaries/`** — after regenerating, rebuild the plugin or delete `Binaries/Win64/` so the editor
compiles the new source instead of loading stale DLLs. Hand-written previews live in
`tools/stub_impl/` and survive regeneration. The generator **fails loudly** rather than guessing:
an unresolvable type, an engine it can't find, or a preview for a class that no longer exists all
stop the run.

---

## Building from source

`Binaries/Win64/` holds 24 prebuilt editor DLLs for **UE 5.3.2 on Win64**, built with MSVC
14.36. Debug symbols are not shipped — they are 153 MB against 2.7 MB of DLLs.

To build it yourself you need Visual Studio
2022 with an MSVC toolset **UE 5.3 supports — 14.34 to 14.38**.

A current VS 2022 installs 14.4x, which **UE 5.3 cannot compile with at all**: its own
`ConcurrentLinearAllocator.h` uses `__has_feature`, and `WindowsPlatformCompilerSetup.h` promotes
the resulting warning to an error from inside the engine, where no build flag reaches it. This is
an engine-and-compiler problem, not a problem with this pack. Add the older toolset:

```
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe" modify ^
  --installPath "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools" ^
  --add Microsoft.VisualStudio.Component.VC.14.36.17.6.x86.x64 --quiet --norestart
```

14.36 is on UE 5.3's preferred list, so the build picks it over 14.4x automatically.

---

## Licence and provenance

The class and property names here describe Marvel Rivals' data format, read out of a public SDK
dump. No game code, assets or content are included or redistributed. The stub bodies and the
preview implementations are original.
