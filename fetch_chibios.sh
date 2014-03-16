#!/bin/sh

git clone https://github.com/ChibiOS-Upstream/ChibiOS-RT.git chibios || exit 1
cd chibios || exit 1
git checkout stable_2.6.x
