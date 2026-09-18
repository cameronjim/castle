# Workflow

How to work in this repo, whether you're Cameron, Claude, or a subagent.

## Before touching anything
1. Read `CLAUDE.md`. Then the `claude-docs` file for the area you're changing.
2. Read `docs/DESIGN.md` if the change affects what the game is. Read the stage plan in
   `docs/plans/` for what's in scope right now. Building stage 4 features during stage 2
   is the most common way this project dies.
3. `git status` and `git log -5`. Know what's uncommitted and what just landed.
4. If the editor is open, know it. Building C++ under an open editor without Live Coding
   wastes the build.

## Making a change
1. **Say what you're doing in one line before you do it.** In chat for Claude, in a
   commit message draft for a human.
2. **Data first.** Can this be a property, a data asset, a Blueprint default? Then do
   that. C++ is for rules and reusable machinery.
3. **Semantics doc before code**, for any new rule. Add the bullet to
   `gameplay-semantics.md`, write the test, then make it pass. This is not ceremony:
   the semantics doc is what keeps eight missions consistent.
4. **Build after every C++ change.** Fix warnings you introduced.
5. **Run the tests** (`Tools\run-tests.ps1`) after touching any system that has them.
6. **Human checklist** (`testing.md`, section 3) after anything that affects feel.
7. **Commit and push.** Small commits, often: each class, each test file, each script,
   each doc is its own commit. Message: lowercase imperative summary under 70 chars,
   optional blank line and a short why. No co-author, no generated-by trailer, nothing
   after the body. `git push origin main` after every commit or two. Remote:
   https://github.com/cameronjim/castle.git

## Agent work
- **Read the relevant `claude-docs` file first, every time.** Context from a previous
  session is not in your head.
- **Stay in your lane.** If you were asked to fix compile errors, don't refactor. If you
  were asked to add a test, don't change the semantics it tests. Report what else you
  noticed instead.
- **Never leave the tree broken.** If you can't get the build green, revert your changes
  and report the errors. A red build blocks everyone.
- **Headless over visual.** Use `UnrealEditor-Cmd.exe -nullrhi` for builds, tests, cooks,
  and Python asset scripts. Launch the full editor only when explicitly asked to check
  something visual, and close it when done.
- **Don't guess engine APIs.** UE 5.8 differs from what most training data shows. If a
  call doesn't compile, look in `C:\Program Files\Epic Games\UE_5.8\Engine\Source` for
  the header (grep is fine) rather than trying variations.
- **Report honestly.** "Built, tests pass" only if you ran them and they did. If a
  test is flaky, say so. If you skipped something, say what.
- **Python asset scripts are repeatable.** Every script in `Tools/Editor/` must be safe
  to run twice: check whether the asset exists before creating it, and save only what
  changed.
- **Don't touch:** the `ProjectID` in `DefaultGame.ini`, the project or module name,
  `.gitattributes` LFS rules (add, never remove), anything under `docs/` unless the task
  is docs.
- **Model routing** (Cameron's rule): thinking, design, and review in the main session.
  Delegated agents get `opus` for hard work (compile-fixing, systems code, AI) and
  `sonnet` for easy work (file moves, running scripts, checks).

## When something breaks
- Compile error: read the first error, not the last. UHT errors before compiler errors.
- Editor crash on startup: check `Saved\Logs\Castle.log` for the last `LogCastle` line and
  `Saved\Crashes\` for the callstack. Usually a constructor touching something that isn't
  ready, or a bad config value.
- Asset won't load / "failed to load": a redirector or a renamed class. Fix Up
  Redirectors, or add a `[CoreRedirects]` entry in `DefaultEngine.ini` for the rename.
- Test fails only headless: it's touching rendering, input, or audio. Assert on calls,
  not effects.
- Out of memory: close the editor, close the browser, retry. If it recurs in the same
  map, that map needs streaming or fewer lights.

## Definition of done, for a task
- Builds clean.
- Tests pass, and new rules have tests.
- Docs updated if semantics, structure, or conventions changed.
- Committed with a message that explains why.
- Anything left undone or noticed along the way is written in the report or as a
  `// TODO(stageN)` in code.
