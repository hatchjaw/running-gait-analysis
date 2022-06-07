# running-gait-analysis
A running gait analysis miniproject for the Embodied Interaction 
course at AAU, with a particular focus on gait asymmetry as
represented by left-right ground contact time balance.

## Structure

`/captures` contains IMU capture data from Delsys sensors in .csv 
format.

`/analysis` contains MATLAB scripts for analysing the data, syncing 
it with video footage, and identifying gait events.

`/sonification` contains a port of the gait event detection system
to the JUCE framework, as an app for playing back the data and
sonifying it in real time. 

### Building the sonification app
Download JUCE, put it in subdirectory `/sonification/JUCE`, comment this
line in CMakeLists.txt:

```cmake
find_package(JUCE CONFIG REQUIRED) # If you've installed JUCE to your system
```
and uncomment this line:

```cmake
add_subdirectory(JUCE) # If you've put JUCE in a subdirectory called JUCE
```

then run:

```shell
cd sonification
cmake -B cmake-build
cmake --build cmake-build
```

or use your fave IDE.

Alternatively, install JUCE to your system and use CMake flag 
`-DCMAKE_PREFIX_PATH=/path/to/juce-install`
as per instructions 
[here](https://forum.juce.com/t/native-built-in-cmake-support-in-juce/38700/13).

### N.B.
The video files aren't held in this repository, but IMU data can be
played back and sonified in the app without the accompanying video.

If you *do* have the associated video files, these should be placed 
in the `/videos` directory.