rtftpd
======

A rudimentary non-blocking read-only TFTP server supporting concurrent connections.

## usage:

    $ make
    $ ./tftpd 6969

## notes

It can serve anything that the current user can access and has read permissions for.

## to-do
Support write requests (wrq)

