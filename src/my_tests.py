#!/usr/bin/python

import sys

from test import spawn_lnxsh, issue, do_exit


def mkfs_tests():
    print '***** Mkfs Tests *****'

    # Should create root directory with '.' and '..'
    issue('mkfs')
    issue('ls')

    # Should format disk, mount root directory, and init fd table
    issue('create f1 10')
    issue('open f2 3')
    issue('mkdir d1')
    issue('cd d1')
    issue('open f3 2')
    issue('link f3 f4')
    issue('create f5 10')
    issue('ls')
    issue('mkfs')
    issue('ls')
    issue('open f6 3')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def open_tests():
    print '***** Open Tests *****'
    issue('mkfs')

    # Try to open new file read-only (should fail)
    issue('open fake 1')

    # Try to open file with bad flags (should fail)
    issue('open a 0')
    issue('open a 4')
    issue('open a -1')
    issue('open a FS_O_RDWR')

    # Create a file and then open it
    issue('create a 0')
    issue('open a 3')

    # Open new file write-only, then read-only
    issue('open b 2')
    issue('open b 1')
    issue('ls')

    # Close and reopen file
    issue('close 0')
    issue('open a 3')

    # Open a directory read-only, but not write-only or read-write
    issue('open . 1')
    issue('open . 2')
    issue('open . 3')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def close_tests():
    print '***** Close Tests *****'
    issue('mkfs')

    # Try to close a non-existent fd (should fail)
    issue('close 0')

    # Try to close bad fd's (should fail)
    issue('close -1')
    issue('close a')
    issue('close 256')

    # Open a file and then close it
    issue('open a 3')
    issue('close 0')
    issue('ls')

    # Open a directory and then close it
    issue('open .. 1')
    issue('close 0')

    # Open two files and then close them
    issue('open a 1')
    issue('open b 2')
    issue('close 0')
    issue('close 1')

    print do_exit()
    print '***********************'
    sys.stdout.flush()
 

def read_tests():
    print '***** Read Tests *****'
    issue('mkfs')
    issue('ls')

    # Try to read from unopened file descriptor (should fail)
    issue('read 0 1')

    # Read 0 bytes from unopened file descriptor
    issue('read 0 0')

    # Try to read from closed file descriptor (should fail)
    issue('create a 32')
    issue('open a 1')
    issue('close 0')
    issue('read 0 1')

    # Read from a directory
    issue('open . 1')
    issue('read 0 10')
    issue('close 0')

    # Read from read-only and read-write files but not write-only
    issue('open a 1')
    issue('read 0 1')
    issue('close 0')
    issue('open a 2')
    issue('read 0 1')
    issue('close 0')
    issue('open a 3')
    issue('read 0 1')
    issue('close 0')

    # Read from two files twice each
    issue('create b 16')
    issue('open a 3')
    issue('open b 3')
    issue('read 0 2')
    issue('read 1 2')
    issue('read 0 4')
    issue('read 1 3')
    issue('close 0')
    issue('close 1')

    # Try to read negative bytes (should fail)
    issue('open a 1')
    issue('read 0 -1')
    issue('read 0 -2')

    # Read whole contents of file
    issue('read 0 32')

    # Try reading past end of file
    issue('read 0 1')
    issue('read 0 10')

    # Read from different points in file
    issue('lseek 0 7')
    issue('read 0 2')
    issue('lseek 0 30')
    issue('read 0 1')
    issue('lseek 0 12')
    issue('read 0 1')
    issue('lseek 0 0')
    issue('read 0 1')
    issue('lseek 0 17')
    issue('read 0 1')
    issue('lseek 0 2')
    issue('read 0 1')

    print do_exit()
    print '***********************'
    sys.stdout.flush()


def write_tests():
    print '***** Write Tests *****'
    issue('mkfs')

    # Try to write to unopened file descriptor (should fail)
    issue('write 0 x')

    # Try to write to a closed file descriptor (should fail)
    issue('create a 32')
    issue('open a 2')
    issue('close 0')
    issue('write 0 x')

    # Write to write-only and read-write files but not read-only
    issue('open a 1')
    issue('write 0 x')
    issue('close 0')
    issue('open a 2')
    issue('write 0 y')
    issue('close 0')
    issue('open a 3')
    issue('write 0 z')
    issue('close 0')

    # Write to two files twice each
    issue('open b1 3')
    issue('open b2 3')
    issue('write 0 hi,')
    issue('write 1 sup,')
    issue('write 0 nmhbu')
    issue('write 1 same')
    issue('cat b1')
    issue('cat b2')
    issue('close 0')
    issue('close 1')

    # Try writing past end of file
    issue('create s 0')
    issue('open s 3')
    issue('lseek 0 1')
    issue('write 0 *')
    issue('lseek 0 3')
    issue('write 0 do_you_wanna_have_a_bad_time?')
    issue('cat s')

    # Write to different points in file
    issue('lseek 0 7')
    issue('write 0 dirty')
    issue('lseek 0 31')
    issue('write 0 bit')
    issue('cat s')

    print do_exit()
    print '***********************'
    sys.stdout.flush()


def mkdir_tests():
    print '***** Mkdir Tests *****'
    issue('mkfs')

    # Try to make meta-directories (should fail)
    issue('mkdir .')
    issue('mkdir ..')

    # Make a new directories
    issue('mkdir d')
    issue('ls')

    # Try to make directory with same name (should fail)
    issue('mkdir d')

    # Make some more directories for lulz
    issue('mkdir ty_bit')
    issue('mkdir ected_edge')
    issue('mkdir ridkm')
    issue('ls')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def rmdir_tests():
    print '***** Rmdir Tests *****'
    issue('mkfs')

    # Try to remove non-existent directory (should fail)
    issue('rmdir fake')

    # Try to remove meta-directories (should fail)
    issue('rmdir .')
    issue('rmdir ..')

    # Make an empty directory and remove it
    issue('mkdir d')
    issue('ls')
    issue('rmdir d')
    issue('ls')

    # Make a non-empty directory and try to remove it (should fail)
    issue('mkdir d')
    issue('cd d')
    issue('create a 0')
    issue ('cd ..')
    issue('ls')
    issue('rmdir d')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def cd_tests():
    print '***** Cd Tests *****'
    issue('mkfs')
    issue('ls')

    # Try to cd to non-existent directory (should fail)
    issue('cd fake')

    # cd to self through meta-directories
    issue('cd .')
    issue('ls')
    issue('cd ..')
    issue('ls')

    # Follow directory a level down, self-link, and back up
    issue('mkdir a')
    issue('cd a')
    issue('ls')
    issue('cd .')
    issue('ls')
    issue('cd ..')
    issue('ls')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def link_tests():
    print '***** Link Tests *****'
    issue('mkfs')
    
    # Try to link non-existent file (should fail)
    issue('link fake fake2')

    # Try to link directory (should fail)
    issue('link . this')

    # Try to link a file to itself (should fail)
    issue('create f 0')
    issue('ls')
    issue('link f f')

    # Try to name link same as existing file (should fail)
    issue('create g 0')
    issue('ls')
    issue('link f g')

    # Create some links
    issue('link f f1')
    issue('link f1 f2')
    issue('link g g0')
    issue('link g gz')
    issue('ls')

    print do_exit()
    print '***********************'
    sys.stdout.flush()


def unlink_tests():
    print '***** Unlink Tests *****'
    issue('mkfs')

    # Try to unlink non-existent file (should fail)
    issue('unlink fake')

    # Try to unlink a directory (should fail)
    issue('unlink .')

    # Create a file and unlink it
    issue('create a 0')
    issue('ls')
    issue('unlink a')
    issue('ls')

    # Create two links and unlink them
    issue('create a 0')
    issue('link a b')
    issue('ls')
    issue('unlink a')
    issue('ls')
    issue('unlink b')
    issue('ls')

    # Don't free data of unlinked file until fds are closed
    issue('create a 5')
    issue('open a 3')
    issue('unlink a')
    issue('read 0 5')
    issue('close 0')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def stat_tests():
    print '***** Stat Tests *****'
    issue('mkfs')

    # Try to stat non-existent directory (should fail)
    issue('stat fake')

    # Stat meta-directories
    issue('stat .')
    issue('stat ..')

    # Create a file and stat it
    issue('create a 10')
    issue('stat a')

    # Create link to existing file and stat both
    issue('link a b')
    issue('stat a')
    issue('stat b')

    # Unlink original and stat again
    issue('unlink a')
    issue('stat b')

    # Create a directory and stat it
    issue('mkdir c')
    issue('stat c')

    # Create a file spanning multiple blocks and stat it
    issue('create d 1024')
    issue('stat d')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def misc_tests():
    print '***** Misc Tests *****'
    issue('mkfs')

    # Create a file with max name length and max size
    issue('create ' + ('.' * 32) + ' 4096')
    issue('ls')
    issue('cat ' + ('.' * 32))

    # Traverse deeply nested directory
    issue('mkfs')
    for i in xrange(256):
        issue('mkdir deeper')
        issue('cd deeper')
    for i in xrange(256):
        issue('cd ..')

    # Thrash on directory entry block boundary
    issue('mkfs')
    issue('create f 0')
    for i in xrange(6):
        issue('link f f' + str(i))
    issue('ls')

    for i in xrange(32):
        issue('unlink f' + str((i * 2) % 6))
        issue('link f f' + str((i * 2) % 6))
    issue('ls')
    
    print do_exit()
    print '***********************'
    sys.stdout.flush()


def main():
    print '============================'
    print ' Running my custom tests... '
    print '============================'
    print

    spawn_lnxsh()
    mkfs_tests()

    spawn_lnxsh()
    open_tests()

    spawn_lnxsh()
    close_tests()

    spawn_lnxsh()
    read_tests()

    spawn_lnxsh()
    write_tests()

    spawn_lnxsh()
    mkdir_tests()

    spawn_lnxsh()
    rmdir_tests()

    spawn_lnxsh()
    cd_tests()

    spawn_lnxsh()
    link_tests()

    spawn_lnxsh()
    unlink_tests()

    spawn_lnxsh()
    stat_tests()

    spawn_lnxsh()
    misc_tests()


if __name__ == '__main__':
    main()
