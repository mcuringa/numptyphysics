#!/bin/sh
CURDIR=$1

if [ x${CURDIR} = x ]; then
  echo CURDIR not set!!
  exit 1
fi

mkdir -p ${CURDIR}/debian/numptyphysics/usr/bin
cp `uname -m`/Game ${CURDIR}/debian/numptyphysics/usr/bin/numptyphysics

mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/numptyphysics
cp *.nph ${CURDIR}/debian/numptyphysics/usr/share/numptyphysics
cp *.png *.jpg ${CURDIR}/debian/numptyphysics/usr/share/numptyphysics

mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/pixmaps
cp debian/numptyphysics64.png ${CURDIR}/debian/numptyphysics/usr/share/pixmaps/numptyphysics.png

if [ -x /usr/bin/hildon-desktop ]; then
  #maemo
  mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/icons/hicolor/scalable/hildon/
  cp debian/numptyphysics64.png ${CURDIR}/debian/numptyphysics/usr/share/icons/hicolor/scalable/hildon/numptyphysics.png
  
  mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/applications/hildon
  cp debian/numptyphysics.desktop ${CURDIR}/debian/numptyphysics/usr/share/applications/hildon

  mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/dbus-1/services
  cp debian/numptyphysics.service ${CURDIR}/debian/numptyphysics/usr/share/dbus-1/services/org.maemo.garage.numptyphysics.service

  mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/mime/packages
  cp debian/numptyphysics-mime.xml ${CURDIR}/debian/numptyphysics/usr/share/mime/packages
else
  #ubuntu
  mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/applications
  cp debian/numptyphysics.desktop ${CURDIR}/debian/numptyphysics/usr/share/applications

  mkdir -p ${CURDIR}/debian/numptyphysics/usr/share/mime/applications
  cp debian/numptyphysics-mime.xml ${CURDIR}/debian/numptyphysics/usr/share/mime/applications/x-numptyphysics-level.xml
fi
