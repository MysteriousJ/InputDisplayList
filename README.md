This program displays your inputs as you enter them in a list format. The list can be vertical or horizontal by making the window tall or wide. The background can be transparent or a solid color, and the window border will disappear when it is not focused. A pre-built exe is included in the repo.

# Platforms
Supported by Windows Vista and later.

# Config File
You can customize the program and map your controls by editing config.txt. The order of the settings is important, so don't change the formatting, but you can add and remove buttons to suit your joystick.

You may want to have more than one config file for different games and joysticks. By default, the program will load config.txt at startup, but you can load a specific config file by passing it as a launch option. The easy way to do this is to start the program by clicking and dragging a config file onto the exe's icon.

# Building
Open build.bat in a text editor and set the paths for SDL include and lib directories (The code expects the include path to have the headers in an "SDL" folder). Run build.bat from a Visual Studio command line (search "dev" on the start menu).
