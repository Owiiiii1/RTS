# GP-0602 Implementation Slices

## Goal

Після approval розбити implementation на маленькі reviewable slices.

## Inputs

- Approved `GP-0601 Architecture Proposal`.
- Relevant GDD/TDD pages for each slice.

## Code Allowed

Only after explicit human approval.

## Scope

Implementation planning. Code changes happen one slice at a time.

## Required Skill Pass

- `ue5-architecture`
- relevant UE5 implementation skill per slice.

## Slice Order

1. Base classes and module sanity.
2. GAS attributes/tags.
3. CameraPawn.
4. Selection.
5. Commands.
6. Unit/Building.
7. Resource loop.
8. Damage and match end.
9. Steam MVP.

## Deliverables Per Slice

- One focused code change.
- Matching documentation update.
- Compile/build check.
- Playtest note or manual validation note.
- No unrelated refactor.

## Validation

- Slice can be reviewed independently.
- No generated/cache files committed.
- No gameplay code outside approved scope.

## Stop Condition

After each slice, stop for review before next slice.

## Output

- Canonical slice format: [`../Slice_Template.md`](../Slice_Template.md) — header structure, branch convention, commit convention, CI gates, granularity guardrails, anti-patterns.
- Slice 1 (Foundation) materialized as concrete task files:
  - [`GP-S01_Module_Scaffolds`](GP-S01_Module_Scaffolds.md)
  - [`GP-S02_Native_Gameplay_Tags`](GP-S02_Native_Gameplay_Tags.md)
  - [`GP-S03_Attribute_Sets`](GP-S03_Attribute_Sets.md)
  - [`GP-S04_AbilitySystemComponent_Subclass`](GP-S04_AbilitySystemComponent_Subclass.md)
  - [`GP-S05_Damage_Calculation_MMC`](GP-S05_Damage_Calculation_MMC.md)
- Backlog index amended з Phase 6A.
- Slices 2-12 (S06-S58) — listed у [`../../TDD/13_Architecture_Proposal.md`](../../TDD/13_Architecture_Proposal.md) §Implementation Order. Each materializes into concrete task file upon predecessor slice merge.
- CI gates documented у `Slice_Template.md` §CI Gates (hard-ref grep, LoadObject grep, magic-string-tag grep, widget→ASC grep, balance grep, replication completeness, AddToViewport check).
- Slice granularity guardrails: ≤ 3 classes, ≤ 10 files, ≤ 1000 LOC, ≤ 1 hour review, ≤ 1 day work.
