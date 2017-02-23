#!/bin/sh

set -e

mknod -m 666 "${1}/dev/random" c 1 8
mknod -m 666 "${1}/dev/urandom" c 1 9
chown root:root "${1}/dev/random" "${1}/dev/urandom"
