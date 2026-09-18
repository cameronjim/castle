# Stage 4: Content

Goal: all eight missions, four bosses, and four flashbacks playable in grey boxes, start
to finish, with real text and placeholder audio. The game exists after this stage. It's
just ugly.

## Mission build order
Not 1 to 8. Build in this order so the hard problems surface early and the story can
change while it's cheap to change:

1. **M7 The Warden** (duel boss, the ending, the choice). If the ending doesn't land,
   everything before it needs rethinking. Find that out now.
2. **M2 The Infirmary** (brawl boss). Proves the brawl framework on a real boss.
3. **M5 Solitary** (duel boss in darkness, the Doctor). Proves lights-out gameplay and
   the non-combat choice moment.
4. **M3 Yard Riot** (arena, crowd). Probably the most expensive level for performance.
   Test 30+ AI on this machine early.
5. **M4 Records** (quiet, no boss). Cheap. A breather for you as much as the player.
6. **M6 Armory** (power fantasy). Add the shotgun and rifle as weapon data.
7. **M8 Exit** (escape sequence, skull). Short. Scripted.
8. **M1 revision.** You'll have learned a lot. Rebuild the tutorial last so it teaches
   the game you actually made.

## Per-mission checklist
Each mission gets a one-page doc in `docs/missions/M0X.md` before you build it:
- Beat list (what happens, in order, one line each)
- Rooms and what's in each
- Guard count and types per room
- Objectives as they appear in `DA_M0X`
- Frank's lines (aim for 3 to 6 per mission)
- What the player learns or gets (new weapon, new fact, new mechanic)
- Estimated play time

Then build: block out, populate, script the beats, add objectives, play it to the end
three times, commit. Two to three weeks per mission is realistic solo. Bosses add a week.

## Bosses
Each boss is: a child of the test boss Blueprint, a behavior tree with one branch per
phase, an arena controller, and a health bar. Design per boss is in DESIGN.md. Build order
matches the mission order above: Warden, Orderly, Sniper, Deacon.

Rules that keep bosses fair:
- Every attack has a tell at least 0.4 seconds before it lands.
- Phase transitions are 2 seconds of invulnerability with an obvious visual.
- Duel bosses never one-shot the player. Two hits to kill from full health, minimum.
- The player can always see where the boss is or hear where he's going. In the Sniper
  fight, the rifle's muzzle flash and the sound are that information.

## Flashbacks
Four data assets, 6 to 10 slides each. For now: stock photos, or generated stills, or
your own photos, whatever gets the pacing testable. Real images come in stage 5.

Write the captions first. All four flashbacks, every caption, in one sitting, in one
document. Read them in order. The lie in 1 and 2 has to be convincing on its own and
obvious in hindsight. Flashback 3's glitch slides need a clear before and after. Cut
any caption that explains what the image already shows.

Voice for Frank: record yourself as scratch audio. Bad temp audio beats no audio for
pacing. Real voice, if any, is stage 5.

## Dialogue
One data table for the whole game. Books' vent lines, the Doctor's speech, the Warden's
monologue during the duel. Write every line before recording scratch audio. Read the
Warden's lines out loud while playing the fight. If you can't finish a line before the
next attack, it's too long.

## The choice moments
Two: the Doctor (kill or spare) and the Warden (kill or spare). Both flags save. They
change one thing each: the Doctor's fate is mentioned in a line during M8, and the
Warden's changes the final image of the skull (painted in blood or in paint). Small,
cheap, and it makes the choice feel seen. Don't build branching levels.

## Done when
- [ ] New Game to credits with no console commands, about 2 hours
- [ ] All 4 bosses beatable and all 4 lose-able (you can die to each)
- [ ] All 4 flashbacks play at the right moment with real captions
- [ ] Both choices save and show up later
- [ ] Two people who aren't you have finished it and you watched both
- [ ] A `docs/missions/` folder with 8 one-pagers matching what got built

## When to cut
If at the end of M5 (the third mission built) you're 2 months over the estimate, cut
M4 and M6. The story survives without them: M3 leads into M5, M5 into M7. That's a
5-mission game with 3 bosses, and it's still the whole arc.
