/* README.txt
 *
 * Author: Curtis Belmonte
 * Final Project: Unix File System
 * COS 318, Fall 2015, Princeton University
 */

Files modified/added
====================

Source files:
- fs.c
- fs.h
- shell.c

Build files:
- Makefile (added -Werror flag)

My test cases: my_tests.py

Design document: Project6DesignDocument.pdf


Implementation details
======================

Max number of data blocks/i-nodes: 1536

Blocks per i-node: 8

Size of i-node struct: 32 bytes

Size of directory entry: 64 bytes

Max number of open file descriptors: 256

* See design document for more details


Submission status
=================

This submitted program appears to pass all tests provided in test.py, as well
as my own custom tests in my_tests.py. There are no bugs that I am aware of in
the implementation.


Compiling and running
=====================

Builds and runs with the provided Makefile and executable lnxsh. In order to
run tests, make sure my_tests.py is executable, then run `./my_tests.py` or
`python my_tests.py`.
