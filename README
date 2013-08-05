HydrixOS Operating System
http://www.hydrixos.org/

1. Preface
==========
This is the CVS release of the hydrixOS source code and documentation.
The documentation is written LyX. You can get it from http://www.lyx.org/.

All sources are published under the GNU General Public License 2.0 (you
will find it at the file "copying") or the GNU Lesser General Public 
License 2.0 (you will find it at the file "copying.library"). The license
used in the different source files is noted at the beginning of it.

HydrixOS is a project in an early alpha development state. We are just
working on the basical system. Currently we have released a microkernel 
and some basical libraries. For more informations, visit our homepage:

http://www.hydrixos.org/

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the source
code licenses for more details.


2. Preparing the build
======================
The build program will copy all binary files to a disk image, so you can
test HydrixOS using BOCHS, QEmu or VMWare.

You will find a compressed disk image at bin/disk144.img.gz. Just decompress
it using

gzip -d disk144.img.gz

It contains a copy of the GRUB bootloader and the file system structure
you need to install HydrixOS on a 1.44 MiB disk with ext2fs as the
file system.

The build program is required to mount the disk image as a virtual
disk. You should have enabled loop-device support in your operating
system. In a linux system you have to add the following line to your 
/etc/fstab file:

/path/to/hydrixos/bin/disk144.img     /path/to/hydrixos/bin/floppy  ext2  loop,noauto,noatime,users,rw  0 0

Everytime you run the build script "makeits" the program will mount the
disk image to "/path/to/hydrixos/bin/floppy" and copy the binary files
of hydrixOS to that directory. After that the script unmounts the directory
again and you can use disk144.img in your favourite emulator.

You can also write it to a disk using:

dd if=disk144.img of=/dev/fd0

If you want to configure GRUB to boot HydrixOS from a ext2 hard disk,
just read chapter 4.


3. Building the sources
=======================
To build the sources we recommend the using of a UNIX system like Linux
or FreeBSD. You need at least GNU gcc 3.4.6, GNU binutils 2.16.1 and GNU
make 3.80.

To delete all object files, just enter

./cleanit

To build the sources, just enter

./makeits

The script will automatically install the script on the disk image 
disk144.img. If you don't have accomplished the instructions on chapter 2,
makeits will stop after building the sources with an error message. If
you want to use your own installation environment, just change the content
of the script "bin/install".


4. Configuring GRUB for booting from hard disk
==============================================
If you have your HydrixOS directory stored at (hd1,0) you just have
to write the following lines into the configuration file of GRUB
"menu.lst":

title=HydrixOS
root (hd1,0)
kernel --type=multiboot (hd1,0)/bin/hymk.bin
module (hd1,0)/bin/hyinit.bin

