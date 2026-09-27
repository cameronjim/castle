# Third unattended run, 2026-09-27

For Cameron. Everything below is on main and passed the QA gate. The story is untouched:
every bit of narrative text in the game is a bracketed placeholder waiting for you.

## What landed
1. **Combat tuning.** Archers hold a draw when you duck and fire when you pop up, with a
   loose window after each shot. A heavy with a riot shield that blocks arrows and light
   strikes; heavy strikes, bola, or explosive stagger him. Alert glyphs over thugs, thug
   health bars, a clearer aim camera. Scripted fights: every hit telegraphed, zero
   untelegraphed damage.
2. **Controller fix.** Stick up looks up. Invert Y toggles for mouse and pad in Settings.
3. **Sound.** 39 synthesized MetaSounds, nothing downloaded: bow, arrows by surface,
   grapple, footsteps on snow, landings, melee, thugs, trick arrows, ambience, lamp buzz,
   UI. Master, effects, and ambience sliders.
4. **Effects.** Real particles from the engine's Niagara templates: smoke, EMP, explosion
   with a scorch, putty, trails, hit sparks, snow puffs, the zip line, gunshot tracers,
   falling snow, chimney wisps.
5. **Narrative plumbing.** A phone with message threads, a chapter title card, an examine
   object that ends the chapter with a close-up and end card, a playable-scene hook after
   flashbacks, and a dialogue system with queued sequences. All placeholder text.
6. **Stability.** First-frame hitch gone, reloads from 13 s to about 2 s, screenshot
   captures verified, five loose ends fixed, every scripted test passed three times over.
   Load time from the editor build is about 21 s and mostly uncooked animation data; a
   packaged build is being made now to fix that.

## Numbers
| Metric | Value |
|---|---|
| Commits on main | 529 |
| Automation tests | 268, all passing |
| MetaSounds / Niagara systems | 39 / 18 |
| Scripted fights | roof 6.5 s 0 hits; duel 12.8 s 1 hit; street 24.2 s 0 hits |
| Lap frames over 100 ms | 0 |

## New controls
| Action | Keyboard | Pad |
|---|---|---|
| Phone | P | Hold D-pad down |
| Examine the chapter-end object | E | Y |

## How to play
Play Hawkeye on the desktop (or the packaged shortcut once it exists). Follow the
marker to the three roofs; the third ends at a purple-fletched arrow in a water tower;
examine it and the chapter ends into a placeholder flashback and a placeholder scene room,
then back to the street. Fight the roof pair, duel the archers (duck the purple glint,
stand and shoot when it fades), meet the heavy on the park's south corner.

## For tomorrow's story session
The placeholders you'll fill: chapter title and subtitle, end card line, four phone texts,
one opening dialogue sequence of three lines, three flashback slide captions, and whatever
Chapter 1's beats become. The data tables and assets are named in
`claude-docs/gameplay-semantics.md` under "Narrative plumbing".

## Known rough edges
- Placeholder scene room ceiling reads hot. Explosion still a little washed out.
- Thug hit spark competes with the hit flash. Footstep snow kick faint.
- Sample foley may double footsteps (unconfirmed).
- No attack animations yet (strikes are lunges). Fingers don't curl on the bow.

## Added after the brief: a packaged build
`Play Hawkeye (Packaged)` on the desktop runs the game without the engine. It loads in
about 5.5 s the first time and 3 s after, against 21 s from the editor build. Made by
`Tools\package.ps1` in a few minutes; the zip is under `Saved\Packaged`.
