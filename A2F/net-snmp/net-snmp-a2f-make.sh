#!/bin/sh

A2F_APPS8K="snmpget snmpset"
A2F_APPS64K="snmptrap snmptrapd"
A2F_AGENT32K="snmpd"
A2F_CPREFIX=arm-uclinuxeabi
A2F_FLTHDR=$A2F_CPREFIX-flthdr

#configure
CFLAGS="-Os -mcpu=cortex-m3 -mthumb -I${A2F_DESTDIR}/usr/include" \
LDFLAGS="-mcpu=cortex-m3 -mthumb -L${A2F_DESTDIR}/usr/lib" \
./configure --host=arm-uclinux			\
	--build=i386-linux			\
	--with-cc=$A2F_CPREFIX-gcc		\
	--disable-debugging			\
	--enable-mini-agent			\
	--with-transports=UDP			\
	--disable-mib-loading			\
	--disable-manuals			\
	--disable-scripts			\
	--disable-mibs				\
	--with-security-modules=		\
	--disable-des				\
	--disable-privacy			\
	--disable-md5				\
	--with-out-transports="TCP TCPBase"	\
	--with-default-snmp-version=2		\
	--with-sys-contact=admin@no.where	\
	--with-sys-location=Unknown		\
	--prefix=${A2F_DESTDIR}/usr		\
	--with-logfile=/var/log/snmpd.log	\
	--with-persistent-directory=/var/lib/snmp

# make
make

# raise stack of programs
for app in ${A2F_APPS8K}
do
	${A2F_FLTHDR} -s 8192 apps/${app}
done

for app in ${A2F_APPS64K}
do
        ${A2F_FLTHDR} -s 65536 apps/${app}
done

for app in ${A2F_AGENT32K}
do
	${A2F_FLTHDR} -s 32768 agent/${app}
done

# install
make install

# clean
make clean

