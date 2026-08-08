# Cursor Work Report — AGP_Unit Composition Audit

## Status
GP-UNIT-COMPOSITION-AUDIT_READY_FOR_REVIEW

## Branch
feature/gp-s29r-combat-los-healthbar-teamcolors

## HEAD at audit
9f471a0ddc13f8fd563a02b038c52a83887facb9

## Scope
AUDIT ONLY. No production code changes. No Salvage Walker. No Blueprint edits. No automatic fixes.

---

## 1. Executive conclusion

| Question | Verdict |
| --- | --- |
| **REAL DUPLICATION** | **NO** (component instances). **PARTIAL** (Details category UX). |
| **Movement duplication** | **Not real.** Exactly **one** `UGP_MovementComponent`. Two Details "Movement" headers are category-leaf UX (`GP\|Movement` on actor pointer + same category on component properties). No second movement component / no `UCharacterMovementComponent` / no `UFloatingPawnMovement`. |
| **Visual duplication** | **Not real as dual visual owners.** Exactly **one** `UGP_UnitVisualComponent`. Additional StaticMesh parts under NativeFallback are **runtime/editor-generated** (`NewObject` + `RF_Transient`), owned by the visual builder pipeline — not a second default-subobject visual system. Two Details "Visual" headers are category-leaf UX (`GP\|Visual` on actor pointer + same category on `VisualSourceMode` / `VisualArchetype`). |

Operator observation maps to: **B + D(docs) + partial C(engine Pawn categories exist but are labeled "Pawn", not "Movement")**, not A.

---

## 2. Exact class hierarchy

Proven from constructors / headers:

```
AActor
  APawn
    AGP_UnitBase          // abstract
      AGP_MobileUnit      // abstract
        AGP_Unit          // concrete Blueprintable
```

(Related, not on this path: `AGP_BuildingBase` / `AGP_Worker` also inherit `AGP_UnitBase` / `AGP_MobileUnit` respectively.)

### Components added per level (constructors only)

| Level | CreateDefaultSubobject adds |
| --- | --- |
| **AGP_UnitBase** | `UnitCommandComponent`, `CombatPresentationComponent`, `TeamPresentationComponent`, `HealthBarComponent`, `AbilitySystemComponent`, `UnitAttributeSet` (UObject, not ActorComponent) |
| **AGP_MobileUnit** | `MovementComponent` (`UGP_MovementComponent`) |
| **AGP_Unit** | `CapsuleComponent` (root), `UnitVisualComponent` (`UGP_UnitVisualComponent`) |

No root is created in `AGP_UnitBase`. Root is set in `AGP_Unit` via Capsule.

---

## 3. Exact component inventory

Source: native CDO reflection via UnrealEditor-Cmd Python (`Default__GP_Unit`).

`COMP_COUNT=8` ActorComponents on CDO:

| Component name | Created in | Type | Scene? | ActorComponent? | Attach parent (CDO) | Gameplay / presentation | Tick (default) | BP Components panel | Details categories |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| UnitCommandComponent | UnitBase | `UGP_UnitCommandComponent` | no | yes | n/a | gameplay (commands / Attack FSM host) | yes (command tick paths) | yes | `GP\|Command` (pointer) |
| CombatPresentationComponent | UnitBase | `UGP_CombatPresentationComponent` | no | yes | n/a | presentation (combat VFX channel) | no | yes | `GP\|Combat\|Presentation` |
| TeamPresentationComponent | UnitBase | `UGP_TeamPresentationComponent` | no | yes | n/a | presentation (team tint) | no | yes | `GP\|Presentation\|Team` |
| HealthBarComponent | UnitBase | `UGP_HealthBarComponent` (`UWidgetComponent`) | **yes** | yes | attached to Capsule root at PostInit/BeginPlay | presentation | Automatic (Screen-space draw) | yes | `GP\|Presentation\|Health` |
| AbilitySystemComponent | UnitBase | `UGP_AbilitySystemComponent` | no | yes | n/a | gameplay (GAS) | ASC default | yes | `GP\|GAS` |
| MovementComponent | MobileUnit | `UGP_MovementComponent` | no | yes | n/a | gameplay (move backend) | can tick when moving; starts disabled | yes | `GP\|Movement` |
| CapsuleComponent | Unit | `UCapsuleComponent` | **yes** | yes | **ROOT** | gameplay (collision / Visibility block / selection bound) | no | yes | engine Shape/Collision (+ actor `GP\|Components` pointer) |
| UnitVisualComponent | Unit | `UGP_UnitVisualComponent` | no | yes | n/a | presentation owner | no | yes | `GP\|Visual` |

Also owned (not ActorComponent): `UnitAttributeSet` on UnitBase.

### Generated (not CDO default subobjects)

Under `VisualSourceMode=NativeFallback`, `GPPrimitiveVisualBuilder` creates `UStaticMeshComponent` parts via `NewObject(..., RF_Transient)`, attach under actor root / parent part, `RegisterComponent`. Destroyed by `ClearVisual` / EndPlay / mode switch. **Not** `CreateDefaultSubobject`. May appear in editor Components list on non-template preview instances when `OnRegister` calls `RefreshVisualMode` (`!IsTemplate()` editor path).

---

## 4. Why UE shows two Movement sections

### Answers to required questions

1. **How many `UGP_MovementComponent` on AGP_Unit?** → **1** (`MovementComponent`).
2. **Second movement component of another type?** → **No** (CDO inventory + ctor search: only MobileUnit creates movement; no CMC / FloatingPawnMovement).
3. **What are the two Details "Movement" sections?** → **Same category leaf twice**, not two components:
   - Actor UPROPERTY pointer: `AGP_MobileUnit::MovementComponent` → `Category = "GP|Movement"`.
   - Component properties: `MoveSpeed`, `AcceptanceRadius`, `RotationSpeed`, `bRotateToMovement` on `UGP_MovementComponent` → also `Category = "GP|Movement"`.
   - UE Details hierarchical display shows the leaf name **"Movement"** (under parent **GP**) for both the Class Defaults actor view (component reference) and the selected-component / inlined default-subobject property groups.
4. APawn inherited categories are labeled **`Pawn`**, not `Movement` — they do **not** explain a second "Movement" header.

**Verdict:** UX/category duplication (**B**), not real movement duplication (**not A**).

---

## 5. Why UE shows two Visual sections

### Answers

4. **How many visual/presentation components on AGP_Unit?** → Presentation-related ActorComponents: **UnitVisualComponent**, **CombatPresentationComponent**, **TeamPresentationComponent**, **HealthBarComponent** (4). Geometry presentation owner: **1** (`UnitVisualComponent`).
5. **Two components both owning unit geometry?** → **No.** Geometry primitives are generated under UnitVisual NativeFallback (or BP/SCS authored meshes). Team/Combat/HealthBar do not build body geometry.
6. **What are the two Details "Visual" sections?** → Again **category leaf collision**:
   - Actor pointer `AGP_Unit::UnitVisualComponent` → `Category = "GP|Visual"`.
   - Component fields `VisualSourceMode`, `VisualArchetype` (+ CallInEditor `RefreshVisualMode`) → `Category = "GP|Visual"`.
   - Leaf display name **"Visual"** appears twice when Class Defaults shows the pointer group and the component property group.
7. **BuiltVisual meshes persistent default subobjects?** → **No.** `NewObject` + `RF_Transient`; runtime/editor generated; cleared by `ClearVisual`.
8. **Can NativeFallback create appearance of an "extra visual component"?** → **Yes, in editor Components list:** generated StaticMesh parts register on the owner and can show as additional mesh components beside `UnitVisualComponent`, even though they are owned by the builder result, not a second visual *system*.
9. **Legacy visual/movement still present?** → Legacy single `VisualMesh` cylinder path removed (`HasLegacyVisualMesh()` always false). No legacy second movement component in code.
10. **Architectural duplication vs UX?** → **UX/category + optional editor-generated mesh listing**, not duplicated movement/visual architecture.

---

## 6. NativeFallback vs AuthoredComponents

| Mode | Behavior |
| --- | --- |
| **NativeFallback** (AGP_Unit C++ default) | `RefreshVisualMode` → `RebuildVisual` → builder creates Engine basic-shape StaticMesh parts into `BuiltVisual`. Owned as transient registered components on the actor. Cleared on EndPlay / ClearVisual / switch to Authored. Dedicated server suppresses construction. |
| **AuthoredComponents** | Clears generated `BuiltVisual` parts only. Does **not** destroy Blueprint/SCS/gameplay components. Blueprint meshes become the presentation source. Safe path for operator BP built from Engine primitives (intended). |

Who owns: `UGP_UnitVisualComponent` owns the *policy* and `BuiltVisual` bookkeeping; mesh UObjects are outer=`Owner` actor.

---

## 7. InfantryMelee meaning

- `EGP_VisualArchetype::InfantryMelee` is **cosmetic only** (header comment + `GPPrimitiveVisualTypes.h`).
- Selects which native primitive definition `MakeInfantryMeleeDefinition()` builds (≤4 Engine shapes).
- **No influence** on Attack range, damage, cadence, LOS, TeamId, or GAS attributes.
- Present on generic `AGP_Unit` because GP-S26B1 used InfantryMelee as the single shipped prototype archetype for the first concrete combat-capable unit actor; Worker intentionally uses a different presentation composition (PresentationRoot anchors, no UnitVisual on Worker).

---

## 8. ResourceNode comparison

| | AGP_Unit | AGP_ResourceNode |
| --- | --- | --- |
| Visual owner component | `UGP_UnitVisualComponent` | `UGP_ResourceNodeVisualComponent` |
| Actor pointer category | `GP\|Visual` | `GP\|Resource\|Visual` |
| Mode property category | `GP\|Visual` | `GP\|Visual` |
| Extra actor toggle | (none beyond mode on visual comp) | `bUseGeneratedPrototypeVisual` under `GP\|Presentation` |
| Builder | same `GPPrimitiveVisualBuilder` / `RF_Transient` parts | same pattern (Ore definition) |

Why ResourceNode toggle "looks different": actor exposes an extra **Presentation** bool alongside a pointer under **Resource|Visual**, while Unit collapses mode+archetype under the same **GP|Visual** leaf as the component pointer — amplifying the double-"Visual" Details impression on Unit BPs.

---

## 9. Documentation / code mismatches

Do **not** fix docs in this audit — list only.

From `Docs/TDD/05_Unit_Architecture.md` vs code:

| Doc claim | Actual code |
| --- | --- |
| Hierarchy stops at MobileUnit / BuildingBase; no `AGP_Unit` | `AGP_Unit` exists as concrete MobileUnit child |
| UnitBase owns StaticMesh/SkeletalMesh + Sphere root | Capsule root on Unit/Worker/MainBase; visuals via UnitVisual / BP / builder |
| ASC replication `Minimal` | Project uses Mixed on GP ASC |
| Replicated `UnitDefinitionAsset` / `UnitTags` snapshot on UnitBase | Not present as described; CapabilityTags EditDefaultsOnly; TeamId exists |
| Optional CharacterMovement vs custom | Custom `UGP_MovementComponent : UActorComponent` only |
| `UGP_CombatComponent` / `UGP_TargetingComponent` on combat units | Attack is UnitCommand FSM; no CombatComponent / TargetingComponent yet (Slice7 / S29R path) |
| `UGP_CommandReceiverComponent` | Commands via UnitBase ReceiveCommand → UnitCommandComponent |
| Worker composition sketch partially outdated | Worker has Cargo+Mining+PresentationRoot; correct direction but doc still mixes deferred CombatComponent story |

`Docs/GDD/04_Units.md`: roster (Worker / Salvage Walker) and pillars still valid as design intent; Salvage Walker **not implemented** as AGP_Unit child yet. GDD does not claim dual Movement/Visual components.

---

## 10. Recommended cleanup BEFORE Salvage Walker

Recommendations only — **no changes in this task**.

1. **Details UX:** Differentiate categories so Class Defaults cannot show two identical leaf headers — e.g. actor pointer `GP|Components` / `GP|Components|Visual` vs component tunables `GP|Visual|Native` (or DisplayName differentiation). Same for Movement pointer vs MoveSpeed group.
2. **Salvage Walker composition:** Prefer `AGP_Unit` (Capsule + UnitVisual + MobileUnit Movement) or a thin subclass; do **not** add a second movement or second geometry owner.
3. **AuthoredComponents default for new combat BP:** Set BP CDO `VisualSourceMode=AuthoredComponents` when artists add Engine/SCS meshes, to avoid NativeFallback transient meshes cluttering Components panel.
4. **Docs:** Refresh TDD/05 hierarchy to `UnitBase → MobileUnit → Unit/Worker` and remove Sphere/CombatComponent as current fact before Salvage Walker coding.
5. **Do not** remove Team/Combat/HealthBar presentation components — they are distinct responsibilities, not duplicates of UnitVisual.
6. **Do not** merge UnitVisual into TeamPresentation — different SoT (geometry mode vs team tint).

---

## 11. Files inspected

- `Docs/README.md`, `DOCUMENTATION_INDEX.md`, `AI_Project_Log.md`
- `Docs/GDD/04_Units.md`, `Docs/TDD/05_Unit_Architecture.md`, `Docs/TDD/04_RTS_Selection_And_Commands.md` (spot)
- `Docs/Development/Claude_Tasks/GP-S29R_Combat_LOS_HealthBar_TeamColors.md`
- `GPUnitBase.h/.cpp`, `GPMobileUnit.h/.cpp`, `GPUnit.h/.cpp`, `GPWorker.h/.cpp` (ctors)
- `GPMovementComponent.h/.cpp`
- `GPUnitVisualComponent.h/.cpp`, `GPPrimitiveVisualTypes.h`, `GPPrimitiveVisualBuilder.cpp`
- `GPTeamPresentationComponent.h`, `GPCombatPresentationComponent.h`, `GPHealthBarComponent.h`
- `GPResourceNode.h/.cpp`, `GPResourceNodeVisualComponent.h`
- Engine `Pawn.h` category spot-check
- Runtime CDO inventory: UnrealEditor-Cmd + `Tools/_audit_gp_unit_cdo.py` (local tool; not committed)

---

## 12. Production files changed
**NONE**

## 13. Operator assets changed
**NONE**

## Commit
PENDING_SHA
