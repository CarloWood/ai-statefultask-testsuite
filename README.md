### Prerequisites ###

    sudo apt install libboost-dev libfarmhash-dev libsparsehash-dev doxygen graphviz

    export GITACHE_ROOT=/opt/gitache_root

Where `/opt/gitache_root` is some (empty) directory that you
have write access to. It will be used to cache packages so you
don't have to recompile them (it is a 'cache', but you might
want to keep it around).

### Downloading ###

    git clone --recursive https://github.com/CarloWood/ai-statefultask-testsuite.git

### Configuration ###

    cd ai-statefultask-testsuite
    mkdir build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebug -DEnableDebugGlobal:BOOL=OFF
    
Optionally add `-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON` to see the compile commands.
Other build types are: `Release`, `RelWithDebInfo`, `Debug` and `BetaTest`.
See [stackoverflow](https://stackoverflow.com/a/59314670/1487069) for an explanation
of the different types.

### Building ###

    NUMBER_OF_CORES=8
    cmake --build build --config RelWithDebug --parallel ${NUMBER_OF_CORES}
