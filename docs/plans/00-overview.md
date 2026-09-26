# Quiver: build plan overview

Rewritten 2026-09-25 for the Hawkeye pivot. Six stages. Stage 1 is done (the engine,
compiler, repo, pipeline, tests, and content scripts from the Punisher build all carry
over). Each stage has a "done when" list; don't start the next until every item holds.

| Stage | Doc | What you have at the end | Rough solo time |
|-------|-----|--------------------------|-----------------|
| 1 | [01-setup.md](01-setup.md) | Done. Plus a cleanup pass that removes first-person code | 2 days |
| 2 | [02-prototype.md](02-prototype.md) | One real-data city block, third-person Kate, parkour, grapple arrow, a bow, four thugs. Fun or not | 4-6 weeks |
| 3 | [03-core-systems.md](03-core-systems.md) | Trick arrows, melee, partner AI, switching, streaming, save, thugs and archers | 6-8 weeks |
| 4 | [04-content.md](04-content.md) | Six chapters, three bosses, flashbacks, one finished district | 4-6 months |
| 5 | [05-art-audio.md](05-art-audio.md) | Winter Manhattan that looks and sounds like it | 3 months |
| 6 | [06-polish-ship.md](06-polish-ship.md) | Tested, packaged, playable by strangers | 1 month |

Those are evenings-and-weekends numbers and they are bigger than the Punisher plan,
because an open city in third person is more game. If they look scary, the answer is a
smaller district and fewer chapters, not skipping stages.

The one rule that matters more than any doc here: **stage 2 decides whether the game
exists.** If running across a greybox block isn't fun by the end of stage 2, stop and fix
that. No amount of story or art fixes traversal that doesn't feel good.

## Reference points
- Design: [../DESIGN.md](../DESIGN.md)
- Research: [../research/nyc-hawkeye.md](../research/nyc-hawkeye.md) (show locations, city data pipeline)
- Previous build (archived): [../archive/punisher/](../archive/punisher/)
- Code: `Source/Hawkeye/` (module renamed from Castle on 2026-09-26)
- Machine: RTX 4070, i7-13700K, 16 GB RAM. An open world will push the RAM harder than a
  prison did. World Partition streaming is not optional.
