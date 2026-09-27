# Second unattended run, 2026-09-26

For Cameron. Everything below is on main and passed the QA gate: build, tests, content
scripts a no-op, verify scripts, and renders from the standalone game with a clean log.
Repo is `github.com/cameronjim/hawkeye`; the project was renamed from Castle to Hawkeye
and the prison was deleted in the same day.

## What landed, in order
1. **Objective markers and compass.** A diamond with distance on the current objective,
   an edge arrow when it's off-screen, a compass strip, toasts on completion, and purple
   beacons on the three objective roofs.
2. **The block's look.** Brick facades with window grids and storefronts, snow on every
   upward face, a moonlit sky with stars, purple-shadow grading, water towers, HVAC,
   chimneys, hydrants, bins, scaffolding, parked cars. Kate in matte black with purple
   panels. Windows then dimmed and varied so brick reads.
3. **Trick arrows.** Putty (holds), bola (trips), smoke (blinds), EMP (kills lamps and
   jams pistols), explosive. A radial wheel on held Tab with the game slowed.
4. **Clint.** Follows, takes cover, shoots what you hit, revives you once per fight,
   zips to catch up on rooftops. Switch with X (LB on a pad). Banter subtitles, 48 lines.
5. **Saves.** Open-source SPUD plugin. Autosave on objectives, safehouse, and roaming.
   Death reloads the last save. A safehouse on East 7th Street with refill and save. A
   main menu with Continue and New Game.
6. **Enemies.** Gunners telegraph, burst, take cover, and relocate. Purple-fletched
   archers on the roofs across from the arrow objective, with a draw you can interrupt.
   Squad alert. Thug behaviour moved to a StateTree with EQS cover.
7. Controller support and the Castle-to-Hawkeye rename earlier the same day.

## Numbers
| Metric | Value |
|---|---|
| Commits on main | 432 |
| Automation tests | 217, all passing |
| Enemies on the block | 4 tracksuits, 2 archers |
| Scripted lap | 33.7 s, mid-air grapple redirect, no wall contact |
| Scripted archer duel | won in 8.0 s, 0 hits taken |
| Frame time, RTX 4070, 1280x720 | about 9.6 ms |

## How to play it
**Play Hawkeye** on the desktop, or plug in a controller first.

| Key | Pad | Action |
|-----|-----|--------|
| WASD, mouse | Sticks | Move, look |
| Shift | L3 | Sprint; auto vault and mantle |
| Space | A | Jump, climb from a ledge |
| Ctrl | B | Tap dodge, hold crouch, sprint slide, edge drop-to-hang |
| Q | RB | Grapple to the green diamond, again mid-zip to chain |
| Left click | RT | Draw, release to fire |
| Right click | LT | Aim |
| 1 to 6, wheel; hold Tab | D-pad; hold View | Arrow type; radial wheel |
| V | X | Strike, hold for heavy |
| F | Y | Takedown from behind, or interact |
| X | LB | Switch to Clint and back |
| T | none yet | Send Clint to a point |
| E | Y | Safehouse door, pickups |
| Esc | Menu | Pause, settings, quit to menu |

Suggested route: the safehouse first (E at the purple door on East 7th facing the park)
to see the menu. Then the three rooftop objectives following the marker, up a fire escape
or by grapple. The roof pair is a melee fight; try putty on one and heavy strikes. The
archers across from the third roof are a bow duel: crouch behind the parapet when the
purple glint appears, stand and shoot between draws. Switch to Clint once and feel the
heavier draw.

## What I want from you
The same question as before, now with more to judge: how does moving feel, does the
marker and compass keep you oriented, is the fight fun or annoying, does the archer
duel feel fair, is Clint useful or in the way. Anything you tried and couldn't do.

## Known rough edges
- No draw or attack animations: the bow hovers by her shoulder when drawing, strikes are
  lunges, thugs get up by blending out of ragdoll. Archers draw from the hip.
- Smoke, EMP ring, and the fireball are placeholder shapes.
- Perfectly timed ducking breaks every archer draw; they'll need to hold a draw briefly.
- Clint's arrows end up in your quiver. Clint has no pad button for the mark command.
- Facade brick is dark away from lamps.
- The safehouse door sign blows out to white.
- Load takes about 14 s.

## Next
Your notes. Then the likely candidates: a bow-draw animation layer (needs an animation
source; Mixamo retargeted by script is the plan), Niagara effects for the trick arrows,
the heavy enemy, archers holding a draw, World Partition when a second district exists.
