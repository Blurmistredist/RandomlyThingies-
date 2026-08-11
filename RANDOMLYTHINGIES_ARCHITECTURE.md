# RandomlyThingies architecture

RandomlyThingies is a separate preload-native addon.

It builds:
- `libRandomlyThingies.so`
- `RandomlyThingies.levipack`

It does not package `libBedrockTools.so` and its manifest has no overwrite entries.

The addon uses the public BedrockTools ABI (`BedrockTools_GetApi`) to:
- resolve BedrockTools signatures
- obtain the current ClientInstance
- subscribe to Frame, MouseInput, and ScreenState events

The addon has its own module registry and module IDs use the `randomlythingies.` prefix.

Included modules:
- FPS Graph
- Chunk Fade
- Custom Camera Offsets
