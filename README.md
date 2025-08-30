# ps4fancontrol

Just a simple program to change the fan speed from linux (PS4)

## Changes

To change size, modify the line
```
#define SCALE 1
```
To **2** or **more (no recommended)**

## Build
Download, build, install the XForms Toolkit: http://xforms-toolkit.org/

Type 
```
make
```
That's all

## Usage
You need run ps4fancontrol with root privileges once (to create config and /dev/icc and set permissions for any user)
```
sudo ps4fancontrol
```
After once executed with root privileges, you can run it using
```
ps4fancontrol
```

### It is not necessary to put it at startup (at least in my case) since it saves it and even in OrbisOS (PS4) the fan works the same as in Linux

Select your favorite threshold temperature, save and exit.
The selected temperature will be saved in a config file and loaded when ps4fancontrol starts.
If you want load automatically ps4fancontrol at boot of your distro just put
```
sudo mv service/ps4fancontrol.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable ps4fancontrol
```
Or
```
ps4fancontrol --no-gui
```
in a unit configuration file: https://wiki.archlinux.org/index.php/Systemd#Writing_unit_files or use crontab or similar...

## Kudos
Thanks to Zer0xFF for finding the right icc cmd to change the threshold temperature
and to shuffle2 for the patch to expose the icc to usermode.