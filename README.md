# Emu68-tools: Utilities for your m68k Raspberry Pi

Welcome to my #Emu68 companion project. The intention behind Emu68-tools is to separate the software written for the AArch32/AArch64 host and software and tools written for the m68k side of Emu68. The tools include resources, libraries and (in future) small applications which will allow to monitor or control behaviour of Emu68. At the moment of writing this README file, two subproject are defined.

## devicetree.resource

The idea behind Emu68 is to create a bare metal low level virtual m68k machine which is not so strictly bound to the underlying hardware. As simple as it sounds, such approach has several drawbacks. The software running on m68k has no knowledge about memory layout, peripherals available to the system or even the hardware it is running on. To overcome this limitation Emu68-tools brings a special resource for handling the device tree. The tree itself is a well established standard for exposing hardware to the operating system, born in the deep era of OpenFirmware. The tree has a structure of a file system with devices represented by nodes, and features of devices represented by node properties. The devicetree.resource is embedded in a ROM image of a virtual Zorro3 card. It allows programmer to query for the nodes, iterate through nodes, search them and query their properties.

Detailed description of the resource will be given in corresponding AutoDoc file.

## brcm-sdhc.device

This is a device driver for SD Host Controller compatible with the official SDHC specification. Due to some internal quirks it is targeted at the Arasan controllers embedded in the Broadcom CPUs present in RaspberryPi machines. The device driver is embedded in ROM image of a virtual Zorro3 card. Once initialised, it reads the geometry of SD card, initialises physical and virtual units and subsequently adds corresponding Boot Nodes in order to make the Amiga partitions on SD card bootable.

Detailed information on preparing SD card for use with this device will follow shortly.