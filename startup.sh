#!/usr/pkg/bin/bash
cp -rf /usr/src/include /usr/
make includes -C /usr/src/releasetools

make -C /usr/src/lib
make install -C /usr/src/lib

make -C /usr/src/servers
make install -C /usr/src/releasetools

