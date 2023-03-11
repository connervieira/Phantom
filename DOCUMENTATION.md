# Documentation

## Installing

### Ubuntu

1. Install the required dependencies.
    - `sudo apt-get install libopencv-dev libtesseract-dev git cmake build-essential libleptonica-dev; sudo apt-get install liblog4cplus-dev libcurl3-dev`
2. Prepare the build directory.
    - `cd Phantom/src; mkdir build; cd build`
3. Compile Phantom.
    - `cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_INSTALL_SYSCONFDIR:PATH=/etc ..; make`
4. Install Phantom.
    - `sudo make install`
5. Verify installation.
    - `alpr --version`
