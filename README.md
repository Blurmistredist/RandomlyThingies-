# RandomlyThingies

A standalone BedrockTools addon containing three visual modules:

- FPS Graph
- Chunk Fade
- Custom Camera Offsets

It builds a separate `libRandomlyThingies.so` and `RandomlyThingies.levipack`.

## Relationship to BedrockTools

RandomlyThingies is designed as an addon for the original BedrockTools runtime. It does not package or replace `libBedrockTools.so`.

The addon uses BedrockTools' public ABI (`BedrockTools_GetApi`) for signature resolution, client access, and event delivery.

## Build

GitHub Actions builds the Android ARM64 package automatically.

Output:

```text
RandomlyThingies.levipack
```

The package contains:

```text
manifest.json
libRandomlyThingies.so
icon.png
resources/minecraft.ttf
```

Author: Blurmistredist


## BedrockTools extension architecture

RandomlyThingies is an external BedrockTools extension. It is still packaged as a
loadable LEVIPack because the preloader needs a mod entrypoint, but it does not
install a second BedrockTools runtime, signature scanner, event bus, or game-hook
core. Its modules resolve targets through `BedrockTools_GetApi()` and consume
BedrockTools events through the public API.

The GitHub Actions workflow builds both `libRandomlyThingies.so` and
`RandomlyThingies.levipack` and uploads them as a single artifact.
