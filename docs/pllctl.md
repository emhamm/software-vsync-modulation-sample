# pllctl - PLL Frequency Control Utility

## Overview

`pllctl` is a utility for manual PLL (Phase-Locked Loop) frequency adjustment and drift testing on a single system. It allows direct control over display refresh timing by manipulating PLL clock values, introducing controlled drift for testing synchronization methodology, and validating the effectiveness of SW Genlock's synchronization approach.

## Key Features

- **Direct PLL Clock Control**: Set absolute PLL frequency values
- **Controlled Drift Testing**: Introduce specific microsecond-level drift for validation
- **Refresh Interval Adjustment**: Set desired refresh interval in microseconds
- **Stepped Frequency Adjustments**: Prevent display blanking during large frequency changes
- **Hardware Timestamping Support**: Compatible with Hammock Harbor mode
- **Preview Mode**: Calculate and display changes without applying them
- **Persistent Mode**: Keep modified PLL values after exit for debugging

## Usage

```console
Usage: ./pllctl [-p pipe] [-d delta] [-s shift] [-v loglevel] [-h]

Options:
  -p pipe            Pipe to get stamps for. 0,1,2 ... (default: 0)
  -d delta           Drift time in us to achieve (default: 1000 us) e.g 1000 us = 1.0 ms
  -s shift           PLL frequency change fraction (default: 0.01)
  -x shift2          PLL frequency change fraction for large drift (default: 0.0; Disabled)
  -e device          Device string (default: /dev/dri/card0)
  -f frequency       Clock value to directly set (default -> Do not set : 0.0)
  -v loglevel        Log level: error, warning, info, debug or trace (default: info)
  -t step_threshold  Delta threshold in microseconds to trigger stepping mode (default: 1000 us)
  -w step_wait       Wait in milliseconds between steps (default: 50 ms)
  -R refresh_time    Set PLL frequency to achieve desired interval in micro seconds (default: disabled). e.g 16666.666 us
  -r or --no-reset   Do not reset to original values. Keep modified PLL frequency and exit (default: reset)
  -c or --no-commit  Do not commit changes. Just print (default: commit)
  -H or --hh         Enable hardware timestamping (default: disabled)
  -m or --m          Use DP M & N Path (default: no)
  -h                 Display this help message
```

## Operating Modes

### 1. Controlled Drift Testing (Default)

Introduce a specific amount of drift to test synchronization methodology. By default, pllctl applies a 1000 µs (1 ms) drift.

**Basic Example:**
```bash
./pllctl
```

**Custom Drift:**
```bash
# Introduce 500 µs drift on pipe 0
./pllctl -p 0 -d 500

# Introduce 2000 µs (2 ms) drift on pipe 1
./pllctl -p 1 -d 2000
```

**How It Works:**
1. Reads current PLL clock value and vblank interval
2. Calculates required PLL frequency change to achieve desired drift
3. Applies stepped adjustments if drift exceeds threshold (-x param)
4. Maintains drift for observation period
5. Reverts to original PLL values on exit (unless `--noeset` used)

### 2. Direct PLL Clock Setting

Set an absolute PLL frequency value directly. Useful when you know the exact clock value needed.

**Note:** The frequency is converted to register values using fixed-point arithmetic. The final value written in the form of a fraction may not regenerate the exact requested frequency but will be very close to it due to hardware precision limitations.

**Example:**
```bash
# Set PLL clock to specific value
./pllctl -f 8100.532

# Set for pipe 1
./pllctl -p 1 -f 8100.532
```

**Use Cases:**
- Apply known-good PLL values from previous runs
- Initialize system with optimal frequency
- Skip learning phase in multi-system setups

User can also update pll clock in user program via vsync lib function call.  The **set_pll_clock** API, allowing for precise and direct control over PLL clock frequencies. To handle large frequency changes gracefully, the implementation incorporates an stepped adjustment mechanism. When a significant difference exists between the current and desired PLL clock frequencies, the function automatically calculates the optimal number of steps and adjusts the frequency incrementally based on given shift factor. This ensures stability and reliable locking without blank screens, even during substantial frequency transitions. The synctest application has been updated to demonstrate the usage of this new set_pll_clock API, providing a practical reference implementation for developers. For more details refer to the set_pll_clock api documentation.

**Get Current PLL Value:**
```bash
# Run without -f to see current PLL value in output
./pllctl -c  # Preview mode shows current value
```

### 3. Refresh Interval Adjustment

Set the desired refresh interval in microseconds. pllctl calculates and applies the appropriate PLL frequency to achieve the target interval.

**Example:**
```bash
# Set to 60 Hz (16666.666 µs interval)
./pllctl -R 16666.666

# Set to 59.94 Hz
./pllctl -R 16683.333

# Set to 120 Hz
./pllctl -R 8333.333
```

**Notes:**
- Achieved interval will be very close but may not be exact
- Run multiple times for finer precision
- Useful for aligning different PLL clock values before synchronization

This option is useful in cases where there is a noticeable deviation between different PLL clock values and the user wants to bring them closer before starting normal operations. While the adjustment may not yield the exact requested value, the achieved refresh interval will be very close to the target. For finer alignment, the command can be run multiple times until the desired precision is reached.

### 4. Preview Mode (No Commit)

Calculate and display PLL changes without applying them to registers.

**Example:**
```bash
./pllctl -c -d 1000
./pllctl --no-commit -f 8100.532
```

**Use Cases:**
- Preview changes before applying
- Verify calculations
- Check current PLL values and vblank intervals
- Dry-run testing

### 5. Persistent Mode (No Reset)

Keep modified PLL frequency values after exit without reverting to original values.

**Example:**
```bash
./pllctl -r -d 500
./pllctl --no-reset -f 8100.532
```

**Use Cases:**
- Debugging and troubleshooting
- Maintain specific frequency for extended testing
- Apply permanent frequency adjustments

**Important:** You must manually reset PLL values later or reboot the system.

## Advanced Features

### Step-Based Frequency Adjustment

When large frequency changes are required, pllctl automatically applies them in smaller steps to prevent display blanking.

**Configure Stepping:**
```bash
# Use 0.01% shift for fine adjustments, 0.1% for large drift
./pllctl -d 2000 -s 0.01 -x 0.1 -t 1000 -w 50
```

**Parameters:**
- `-s 0.01`: Fine-grained shift (0.01% PLL frequency change)
- `-x 0.1`: Large shift for rapid recovery when drift exceeds threshold
- `-t 1000`: Trigger large shift when drift exceeds 1000 µs
- `-w 50`: Wait 50 ms between steps for frequency to settle

**How It Works:**
1. If drift < threshold: Use fine shift (`-s`)
2. If drift ≥ threshold: Split large shift (`-x`) into multiple fine shifts
3. Apply each step with wait period for stability
4. Prevents display blanking from abrupt frequency changes

### Hardware Timestamping

Use hardware registers for nanosecond-level timestamp accuracy.

**Example:**
```bash
./pllctl -H -d 1000
./pllctl --hh -f 8100.532
```

**Benefits:**
- Nanosecond-level accuracy
- Kernel version independent
- True display scanout boundary measurement

### DP M & N Path

Use DisplayPort M & N path for frequency control (platform-specific).

**Example:**
```bash
./pllctl -m -d 1000
./pllctl --m -R 16666.666
```

## Sample Output

```console
$ ./pllctl
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

## Common Use Cases

### 1. Test Synchronization Methodology

Verify that SW Genlock can correct controlled drift:

```bash
# On System 1: Introduce 500 µs drift
./pllctl -r -d 500

# On System 2: Run swgenlock to sync with System 1
# Observe how long it takes to achieve synchronization
```

### 2. Initialize PLL for Multi-System Setup

Use logged PLL frequency from previous successful sync to skip learning phase:

```bash
# Set known-good PLL value on secondary system
./pllctl -r -f 8100.532

# Then run swgenlock without learning mode
./swgenlock -m sec -i enp176s0 -c MAC_ADDR -p 0 -d 100
```

### 3. Align Different PLL Clocks

When PLL values differ significantly between systems:

```bash
# Bring secondary closer to primary's refresh rate
./pllctl -R 16666.666  # Target 60 Hz

# Run multiple times for finer alignment
./pllctl -R 16666.666
```

### 4. Debug PLL Behavior

Preview calculations without affecting display:

```bash
# See what would happen with 1000 µs drift
./pllctl -c -d 1000

# Check current PLL value
./pllctl -c
```

### 5. Validate Stepped Adjustments

Test stepping mechanism with various parameters:

```bash
# Small shift with quick steps
./pllctl -d 2000 -s 0.005 -x 0.05 -t 500 -w 25

# Larger shift with longer settling time
./pllctl -d 5000 -s 0.01 -x 0.1 -t 1000 -w 100
```

## Installation

```bash
# Set library path (if using dynamic linking)
export LD_LIBRARY_PATH=/path/to/build/lib:$LD_LIBRARY_PATH

# Run from build directory
cd build/pllctl  # or builddir/pllctl for Meson
./pllctl
```

## Tips and Best Practices

### Choosing Shift Values

- **Fine Shift (`-s`)**: 0.005 - 0.01 for gradual, stable adjustments
- **Large Shift (`-x`)**: 0.05 - 0.1 for rapid recovery from large drift
- Smaller values = more stable but slower convergence
- Larger values = faster but risk display instability

### Step Threshold and Wait Time

- **Threshold (`-t`)**: Set based on acceptable drift (500-2000 µs typical)
- **Wait (`-w`)**: 25-100 ms for frequency to settle between steps
- Very small shifts with long wait times may cause minor overshoot

### Persistent Changes

When using `--no-reset`:
- Document the applied PLL value for reference
- Remember to reset manually or reboot when done
- Use for debugging, not for regular testing

### Preview Before Apply

Always use `--no-commit` first when:
- Trying new parameter combinations
- Working with critical displays
- Validating calculations

## Troubleshooting

### Display Goes Blank

**Cause:** Large frequency change applied too quickly

**Solution:**
```bash
# Use stepped adjustments
./pllctl -d 2000 -s 0.01 -x 0.05 -t 1000 -w 100
```

### Drift Not Achieved

**Cause:** Shift value too small or PLL limits reached

**Solution:**
```bash
# Increase shift value
./pllctl -d 1000 -s 0.02

# Try larger shift2
./pllctl -d 1000 -s 0.01 -x 0.1
```

### Refresh Interval Not Exact

**Cause:** PLL granularity limitations

**Solution:**
```bash
# Run multiple times to refine
./pllctl -R 16666.666
./pllctl -R 16666.666
```

### Changes Don't Persist

**Cause:** Default behavior resets on exit

**Solution:**
```bash
# Use --no-reset flag
./pllctl -r -f 8100.532
```

## See Also

- [swgenlock](swgenlock.md) - Multi-system synchronization application
- [vblmon](vblmon.md) - VBlank monitoring tool
- [Main Documentation](../README.md)
- [System Requirements](requirements.md)
- [Build Instructions](build.md)
