## System Requirements

Before the user begin the installation, ensure the systems meet the following criteria:

The build system assumes the following, prior to executing the build steps:

1) The system has [Ubuntu 22.04 or 24.04](https://ubuntu.com/tutorials/install-ubuntu-desktop#1-overview).  Other Linux flavors should work too.
1) Libraries installed: `sudo apt install -y git build-essential make`
1) [Disable secure boot](https://wiki.ubuntu.com/UEFI/SecureBoot/DKMS)
1) MUST apply the [PLL kernel patch](./resources/shared_dpll-kernel-6.xx.patch) located in resources folder to disable shared PLL optimization.

	**Why This Patch is Required:**
	By default, the Intel display driver includes an optimization feature that shares a single Phase-Locked Loop (PLL) across multiple display pipes when they have identical configurations (same resolution, refresh rate, and color depth). This optimization is automatically applied by the driver and is particularly common when using identical monitors connected via the same type of port (e.g., multiple HDMI or DisplayPort connections with the same display model).

	However, SW Genlock requires independent PLL control for each display pipe to achieve precise, per-pipe vblank synchronization. When pipes share a PLL, adjusting the clock frequency for one pipe affects all pipes sharing that PLL, making individual synchronization impossible. This patch disables the shared PLL optimization, ensuring that each pipe is driven by its own dedicated PLL, which is essential for SW Genlock's synchronization mechanism to function correctly.

	Apply shared_dpll-kernel-x.x.patch according to kernel version.  e.g shared_dpll-kernel-6.10+.patch applicable from 6.10 till 6.13.  6.14 has its own patch in resource directory. Note: This patch applies to the i915 display component, which is shared with the xe driver.

		e.g patch -p1 < <SWGenLockdir>/resources/shared_dpll-kernel-6.10+.patch

   Compile the kernel and update either the entire kernel or just the i915.ko/xe.ko module, depending on the Linux configuration. Note: In some setups, the i915.ko/xe.ko module is integrated into the
   initrd.img, and updating it in /lib/module/... might not reflect the changes.
   - Following the application of the patch, edit the grub settings:
 ```console
 $ sudo vi /etc/grub/default
 # add i915.share_dplls=0 (for i915 driver) or xe.share_dplls=0 (for xe driver) to the GRUB_CMDLINE_LINUX options:
 # GRUB_CMDLINE_LINUX="i915.share_dplls=0"
 # OR for xe driver:
 # GRUB_CMDLINE_LINUX="xe.share_dplls=0"
 # Save and exit, then update grub
 update-grub
 ```

1) Apply **[the monotonic timstamp patch](./resources/0001-Revert-drm-vblank-remove-drm_timestamp_monotonic-par.patch)**
 to Linux kernel drm module which allows it to provide vsync timestamps in real time
 instead of the default monotonic time.<br>
 Note that the monotonic timestamp patch is generated based on Linux v6.4 and has been tested upto v6.15.<br>
 **Recommendation**: For multi-system synchronization in non-Hammock Harbor mode, all participating machines should use the **same kernel version** to ensure consistent timestamp retrieval and avoid timing delay variations between different kernel implementations. These variations are typically only detectable through oscilloscope measurements of the actual display signals; console sync messages will continue to indicate synchronized status. Hammock Harbor mode (`-H` flag) is not affected by kernel version differences.<br>
 Please follow the steps to disable monotonic timestamp after installing the local built Linux image on a target. Compile the kernel and update either the entire kernel or just the drm.ko module based on the installed Linux configuration. <br>
  1. Add ```drm.timestamp_monotonic=0``` option to **GRUB_CMDLINE_LINUX** in /etc/default/grub
  2. Update **GRUB_DEFAULT** in /etc/default/grub to address the local built Linux image
  3. Run ```update-grub``` to apply the change
  4. Reboot a target
1) Turn off NTP time synchronization service by using this command:
 ```timedatectl set-ntp no```
1) All the involved systems should support PTP time synchronization via ethernet (e.g ptp4l, phc2sys)
   To verify if an ethernet interface supports PTP, the `ethtool` linux tool can be used. Run the following command, replacing `eth0` with the relevant interface name:

   ```console
   $ ethtool -T eth0
   ```

   If the interface supports PTP, the output will display specific PTP hardware clock information.
   To ensure proper synchronization, the Ethernet interfaces on the involved systems should be directly connected either via a crossover cable or through a PTP-supported router or switch. If the systems only have a single network interface, it may be necessary to add a separate network adapter, such as a USB-to-Ethernet or USB-to-WiFi dongle, to maintain SSH access and connectivity to external networks.

	Alternatively, `Chrony` can be used for real-time clock synchronization. While it offers easier setup and broader compatibility, its precision is generally lower, and time drift may not be as stable compared to hardware-assisted PTP.

1) Displays must be at same refresh rate (e.g 60Hz)
