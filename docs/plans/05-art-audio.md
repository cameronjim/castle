# Stage 5: Art and audio

Goal: winter Manhattan that looks and sounds like the show. No new gameplay in this stage.

## Art direction decision
Two options, same as before, different answer:

**Realistic.** Quixel Megascans (free with Unreal), real building textures, snow. Looks
like the show. Expensive per building, and an open district has hundreds.

**Stylised, comic-inspired.** Flat lighting, strong colour, the Fraction and Aja palette:
purple, cream, grey, black. Cheap per building, distinctive, hides generated geometry.

For an open city solo, stylised is the only sane answer. A hero block or two can be
pushed further. The show's warmth comes from colour and light, not texture detail.

## The city
- Generated buildings get a small set of facade materials (brick, brownstone, glass,
  concrete) assigned by footprint size and OSM tags, with window grids from a material
  function, not geometry.
- Snow: a world-aligned material layer on upward faces, plus decals for footprints and
  slush at kerbs. Christmas leftovers: string lights on a few facades, a wreath on doors,
  a fallen tree bagged on a kerb.
- Winter light: low sun, long shadows, blue shade, warm windows. Evening as the default
  time; the show is mostly night and dusk.
- Street furniture and rooftop clutter from Fab packs and simple primitives.

## Characters
- Kate and Clint: MetaHuman or a stylised Fab character base, with the show's costumes
  (Kate's purple and black, Clint's dark jacket). Hair and face don't need to be likenesses;
  silhouette and colour do the work.
- Thugs: the tracksuits. One mesh, three colour variants, ski masks.
- Barney: the one man in a proper costume. Purple, sharper than Kate's, older.
- Lucky: a dog mesh from Fab with a basic follow animation set.
- Animation: Game Animation Sample for locomotion, Mixamo for combat and parkour, IK
  Retargeter by script. Buy the few signature moves Mixamo doesn't have.

## Flashback images
Commission or generate in one consistent style. Carnival at night, warm bulbs, two boys.
Clint's memories cool, Barney's warm, so the player feels whose version they're seeing.

## UI
Comic-inspired: chunky sans, purple accent, arrow icons that read at a glance. Quiver
wheel, objective markers, phone panel, minimal HUD. Menus match.

## Audio
- Bow: draw creak, release, arrow whistle, impact by surface. The most-heard sound in
  the game; make it good.
- City: traffic bed by district, sirens far off, wind on rooftops, snow crunch underfoot,
  an argument through a window. Ambience is what sells the open world.
- Music: sparse, jazzy, warm. A theme each for Kate, Clint, and Barney. Silence on
  rooftops at night.
- Voice: Kate and Clint have a lot of lines. Two actors, hired. Everything else can be
  scratch or text for a long time.

## Done when
- [ ] No greybox visible in the district in a full playthrough
- [ ] The district reads as winter New York from any rooftop
- [ ] Kate and Clint are recognisable in silhouette from across a street
- [ ] All chapter dialogue recorded, scratch or final, and subtitled
- [ ] 60 fps at 1080p on this machine everywhere in the district
- [ ] Editor and game fit in 16 GB without paging

## Budget note
Spend on two voice actors and one good NYC facade pack. Everything else can be free.
