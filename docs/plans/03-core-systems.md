# Stage 3: Core systems

Goal: every system that missions 2 through 8 share, built once, so that a mission is
mostly data plus a level. After this stage, building a mission means placing actors and
filling in data assets, not writing new logic.

## Systems to build

### Save and checkpoints
- A `SaveGame` subclass storing: current mission, completed objectives, ammo, health,
  which flashbacks have played, the Doctor and Warden choices.
- Checkpoint actors in levels (invisible volume, saves on overlap). Reload from
  checkpoint on death. Death without a checkpoint restarts the mission.
- Main menu: Continue, New Game, Quit. Nothing else yet.

### Combat, finished version
- Weapon component gets: hip fire vs aim (right mouse tightens spread and zooms slightly),
  spread that widens while moving, a damage falloff curve, hit markers, and a headshot
  multiplier. Still one pistol. Missions 6 to 8 add a shotgun and a rifle by making new
  data, not new code. So make weapon stats a data asset now.
- Melee: a light punch (LMB with no weapon) and the takedown. Punches stagger guards.
  Brawl bosses are built entirely on punch, dodge, block, and stagger, so get punch
  feel right here. Hitstop of 2-3 frames, camera shake, a sound. Test on a guard.
- Dodge: a short directional dash on Shift-tap while not sprinting. Needed for brawls.
- Damage types: bullet, melee, explosion. Health component reacts per type (melee
  staggers, bullets don't).

### Boss framework
- `BossPhaseComponent` is already scaffolded. Build one test boss in the sandbox using
  it: a beefy guard with 3 phases who swaps behavior tree branches on phase change and is
  invulnerable for 2 seconds during the swap. Boss health bar widget with segments per
  phase. If this works for the test boss it works for all four.
- Arena controller actor: listens to phase changes and does level things (kill lights,
  open a spawner door, trigger a sound). Each boss in stage 4 gets one.

### Guard AI, finished version
- Cover: EQS query for cover points, guards move between them under fire.
- Squad awareness: one guard spotting you alerts the room after a 1.5 second delay
  (gives the player a takedown window).
- Alert levels: calm, suspicious, alerted, with a HUD indicator. Stealth is a game about
  reading these states, so make them legible.
- Guard variants as child Blueprints: baton (melee only, early game), pistol, rifle
  (late game), and the doped "broken" inmate type for Mission 5 who charges unarmed.

### Mission and narrative plumbing
- Objective HUD that updates from the subsystem, with the "no HUD early game" rule from
  the design doc: `MissionDefinition` gets a `bShowObjectiveText` flag. Missions 1-3 off,
  4-8 on.
- Mission end card widget: black screen, mission name, a line of Frank's, hold 4 seconds,
  then flashback or next level.
- Subtitle widget for Frank's lines and for Books through the vent. A `DialogueLine` data
  table (speaker, text, audio, duration) and a Blueprint function to play one.
- Flashback widget, finished: the corrupt-and-replace effect for Flashback 3. Two extra
  fields on `FFlashbackSlide`: an optional `ReplacedByImage` and a `GlitchSeconds`. When
  set, the slide shows, glitches (a material with noise and a scanline), then swaps.

### Level building kit
- A set of grey-box modular pieces at fixed sizes: wall 400x400, door frame, corridor
  segment, cell, stair, vent cover. Blueprints for door (locked, keycard, powered),
  vent (crawl through), light (can be shot out), generator (kills lights in a zone),
  switch. Every mission is assembled from these. Art replaces the meshes in stage 5
  without touching the logic.

## Order to build in
Save system first (you'll want it for testing everything else), then melee and dodge,
then the test boss, then guard AI, then narrative plumbing, then the kit.

## Done when
- [ ] Sandbox level has: a checkpoint, 4 guard types, a cover fight, a test boss with 3
      phases, a door of each type, a generator that kills lights, a dialogue line that
      plays with subtitles, and a flashback with the glitch effect
- [ ] Die, reload at checkpoint, state is correct
- [ ] Quit to menu, Continue, you're back where you left
- [ ] Mission 1 from stage 2 still plays, now with the finished systems swapped in
- [ ] You can build a new 5-room test level with 6 guards in under 2 hours using the kit

## Don't do these yet
Multiplayer anything. Inventory UI. Skill trees. Difficulty settings beyond one number
for guard damage. Any of these triples the work for a game about one man and a pistol.
