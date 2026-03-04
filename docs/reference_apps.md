## VSYNC Test App
Runs in either primary mode or secondary mode.  primary mode is run on a single PC whereas all other PCs are run as secondary mode with parameters pointing to primary mode system ethernet address.  The communication is done either in ethernet mode or IP address mode.

```console
Usage: ./swgenlock [-m mode] [-i interface] [-c mac_address] [-d delta] [-p pipe] [-P primary_pipe] [-s shift] [-v loglevel] [-h]
Options:
  -m mode           Mode of operation: pri, sec, pipelock (default: pri)
                      pri      - Primary mode (server)
                      sec      - Secondary mode (client)
                      pipelock - Primary + Secondary in same process (no network)
  -i interface      Network interface to listen on (primary) or connect to (secondary) (default: 127.0.0.1)
  -c mac_address    MAC address of the network interface to connect to. Applicable to ethernet interface mode only.
  -d delta          Drift time in microseconds to allow before pll reprogramming (default: 100 us)
  -p pipes          Pipe(s) for secondary. Use 4 for all pipes or comma list like 0,1,2,3 (default: 0)
  -P primary_pipe   Primary pipe for pipelock mode (default: -1)
  -s shift          PLL frequency change fraction as percentage (default: 0.01 = 0.01%)
  -x shift2         PLL frequency change fraction for large drift (default: 0.0; Disabled)
  -f frequency      PLL clock value to set at start (default -> Do Not Set : 0.0)
  -e device         Device string (default: /dev/dri/card0)
  -v loglevel       Log level: error, warning, info, debug or trace (default: info)
  -k time_period    Time period in seconds during which learning rate will be applied.  (default: 240 sec)
  -l learning_rate  Learning rate for convergence. Secondary mode only. e.g 0.00001 (default: 0.0  Disabled)
  -o overshoot_ratio Allow the clock to go beyond zero alignment by a ratio of the delta (value between 0 and 1).
                        For example, with -o 0.5 and delta=500, the target offset becomes -250 us in the apposite direction (default: 0.0)
  -t step_threshold Delta threshold in microseconds to trigger stepping mode (default: 1000 us)
  -w step_wait      Wait in milliseconds between steps (default: 50 ms)
  -H                Enable hardware timestamping (default: disabled)
  -n                Use DP M & N Path. (default: no)
  -h                Display this help message
```

## VBL Test App

Print average vblank period between sync.  This is useful in verification and validation.

```console
[INFO] Vbltest Version: 2.0.0
 Usage: ./vbltest [-p pipe] [-c vsync_count] [-v loglevel] [-h]
 Options:
  -p pipe        Pipe to get stamps for.  0,1,2 ... (default: 0)
  -c vsync_count Number of vsyncs to get timestamp for (default: 300)
  -e device      Device string (default: /dev/dri/card0)
  -l loop        Loop mode: 0 = no loop, 1 = loop (default: 0)
  -H             Enable hardware timestamping (default: disabled)
  -v loglevel    Log level: error, warning, info, debug or trace (default: info)
  -h             Display this help message
```
## SYNC Test App
Used to manually drift vblank clock on a single display by certain period such as 1000 microseconds (or 1.0 ms).

```console
[INFO] Synctest Version: 2.0.0
Usage: ./synctest [-p pipe] [-d delta] [-s shift] [-v loglevel] [-h]
Options:
  -p pipe            Pipe to get stamps for.  0,1,2 ... (default: 0)
  -d delta           Drift time in us to achieve (default: 1000 us) e.g 1000 us = 1.0 ms
  -s shift           PLL frequency change fraction (default: 0.01)
  -x shift2          PLL frequency change fraction for large drift (default: 0.0; Disabled)
  -e device          Device string (default: /dev/dri/card0)
  -f frequency       Clock value to directly set (default -> Do not set : 0.0)
  -v loglevel        Log level: error, warning, info, debug or trace (default: info)
  -t step_threshold  Delta threshold in microseconds to trigger stepping mode (default: 1000 us)
  -w step_wait       Wait in milliseconds between steps (default: 50 ms)
  -R refresh_time    Set PLL frequency to achieve desired interval in micro seconds (default: disabled). e.g 16666.666 us
  -r or --no-reset   Do no reset to original values. Keep modified PLL frequency and exit (default: reset)
  -c or --no-commit  Do no commit changes. Just print (default: commit)
  -H or --hh         Enable hardware timestamping (default: disabled)
  -m or --m          Use DP M & N Path. (default: no)
  -h                 Display this help message
```

**synctest sample output:**

```console
$ ./synctest
[INFO] Synctest Version: 2.0.0
[INFO] DRM Info:
[INFO]   CRTCs found: 4
[INFO]    Pipe:  0, CRTC ID:   80, Mode Valid: Yes, Mode Name: , Position: (   0,    0), Resolution: 1920x1080, Refresh Rate: 60.00 Hz
[INFO]    Pipe:  1, CRTC ID:  131, Mode Valid:  No, Mode Name: , Position: (   0,    0), Resolution:    0x0   , Refresh Rate: 0.00 Hz
[INFO]    Pipe:  2, CRTC ID:  182, Mode Valid:  No, Mode Name: , Position: (   0,    0), Resolution:    0x0   , Refresh Rate: 0.00 Hz
[INFO]    Pipe:  3, CRTC ID:  233, Mode Valid:  No, Mode Name: , Position: (   0,    0), Resolution:    0x0   , Refresh Rate: 0.00 Hz
[INFO]   Connectors found: 6
[INFO]    Connector: 0    (ID: 236 ), Type: 11   (HDMI-A      ), Type ID: 1   , Connection: Disconnected
[INFO]    Connector: 1    (ID: 246 ), Type: 11   (HDMI-A      ), Type ID: 2   , Connection: Connected
[INFO]    Encoder ID: 245, CRTC ID: 80
[INFO]    Connector: 2    (ID: 250 ), Type: 10   (DisplayPort ), Type ID: 1   , Connection: Disconnected
[INFO]    Connector: 3    (ID: 259 ), Type: 11   (HDMI-A      ), Type ID: 3   , Connection: Disconnected
[INFO]    Connector: 4    (ID: 263 ), Type: 10   (DisplayPort ), Type ID: 2   , Connection: Disconnected
[INFO]    Connector: 5    (ID: 272 ), Type: 10   (DisplayPort ), Type ID: 3   , Connection: Disconnected
[INFO] VBlank interval before starting synchronization: 16.667000 ms
[INFO] VBlank interval during synchronization ->
[INFO]   VBlank interval on pipe 0 is 16.6650 ms
[INFO]   VBlank interval on pipe 0 is 16.6650 ms
[INFO]   VBlank interval on pipe 0 is 16.6650 ms
[INFO]   VBlank interval on pipe 0 is 16.6650 ms
[INFO]   VBlank interval on pipe 0 is 16.6640 ms
[INFO]   VBlank interval on pipe 0 is 16.6650 ms
[INFO]   VBlank interval on pipe 0 is 16.6650 ms
[INFO]   VBlank interval on pipe 0 is 16.6660 ms
[INFO] VBlank interval after synchronization ends: 16.667000 ms
```

# Generating Doxygen documents

Please install doxygen and graphviz packages before generating Doxygen documents:
 ```sudo apt install doxygen graphviz```

1. Type ```make doxygen VERSION="1.2.3"``` from the main directory. It will generate SW Genlock doxygen documents
 to output/doxygen folder. Change the version to be that of the release number for the project.
2. Open output/doxygen/html/index.html with a web-browser
