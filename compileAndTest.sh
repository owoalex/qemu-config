#!/bin/bash
if make; then
    build/qemu-config -f "./tmp"
else
    echo "BUILD FAILED"
fi
