# Kup Backup System #

Kup gives you fast incremental backups and makes it very easy to take them. If too long time has
passed since the last backup Kup will remind you to take a new one, but in general Kup tries to
not disturb you needlessly.

Only the small parts of your files that has actually changed since last backup will be saved
and therefore incremental backups are very cheap. This is especially useful if you are working on
big files.
At the same time it's as easy to access your files as if a complete backup was taken every time.
Every backup contains a complete version of your directories, behind the scenes all the content
that is actually the same is only stored once. To make this happen Kup runs the backup program
"bup" in the background, look at https://github.com/bup/bup for more details.

## What the Kup backup system consists of ##
- Configuration module, available in your system settings. Here you can configure backup plans,
  what to include, where to backup to and how often. You can also see the status for these
  backup plans to monitor if you're running low on disk space, etc.
- A small program running in the background. It will monitor to see when your backup destination
  is available, schedule and run your backup plans. It has a system tray icon that shows up when
  a backup destination is available.

## Current features ##
- backup destinations:
  - local filesystem, monitored for availability. That means you can set a destination folder
    which only exist when perhaps a network shared drive is mounted and Kup will detect when it becomes available.
  - external storage, like usb hard drives. Also monitored for availability.
- schedules:
  - manual only (triggered from tray icon popup menu)
  - interval (suggests new backup after some time has passed since last backup)
  - usage based (suggests new backup after you have been active on your computer for some hours since last backup).
- helping you mount and unmount the backup archive so you can easily access archived files.

## Needed backup programs ##

To actually create backups of your data you will need either "bup" or "rsync" installed. They
provide the implementations for the two different types of backups that Kup supports.

## Compiling from source ##
To compile you need:
- CMake
- Development headers for
  - libqt4
  - libkdeui
  - libkfile
  - libsolid
  - libgit2, version 0.19

Run from the source directory:
```
mkdir build
cd build
cmake ..
make
sudo make install
```
