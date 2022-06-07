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
to the JUCE framework, an app for playing back the data and
sonifying it in real time.

### N.B.
The video files aren't held in this repository. IMU data can be
played back and sonified in the app with the accompanying video.