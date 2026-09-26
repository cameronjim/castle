# Hawkeye primer: how 3D games work, how Unreal works, how this project works

Written for Cameron, who is new to both. Read it once, top to bottom, in about 20 minutes.
Skip anything that's obvious. Come back to the glossary when a word in chat is unfamiliar.

---

## Part 1: How a 3D game works (in general)

A 3D game is a simulation that redraws the screen 60 times a second. Each redraw is a
**frame**. Between frames the game runs a tiny bit of logic for everything in the world,
then draws it all from the camera's point of view. That loop (update, draw, update, draw)
is the whole thing. Everything else is detail.

### The world is made of actors
Everything you see or interact with is an **actor**: a thug, a wall, a light, the door,
the invisible box that completes an objective when you walk into it. An actor is a thing
with a position in the world. It's built from **components**, which are the parts that do
the work:

- A **mesh** component gives it a shape (a 3D model made of triangles).
- A **material** gives the mesh a surface: colour, roughness, shininess, glow. Our concrete
  walls are a plain cube mesh with a concrete material on it.
- A **collision** shape says what it blocks. Kate's collision is an invisible capsule; that's
  what stops her walking through walls, not her visible body.
- A **light** component emits light. Everything you can see is lit by something.
- Custom components carry logic: our `HealthComponent` tracks hit points, our
  `WeaponComponent` handles ammo and shooting.

### The camera follows the character
In a third-person game the camera sits on an invisible arm behind and above the character.
The arm shortens when a wall is in the way and swings over the shoulder when you aim.
Moving the mouse turns the camera; pushing a direction turns the character to run that
way. That split is why third person feels different from first person.

### Physics and collision
The engine checks collisions every frame: can Kate move here, did an arrow hit that thug.
Arrows in our game are real objects: fired from the bow, they fly with gravity and take
time to arrive, so leading a moving target matters. Parkour is the character detecting an
obstacle with a few invisible rays, picking the right move, and playing it. Ragdoll is the
engine taking over a downed thug's skeleton and letting gravity and joints move it.

### AI is just rules that run each frame
A thug has a **state**: Calm, Suspicious, or Alerted. Every quarter second the thug's
controller looks at what it can see and hear and decides whether to change state. Calm
thugs walk between patrol points. Suspicious thugs walk to where they heard a noise.
Alerted thugs come at you or shoot. Movement uses a **nav mesh**: an invisible walkable
surface the engine generates over the floor so AI can plan paths around walls.

### Lighting sets the mood more than anything
Our room has a few fluorescent tubes, a red emergency light, thin fog, and an exposure
setting that decides how bright the final image is. Same room, different exposure, totally
different feeling. That's why "make it brighter" was a one-number change.

### Units
1 Unreal unit = 1 centimetre. Kate is about 170 cm tall. A Manhattan side street is about
1800 cm wide between building faces. When I say the grapple reaches 2500 units, that's
25 metres.

---

## Part 2: How Unreal Engine is organised

Unreal is the engine (the simulation loop, rendering, physics, AI tools) plus an editor
(the app for building levels and assets). Our project is a folder that the engine loads.

### The project folder (`C:\Users\camer\code\fps-game`)
| Folder | What it is |
|--------|------------|
| `Hawkeye.uproject` | The project file. Double-clicking it is unreliable on Windows 11; use the shortcuts instead. |
| `Source/` | C++ code. The rules of the game: health, weapons, missions, AI, flashbacks. Compiled into a DLL the engine loads. |
| `Content/` | Assets: maps, Blueprints, meshes, materials, textures, sounds. Binary files the editor reads. |
| `Config/` | Settings files (text). Which map starts, what the input keys are, rendering options. |
| `Tools/` | Our scripts. They build every asset in Content so nobody has to click through the editor. |
| `docs/` | Design, plans, mission notes, playtest notes, this file. |
| `Saved/` | Logs, screenshots, crash dumps. Regenerated, not committed. |

### Assets you'll hear about
- **Map / Level** (`L_District_LES`, `L_CH04_AuctionHouse`): a world with actors placed in it.
  The open district is one map streamed in pieces; interior chapters are their own maps.
- **Blueprint** (`BP_Thug`): a class defined visually in the editor. Ours are thin: each is
  "the C++ class plus which meshes and settings to use."
- **Data asset** (`DA_CH01_Rooftops`, `DA_Arrow_Grapple`): pure data. A chapter's objectives
  and flashback, or an arrow type's damage and effect.
- **Material** (`M_Brick`): the surface recipe for a mesh.
- **Widget Blueprint** (`WBP_Hud`): a piece of UI.
- **Input action** (`IA_Draw`): a named thing the player can do; a mapping context binds
  keys to it. This is why rebinding keys later is easy.

### C++ vs Blueprints
Unreal lets you write logic either in C++ or in Blueprints (visual node graphs). We put all
rules in C++ because it can be tested automatically and because I can write it as text.
Blueprints hold only settings and asset choices, and our scripts generate them.

### Play in Editor vs standalone
The editor has a Play button that runs the game inside the editor window. It's what most
tutorials show. We don't use it. **Play Hawkeye** launches the game as its own window with
no editor at all, which is simpler and closer to what a player would get. (It was called
Play Castle until the project was renamed on 2026-09-26.)

### The editor, if you ever open it
- Centre: the 3D viewport. Right-click-drag to look, WASD while holding right-click to fly.
- Top right, Outliner: every actor in the level, as a list.
- Bottom right, Details: settings of whatever you clicked.
- Ctrl+Space: the Content Drawer, a file browser for `Content/`.
- Green Play button: play inside the editor. Esc stops.
You won't need any of it for this project unless I ask you to look at something specific.

---

## Part 3: How THIS project works

### The division of labour
I build. You play and report. Every asset, building, light, and enemy placement comes from
a script in `Tools/Editor/`. The city itself is generated from real map data by script.
When you tell me "that rooftop gap is too wide," I change a number, rerun, and the district
updates. Nothing is hand-placed, so nothing is lost when we regenerate.

### The pipeline, when I say "building"
1. **Build**: compile the C++ in `Source/` into a DLL. About 80 seconds.
2. **Test**: run the automation tests headless. Over a hundred tests check rules like "a full
   draw does full damage" and "the chapter completes when all objectives are done."
3. **Content**: run the Python scripts inside a headless editor to create or update assets and
   maps. Idempotent: running twice changes nothing the second time.
4. **Verify**: scripts load every asset and check it's wired correctly.
5. **Render**: a test loads the map and takes screenshots so we can both see the room without
   opening the editor.
6. **Commit and push**: small commits, all lowercase, to github.com/cameronjim/hawkeye.

All of that needs the editor closed, because a running editor locks the compiled DLL. That's
the one rule you have to remember: **close the game or editor window before I build.**

### How to test
1. Double-click **Play Hawkeye** on the desktop. It opens a 1600x900 game window.
2. Play with these controls (the third-person set; it will grow as systems land):

   | Key | Action |
   |-----|--------|
   | WASD, mouse | Move, look |
   | Shift | Sprint; hold it into obstacles to vault, mantle, and climb automatically |
   | Space | Jump, or climb up from a ledge |
   | Ctrl | Crouch, slide while sprinting |
   | Left click | Draw the bow; release to fire |
   | Right click | Aim over the shoulder |
   | Mouse wheel, 1 to 6 | Arrow type |
   | Q | Grapple arrow at the marked anchor |
   | F | Takedown, when behind a thug and the prompt shows |
   | E | Pick up, interact |
   | Tab | Quiver and inventory |
   | Esc | Pause menu (Resume, Settings, Restart, Quit) |

3. Until the district exists, the test space is a single greybox block. Run laps of it, get
   on the roofs, get back down, pick a fight with the thugs. Time yourself.
4. Notice things. Anything that feels wrong is a valid note: "got stuck on that ledge,"
   "camera too close," "arrow felt slow," "I didn't know where to go." Plain words are best.
5. Quit (Esc, Quit) and tell me. Or write it in the current chapter's file in `docs/chapters/`.

### What I do with your notes
I read the log at `Saved/Logs/Hawkeye.log`, which records chapter starts, pickups, hits,
takedowns, and traversal moves. Combined with your report, that's usually enough to find the
cause. Then I make the change, run the pipeline, render a screenshot from the real game
process, and tell you to relaunch.

### Where the design lives
- `docs/DESIGN.md`: what the game is. Story, chapters, bosses, systems.
- `docs/research/nyc-hawkeye.md`: what's sourced about the show's locations and how the
  city gets generated.
- `docs/plans/`: the six build stages with "done when" checklists. We're in stage 2.
- `docs/chapters/`: one file per chapter, written before it's built.
- `docs/playtests/`: one file per play session.
- `docs/archive/punisher/`: the previous build, for reference.

---

## Part 4: Learning more, if you want to

You don't need any of this to keep going. But if you want to understand what I'm doing,
these are the places I'd start. Search the titles on YouTube; channels move videos around
so I'm not pasting links that might be dead.

### Unreal Engine basics (watch one, not all)
- **"Your First Hour in Unreal Engine 5"** on the official Unreal Engine channel. Made by
  Epic, free, slow, and thorough. The editor, viewport, actors, Blueprints.
- **Unreal Sensei, "Unreal Engine 5 Beginner Tutorial"**. Long (about 5 hours), the most
  recommended starting point on the internet. You'll understand every word I use after it.
- **Gorka Games** for short focused videos: "how to make a door," "how to make AI patrol."
  Good when you want to see one mechanic built by hand.

### Understanding game design (why we made the choices in DESIGN.md)
- **Game Maker's Toolkit** (Mark Brown). Start with "What Makes a Good Combat System?",
  "The Rise of the Systemic Game," and anything on stealth. Best channel on the topic.
- **GDC talks** on YouTube: search "GDC level design" and "GDC stealth AI." The Splinter
  Cell and Dishonored talks are directly relevant to our thugs.
- **Design Doc** channel: "Good Design, Bad Design" series. Short and concrete.

### How 3D rendering works (optional, for curiosity)
- **Branch Education, "How do Video Game Graphics Work?"** A single clear video.
- **Sebastian Lague**, any video. Beautiful explanations of meshes, lighting, and physics.

### Reading the code, if you're ever curious
`claude-docs/architecture.md` has a diagram of how our systems talk to each other, and
`claude-docs/gameplay-semantics.md` is a plain-language list of every rule the game
enforces. Neither requires knowing C++.

---

## Glossary

| Word | Meaning |
|------|---------|
| Actor | Any object in the world |
| Component | A part of an actor that does one job (mesh, light, health) |
| Mesh | A 3D shape. Static mesh = doesn't bend (walls). Skeletal mesh = has bones (thugs) |
| Material | The surface recipe on a mesh |
| Texture | An image a material uses |
| Blueprint | A class defined in the editor; ours hold settings, not logic |
| Level / Map | A world with actors in it |
| Tick | One frame's worth of update, about 60 times a second |
| Line trace | An invisible ray used to check what's in front of something (our bullets) |
| Collision | The invisible shapes that decide what blocks what |
| Nav mesh | The walkable surface AI uses to path |
| Ragdoll | Physics taking over a dead character's skeleton |
| Widget / HUD | On-screen UI |
| Data asset | A file of pure settings |
| Exposure | How bright the final image is, in stops (EV). Lower is darker |
| Lumen | Unreal 5's real-time lighting system, why our lights need no baking |
| PIE | Play In Editor. We use standalone instead |
| Headless | Running the engine with no window, for builds, tests, and scripts |
| LFS | Git Large File Storage, how binary assets get into the repo |
| Idempotent | Safe to run twice; the second run changes nothing |
