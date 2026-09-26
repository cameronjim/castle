# Working title: QUIVER (Hawkeye: Kate Bishop and Clint Barton)

Status: pivot, 2026-09-25. Engine: Unreal Engine 5.8, C++ base with generated content.
Internal code name stays `Castle` (project and module name; renaming a UE C++ project is
pain for no gain). Everything player-facing says Hawkeye.

The Punisher design this replaced is in `docs/archive/punisher/`. Most of its systems
carry over; see "What we keep" at the end.

## One-line pitch
A spin-off of the Disney+ Hawkeye series. Weeks after Christmas, Kate Bishop is trying to
be Hawkeye in a city that doesn't know it needs one, Clint Barton is trying to stay
retired, and Clint's brother Barney comes back to New York as Trickshot to settle a debt
neither of them can name. Third person. A slice of Manhattan you can run, climb, and zip
across. Bows, trick arrows, and a dog.

## Pillars
1. **Two Hawkeyes, one bow style each.** Kate is fast, improvises, talks too much. Clint is
   economical, hurts, doesn't miss. You play Kate by default. Clint is beside you as AI
   support, and in some chapters you switch to him. Same controls, different feel.
2. **Traversal is the game.** Insomniac's rule: if moving through the city isn't fun on
   its own, nothing else matters. No web swinging, so it has to be grounded parkour that
   flows: vaults, mantles, ledges, fire escapes, rooftop runs, and the grapple arrow for
   the gaps. Fast, forgiving, never stuck on geometry.
3. **Trick arrows are the toolkit.** Every problem in the city has an arrow. Combat,
   traversal, stealth, and puzzles all draw from the same quiver. Ammo is scarce enough to
   make the choice matter.
4. **The city is real.** Building footprints and heights come from real map data. The
   neighbourhoods the show uses are the neighbourhoods we build. A New Yorker should
   recognise the corner.
5. **Show tone.** Warm, funny, grounded. Christmas lights still up in January. Grief
   underneath the jokes. The Fraction and Aja comic run is the visual and structural
   reference, exactly as it was for the show.

## Setting
Manhattan in the second week of January, right after the show. Snow on the ground,
Christmas decorations coming down, the Rockefeller tree already gone. The playable slice
is built around the show's locations (see `docs/research/nyc-hawkeye.md` for what is
sourced and what isn't). Start small: one district that contains Kate's neighbourhood and
a route to Midtown. Expand only when that district is finished.

## Cast
- **Kate Bishop.** Lead. Twenty-two, rich family in freefall after her mother's arrest,
  living in a friend's spare room, working out what "Hawkeye" means without Clint around.
  Playable.
- **Clint Barton.** Retired, in Iowa, until he isn't. Returns to New York because of
  Barney. Playable in flashbacks and in specific chapters; AI support the rest of the time.
- **Barney Barton, Trickshot.** Clint's older brother. Same carnival childhood, same
  training under Buck Chisholm, the original Trickshot. Where Clint got out, Barney stayed
  in the life. He is the better archer on paper and knows it. Villain of Part 1.
- **Lucky the Pizza Dog.** Companion. Finds things, distracts thugs, is a good dog.
- **Grills.** Kate's LARPer friend, fire-fighter, source of intel and comic relief.
- **The Tracksuit Mafia.** Leaderless after the show. Someone new is paying them. Bro.
- **Yelena Belova.** Optional cameo, Part 2 material.

Do not contradict the show's ending (Kingpin, Eleanor, Maya, Jack). The research doc
tracks their status.

## Story shape, Part 1 (six chapters, we refine together)
| # | Chapter | Playable | Beat |
|---|---------|----------|------|
| 1 | **Rooftops** | Kate | Tutorial. Kate on a self-assigned patrol. Traversal, first Tracksuit fight, the grapple arrow. Ends with an arrow that isn't hers pinned in a wall, fletched purple. |
| 2 | **Carnival** (flashback) | Clint, young | Slideshow plus one playable scene: two boys, a bow, a carnival at night. Barney teaches Clint to shoot. Buck Chisholm watches. |
| 3 | **Iowa to JFK** | Kate, then Clint | Kate calls Clint. He says no. Then he sees the arrow. Clint chapter: airport to city, his first traversal, his style. |
| 4 | **The Auction House** | Kate + Clint (support) | Barney is fencing Tracksuit loot through the same channels Eleanor used. Stealth mission in a building. Bow duel at the end with one of Barney's archers. |
| 5 | **The Bridge** (flashback + present) | Clint | Barney's version of the past. What Clint did when he left the carnival. Then a present-day rooftop chase across the district. |
| 6 | **Trickshot** | Kate, switching to Clint | Barney has Kate. Clint goes alone, until he doesn't. Two-phase boss: Clint vs Barney at range, then Kate freed and the fight moves close. Ends on the brothers talking, not shooting. |

Flashbacks reuse the slideshow system from the previous design, with one difference: the
carnival ones can end in a short playable scene.

## Systems
### Traversal
- Run, sprint, crouch, slide. Vault low, mantle mid, ledge-grab high. Ledge climb and
  shimmy. Fire escapes as ladders. Rooftop edge slides.
- **Grapple arrow**: aim at a marked anchor within range, fire, zip. Chains between
  rooftops. The core of moving across the district fast.
- **Contextual auto-parkour** when sprinting into obstacles: the player holds one button,
  the character picks the move. Insomniac's approach. Never an animation lock longer than
  the move needs.
- Fall damage is soft; there's always a roll.

### Bow
- Hold to draw, release to fire. Draw time sets power and accuracy. Arrows are projectiles
  with an arc and travel time; no hitscan.
- Over-the-shoulder aim on right click with slow-motion "focus" while airborne (Kate) or
  when it would be a good moment (Clint), a limited meter.
- Quiver: standard arrows are plentiful; trick arrows are a slot each with small counts.
  The hotbar and inventory from the previous design become the quiver.
- Trick arrows, Part 1: **grapple** (traversal), **putty** (glue, holds an enemy),
  **bola** (trips), **smoke** (stealth), **EMP** (lights and cameras), **explosive**
  (rare). Later: pym, USB, sonic.
- Bow melee: the bow is a staff at close range. Light and heavy, parry, dodge.

### Combat
- Small groups, four to eight thugs, in streets, rooftops, interiors. Melee and arrows
  mix: knock down, pin with putty, finish with a takedown.
- Enemies telegraph. Dodge windows are generous. Health regenerates out of combat.
- Archer enemies (Barney's people) create bow duels: range, cover, timing. The Sniper
  fight from the old design becomes a rooftop archer fight.
- Stealth still exists: rooftops let you pick off patrols; smoke arrows make escapes.

### The partner
- When Kate leads, Clint is an AI ally: he takes a position, shoots what you tag, and
  can be sent to a marker. Not a babysitter; he's competent and occasionally does the
  job before you do. Reverse when Clint leads.
- **Switching**: in chapters that allow it, one key swaps control. The other becomes AI.
  Technically this is repossessing a pawn; it's cheap, and we'll try it in stage 3.

### The city
- Open district with free roam between chapters. Side content: Tracksuit crimes, lost
  Lucky, LARP favours for Grills, archery challenges on rooftops.
- Generated from real data: OpenStreetMap footprints and heights extruded into greybox by
  script, then hero blocks hand-dressed. World Partition for streaming. Snow, winter light,
  leftover Christmas.

## Bosses
- **Tracksuit lieutenant** (Ch. 1 or 4): brawl in a confined space. Teaches melee.
- **Barney's archer** (Ch. 4): first bow duel.
- **Trickshot** (Ch. 6): two phases across two rooftops, then close. Uses every trick
  arrow against you before you've unlocked it. The boss framework from the old design
  drives it unchanged.

## MVP (the first thing we build)
One greybox district block from real map data, third-person Kate with locomotion from
Epic's Game Animation Sample, vault, mantle, ledge grab, grapple arrow zip between two
rooftops, one bow with standard arrows, four thugs. If running across that block and
picking a fight is fun, the game exists.

## What we keep from the Punisher build
Missions and objectives, flashback slideshows, end cards, health and boss phases, the
inventory and hotbar (now the quiver), settings, pause, HUD, doors and pickups, guard AI
with sight and hearing (now thugs), materials and lighting scripts, the whole headless
pipeline, the tests, and the docs structure. Removed: the first-person viewmodel and arms,
the hitscan pistol, the prison level.

## IP note
Marvel and Disney own all of this. A free fan project is fine; nothing commercial.

## Open questions (for us to settle together)
- Kate's home base in-game: her real apartment from the show, or a new place?
- How much of Barney's backstory do we take from the comics versus invent?
- Does Part 1 end with Barney alive? (I'd say yes. He's a Part 2 problem too.)
- Single district or two for Part 1?
- Lucky: cosmetic companion, or a mechanic (finds items, distracts)?
