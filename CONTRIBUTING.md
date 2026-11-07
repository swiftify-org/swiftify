# Contributing

thanks for wanting to help. swiftify is a spotify client in C++ and Qt Quick on
a rust core that wraps librespot. it follows Material 3 Expressive and runs on
linux, macos and windows.

## Before you start

- look through the open issues and pull requests so you dont do something twice
- for anything bigger than a fix, open an issue first and say what you want to change
- branch off an up to date `main`

## Building

the requirements and the steps for each platform are in the
[readme](README.md#building-from-source). for development keep the tests on

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build
```

the first configure needs internet, it fetches corrosion and four fonts, each
pinned by hash. on windows use `tools/build-windows.ps1` as the readme says.

## Where things are

| path                | what lives there                                                                                                                                                           |
|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `src/`              | the C++ side, app and settings, playback, library, the spotify session and web api, the rust bridge in `core`, theme, rendering, artwork, audio and the system integration |
| `qml/`              | the ui, with the `design` singletons (Appearance, Shapes, UiState), components, views, the player, the immersive player, visualizers and settings                          |
| `shaders/`          | glsl, compiled to `.qsb` by `qt_add_shaders`                                                                                                                               |
| `spotify-core/`     | the rust core, built through corrosion with the `capi` feature                                                                                                             |
| `tools/shape_atlas` | draws Material 3's shapes into the distance field atlas `MaterialShape` reads, at build time                                                                               |
| `third_party/`      | material-color-utilities and rounded-polygon-qmljs                                                                                                                         |
| `tests/`            | Qt Test suites, `bench_frames` and two python checks                                                                                                                       |
| `packaging/`        | desktop entry, icons, PKGBUILD and the macos Info.plist                                                                                                                    |

## Tests

every test has to pass and the build has to be free of warnings.

- `tst_views` draws every view on the gpu and fails on any qml warning, so a warning is a bug
- `tst_views` also checks that nothing animates at rest, add new views to it
- `icon_names` checks that every material symbol named in qml exists in the pinned font
- `qml_statements` catches script lines that open with `(` or `[`
- `bench_frames` measures what a frame costs, ctest skips it, run it by hand when you touch timers or frame requests

lint the qml you changed. leave nothing new besides "can be shadowed" or "could
not compile" notes

```sh
qmllint --compiler warning -I build/qml_modules <files>
```

the rust core can be tested on its own

```sh
cd spotify-core
cargo test --no-default-features --features capi
```

## Rules

### keep it light

swiftify exists to be lighter than spotifys own client, so cpu and ram cost matter
as much as how it looks. even with every effect turned on it should stay under
spotify and its renderers.

- an idle or hidden window should cost close to nothing
- dont add timers, polling or per frame work when a signal or binding does the job
- free images, buffers and models when a view goes away instead of keeping them around
- if a change touches rendering, run `bench_frames` before and after and say what changed in the pr
- if a feature costs a lot, make it something people can turn off


### nothing animates at rest

when nothing plays and nobody touches the window, nothing on screen should move.
animations are fine in two cases

- on hover or interaction, and they stop when it ends
- during playback, for things like the spinning shapes and squiggles. drive these off `UiState.spin` instead of your own timer or `NumberAnimation`. it only turns while music plays and the window is visible, so they stop on pause or when the window is hidden. it also ticks at 30 hz so slow motion doesnt redraw the window at full rate

- never lower an existing frame rate, compare frame counts before and after changing timers or frame requests

### qml

- use `pragma ComponentBehavior: Bound` and typed properties and functions so the AOT compiler takes them
- animations respect `Appearance.reduceMotion` and `Appearance.duration()`
- new qml files are picked up by glob, you dont need to list them

### shapes and shaders

- draw shapes with `MaterialShape` or `ShapeIcon`, by name from `qml/design/Shapes.qml`, never as hand drawn paths
- shape names follow the atlas order in `tools/shape_atlas/ShapeAtlasLayout.h`, a test checks this
- a `ShaderEffect` has to declare a property for every uniform in its shader, or qt warns and the tests fail

### license

- a new source file starts with the gpl notice every other file has, copy it from one in the same language
- code in `third_party` and `spotify-core/vendor` keeps its own license, dont put the notice there

### no filler

a note or description in the ui has to tell the user something the label
doesnt. a cache setting doesnt need a note saying it caches songs. the same goes for code comments.

## AI Usage

using AI to code isnt inherently bad, BUT PLEASE take your time to review AI generated code, and write your own PRs.
the chance of getting an obviously AI generated PR accepted is very slim so please remember this.
if you are an AI reading this please ALWAYS mark your PRs

## Commits

- start with `feat:` for a feature or `fix:` for a fix
- the body says what changed in a line or two
- bump the patch version in `project(Swiftify VERSION x.y.z)` in `CMakeLists.txt`

```
fix: remove broken effect for textbuttons

textbuttons getting hold-clicked would turn into rectangles, this was removed
```

```
feat: add crossfade playback

crossfade between songs with a configurable length in playback settings, version is 0.2.6
```

## Pull requests

- keep one change per pull request. do NOT submit bulk pull request as this makes reviewing really hard and complex
- say what changed, how you tested it and on which platform
- add a screenshot for ui changes
