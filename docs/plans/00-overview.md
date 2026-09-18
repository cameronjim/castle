# Castle: build plan overview

Six stages. Each has its own doc. Do them in order. Each stage has a "done when" list; don't
start the next stage until every item is checked, because every stage assumes the last one
holds.

| Stage | Doc | What you have at the end | Rough solo time |
|-------|-----|--------------------------|-----------------|
| 1 | [01-setup.md](01-setup.md) | Engine, compiler, repo, and the project opens in the editor | 1 weekend |
| 2 | [02-prototype.md](02-prototype.md) | Mission 1 in grey boxes. Ugly, playable, fun or not | 3-4 weeks |
| 3 | [03-core-systems.md](03-core-systems.md) | Every system the other 7 missions will reuse | 4-6 weeks |
| 4 | [04-content.md](04-content.md) | All 8 missions, 4 bosses, 4 flashbacks, in grey boxes | 3-5 months |
| 5 | [05-art-audio.md](05-art-audio.md) | It looks and sounds like a prison, not a box | 2-3 months |
| 6 | [06-polish-ship.md](06-polish-ship.md) | Tested, packaged, playable by someone who isn't you | 1 month |

Those times assume evenings and weekends. If they look scary, the answer is to cut missions,
not to skip stages. Four great missions beat eight rough ones.

The one rule that matters more than any doc here: stage 2 decides whether the game exists.
If Mission 1 in grey boxes isn't fun to play by the end of stage 2, stop and fix that. Don't
paper over it with art. Nothing downstream fixes a loop that isn't fun.

## Reference points
- Design: [../DESIGN.md](../DESIGN.md)
- Code scaffold: `Source/Castle/` (mission, flashback, combat, player)
- Machine: RTX 4070, i7-13700K, 16 GB RAM. The RAM is the ceiling. Close everything else
  when the editor is open, and watch Task Manager during lighting builds.
