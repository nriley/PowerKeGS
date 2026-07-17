# PowerKeGS

Software power control for the Apple IIGS through the Sophisticated Circuits PowerKey

**What is it?** On an Apple IIGS plugged into a Sophisticated Circuits PowerKey (not PowerKey Pro) with PKGS.Init installed, PowerKeGS tells the PowerKey to power off when you shut down from GS/OS.  (More information about the PowerKey is at [this 68KMLA thread]([url](https://68kmla.org/bb/threads/reverse-engineer-the-powerkey-by-sophisticated-circuits.40286/))).

**How do I install it?** Drag PKGS.Init into the System folder of your startup disk with System 6.0.1 or later. Let the Finder put it where it belongs. Then restart your IIGS.

<img width="1554" height="692" alt="PowerKeGS disk contents" src="https://github.com/user-attachments/assets/41ca6226-2c50-4f75-8eb2-bc470e5d4e05" />

**How do I use it?** Shut down your IIGS from GS/OS, such as by selecting Shut Down from the Finder's Special menu. Your IIGS should turn off if it's plugged into the PowerKey. You can use the power/Reset key on your ADB keyboard to turn it back on.

PowerKeGS switches to 80-column text mode. If it experiences a problem, it displays it and you can press the space bar to see "You may now switch off your Apple IIGS safely", then turn off or restart your IIGS.

**How do I uninstall it?** Open the System folder on your startup disk, and the System.Setup folder within it. Drag PKGS.Init to the Trash. Then restart your IIGS.

**Doesn't the PowerKey have more features?** Yes, it includes power on and off timers that can support scheduled power on/off as in the Mac PowerKey software. Support for these may be added in future.

**What if I don't have a PowerKey?** Try out ADB Explorer, which lists ADB devices connected to your IIgs. It's in a separate disk image.

<img width="2562" height="1825" alt="ADB Explorer" src="https://github.com/user-attachments/assets/362b943f-c922-4a54-9f63-88a921f42087" />

**How do I build this?** PowerKeGS and ADB Explorer are written in ORCA/C and ORCA/M,
 built with Jeremy Rand's excellent [Apple2GSBuildPipeline](https://github.com/jeremysrand/Apple2GSBuildPipeline). You don't need a Mac/Xcode to build but you do need Golden Gate and the ORCA tools installed; links to all of these are available [here](https://github.com/jeremysrand/Apple2GSBuildPipeline#apple2gsbuildpipeline).
