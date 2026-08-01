# Slice Template

Canonical structure для code-implementation slice tasks (`GP-S##`) у GrimProtocol. Per [`Claude_Tasks/GP-0602_Implementation_Slices`](Claude_Tasks/GP-0602_Implementation_Slices.md) і [`../TDD/13_Architecture_Proposal`](../TDD/13_Architecture_Proposal.md) Implementation Order.

## Mandatory Header Format

```markdown
# GP-S## <Short Slice Name>

## Slice Group
<e.g., Slice 1 — Foundation>

## Code Allowed
Yes — only after explicit approval of the previous slice (or GP-0601 for the first slice).

## Depends On
- `GP-S##` (previous slice IDs, comma-separated). For S01: "GP-0601 approved".

## Goal
<one-line statement of what this slice produces — single focused change>

## Scope
Bullet list of concrete deliverables. Each bullet — single file or single coherent change. No optional items.

## Out of Scope
Explicit list of nearby work this slice does NOT touch. Prevents scope creep.

## Required Skill Pass
- `ue5-architecture`
- `<system-specific>` (e.g., `ue5-gas`, `ue5-replication`, `ue5-rtsgameplay`, `ue5-multiplayer` per slice)
- `gp-mechanics-validator` (if gameplay-facing)

## Files Touched
- `Path/To/File.h` — what changes
- `Path/To/File.cpp` — what changes
- `Path/To/DataAsset.uasset` — created / edited

## Acceptance Criteria
- [ ] Compiles editor + standalone (clang / msvc per CI).
- [ ] No new compile warnings (engine + project).
- [ ] No hardcoded balance values (per ADR-0002).
- [ ] All content refs `TSoftObjectPtr` / `TSoftClassPtr` (per ADR-0002).
- [ ] Replication conditions explicit on every `UPROPERTY(Replicated)` (per CONTRIBUTING).
- [ ] No `Tick` polling, no widget-side gameplay queries (per TDD/12).
- [ ] CommonUI + MVVM rules respected (per TDD/12) — if slice touches UI.
- [ ] Server-authoritative writes only (per CONTRIBUTING / TDD/03) — if slice touches gameplay state.
- [ ] No new module created (per ADR-0006).
- [ ] No new subsystem (per ADR-0006) — except documented `UGP_SessionSubsystem` and `UGP_MatchAssetLoader`.
- [ ] Soft cap file size ≤ 1000 lines (per CONTRIBUTING).
- [ ] Naming per `STYLE.md` (`GP` prefix, `UGP_*` / `AGP_*` / `FGP_*` / `EGP_*`).
- [ ] No leftover `// TODO` без ticket reference.
- [ ] No leftover `UE_LOG` debug spam (`LogTemp`, etc.).
- [ ] Matching docs update (TDD section або task file Output amended).

### Pillar 8 — MVP Production Rule (gate для будь-якої нової mechanic у slice)

Якщо slice додає або змінює gameplay mechanic (не pure infra / refactor), кожна нова mechanic проходить 5-question gate per [`../GDD/01_Game_Pillars`](../GDD/01_Game_Pillars.md) §Pillar 8:

- [ ] Чи весело це вже зараз? (playable у v1, не "буде весело пізніше")
- [ ] Чи зрозуміло це новому гравцю? (1-2 sentence explanation, no tutorial overlay)
- [ ] Чи додає це новий тип рішення? (не redundant variant of existing)
- [ ] Чи можна це реалізувати дешево і швидко? (fits slice granularity)
- [ ] Чи масштабується це далі контентом? (scales via DataAsset content, not via additional rules)

"Ні" хоча б на декілька → slice rejected, mechanic decomposed або deferred.

### Documentation Sync per Pillar 8

- Будь-яка mechanic-introducing slice **must** документувати pillar fit у task file / TDD section: "Per Pillar X (...), Pillar 8 — gate passed (1-2 sentence: ...)".

## Playtest / Validation Note
Manual check the slice owner performs у editor. Example: "Load `MAP_GP_Test_Camera`, possess CameraPawn, verify WASD pan and mouse wheel zoom respond. No console errors."

## Risks / Edge Cases
Slice-specific risks per Architecture Proposal `Risks` section. Mitigation per risk.

## Linked
- TDD section(s) this slice implements.
- ADR(s) that govern decisions.
- Architecture Proposal slice ID and predecessor slice.

## Stop Condition
After this slice — STOP. Await explicit approval before next slice. Commit + PR description per [`Git_Workflow`](Git_Workflow.md).
```

## Branch Convention

Per [`Git_Workflow`](Git_Workflow.md):

```
feature/gp-s##-<short-desc>
```

Examples:
- `feature/gp-s01-module-scaffolds`
- `feature/gp-s13-camera-pawn`
- `feature/gp-s30-targeting-component`

## Commit Convention

Per [`Git_Workflow`](Git_Workflow.md). One slice = one or more atomic commits, all on one feature branch.

```
GP-S##: <subject ≤ 72 chars>

<body — context, decisions, references to TDD / ADR>

Refs: GP-S##, TDD/##_<...>, GP-0601
```

## PR Template (Slice-Specific Addition)

Augment standard PR template з:

```markdown
## Slice
GP-S## — <Short Name>

## Slice Group
<Slice 1 — Foundation, etc.>

## Previous Slice
GP-S(##-1) — confirmed merged.

## Acceptance Checklist
- [ ] Build green
- [ ] No new warnings
- [ ] CI lint passes (soft-ref grep, hard-ref grep, magic-string-tag grep)
- [ ] TDD updated where applicable
- [ ] Manual playtest note completed
```

## CI Gates (Project-Level, Apply to All Slices)

Automated checks для PR (configured у CI but documented тут for slice author):

1. **Hard-ref grep:** `grep -rEn 'TObjectPtr<U.*_Definition>|TSubclassOf<UGameplay(Ability|Effect)>' GP/Source/` → must yield 0 matches outside `Transient` cached resolvers.
2. **`LoadObject` grep:** `grep -rEn '(LoadObject<|StaticLoadObject|ConstructorHelpers::FObjectFinder)' GP/Source/` → 0 matches for content (only engine system permitted).
3. **Magic-string tag grep:** `grep -rEn 'FGameplayTag::RequestGameplayTag\(\"' GP/Source/` → 0 matches.
4. **Widget→ASC grep:** `grep -rEn '(GetNumericAttribute|FindComponentByClass)' GP/Source/GPUIRuntime/` → 0 matches (widgets must go through VM).
5. **Hardcoded balance grep:** Run `gp-mechanics-validator` skill pass on touched files.
6. **Replication completeness:** Every `UPROPERTY(Replicated)` має parent у `GetLifetimeReplicatedProps` з explicit `DOREPLIFETIME_*` condition.
7. **`AddToViewport` in screens:** `grep -rEn 'AddToViewport\(' GP/Source/GPUIRuntime/` → match only allowed for HUD root.

## Per-Slice Documentation Sync

Every slice updates exactly **one** TDD section authoritatively. If slice spans multiple TDD pages — split slice. Cross-TDD references лишаються links, не duplicated content.

## Slice Granularity Guardrails

| Property | Target |
| --- | --- |
| C++ classes added | ≤ 3 per slice |
| Files touched | ≤ 10 |
| Diff size | ≤ ~1000 LOC |
| Review duration | ≤ 1 hour |
| Time on slice | ≤ 1 day за дефолтом, ≤ 3 days для UI / Steam |

Якщо slice виходить за рамки — split.

## Anti-Patterns у Slice Work

Review-blocking під час slice implementation:

- ❌ Bundling several `GP-S##` slices у один PR.
- ❌ Touching code outside slice scope ("while я тут, fix unrelated thing"). Open separate ticket.
- ❌ Adding `UE_LOG` debug у production paths без `Verbose` level guard.
- ❌ Skipping documentation sync.
- ❌ "Just a small subsystem" without ADR justification.
- ❌ Hardcoded testing strings / paths у gameplay code.
- ❌ Disabling existing tests to make slice green.
- ❌ Re-creating already-existing class instead of extending.

## References

- Architecture Proposal — [`../TDD/13_Architecture_Proposal`](../TDD/13_Architecture_Proposal.md).
- Git workflow — [`Git_Workflow`](Git_Workflow.md).
- Coding rules — [`Coding_Rules`](Coding_Rules.md).
- Style — [`../../STYLE.md`](../../STYLE.md).
- Contributor rules — [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md).
- ADRs — [`../Architecture_Decisions/`](../Architecture_Decisions/).
- Per-task index — [`Claude_Tasks/README.md`](Claude_Tasks/README.md).
- Backlog index — [`Claude_Task_Backlog.md`](Claude_Task_Backlog.md).
