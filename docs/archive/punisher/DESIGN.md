# Working Title: CASTLE (Punisher origin FPS)

Status: brainstorm, 2026-09-17. Engine: Unreal Engine 5 (Blueprint-first).

## One-line pitch
You are Frank Castle before the skull. Locked in a black-site prison that has broken
everyone else, you break out instead, and each step toward the exit drags up the
memory of why you're here.

## Pillars (the 3 things every decision serves)
1. **Restraint, then violence.** Frank isn't a superhero. Guns feel heavy, ammo is scarce
   in the early missions, every kill is deliberate. Power grows across the game.
2. **The prison is a character.** Every mission reveals something about who runs it and
   why they wanted Frank alive.
3. **Memory as reward.** Flashbacks are unlocked by progress. The player *earns* the
   truth about the family. Never front-load the tragedy.

## Structure: 8 missions + 4 flashback interludes

| # | Mission | Type | Flashback after? |
|---|---------|------|------------------|
| 1 | **Cell Block D** | Tutorial. Wake in a cell, no weapon. Stealth, melee, one stolen pistol. | Yes: "The Park" (happy family, sunlight, no context) |
| 2 | **The Infirmary** | First real firefight. Learn the prison runs medical experiments on inmates. | No |
| 3 | **Yard Riot** | Open arena. Frank triggers a riot as cover. Moral beat: inmates get slaughtered too. | Yes: "The Picnic" (the day it happened, cut before the shooting) |
| 4 | **The Warden's Records** | Slow, tense. Find files: Frank was kept alive as a witness who "needs to disappear." | No |
| 5 | **Solitary** | Horror-tinged. Lights out, generators, the "broken" inmates. | Yes: "The Hospital" (Frank wakes alone, learns the family is gone) |
| 6 | **Armory** | Power fantasy begins. Full loadout. Kill count spikes. | No |
| 7 | **The Warden** | Boss encounter. Choice to execute or leave him alive (sets up the skull). | Yes: "The Funeral" (three coffins, Frank in dress uniform, decides) |
| 8 | **Exit** | Escape sequence. Ends on Frank painting the skull on a stolen vest. Cut to black. | Credits |

Flashbacks = slideshows: 6-10 still images, slow crossfade, ambient audio, minimal text
or Frank's voice. Each one ends a bit later in the timeline than the last, so the
player pieces the day together. Cheap to build, emotionally effective if paced well.

## Tone references
- Punisher (2004 film) and The Punisher Netflix S1 for Frank's voice
- Riddick / Escape From Butcher Bay for prison-FPS structure
- Metro / Wolfenstein for weighty gunplay and quiet story beats between action
- Hotline Miami's flash-cut memory scenes for the slideshow idea

## Core loop (per mission)
Infiltrate -> find the objective (key, file, person) -> complication -> fight out ->
mission end card -> (optional) flashback.

## Difficulty and weapon pacing (added 2026-09-18)
Frank starts every early mission unarmed and stays that way for most of it. In Cell Block D
the pistol arrives near the end, as a reward, not from the first guard. Hands are a real
weapon slot: punches stagger, takedowns kill. The game is meant to be hard-ish through
scarcity and stealth, not through guard health. Later missions widen the arsenal (pistol,
then a rifle) and the hotbar and inventory exist from the start so that growth is visible.

## MVP scope (the first thing we actually build)
Mission 1 only, greybox, no art:
- First-person character with walk/sprint/crouch
- Melee takedown from behind
- One pistol: aim, fire, reload, limited ammo
- 3 guard AI with patrol -> investigate -> attack states
- Doors that need a keycard picked up off a body
- Mission-complete trigger volume
- One flashback slideshow (placeholder images) that plays after the mission

If that is fun, everything else is more of the same plus content.

## Technical plan
- UE 5.x, **First Person template** (ships with a working gun and character)
- Blueprints for everything at first; C++ only if we hit a perf wall
- Slideshow = a UMG Widget Blueprint with an image array and a timer. Trivially reusable.
- Free assets: Quixel Megascans (industrial/concrete), Epic marketplace free monthly items,
  Mixamo for animations
- Version control: git + Git LFS (UE .uasset files are binary and big)

## IP note
The Punisher is Marvel/Disney IP. This is fine as a personal or portfolio project, but it
cannot be sold or distributed commercially. If that ever matters, "Frank Castle" becomes an
original character with the same silhouette and nothing else changes.

## Open questions
- Length target? (8 missions at ~15 min = ~2 hours of play, which is a LOT of work solo)
- Voice acting for Frank, or silent protagonist with text?
- Do we want any choice/consequence, or is this strictly linear?
- Art direction: realistic (Megascans) or stylized (cheaper, hides weaknesses)?

---

# Story v2 (2026-09-17, after brainstorm round 2)

## The clean version, in one paragraph
Frank Castle wakes up in a high-tech black-site prison with a hole in his memory. He knows
his name, he knows how to fight, and he knows something is wrong. He does NOT remember
his family or why he's here. The prison isn't punishing him. It's *erasing* him. The
people who run it are the federal task force that used a mob meeting in Central Park as a
sting operation, let it go hot, and got Frank's family killed as collateral. Frank is the
last living witness. Killing a decorated Marine draws headlines; a Marine who came out of
"psychiatric care" a confused shell draws nothing. So they've spent months chemically
wiping him. The escape is the drugs wearing off. Every mission he gets farther from the
infirmary, and every flashback comes back a little clearer.

## Why the head trauma / amnesia works here (and how to avoid the cliche)
Amnesia is a cliche when it's random. Here it is **the villain's weapon**, and undoing it is
the plot. Three rules keep it honest:
1. **Frank never says "I can't remember."** He acts. The player figures it out from the
   flashbacks being incomplete and from the injection scars on his arms in the first mirror.
2. **Memory returns as a game mechanic.** Early missions have no HUD objective text, just
   Frank's gut ("this way"). As memory returns, objectives become explicit. The UI itself
   gets clearer as he does.
3. **Early flashbacks are WRONG.** The drugs planted a cover story: a car accident, Frank
   at the wheel, his fault. Flashback 1 and 2 show the accident version. Flashback 3 has
   slides that visibly *glitch and replace themselves* with the park. Flashback 4 is the
   truth. The player experiences the lie being pulled off. This is the deep part, and it
   costs nothing: it's just more slides.

## The lie vs. the truth (slideshow plan)
| Flashback | What Frank "remembers" | What's actually shown |
|-----------|------------------------|------------------------|
| 1 "Sunday" | Family in a car, laughing. Rain. | Warm, safe. Ends before anything happens. Player suspects nothing. |
| 2 "The Road" | Headlights, a skid, Frank's hands on the wheel. | The accident. Frank's guilt. A caption in the *prison's* voice: "You were driving, Frank." |
| 3 "The Park" | Starts as the car again, then the slides corrupt. Rain becomes sunlight. The car becomes a picnic blanket. Men in suits at the treeline. | The first true memory breaking through. No caption. Gunfire audio only at the end. |
| 4 "Three Coffins" | Frank in dress uniform. Three coffins. A man in a suit at the back of the funeral: **the Warden**, younger, watching. | The truth, and the reveal that the Warden was there that day. |

## Cast (small on purpose)
- **Frank Castle.** Mostly silent. Speaks maybe 40 lines total. Every line matters.
- **Dr. Adaeze Okafor, "the Doctor."** Runs the memory program. Believes she's doing mercy:
  "I gave you a life without the pain. You are choosing the pain." She is the moral
  argument of the game. Not a boss fight. Frank can kill her or not (Mission 5).
- **Warden Ellis Kane.** Ran the sting. Cold, practical, never raises his voice. Final boss.
- **Nico "Books" Marchetti.** Inmate in the next cell. Knows who Frank is before Frank does.
  Feeds him fragments through the wall vent. Twist: Books was a low-level mob guy who was
  *at the park that day*. He's the one who tells Frank the truth, and Frank has to decide
  whether he's a witness or a target. (Optional: Books dies in the Yard Riot so the choice
  is taken away, and that's the moment Frank stops hesitating.)

## Boss design
Two kinds, alternating, exactly like you described:

**Type A: Brawls (Spider-Man style health bars in waves).**
Melee only, arena, the boss has 3 health bars. Each bar depleted = phase change:
new move set, arena changes (lights die, doors open and goons pour in, floor floods).
- **The Orderly (Mission 2, Infirmary).** Huge, doped up on the same drugs, feels no pain.
  Phase 1 slow grabs. Phase 2 he rips a gurney apart for weapons. Phase 3 he's on his knees
  but won't stop; the "kill" is Frank choosing to finish it. First time the player kills
  someone who can't fight back. Sets the tone.
- **Yard Champion "Deacon" (Mission 3).** Fistfight in a riot. Other inmates are the
  arena hazard. Phase changes are the riot escalating around you.

**Type B: Duels (asymmetric 1v1, he has a rifle, you have a pistol).**
Tension, not DPS. The boss out-guns you so the level is the weapon.
- **The Sniper (Mission 5, Solitary).** Dark. He has thermal and a rifle; you have a
  pistol with 12 rounds. Generators you can kill to blind his thermal, steam pipes to
  break sight lines. Win = getting close. Ends in a takedown, not a shot.
- **Warden Kane (Mission 7).** Glass office, he has a shotgun and an armored vest,
  you have the pistol. Phase 1 cover-based, he talks the whole time, tells Frank
  the truth to slow him down. Phase 2 lights out, he hunts you. Phase 3 he's out of shells
  and it's hands. Ends with the choice: pull the trigger or walk away. Either way, the
  skull comes next.

Implementation: one `BossPhaseComponent` (health thresholds -> phase events) drives both
types; Blueprints decide what a phase change *means* per boss. Already in the code scaffold.

## Pacing check (the shape of the game)
Q  M1 stealth -> FB1 (lie) -> M2 brawl -> M3 riot brawl -> FB2 (lie, guilt) -> M4 quiet ->
M5 duel + Doctor -> FB3 (lie breaks) -> M6 power -> M7 Warden duel -> FB4 (truth) -> M8 exit -> skull.

Quiet / loud / quiet / loud. Every lie flashback lands right before a loud mission so the
guilt fuels it. The truth lands right before the exit so the skull means something.
