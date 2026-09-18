# Stage 2: Prototype Mission 1 in grey boxes

Goal: Cell Block D is playable start to finish with zero art. Boxes for walls, capsules
for guards, a cube for the pistol. This stage answers one question: is the loop fun?

## What "the loop" is
Wake up unarmed. Sneak. Take down a guard from behind. Take his pistol and keycard. Get
through a door. One firefight with three guards. Reach the exit. Flashback plays. Credits
placeholder. Eight to twelve minutes.

## Steps, in this order

1. **Player movement first, nothing else.** Make `BP_CastleCharacter` from
   `ACastleCharacter`. Set up the Enhanced Input assets (IMC_Default, IA_Move, IA_Look,
   IA_Jump, IA_Sprint, IA_Crouch, IA_Fire, IA_Reload, IA_Takedown, IA_Interact) and assign
   them on the Blueprint. Walk around the sandbox. Tune walk speed, sprint speed, crouch
   height, and mouse sensitivity until it feels heavy but responsive. Frank is a big man.
   Spend a full session on this. It's the thing the player does 100% of the time.

2. **Block out the level.** New map `Maps/L_M01_CellBlockD`. Use the Modeling Mode cube
   tool. Rooms: your cell, a corridor with 2 guards, a guard station with the pistol and
   keycard, a locked door, the infirmary hallway (3 guard fight), an exit stairwell. Keep
   it small. Walk it as the player with no enemies and time it. Under 4 minutes of pure
   walking, or the level is too big.

3. **Guard AI.** `BP_Guard` from Character with a `HealthComponent`, tag "Guard", and
   the `Takedownable` interface. Behavior Tree with three states: Patrol (waypoints),
   Investigate (heard a noise, walks to it, waits, returns), Attack (sees you, shoots,
   strafes). AI Perception with sight and hearing. Sight cone narrow (about 70 degrees),
   hearing radius generous. Sprinting makes noise, crouching doesn't. This is the whole
   stealth system and it's enough.

4. **Takedown.** Bind IA_Takedown to `TakedownComponent.TryTakedown`. On the guard,
   implement `OnTakedown` to ragdoll and drop a pickup. Add a prompt when a valid target
   is in range so the player knows it's available. Test: can you sneak up on a patrolling
   guard and drop him without the other one noticing? If no, tune hearing radius and
   patrol paths until yes.

5. **Pistol.** Enable `WeaponComponent` on pickup. Line trace, 12 round magazine, 24
   reserve. Damage kills a guard in 3 body shots or 1 headshot. Add a crosshair and an
   ammo counter to a bare HUD widget. Reload takes about 2 seconds. Recoil is a camera
   kick, nothing fancy.

6. **Keycard and door.** `BP_Keycard` pickup sets a bool on the player. `BP_LockedDoor`
   checks it on Interact. Wire the door opening to complete an objective on the
   `MissionSubsystem`.

7. **Mission data.** Make `DA_M01` from `MissionDefinition`. Objectives: "Get out of the
   cell", "Find a weapon", "Get through the security door", "Reach the stairwell".
   Drop `ObjectiveTriggerVolume`s where each completes. Show current objective text on
   the HUD. Confirm the subsystem fires OnMissionComplete at the stairwell.

8. **Flashback 1, placeholder.** `DA_FB01_Sunday` with 6 slides. Use any stock photos or
   solid colours with captions. Confirm the widget plays, crossfades, skips, and returns
   control. This is a smoke test of the pipeline, not the real flashback.

9. **Play it twenty times.** Fix what annoys you. Have one other person play it while
   you watch and say nothing. Write down every place they got stuck or bored.

## Done when
- [ ] Full run from cell to stairwell without cheats, 8-12 minutes
- [ ] Stealth path works (0 shots fired) and loud path works (fight your way out)
- [ ] At least one other person has finished it without you talking
- [ ] You'd play it again right now for fun, not to test something
- [ ] Nothing in the level is final art. If you caught yourself importing textures, stop.

## Cut list if it's dragging
Drop in this order: crouch, investigate state, reserve ammo. The core is
sneak, takedown, shoot, door. Everything else is seasoning.

## What you learn here decides stage 3
Write half a page after playtesting: what felt good, what didn't, what surprised you.
That note reshapes the systems plan before you build anything permanent.
