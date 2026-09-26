# Stage 4: Content

Goal: six chapters, three bosses, the flashbacks, and one finished district, playable in
greybox with real text and placeholder audio.

## Build order
Not 1 to 6. Hard problems first, story last:

1. **Chapter 6, Trickshot.** The two-phase boss and the ending. If it doesn't land, the
   whole arc needs rethinking. Also the most demanding traversal chase.
2. **Chapter 4, The Auction House.** Interior stealth plus the first bow duel. Proves
   interiors and archer enemies.
3. **Chapter 1, Rooftops.** The tutorial, built third so it teaches what actually exists.
4. **Chapter 3, Iowa to JFK.** Clint's chapter. Proves the second character on his own.
5. **Chapter 2 and 5, the flashbacks.** Slideshows plus the carnival scene. Cheapest,
   and the emotional core, so they get built when we know what the present needs them
   to set up.

## Per-chapter one-pager (`docs/chapters/CH0X.md`, before building)
Beats, locations on the district map, enemy counts and types, objectives as they appear
in the data asset, lines for Kate and Clint (aim for 15 to 30 per chapter, they talk),
what the player unlocks, estimated time. Two to four weeks per chapter solo.

## The district
One district for Part 1, built to be finished: generated from real data, hero blocks
dressed, side content placed. Which neighbourhood is settled with the research doc and
you. Side content: six Tracksuit crimes (repeatable ambushes), three Lucky errands, three
Grills favours, five rooftop archery challenges, a dozen collectibles tied to the show.

## Bosses
- Tracksuit lieutenant: melee brawl, three phases, the room changes each phase.
- Barney's archer: bow duel across a rooftop gap. Cover, relocation, timing.
- Trickshot: phase one ranged across two rooftops with Barney using trick arrows against
  Clint; phase two Kate freed, close range, both Hawkeyes. Ends in dialogue, not a kill.

Same fairness rules as before: every attack has a tell, phase transitions are clear,
no one-shots, the player can always see or hear where the boss is.

## Flashbacks
Four slideshows plus one playable carnival scene. Write every caption in one sitting
and read them in order. Clint's and Barney's versions of the same events should differ
in details the player can catch.

## Dialogue
One data table. Kate and Clint banter during traversal is the show's whole voice; write
it in batches by situation (idle roam, after a fight, approaching an objective, low
health) and let the system pick. Scratch audio recorded by you.

## Done when
- [ ] New Game to the end of Chapter 6 with no console commands, about 4 hours
- [ ] Both characters playable, switching works where allowed
- [ ] All three bosses beatable and lose-able
- [ ] Every side activity in the district completable
- [ ] Two people who aren't you have finished it and you watched
- [ ] Six one-pagers in `docs/chapters/` matching what got built

## When to cut
If Chapter 4 (the second built) is a month over, cut Chapter 3 to a cutscene and fold
Chapter 5's present-day chase into Chapter 6. Four chapters and two bosses is still the
arc.
