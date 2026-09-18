# Stage 5: Art and audio

Goal: replace every grey box with something that looks like a black-site prison, and
make the game sound right. No new gameplay in this stage. If you find yourself adding a
mechanic, write it down and go back to placing meshes.

## Art direction decision (make it before you import anything)
Two realistic options for a solo dev on this hardware:

**Realistic, Megascans-heavy.** Concrete, rusted steel, fluorescent light. Quixel is free
with Unreal. Looks great in screenshots, costs a lot of RAM and lighting time, and every
missing detail is obvious because the rest looks real.

**Stylized, low-detail, hard lighting.** Flat materials, strong colour per area (cell
block is sick green, infirmary is bleached white, solitary is near-black with a single red),
heavy shadow. Cheaper, faster, hides weak animation, and it fits the memory theme.

Pick stylized unless you have a specific reason not to. You can always add detail
later. You can't easily remove the expectation realism sets.

## Environment
- Replace kit meshes one type at a time across all missions (all walls, then all doors,
  then all lights). The kit from stage 3 makes this a swap, not a rebuild.
- One material palette per mission area. Write the colour and mood down in the mission
  one-pager before you touch materials.
- Lighting is 70% of how this looks. Lumen is on by default in UE5. On a 4070 it's fine
  at 1080p. If frame rate tanks in the yard, switch that map to baked lighting.
- Decals for grime, blood, scratched tally marks, warning signs. Cheap, and they sell
  the place more than any mesh does.
- Props: gurneys, lockers, a chair, cables, pipes, security cameras, monitors. Free
  Fab packs cover 90%. Buy one good industrial pack if it saves a week.

## Characters
- Frank: first person, so mostly arms and a weapon. MetaHuman or Fab arms. The arms
  need to look strong and scarred (injection marks matter to the story).
- Guards: two body types, three uniform variants (baton, pistol, rifle), one face
  each with a helmet or mask so you're not making faces.
- Bosses: each needs to be recognisable at a glance. The Orderly is enormous and
  shirtless. Deacon has a shaved head and tattoos. The Sniper is never clearly seen.
  The Warden is a man in a suit, the only one in the game.
- Animation: Mixamo for locomotion, punches, deaths. Retarget once with IK Retargeter.
  Don't hand-animate. If a boss needs a signature move Mixamo doesn't have, buy it.

## Flashback images
Now they matter. Options: commission an illustrator for about 34 stills (4 flashbacks
times 8 slides plus alternates) in one consistent style, or generate them and paint over
for consistency, or shoot real photos with friends and treat them heavily. Whatever you
pick, one style for all four, and the lie flashbacks and true flashbacks should differ
in something subtle: colour temperature, film grain, how in-focus faces are.

## UI
- HUD: minimal. Ammo bottom right, objective top left (when enabled), crosshair, hit
  marker, a subtle vignette for damage. No health bar. Screen desaturates as health drops.
- Menus: black background, white text, one typeface. Matches the end cards.
- Boss health bar: segmented per phase, appears when the fight starts, no name plate.

## Audio
- Guns: one pistol sound is heard ten thousand times, so make it good. Layered shot,
  mechanical tail, distinct empty click. Freesound, Sonniss GDC packs (free), or one
  paid pack.
- Footsteps per surface via physical materials. Concrete, metal grate, water, tile.
  Guards' footsteps are how the player tracks them in stealth, so guards are louder
  than Frank.
- Ambience per area: HVAC hum, fluorescent buzz, distant doors, a scream once in a while
  in the infirmary. A single ambient loop per map plus 4 to 6 spot sounds.
- Music: sparse. Nothing during stealth. A low pulse when alerted. A track per boss.
  Silence for the flashbacks except the ambient slide sounds. Consider hiring one
  composer for 5 to 6 short pieces, or use a licensed library.
- Voice: Frank has about 40 lines. Books, the Doctor, the Warden about 30 each. Hire
  voice actors on a freelance site or record friends who can act. The Warden's
  performance carries the ending, so put your budget there.
- Mix pass at the end: play with eyes closed and make sure you can tell what's happening.

## Done when
- [ ] No grey-box material visible anywhere in a full playthrough
- [ ] Every mission has its own colour and sound signature
- [ ] All 4 flashbacks have final images in one style
- [ ] All dialogue is recorded (scratch or final) and subtitled
- [ ] 60 fps at 1080p on this machine in every level including the yard riot
- [ ] Editor and Play in Editor still fit in 16 GB RAM without paging (check Task Manager)

## Budget note
If you spend money anywhere in this project, spend it here: one asset pack, one
illustrator or image pass, one voice actor for the Warden. Everything else can be free.
