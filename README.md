# Jivelite SDL experiments
Repo for POC using SDL2/SDL3

Currently only SDL2

## Requirements
* git
* make
* python3
* gcc
* SDL2
  * SDL2_image
  * SDL2_ttf

optional:
* tslib

## Git
```
git clone https://github.com/blaisedias/jivelite-sdl-experiments.git
cd jivelite-sdl-experiments/
git submodule update --init --recursive
```

## Build
```
    make
```

## Run
```
    ./scripts/runsq.sh
```
help
```
    ./scripts/runsq.sh -h
```

## Test executables
* test_tcache: test the texture cache - provides collision metrics
* test_widgets_json: test and develop json parsing of view widgets layout
* test_touch: tslib touch test program

# piCorePlayer
Scripts in direcrtory pcp support building on piCorePlayer
* download-sdl2.sh : Downloads the packages (tczs) required to build on piCorePlayer
  * packages are only dowloaded but not loaded into memory and are not usable.
  * typically required just once or when updated
* load-sdl2-dev.sh : Loads the packages required to build on piCorePlayer into memory
  * usable after download-sdl2.sh has been invoked at least once
  * this is a subset of the packages loaded by load-sdl2-dev.sh
* load-sdl2-run.sh : Loads the packages required to run on piCorePLayer into memory
  * usable after download-sdl2.sh has been invoked at least once
  * this is a subset of the packages loaded by load-sdl2-dev.sh
  * this set of packages is insufficient to build the executables
