#!/usr/pkg/bin/bash
make -C /usr/src/servers/tst/
cp /usr/src/servers/tst/tst /usr/sbin/
service down tst
service up /usr/sbin/tst -label tst

make -C /usr/src/servers/obs
cp /usr/src/servers/obs/obs /usr/sbin/
service down obs
service up /usr/sbin/obs -label obs