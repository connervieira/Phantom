# Changelog

This document contains a list of all the changes for each version of Phantom.


## Version 1.0.0

### Initial Release

February 14th, 2023

- Core functionality


## Version 1.1.0

### Consistency Update

April 11th, 2023

- Fixed some broken directory and file references.
- Updated manual page.
- Converted more error messages to JSON format.


## Version 1.2.0

### Webcam Update

August 16th, 2023

- Added support for live webcam ALPR processing.


## Version 1.3.0

### Transparency Update

October 21st, 2023

- When processing live video from a connected webcam, Phantom now saves stills from the stream to /dev/shm/phantom-webcam.jpg so other programs can access it without releasing the capture device.


## Version 1.3.1

September 12th, 2024

- Added support for M4V videos.


## Version 1.3.2

October 9th, 2024

- Added support for TS videos.


## Version 1.4.0

*Release date to be determined*

- Added the "save-frames" option to save each processed frame to `/dev/shm/phantomalpr/` with a unique identifier so external programs can correlate results to the frame they were processed from.
    - Frames older than 10 seconds are automatically deleted.
