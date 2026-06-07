# LuaJIT for OpenMW with faster garbage collection and open sandbox

Drop-in **LuaJIT** for OpenMW 0.51, built on GitHub Actions.  
Comes in different Flavors (mix and match):

- **unlock the sandbox**  
  a `select('sandbox.bypass')` escape hatch that returns libraries OpenMW normally hides (ffi, io, os, ...)  
- **smoothen the GC**  
  a reworked garbage collector that cuts the periodic stutter from mods with wasteful memory allocations

Installing is just dropping one file next to `openmw.exe` (or the Linux/macOS equivalent).

## Which build do I want?

Two choices, independent of each other.

**How much GC smoothing?** Higher stages remove more of the stutter but are more experimental.

| build | what you get | risk |
|---|---|---|
| stock | plain upstream LuaJIT | none |
| Good GC | 20% smaller GC lagspikes | very low |
| Better GC | 30% smaller GC lagspikes | low |
| Extreme GC | GC lagspike spread across multiple frames | highest |

On my install these are stable but if you experience crashes, drop down to a less experimental build.

**Sandboxed or not?**

| build | gives you | tradeoff |
|---|---|---|
| sandboxed | nothing extra | safe, but very limited capabilities |
| unsandboxed | ffi, io, os, package, debug, ... | any Lua script gets full machine access |

If you just want the GC smoothing for mods you trust, sandboxed is the safe pick.  
Use unsandboxed only when a mod asks for it.

## The unsandboxed bypass

This adds an escape hatch which returns a table of functions that the sandbox normally hides.

```lua
local bypass = select('sandbox.bypass')
```

- `bypass.ffi` - the LuaJIT FFI, call straight into C and the OS
- `bypass.buffer` - `string.buffer`, fast serialize and deserialize
- `bypass.os` - the full `os` library (`os.time`, `os.getenv`, ...)
- `bypass.io` - file `io`
- `bypass.package`, `bypass.require` - the module system
- `bypass.load`, `bypass.loadstring`, `bypass.loadfile`, `bypass.dofile` - run code from a string or file
- `bypass.jit` - JIT engine control
- `bypass.bit` - bitwise operations
- `bypass.collectgarbage` - garbage-collector control
- `bypass.debug` - the debug library

These are stock LuaJIT. See <https://luajit.org/> for what each one does.

### Bonus: `bypass.shared`

One table shared by **every** script context (player, menu, global, local/actor, ...).  
It's **not** wiped on `reloadlua` because it lives in the real `package.loaded`.  
This contradicts the multiplayer-ready architecture of OpenMW, so try not to base your mod on this.  

## Download

Grab a release from the Releases page. Each GC stage has its own rolling release.

| stage | release tag |
|---|---|
| stock | `stock-gc` |
| Good GC | `good-gc` |
| Better GC | `better-gc` |
| Extreme GC | `extreme-gc` |

Each release carries six files: three platforms, each in a sandboxed and an unsandboxed version.  
Download the one you want, rename it, and overwrite the engine's.

| platform | rename to |
|---|---|
| windows x86_64 | `lua51.dll` |
| linux x86_64 | `libluajit-5.1.so.2` |
| macos arm64 | `libluajit.dylib` |

The `stock-gc` release only comes with the unsandboxed version (no renaming necessary).

## How it works

It's plain LuaJIT (Lua 5.1, the version OpenMW runs) pinned to commit `707c12bf`, with two small patches on top.

The **bypass** is the simple one: a single change to one function in `lib_base.c`.

The **GC stages** (under `patch/gc/`) are the interesting part.  
OpenMW wraps every C++ game object as a userdata with a `__gc` finalizer, and in a heavily-modded setup that list gets *huge*.  
Once per GC cycle LuaJIT stops the world and walks the whole thing looking for objects to finalize, and that pause is the stutter you feel.  
Each stage shrinks it a different way:

- **Good** stops re-walking userdata it has already finalized. No point looking twice. (Roughly what Lua 5.4 does.)
- **Better** also pulls userdata that have no finalizer off the list, so the walk only ever sees objects that actually need it.
- **Extreme** drops the dedicated walk entirely and instead catches dying finalizables during the incremental sweep LuaJIT already runs, spreading it across multiple frames instead of doing everything in one Spike.

Extreme can get away with that because of a quirk in how OpenMW writes its finalizers: every `__gc` is a leaf C++ destructor. It frees a native handle and never reads another Lua object, so running one can't drag anything back from the dead. That lets the collector skip the "stop-the-world bookkeeping" it would otherwise need to keep a finalizer's dependencies alive.

The one thing it still needs is each dying userdata's own metatable, since that's where `__gc` actually lives. So Extreme sweeps userdata before tables and briefly revives just that metatable for the one cycle it's needed before letting it go.

Extreme also lowers the GC stepmul, which now determines over how many frames the work is distributed.

## Compiling

Each build clones LuaJIT, patches the files, and compiles.

**Locally (Windows):**

- `build.bat` - unsandboxed build
- `build.bat sandboxed` - stock `lib_base`
- output goes into `luajit\src\`

**GitHub CI:**

Every build is manual: in **Actions**, pick the workflow and **Run workflow** on the branch you want (`stock-gc`, `good-gc`, `better-gc`, or `extreme-gc`).
