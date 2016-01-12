#!/usr/bin/python

import os, sys, subprocess
fs_size_bytes = 1048576

def spawn_lnxsh():
    global p
    p = subprocess.Popen('./lnxsh', shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

def issue(command):
    p.stdin.write(command + '\n')

def check_fs_size():
    fs_size = os.path.getsize('disk')
    if fs_size > fs_size_bytes:
        print "** File System is bigger than it should be (%s) **" %(pretty_size(fs_size))
    else:
        print "Size is fine"

def do_exit():
    issue('exit')
    return p.communicate()[0]

# Verify that mkfs zeroes out previous file system
def mkfs_test():
    print('*****Make File System Test*****')
    issue('mkfs')
    issue('create f1 10')
    issue('create f2 10')
    issue('create f3 10')
    issue('ls')
    print
    issue('mkfs')
    issue('ls')
    print do_exit()
    print('***************')
    sys.stdout.flush()
    
# Make sure simple errors occur
def simple_errors_test() :
    print('*****Simple Errors Test*****')
    issue('mkfs')
    issue('rmdir non_existing') # Problem with removing directory
    issue('cd non_existing') # Problem with changing directory
    issue('link non_existing non_existing_too') # Problem with link
    issue('unlink non_existing') # Problem with unlink
    print do_exit()
    print('***************')
    sys.stdout.flush()

# Open with read only test.
def open_test():
    print('*****Open Test*****')
    issue('mkfs')
    issue('create testf 10')
    issue('open testf 1')
    issue('read 0 2')
    issue('write 0 TEST') #Error while writing file
    issue('close 0')
    issue('cat testf')
    print do_exit()
    print('***************')
    sys.stdout.flush()
 
# Tests the functionality of lseek
def lseek_test():
    print('*****Lseek Test*****')
    issue('mkfs')
    issue('open testf 3')
    issue('write 0 cos318rules')
    issue('lseek 0 3')
    issue('read 0 3') # 318
    issue('read 0 5') # rules
    issue('close 0')
    issue('cat testf')
    print do_exit()
    print('***************')
    sys.stdout.flush()

# Make number of subdirectories with different contents
# and test navigation to those directories.
def mkdir_test():
    print('*****Make Directory Test*****')
    issue('mkfs')
    # Make 10 new directories with varying contents
    for i in range(0, 10):
        issue('mkdir d' + str(i))
        issue('cd d' + str(i))
        # Make subdirectories
        for j in range(0, i):
            issue('mkdir sd' + str(i) + str(j))
        # Add files intermittently
        if(i % 2 == 0):
            issue('create f' + str(i) + ' 10')
        issue('cd ..')
    issue('ls')
    print

    # Navigate to each directory and display contents
    for i in range(0, 10):
        issue('cd d' + str(i))
        issue('ls')
        issue('cd ..')
        print


    print do_exit()
    print('***************')
    sys.stdout.flush()

# Create 9 links to a file for a total of 10
# links and then check the link count.
def link_test():
    print('*****Link Test*****')
    issue('mkfs')
    # Create target file
    issue('create target 10')

    # Create 9 new links
    for i in range(0, 9):
        issue('link target f' + str(i))

    # Use 'ls' to get inode numbers
    issue('ls')
    print
    # Use 'stat' to get link count
    issue('stat target')

    # Verify that inode numbers match and link count
    # of original file is 10
    print do_exit()
    print('***************')
    sys.stdout.flush()

# Create 9 links to a file for a total of 10
# links, then unlink all links and check link count.
def unlink_test():
    print('*****Unlink Test*****')
    issue('mkfs')
    # Create target file
    issue('create target 10')

    # Create 9 new links
    for i in range(0, 9):
        issue('link target f' + str(i))

    # Unlink each of the new links
    for i in range(0, 9):
        issue('unlink f' + str(i))

    # Use 'ls' to contents of directory
    issue('ls')
    print
    # Use 'stat' to get link count
    issue('stat target')

    # Verify that link count is back to 1
    # and links are all gone from directory
    print do_exit()
    print('***************')
    sys.stdout.flush()

# test that interleaved reads and writes on same file but different
# descriptors work
def test_multi_open():
    print('*****Multiple Open File Descriptors Test*****')
    issue('mkfs')
    issue('open f 2')
    issue('open f 1')
    issue('open f 3')
    issue('write 0 hello')
    issue('read 1 5') # hello
    issue('write 2 thereworld')
    issue('read 1 5') # world
    issue('lseek 1 0')
    issue('read 1 5') # there
    print do_exit()
    print('***************')
    sys.stdout.flush()

# make sure we fail when there are no file handles left
def test_get_all_handles():
    print('*****Get All Handles Test*****')
    issue('mkfs')
    issue('mkdir d1')
    issue('mkdir d2')
    issue('mkdir d3')
    issue('mkdir d4')
    issue('mkdir d5')
    issue('cd d1')
    for x in range(0,62):
        issue('open f' + str(x) + ' 3')
    issue('cd ..')
    issue('cd d2')
    for x in range(0,62):
        issue('open f' + str(x) + ' 3')
    issue('cd ..')
    issue('cd d3')        
    for x in range(0,62):
        issue('open f' + str(x) + ' 3')
    issue('cd ..')
    issue('cd d4')        
    for x in range(0,62):
        issue('open f' + str(x) + ' 3')
    issue('cd ..')
    issue('cd d5')        
    for x in range(0,16):
        issue('open f' + str(x) + ' 3')#should fail after file handle 255
    print('***************')
    print do_exit()
    sys.stdout.flush()
    
# make sure we fail when all inodes are used
# get_inodes is a helper function for the function
# test_get_all_inodes
def get_inodes(n):
    if n == 0:
        return
    else:
        for x in range(0,61):
            issue('open f' + str(x) + ' 3') #should fail after numInodes, 3 times
            issue('close 0')
            n -= 1
            if n == 0:
                return
        issue('mkdir d'+str(n))
        issue('cd d'+str(n))
        n -= 1
        if n == 0:
            return
        get_inodes(n)

# Test to make use of all inodes            
def test_get_all_inodes():
    print('*****Get All Inodes Test*****')
    # Change this variable if your file system has fewer than 2048 inodes
    numInodes = 1536
    
    issue('mkfs')
    get_inodes(numInodes + 2)
    print do_exit()
    print('***************')
    sys.stdout.flush()
    
# Test using up all disk blocks.
def all_blocks_test():
    print('*****All Blocks Test*****')
    issue('mkfs')
    
    # Make 3 subdirectories
    issue('mkdir d1')
    issue('mkdir d2')
    issue('mkdir d3')
    issue('mkdir d4')
    
    #Create large files in each directory
    issue('cd d1')
    for i in range(0, 62):
    	issue('create f' + str(i) + ' 4096') # Max file size is 8*512
    issue('cd ..')
    issue('cd d2')
    for i in range(0, 62):
    	issue('create f' + str(i) + ' 4096') # Max file size is 8*512
    issue('cd ..')
    issue('cd d3')
    for i in range(0, 62):
    	# Should fail after 1 MB - size of metadata
    	issue('create f' + str(i) + ' 4096') # Max file size is 8*512
    issue('cd ..')
    issue('cd d4')
    for i in range(0, 16):
        # Should fail after 1 MB - size of metadata
        issue('create f' + str(i) + ' 4096') # Max file size is 8*512
    
    print do_exit();
    print('***************')
    sys.stdout.flush()

if __name__ == '__main__':
    print "......Starting my tests\n\n"
    sys.stdout.flush()
    spawn_lnxsh()
    mkfs_test()
    spawn_lnxsh()
    simple_errors_test()
    spawn_lnxsh()
    open_test()
    spawn_lnxsh()
    lseek_test()
    spawn_lnxsh()
    mkdir_test()
    spawn_lnxsh()
    link_test()
    spawn_lnxsh()
    unlink_test()
    spawn_lnxsh()
    test_multi_open()
    spawn_lnxsh()
    test_get_all_handles()
    spawn_lnxsh()
    test_get_all_inodes()
    spawn_lnxsh()
    all_blocks_test()
    # Verify that file system hasn't grow too large
    check_fs_size()
