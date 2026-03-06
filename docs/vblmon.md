# vblmon - VBlank Monitoring Tool

## Overview

`vblmon` is a monitoring tool that displays average vblank period for verification and validation. It captures vblank timestamps for a specified pipe and calculates the average interval between them, helping users verify synchronization accuracy and validate that displays are operating at expected refresh rates.

## Key Features

- **VBlank Period Measurement**: Calculates average time between vblank events
- **Configurable Sample Size**: Specify number of vblank timestamps to capture
- **Loop Mode**: Continuous monitoring for long-term observation
- **Hardware Timestamping Support**: Compatible with Hammock Harbor mode for nanosecond accuracy
- **Multi-Pipe Support**: Monitor any display pipe (0, 1, 2, 3)
- **Lightweight**: Minimal overhead, designed for verification and validation

## Usage

```console
Usage: ./vblmon [-p pipe] [-c vsync_count] [-v loglevel] [-h]

Options:
  -p pipe        Pipe to get stamps for. 0,1,2 ... (default: 0)
  -c vsync_count Number of vsyncs to get timestamp for (default: 100)
  -e device      Device string (default: /dev/dri/card0)
  -l loop        Loop mode: 0 = no loop, 1 = loop (default: 0)
  -H             Enable hardware timestamping (default: disabled)
  -v loglevel    Log level: error, warning, info, debug or trace (default: info)
  -h             Display this help message
```

## Operating Modes

### Single Measurement (Default)

Capture a specified number of vblank timestamps, calculate the average interval, and exit.

**Basic Example:**
```bash
# Measure average vblank period on pipe 0 (default 300 samples)
./vblmon

# Measure with 100 samples
./vblmon -c 100

# Measure pipe 1 with 500 samples
./vblmon -p 1 -c 50
```

**Output:**
```console
$ ./vblmon -p 1 -c 50
[INFO] Vbltest Version: 4.0.0
[INFO] Pipe: 1, Device: /dev/dri/card0
[INFO][Px][   0.835] VSYNCS
[INFO][Px][   0.835] Received VBlank time stamp [ 0]: 1769161186991006 -> 01/23/26 17:39:46 [+991006 us]
[INFO][Px][   0.835] Received VBlank time stamp [ 1]: 1769161187007679 -> 01/23/26 17:39:47 [+7679   us]
[INFO][Px][   0.835] Received VBlank time stamp [ 2]: 1769161187024352 -> 01/23/26 17:39:47 [+24352  us]
...
...
[INFO][Px][   0.836] Received VBlank time stamp [48]: 1769161187791012 -> 01/23/26 17:39:47 [+791012 us]
[INFO][Px][   0.836] Received VBlank time stamp [49]: 1769161187807682 -> 01/23/26 17:39:47 [+807682 us]
[INFO] Average VBlank interval: 16666.857 microseconds
```

### Loop Mode (Continuous Monitoring)

Continuously monitor vblank intervals for long-term observation and stability testing.

**Example:**
```bash
# Continuous monitoring on pipe 0
./vblmon -l 1

# Continuous monitoring on pipe 1 with 100 samples per iteration
./vblmon -p 1 -l 1 -c 100
```

**Use Cases:**
- Long-term stability verification
- Monitoring drift over time
- Validating synchronization maintenance
- Performance regression testing

**Note:** Press Ctrl+C to stop loop mode.

### Hardware Timestamping Mode

Use hardware registers for nanosecond-level timestamp accuracy.

**Example:**
```bash
# Hardware timestamping on pipe 0
./vblmon -H

# Hardware timestamping with loop mode
./vblmon -H -l 1
```

**Benefits:**
- Nanosecond-level accuracy
- Kernel version independent
- True display scanout boundary measurement
- More precise interval calculations

## Common Use Cases

### 1. Verify Display Refresh Rate

Confirm that displays are running at expected refresh rates.

**60 Hz Display:**
```bash
$ ./vblmon
[INFO] Average VBlank interval: 16.667 ms (60.00 Hz)
```
Expected interval: 16.667 ms (1000/60)

**120 Hz Display:**
```bash
$ ./vblmon -p 1
[INFO] Average VBlank interval: 8.333 ms (120.00 Hz)
```
Expected interval: 8.333 ms (1000/120)

### 2. Validate Synchronization Accuracy

After running swgenlock, verify that displays maintain tight synchronization.

**Before Synchronization:**
```bash
# System 1
$ ./vblmon
[INFO] Average VBlank interval: 16.667 ms

# System 2
$ ./vblmon
[INFO] Average VBlank interval: 16.665 ms  # 2 µs difference
```

**After Synchronization:**
```bash
# Both systems should show nearly identical intervals
$ ./vblmon
[INFO] Average VBlank interval: 16.667 ms
```

### 3. Monitor PLL Drift Over Time

Use loop mode to observe how vblank intervals drift over extended periods.

```bash
# Continuous monitoring to detect drift
./vblmon -l 1 -c 100

# Sample output over time:
# [INFO] Average VBlank interval: 16.667 ms
# ... (1 minute later)
# [INFO] Average VBlank interval: 16.668 ms  # 1 µs drift
# ... (few minutes later)
# [INFO] Average VBlank interval: 16.669 ms  # 2 µs drift
```

### 4. Validate PLL Adjustments

After using pllctl to adjust PLL frequency, verify the resulting interval.

```bash
# Set target refresh interval
$ pllctl -R 16666.666

# Verify achieved interval
$ ./vblmon -c 70
[INFO] Average VBlank interval: 16.667 ms  # Close to target
```

### 5. Compare Software vs Hardware Timestamping

Measure accuracy difference between timestamping modes.

**Software Timestamping:**
```bash
$ ./vblmon -c 90
[INFO] Average VBlank interval: 16.667 ms
```

**Hardware Timestamping:**
```bash
$ ./vblmon -H -c 90
[INFO] Average VBlank interval: 16.667 ms
```

### 6. Integration Testing

Verify vblank behavior after system changes (kernel updates, driver changes, etc.).

```bash
# Create baseline measurement
./vblmon -c 100 > baseline.txt

# After system change, compare
./vblmon -c 100 > after_change.txt
diff baseline.txt after_change.txt
```

## Output Interpretation

### Expected Values

| Refresh Rate | Expected Interval | Tolerance |
|--------------|-------------------|-----------|
| 60 Hz        | 16.667 ms         | ±0.010 ms |
| 59.94 Hz     | 16.683 ms         | ±0.010 ms |
| 120 Hz       | 8.333 ms          | ±0.005 ms |
| 144 Hz       | 6.944 ms          | ±0.005 ms |
| 240 Hz       | 4.167 ms          | ±0.003 ms |

## Installation

```bash
# Set library path (if using dynamic linking)
export LD_LIBRARY_PATH=/path/to/build/lib:$LD_LIBRARY_PATH

# Run from build directory
cd build/vblmon  # or builddir/vblmon for Meson
./vblmon
```

## Tips and Best Practices

### Sample Size Selection
- **High Confidence**: 100 samples
- **Longer = More Accurate**: Larger sample sizes reduce noise

### When to Use Loop Mode

**Use Loop Mode When:**
- Testing long-term stability
- Monitoring drift over hours
- Validating synchronization maintenance
- Performing stress testing

**Use Single Mode When:**
- Quick verification needed
- Automated testing scripts
- Baseline measurements
- Before/after comparisons

### Hardware vs Software Timestamping

**Use Hardware Timestamping (`-H`) When:**
- Maximum accuracy required
- Measuring sub-microsecond intervals
- Comparing kernel versions
- Validating hardware behavior

**Use Software Timestamping (Default) When:**
- Standard verification sufficient
- Hardware mode not supported
- Debugging kernel-level issues
- Matching swgenlock configuration

### Combining with Other Tools

**vblmon + swgenlock:**
```bash
# Terminal 1: Run swgenlock
./swgenlock -m pri -i enp176s0 -p 0

# Terminal 2: Monitor vblank intervals
./vblmon -l 1 -c 100
```

**vblmon + pllctl:**
```bash
# Apply PLL adjustment
./pllctl -R 16666.666

# Verify result
./vblmon -l 1
```

## Scripting and Automation

### Automated Verification Script

```bash
#!/bin/bash
# verify_refresh.sh - Verify all pipes meet expected refresh rate

EXPECTED_INTERVAL=16.667  # 60 Hz
TOLERANCE=0.010

for pipe in 0 1 2 3; do
    echo "Checking pipe $pipe..."
    interval=$(./vblmon -p $pipe -c 100 | grep "Average" | awk '{print $4}')

    if [ -z "$interval" ]; then
        echo "  No active display on pipe $pipe"
        continue
    fi

    diff=$(echo "$interval - $EXPECTED_INTERVAL" | bc -l | tr -d '-')

    if (( $(echo "$diff < $TOLERANCE" | bc -l) )); then
        echo "  ✓ PASS: $interval ms (within tolerance)"
    else
        echo "  ✗ FAIL: $interval ms (expected $EXPECTED_INTERVAL ms)"
    fi
done
```

### Continuous Monitoring Script

```bash
#!/bin/bash
# monitor_drift.sh - Log vblank intervals over time

LOG_FILE="vblank_monitor_$(date +%Y%m%d_%H%M%S).log"
echo "Timestamp,Pipe,Average_Interval_ms,Min_ms,Max_ms,StdDev_ms" > "$LOG_FILE"

while true; do
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    output=$(./vblmon -p 0 -c 100 2>&1)

    avg=$(echo "$output" | grep "Average" | awk '{print $4}')
    min=$(echo "$output" | grep "Min" | awk '{print $3}')
    max=$(echo "$output" | grep "Max" | awk '{print $3}')
    std=$(echo "$output" | grep "deviation" | awk '{print $3}')

    echo "$timestamp,0,$avg,$min,$max,$std" >> "$LOG_FILE"
    sleep 60  # Check every minute
done
```


## Troubleshooting

### No Output or Errors

**Symptoms:** Tool runs but shows no vblank data

**Possible Causes:**
- No active display on specified pipe
- Incorrect pipe number
- DRM device not accessible

**Solutions:**
```bash
# Check active displays
ls -la /sys/class/drm/

# Try different pipe
./vblmon -p 1

# Check device permissions
ls -la /dev/dri/card0

# Run with elevated privileges (if needed)
sudo ./vblmon
```

### Inconsistent Intervals

**Symptoms:** Large standard deviation or varying averages

**Possible Causes:**
- System load affecting timestamps
- Variable refresh rate (VRR) enabled
- Clock drift
- Hardware issues

**Solutions:**
```bash
# Use hardware timestamping for more accuracy
./vblmon -H

# Check for VRR/Adaptive Sync (disable if enabled)

# Reduce system load
```

### Intervals Don't Match Expected Values

**Symptoms:** Average interval significantly different from expected

**Possible Causes:**
- Display running at different refresh rate
- PLL clock drift
- Incorrect display mode

**Solutions:**
```bash
# Verify display mode settings (xrandr, kernel logs)

# Use pllctl to adjust if needed
pllctl -R 16666.666

# Verify again
./vblmon -c 100
```

### Hardware Timestamping Not Working

**Symptoms:** `-H` flag has no effect or shows errors

**Possible Causes:**
- Platform doesn't support hardware timestamping
- Driver version issues
- Feature not enabled

**Solutions:**
```bash
# Fall back to software timestamping
./vblmon  # without -H flag

# Check kernel/driver version compatibility
uname -r
```

## See Also

- [swgenlock](swgenlock.md) - Multi-system synchronization application
- [pllctl](pllctl.md) - PLL frequency control utility
- [Main Documentation](../README.md)
- [System Requirements](requirements.md)
- [Build Instructions](build.md)
