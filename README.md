# Castle (code name) / Quiver (working title)

Third-person action game in Unreal Engine 5.8: a fan spin-off of the Disney+ Hawkeye
series with Kate Bishop and Clint Barton in a real-data slice of winter Manhattan.
Insomniac Spider-Man structure with grounded parkour and grapple arrows. Solo project.

- What the game is: `docs/DESIGN.md`
- Sourced facts about the show and the city data pipeline: `docs/research/nyc-hawkeye.md`
- Build stages: `docs/plans/00-overview.md`
- New here? `docs/PRIMER.md`
- Engineering rules: `CLAUDE.md` and `claude-docs/`
- The previous Punisher build this pivoted from: `docs/archive/punisher/`

## Requirements

- Unreal Engine 5.8 at `C:\Program Files\Epic Games\UE_5.8`
- Visual Studio 2022 with the **Game development with C++** workload (`.vsconfig` in the repo lists the components)
- Git LFS (`git lfs install`) before touching binary content

## Building and running

Everything runs headless from scripts; the editor is rarely needed.

```powershell
# Build the editor target
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CastleEditor Win64 Development -project="<repo>\Castle.uproject" -waitmutex

# Tests
.\Tools\run-tests.ps1

# Generate or update all content assets (idempotent)
.\Tools\create-content.ps1

# Play as a standalone game window
.\Tools\play.ps1

# Open the editor (rarely needed)
.\Tools\open-editor.ps1
```

Full command reference in `claude-docs/infrastructure.md`. Project files for Visual Studio:
`Build.bat -projectfiles -project="<repo>\Castle.uproject" -game -rocket -progress`.

## Layout

| Path | What |
|------|------|
| `Source/Castle/` | C++: Mission, Flashback, Combat, Player, World, UI, Settings, Tests |
| `Content/` | Generated assets (LFS) |
| `Tools/Editor/` | Python editor scripts that create every asset |
| `Config/` | Engine, game, and input settings |
| `docs/` | Design, research, plans, chapters, playtests, primer, archive |
| `claude-docs/` | Architecture, semantics, style, conventions, infrastructure, testing, workflow |
