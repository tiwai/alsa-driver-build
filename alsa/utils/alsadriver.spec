%define version  0.2.0-pre5
%define kversion 2.0.34

Summary:   ALSA driver
Name:      alsadriver
Version:   %{version}
Release:   1
Source:    alsadriver-%{version}.tar.gz
URL:       http://alsa.jcu.cz
Copyright: GPL
Group:     Base/Kernel
Requires:  kernel kernel-headers
BuildRoot: /tmp/alsadriver-%{version}-root

%description
Advanced Linux Sound Architecture driver. Driver is fully compatible
with OSS/Lite, but contains many enhanced features.

%prep
%setup -n alsadriver

%build
./configure --prefix=/usr
make

%install
rm -rf $RPM_BUILD_ROOT

for I in etc/rc.d/init.d usr/include/linux lib/modules/%{kversion}/misc; do
  mkdir -p $RPM_BUILD_ROOT/$I
done

make prefix=$RPM_BUILD_ROOT/usr \
     moddir=$RPM_BUILD_ROOT/lib/modules/%{kversion}/misc \
     install

install -m755 utils/alsasound $RPM_BUILD_ROOT/etc/rc.d/init.d/alsasound

%post
if [ -x /sbin/chkconfig ]; then
  chkconfig --add alsasound
fi
depmod -a

%postun
if [ "$1" = 0 ] ; then
  if [ -x /sbin/chkconfig ]; then
    chkconfig --del alsasound
  fi
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%attr(-,root,root) %doc CHANGELOG FAQ INSTALL README TODO snddevices doc/
%attr(-,root,root) %config /etc/rc.d/init.d/*
%attr(-,root,root) /usr/include/linux/*.h
%attr(-,root,root) /lib/modules/%{kversion}/misc/*.o
