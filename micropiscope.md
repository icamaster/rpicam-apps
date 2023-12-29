# Description
The *micropiscope* application is an application derived from 'rpicam-jpeg' meant to be used as a fullscreen microscope application.

At the moment it is very basic, but it allows saving images, safely removing a USB drive and powering off using GPIO inputs (buttons). 

# Installation

## Build

Build and install instructions taken and modified from the [official raspberry pi website](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-libcamera-and-rpicam-apps): 

`sudo apt install -y libcamera-dev libepoxy-dev libjpeg-dev libtiff5-dev`

`sudo apt install -y cmake libboost-program-options-dev libdrm-dev libexif-dev`

`sudo apt install -y meson ninja-build`

`meson setup build -Denable_libav=false -Denable_drm=false -Denable_egl=true -Denable_qt=false -Denable_opencv=false -Denable_tflite=false -Denable_micropiscope=true -Denable_rpicam_apps=false`

`meson compile -C build` # use -j 2 on Raspberry Pi with less than 4GB of memory (or increase swap)

`sudo meson install -C build`

`sudo ldconfig` # this is only necessary on the first build

You should now be able to open the app by running the `micropiscope` command. 

## Autorun

As this app was meant to run at boot as a full screen application with no other input other than GPIO (buttons), the following steps are necessary (TODO - make this a bash script):

* Disable removable media autorun popup by going to `File manager -> Edit -> Preferences -> Volume Management` and unticking 'Show available options...'`

* Autohide the mouse pointer:

    `sudo apt update`

    `sudo apt install unclutter`

    Copy `util/unclutter.desktop` to `/etc/xdg/autostart/unclutter.desktop`

*  Autohide the taskbar ([reference](https://forums.raspberrypi.com/viewtopic.php?t=358654)):

    Append the settings below to `~/.config/wf-panel-pi.ini`
    ```
    autohide=true
    autohide_duration=500
    ```
* Run the `micropiscope` app at boot by copying `util/micropiscope.desktop` into `/etc/xdg/autostart`. Make sure to edit the arguments the window size if necessary. 

## Development 
During development, if using remote-ssh or similar, run the app with the following commands:
```
export DISPLAY=:0
sudo -E micropiscope
```
