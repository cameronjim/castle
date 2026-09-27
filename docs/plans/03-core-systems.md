# Stage 3: Core systems

Goal: every system the six chapters share, built once, so a chapter is a district, a data
asset, and scripted beats. After this stage, building a chapter is content, not code.

## Systems to build

### Quiver (the inventory, renamed)
- `UWeaponDefinition` becomes `UArrowDefinition` plus one bow definition per character.
  Arrow types are data: damage, projectile behaviour, effect on hit, count cap, icon.
- Hotbar becomes the quiver wheel: hold a key to open a radial with the arrow types you
  carry; standard arrows are always slot 1. Mouse wheel cycles.
- Trick arrows for Part 1: grapple, putty, bola, smoke, EMP, explosive. Each is an actor
  spawned on hit with a small behaviour: putty spawns a hold volume, bola trips, smoke
  spawns a stealth cloud, EMP kills lights and cameras in a radius, explosive is damage.
- Crafting is out of scope. Arrows refill at safehouses and from ammo pickups.

### Combat, finished
- Melee: light, heavy, combo chains of three, parry, dodge with i-frames, a finisher when
  an enemy is staggered. Hit reactions on enemies by direction. Camera lock-on optional.
- Bow: draw time affects damage and spread; headshots; perfect release window for a
  bonus. Focus meter: slow motion while airborne, drains, refills on hits.
- Damage types: arrow, melee, blunt, explosive. Health component per the existing rules.
- Enemies: thug (melee rush, bat or fist), gunner (pistol, takes cover), archer (bow,
  keeps range, relocates), heavy (blocks, needs a stagger). Squad awareness and alert
  levels from the existing AI.

### Partner and switching
- `UPartnerComponent` on the AI-controlled Hawkeye: follow at distance, take cover, shoot
  tagged enemies, go to a marker on command, revive the player once per fight. Never
  blocks a doorway.
- Switching: `AHawkeyePlayerController::SwitchCharacter()` possesses the other Hawkeye,
  hands the old pawn to an AI controller running the partner logic, swaps the HUD. Camera
  blends over 0.3 s. Chapters set whether switching is allowed.
- Each character has their own bow stats, arrow caps, melee move set, and locomotion
  tuning. Kate faster, Clint heavier.

### Traversal, finished
- Everything from stage 2 plus: fire-escape ladders, pipe climbs, wall runs (short),
  rooftop edge vault, controlled falls. Grapple to moving anchors (later).
- Traversal challenges: timed routes with checkpoints, used for side content.

### City streaming and save
- World Partition with a grid sized for the district; hero blocks in their own cells.
  Streaming distances tuned for 16 GB. A district must load from nothing in under 15 s.
- Safehouses: fast travel, arrow refill, chapter select, outfit later.
- Save: current chapter, objectives, quiver contents, safehouses unlocked, side content
  done, both characters' state. Autosave at chapter beats and safehouses. Same rules as
  the old save contract, extended.

### Narrative plumbing
- Missions and objectives as before, plus objective markers in the world and a compass
  or minimap strip. Dialogue lines from a data table with subtitles, now with a "walk
  and talk" mode where the partner talks during traversal.
- Flashbacks: slideshow as before, plus the ability to end in a playable scene (load a
  small level, play a short mission, return).
- Phone: Kate's texts as a UI panel for side content and character beats. Cheap, on-tone.

### Level kit for the city
- Building generator from OSM (stage 2) extended with: window grids, doors, fire escapes,
  rooftop clutter (HVAC units, water towers, parapets), street furniture (lamps, hydrants,
  bins, scaffolding), and snow decals. All script-placed. Hero interiors built with the
  room-art approach from the Punisher build.

## Order to build in
Quiver and arrows first (they touch everything), then combat, then partner and switching,
then streaming and save, then narrative plumbing, then the extended kit.

## Status (2026-09-26, built ahead of schedule during unattended runs)
- Quiver and trick arrows: built (putty, bola, smoke, EMP, explosive, radial wheel).
  Effects are placeholder shapes; Niagara later.
- Combat: light and heavy strikes, dodge, hit stop, low-health tint, gunners with cover
  and bursts, archers with interruptible draws, squad alert. Not yet: combos, parry,
  finishers, the heavy, lock-on.
- Partner and switching: built on StateTree; banter from a data table.
- Traversal: built through the sample's traversal plus our ledge, hang, drop, roll, and
  grapple; fire escapes generated. Not yet: pipe climbs, wall runs, traversal challenges.
- Save: built on SPUD, with a safehouse and a main menu. Not yet: fast travel, chapter
  select (stubs), World Partition (single block, not needed yet).
- Narrative plumbing: objective markers, compass, toasts, banter subtitles. Not yet: end
  cards wired to chapters, the phone, flashbacks ending in playable scenes, dialogue audio.
- Level kit: generated buildings, streets, facades, snow, clutter, lamps, fire escapes.
  Not yet: interiors for chapters.

## Done when
- [ ] Sandbox district has: every arrow type working, four enemy types, a melee fight
      that feels like Arkham-lite, Clint as partner doing useful things, a switch mid-fight
      that doesn't break anything, a safehouse, a flashback that ends in a playable scene
- [ ] Quit and Continue restores both characters and the quiver
- [ ] District streams in and out with no hitches on this machine
- [ ] A new chapter can be blocked out in a day from the kit and a data asset

## Don't do these yet
Web-swinging equivalents (no). Outfits. Skill trees. Multiplayer. A second district.
