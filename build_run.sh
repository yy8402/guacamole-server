#!/bin/bash -e 

autoreconf -fi 
XORG_MODULE_DIR=/usr/lib/xorg/modules ./configure \
       --with-xorg-client \
       --with-vnc=no \
       --with-rdp=no \
       --with-ssh=no \
       --with-telnet=no \
       --with-kubernetes=no \
       --disable-dependency-tracking 
make 
make install 
ldconfig

cp xorg.conf.sample /etc/guacamole/xorg.conf
echo "Build and installation of guacd with Xorg support completed."


# guacd -f -L debug -b 0.0.0.0 -p 4822
cp guacd.conf.sample /etc/guacamole/guacd.conf
guacd -f -L debug -b

echo "guacd with Xorg support started."