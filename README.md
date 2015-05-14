# Kup Backup System #

Kup is created for helping people to keep up-to-date backups of their personal files. Connecting a USB hard drive is the primary supported way to store files, but saving files to a server over a network connection is also possible for advanced users.

When you plug in your external hard drive Kup will automatically start copying your latest changes, but of course it will only do so if you have been active on your computer for some hourse since the last time you took a backup (and it can of course ask you first, before copying anything). 
In general Kup tries to not disturb you needlessly.

There are two types of backup schemes supported, one which keeps the backup folder completely in sync with what you have on your computer, deleting from the backup any file that you have deleted on your computer etc. The other scheme also keeps older versions of your files in the backup folder. When using this, only the small parts of your files that has actually changed since last backup will be saved and therefore incremental backups are very cheap. This is especially useful if you are working on big files. At the same time it's as easy to access your files as if a complete backup was taken every time; every backup contains a complete version of your directories. Behind the scenes all the content that is actually the same is only stored once. To make this happen Kup runs the backup program "bup" in the background, look at https://github.com/bup/bup for more details.

## What the Kup backup system consists of ##
- Configuration module, available in your system settings. Here you can configure backup plans, what to include, where to store the backup and how often. You can also see the status for the backup plans here.
- A small program running in the background. It will monitor to see when your backup destination is available, schedule and run your backup plans. It has a system tray icon that shows up when a backup destination is available.
- Kioslave for accessing bup archives. This allows you to open files and folders directly from an archive, with any KDE application.
- A file browsing application for bup archives, allowing you to locate the file you want to restore more easily than with the kioslave. It also helps you restore files or folders.

## Detailed list of features##
- backup types:
  - Synchronized folders with the use of "rsync".
  - Incremental backup archive with the use of "bup"
- backup destinations:
  - local filesystem, monitored for availability. That means you can set a destination folder which only exist when perhaps a network shared drive is mounted and Kup will detect when it becomes available.
  - external storage, like usb hard drives. Also monitored for availability.
- schedules:
  - manual only (triggered from tray icon popup menu)
  - interval (suggests new backup after some time has passed since last backup)
  - usage based (suggests new backup after you have been active on your computer for some hours since last backup).

## Needed backup programs ##

To actually create backups of your data you will need either "bup" or "rsync" installed. They
provide the implementations for the two different types of backups that Kup supports.

## Compiling from source ##
To compile you need:
- CMake
- extra-cmake-modules
- The following libraries (including their development headers):
  - qt5-base
  - kcoreaddons
  - kdbusaddons
  - ki18n
  - kio
  - solid
  - kidletime
  - knotifications
  - kiconthemes
  - kconfig
  - kinit

Run from the source directory:
```
mkdir build
cd build
cmake ..
make
sudo make install
```
