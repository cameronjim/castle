# Stage 2: Traversal prototype on one real block

Goal: a third-person Kate running, climbing, and zipping across one greybox block of
Manhattan built from real map data, with a bow and four thugs to fight. This answers the
only question that matters: is moving through this city fun?

## Order of work

1. **Pivot cleanup (2 days).** Remove the first-person arms and viewmodel code, the pistol
   weapon definition and pickups, and the prison map's gameplay actors. Keep the systems.
   Rename `AGuardCharacter` to `AThugCharacter` (or keep the class and rename the
   Blueprint; cheaper). Tests updated, suite green, docs updated. Commit before anything new.

2. **Third-person camera (3 days).** Spring arm behind and above, over-the-shoulder offset
   on aim, camera collision with walls, lag on movement. Tune on the sandbox. Sensitivity
   from the settings subsystem as before.

3. **Locomotion (1-2 weeks).** Bring in Epic's free Game Animation Sample (motion-matched
   locomotion for this mannequin skeleton, hundreds of clips). Walk, run, sprint, turn,
   stop, crouch, slide. This replaces the animation-asset switching from the old build. The
   sample is a full project; extract the animation database, the pose search schema, and
   the character setup into ours by script where possible, by a one-time editor import
   where not. If it can't be done headless, this is the one place a human editor session
   is worth it.

4. **The block (1 week).** `Tools/Editor/generate_city.py`: take a bounding box in
   lat/lon, query OpenStreetMap Overpass for building footprints with height or level
   tags, project to a local tangent plane in centimetres, extrude each footprint into a
   static mesh (procedural mesh or Geometry Script), and place it. Sidewalks, kerbs, and
   street surfaces from the road polygons. Details in `docs/research/nyc-hawkeye.md`. Pick
   the block from the show's neighbourhood. Greybox only: grey buildings, dark streets.
   Add snow later.

5. **Parkour (2 weeks).** Vault (waist height), mantle (chest to head height), ledge grab
   and climb (above head), ledge shimmy, drop-down, roll on landing. Detect with a few
   capsule and line traces ahead of the character, choose the move, play the animation
   with root motion or a procedural curve. Contextual: hold sprint and the character does
   the right thing. Mixamo has vault, climb, and roll animations; retarget by script.

6. **Grapple arrow (1 week).** Anchor actors on rooftops and walls. Aim within range,
   marker shows, fire, arrow flies, zip along the line. Chain between anchors. This is the
   long-range traversal verb. Place enough anchors on the block that rooftop-to-rooftop
   runs flow.

7. **Bow and thugs (1 week).** Draw and release with a projectile arrow (arc, travel time),
   over-the-shoulder aim, a hit reaction on thugs. Four thugs on the block with the
   existing AI (sight, hearing, patrol, attack, now with melee rushes). Kate's melee: light
   and heavy with the bow, a dodge. Enough to fight, not enough to be a combat system.

8. **Play it twenty times.** Run laps. Time a circuit. Note every snag on geometry, every
   move that felt slow, every place you wanted to go and couldn't.

## Done when
- [ ] A full lap of the block, street to rooftop and back, in under 90 seconds, without
      touching a wall you didn't mean to
- [ ] Grapple chains across at least three rooftops without stopping
- [ ] Vault, mantle, ledge, and roll all trigger without thinking about them
- [ ] A fight with four thugs is winnable with bow and melee and isn't annoying
- [ ] One other person has run the block and said it felt good without prompting
- [ ] Still greybox. No art. If there's a texture on a building, take it off.

## Cut list if it's dragging
In this order: slide, ledge shimmy, chaining grapples, thug melee (leave them shooting).
The core is run, vault, mantle, grapple, shoot.

## What you learn here decides stage 3
Half a page after playtesting. Traversal speed, camera distance, how much auto-parkour
felt right versus took control away, whether the block size was fun or tedious.
