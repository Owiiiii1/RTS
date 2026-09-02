# SWARM Architecture

> **Status (2026-09-02):** approved technical direction / documentation-only. Runtime SWARM
> implementation **not started**. Gameplay WHAT: [`../GDD/14_SWARM.md`](../GDD/14_SWARM.md).
>
> Do not treat sketched owner names as existing classes. Exact schema, director class, renderer, and
> navigation backend remain **prototype / TBD**.

## Current Runtime vs This Direction

**Already exists (do not re-invent):**

- Per-team `FerroniteThreatValue` on `AGP_GameState` (`TeamFerroniteThreatValues`,
  `GetFerroniteThreatValueForTeam`). Drop-off raises it; launch lowers it.
- `FerroniteScore` and `OrbitalFerronite` do **not** drive SWARM.
- MainBase annihilation already finishes the match.

**Not implemented:**

- Per-team SWARM director / continuous spawn stream.
- Spawn spline selection.
- Lightweight group simulation (Small / Medium) and Large SWARM actors.
- Crush, corpse obstacles, blood mask.

If older docs or Data Asset sketches still name `ThreatToWaveSize`, `ThreatToWaveFrequency`,
`WaveInterval`, `WaveSize`, `WaveStartDelay`, `WaveSpawnPoints` — treat them as **legacy /
current-runtime placeholders**. Exact replacement schema is a future implementation task. Do not
rename code in this documentation checkpoint.

A leftover scalar `FerroniteThreatValue` on GameState is a **legacy compatibility surface**. Prefer
per-team getters for director integration.

## Authority

- Server simulates **gameplay groups** (and Large Actors), not each visual member.
- Clients reconstruct member offsets, animation phases, and cosmetic variation from replicated
  group state + deterministic seed.
- Do **not** replicate each visual member transform.
- SWARM has no player commands, no economy, no capture of deposits.

## Director

One pressure stream **per team**, aimed at that team's MainBase.

Threat bands (concept, numbers TBD) drive:

- target active swarm budget;
- count of simultaneous spawn segments on the outer closed spline;
- replenishment cadence;
- allowed roster;
- Large spawn chance and cap.

Intensity changes **gradually** when threat rises or falls. Existing creatures are **not** despawned
on threat drop.

Spawn candidates: random admissible segments of a closed **outer spawn spline**, outside bases and
normal play space. Validate reachability to MainBase, no trapped geometry, outside start zones,
acceptable reveal, and direction separation unless a concentrated pulse is chosen.

Do not implement a numbered wave timer as the production model.

## Lightweight Group Simulation (Baseline)

First SWARM implementation **is not required to use Mass**.

Baseline: **lightweight group simulation**.

- Small / Medium: one gameplay Actor (or equivalent authority object) owns HP, path, target,
  collision/footprint, attack, and replication.
- Visual members are presentation-only inside that object.
- Large: full independent gameplay unit.

Player `AGP_MobileUnit` / `UGP_MovementComponent` remains the path for **player** mobile units.
SWARM groups **are not required** to use `UGP_MovementComponent`. Shared helpers are allowed later;
do not force SWARM onto the current player movement component.

SWARM does **not** count toward `CurrentUnits` / `MaxUnits`.

## Group Movement

- Gameplay group **center** is the only owner of the global path and decisions.
- Visual members receive local offsets inside the footprint.
- Members may change local position, speed, and facing; avoid visual overlap; follow ground height.
- Local goals change on an interval and are reached smoothly. **No** random per-frame jitter.
- Visual members have **no** independent global AI / pathfinding.
- The group always skirts an obstacle on **one** side and does not split into independent halves.
- In chokes the footprint **compresses**, then expands after the passage.
- Medium visual members may use light local steering + ground projection.
- Small members may use a cheaper local approximation.

### Small-group vehicle avoidance

Local-only: predicted vehicle corridor from position / facing / speed; sticky side choice; minimal
reaction to stationary vehicles. Does not retarget MainBase or rebuild the global path. See GDD/14.

## Group-to-Group Movement

**Small groups:**

- no hard collision with each other;
- may overlap;
- soft separation steering between centers;
- try not to occupy the exact same point;
- MainBase seek force is **stronger** than separation;
- in a choke they may stack and pass together.

**Medium / Large:**

- physically block each other;
- pass chokes **sequentially**;
- do **not** implement this as chaotic physics pushing.

Reservation / queue **direction** (algorithm is implementation / prototype):

1. first occupant takes the passage;
2. others wait at a standoff distance;
3. next enters when the passage is free;
4. timeout / repath / search for a bypass on a long queue.

## Crush And Corpses (Technical)

Follow GDD/14 crush split (living vs corpses). Living Medium / Large are weapons-only; Small crush
requires motion above a minimum speed.

Corpse obstacles: Medium / Large are temporary blockers; collision disables when the mesh no longer
visually occludes the path; sink-then-remove, no instant despawn.

**Navigation:** do **not** mandate a full NavMesh rebuild. Prefer:

- transient obstacle / traversability data, or
- local probe + repath.

Align with [`16_Voxel_Terrain_And_Foundations.md`](16_Voxel_Terrain_And_Foundations.md): naive full
NavMesh rebuilds after every deformation are already rejected there. SWARM corpses use the same
spirit. Exact backend after prototype / profile.

## Animation

- Different visual members use different **start phases** of the same animation.
- Small play-rate variation and a few variants of one state are allowed.
- Shared states: Move / Attack / Death (others only if needed).
- Medium nearby: several `USkeletalMeshComponent`s on one gameplay Actor are allowed.
- Do not put a heavy unique AnimBP on every visual member without proven need.
- Animation Sharing is an allowed optimization (shared poses / state buckets).
- On a death threshold, that member switches to death / corpse presentation.

## Rendering LOD

The camera rotates freely and may zoom roughly **2×** closer than a typical RTS view. Sprites are
**not** the primary near / mid representation.

Recommended direction:

| Class | Near / mid | Far |
| --- | --- | --- |
| Large | full skeletal Actor; conventional LOD at distance | conventional LOD |
| Medium | skeletal near; VAT / instanced mid/far if visually acceptable | instanced / simplified |
| Small | Niagara Mesh Renderer + VAT / AnimToTexture | animated sprites only as **far LOD**; very far / offscreen — aggressive simplification or no individual representation |

Constraints:

- Niagara / VAT are **visual presentation only**, not gameplay authority.
- Sprites must tolerate free camera rotation; directional sprite sets may be too expensive in
  content production and remain a far-LOD option.
- Masked sprites are preferred over many overlapping translucent sprites (overdraw).
- Concrete renderer is chosen after a visual / performance prototype.

Offscreen visual members are **not** rendered. Use distance / visibility / importance LOD for
representation and update frequency.

## Mass Entity Decision

Mass was **considered** and is **not** the baseline.

**Pros:** data-oriented fragments / archetypes / processors; batch processing; simulation /
representation / replication LOD; Actor / ISM / no-representation switching; fits thousands of
independent entities.

**Cons for Grim Protocol:** a second architectural model beside existing `AGP_Unit` / component /
GAS runtime; costly integration with damage, targeting, buildings, commands, navigation, and
listen-server replication; extra C++ + profiling for Mass representation / replication; overkill if
~500 visuals collapse to tens of gameplay groups.

**Decision:**

- first SWARM implementation **need not** use Mass;
- lightweight group simulation is the **baseline direction**;
- Mass remains a **reserve** after prototype / profile;
- do not adopt Mass only for visual members inside a group — Niagara / VAT is simpler;
- do not lock the backend before measurements.

This is consistent with Pillar 9 / ADR-0006: Mass is not the default gameplay stack.

## Performance And Networking Targets

These are **prototype targets**, not measured guarantees.

- Up to **~500 visual SWARM creatures per player**, plus that player's army.
- Each player has their own SWARM stream and MainBase target.
- Expected peak visible frame: roughly **600–700** simultaneous units / creatures.
- More may exist on the map, distributed around different bases.
- Server simulates groups, not each visual.
- Orientation: 500 visuals → **tens of groups** plus a small number of Large Actors.
- Client: member offsets / phases / cosmetic variation from replicated group state + seed.
- Mandatory **performance spike** before a final Mass / VAT / Niagara backend choice.

## Blood Presentation

Cosmetic-only. One death → one stamp with the corpse; no spread-over-time; no persistent splash
decals; match-long accumulation with intensity cap / partial restamp. Do not ship thousands of
permanent Decal Actors. Prefer accumulated world-space blood mask, tiled RT / splat map, or
RVT-compatible approach. Free camera + ~2× zoom must remain readable. See GDD/14.

## Data Assets (Future)

Exact types are **not** frozen here. Expected future surfaces (names TBD):

- threat-band / director parameters keyed on per-team `FerroniteThreatValue` (not wave-size curves);
- spawn spline / admissibility config;
- Small / Medium / Large presentation + aggregate combat tunables;
- crush / corpse / blood presentation tunables.

`UGP_SwarmThreatCurves` with `ThreatToWaveSize` / `ThreatToWaveFrequency` is **superseded as the
approved production schema**. Keep any existing fields only as placeholders until implementation
reconciliation.

## Validation Notes

- No Tick/polling requirement is implied for HUD; threat already pushes through GameState delegates.
- Do not add client-authored SWARM spawn or damage.
- Do not select SWARM.
- Builds / tests are **not** part of this documentation checkpoint.

## References

- Concept — [`../GDD/14_SWARM.md`](../GDD/14_SWARM.md).
- Player units — [`05_Unit_Architecture.md`](05_Unit_Architecture.md).
- Threat accounting — [`07_Resource_Architecture.md`](07_Resource_Architecture.md).
- Data Assets inventory — [`10_Data_Assets.md`](10_Data_Assets.md).
- Terrain / nav — [`16_Voxel_Terrain_And_Foundations.md`](16_Voxel_Terrain_And_Foundations.md).
- Architecture map — [`13_Architecture_Proposal.md`](13_Architecture_Proposal.md).
