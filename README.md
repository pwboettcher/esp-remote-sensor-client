esp-remote-sensor-client

This repository: https://github.com/pwboettcher/esp-remote-sensor-client

This is the device-side code for a simple ESP8266 remote sensor system.
It currently measures a passive-IR "activity level" and a strain-gauge
level, and uploads the measurements periodically via a JSON message to
a REST endpoint.

The skeleton is present to allow additional sensors, though it all needs
some serious cleanup.

The software is set up to run on a clone of the Wemos D1 mini (I used this
device: https://www.amazon.com/gp/product/B076F53B6S)

CONFIGURING

Create the file `src/networks.h` based on the template at the top of main.c,
and add the names and password of any wifi networks you'd like the device
to connect to.  You'll also need the name of the server that you will be
sending measurements to.  I used [PythonAnywhere](https://pythonanywhere.com)
as the hosting service for the server-side code (see the server-side 
repository at https://github.com/pwboettcher/esp-remote-sensor-server),
so the server string is `username.pythonanywhere.com`.

COMPILING

The client is based on the Arduino framework, compiled and loaded through
the terrific [PlatformIO](https://platformio.org), which downloads all
the necessary compilers and frameworks.  I use
[Microsoft Visual Studio Code](https://code.visualstudio.com) as an IDE,
which will automatically call for PlatformIO extension, install it,
and provide the menu options to build the solution.

Alternatively, you can download the command-line tools for platformio
and run the compile at the command line.

INSTALLING

After plugging the device into the USB port, you may have to adjust the
serial port name in `platformio.ini`.  Then use PlatformIO "Upload" to
upload the code.

CONTRIBUTING

Feel free to contribute patches directly into this repository, or to fork
the repository and do your own thing.  But do let me know... I'd love
to upstream whatever you come up with

