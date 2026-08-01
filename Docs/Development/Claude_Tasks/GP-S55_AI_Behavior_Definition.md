# GP-S55 AI Behavior DataAsset

## Slice Group
Slice 10 — AI Opponent

## Code Allowed
Yes — after GP-S54 approval.

## Depends On
- GP-S54.

## Goal
Implement `UGP_AIBehaviorDefinition` як DataAsset, що conf-ить state machine thresholds, decision interval, drop biases. Single MVP behavior tier (DA instance: `DA_GP_AI_Behavior_Default`).

## Scope

- Create `UGP_AIBehaviorDefinition : UPrimaryDataAsset` у `GPRuntime/Public/AI/`.
- Fields (всі DA-editable):
  - `DecisionInterval : float` — seconds between decisions (placeholder 3.0).
  - `TargetWorkerCount : int32` — bias toward Order Worker (placeholder 6).
  - `TargetCombatRoster : int32` — bias toward Salvage Walker (placeholder 4).
  - `TargetTurretCount : int32` — bias toward Defensive Turret (placeholder 3).
  - `ShipUrgencyThreshold : float` — container fill % to prioritize Ship state (placeholder 0.8).
  - `DefenseThreshold : float` — FerroniteThreatValue cross to switch to Defend (placeholder 50.0).
  - `BaseHealthDefenseThreshold : float` — MainBase HP % to trigger Defend (placeholder 0.3).
  - `EnemyBaseRadiusForDefend : float` — cm; if enemy visible within → Defend (placeholder 2000.0).
  - `PreferredWorkerDrop : TSoftObjectPtr<UGP_OrbitalDropDefinition>` — soft ref.
  - `PreferredCombatDrop : TSoftObjectPtr<UGP_OrbitalDropDefinition>` — soft ref.
  - `PreferredTurretDrop : TSoftObjectPtr<UGP_OrbitalDropDefinition>` — soft ref.
  - `PreferredLogisticsHubDrop : TSoftObjectPtr<UGP_OrbitalDropDefinition>` — soft ref.
  - `WorkerDropRadiusFromBase : float` — drop bias.
  - `CombatDropRadiusFromBase : float` — drop bias.
  - `TurretDropPreferredChokeZones : TArray<FVector>` — optional manual list per map; else AI computes from FoW edge.
  - `bUsesWalls : bool` — placeholder false для MVP.
  - `MinStateDuration : float` — minimum seconds before re-evaluating (anti-thrash, placeholder 1.0).
- Asset instance `DA_GP_AI_Behavior_Default.uasset` у `/Game/GrimProtocol/DataAssets/AI/`.
- Asset Manager primary asset type registration `GP_AIBehavior`.

## Out of Scope

- Multiple difficulty tiers (post-MVP).
- Behavior Tree assets (post-MVP).
- Per-faction AI variations.

## Required Skill Pass

- `ue5-gas` (DataAsset patterns).

## Files Touched

- `GP/Source/GPRuntime/Public/AI/GPAIBehaviorDefinition.h` — new
- `GP/Source/GPRuntime/Private/AI/GPAIBehaviorDefinition.cpp` — new (validation rules у `IsDataValid`)
- `GP/Source/GPRuntime/Private/AI/GPAIPrimaryAssetType.cpp` — primary asset type registration

## Acceptance Criteria

- [ ] Compiles clean.
- [ ] `UGP_AIBehaviorDefinition` selectable у Asset Manager picker.
- [ ] All numeric fields documented inline with placeholder rationale.
- [ ] All asset refs use `TSoftObjectPtr` / `TSoftClassPtr` (per ADR-0002).
- [ ] `IsDataValid` checks: positive intervals, valid ranges, soft refs not null where required (warning for missing PreferredDrops).
- [ ] `DA_GP_AI_Behavior_Default` instance created у Content/ with placeholder values.
- [ ] Asset Manager registers `GP_AIBehavior` primary type.

## Playtest / Validation Note

Open Asset Manager у editor → confirm `GP_AIBehavior` type exists. Open `DA_GP_AI_Behavior_Default` → confirm all fields editable, defaults reasonable.

## Risks / Edge Cases

- Forgotten `IsDataValid` warning → designer ships broken DA → AI crashes. Mitigated by per-field defaults + IsDataValid warnings.
- Soft-ref не resolved at AI spawn → `CachedBehavior` null deref. Mitigated by async-load `OnPossess` callback (per GP-S54).

## Linked

- [GP-0306 AI Opponent design](GP-0306_AI_Opponent.md) — field rationale.
- [GP-S54 AI PlayerController](GP-S54_AI_PlayerController.md) — consumer.
- [`../../TDD/10_Data_Assets.md`](../../TDD/10_Data_Assets.md) — Asset Manager loading flow.

## Stop Condition
STOP. Await approval before GP-S56.
