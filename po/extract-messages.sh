#!/bin/sh
BASEDIR="../"	# root of translatable sources
PROJECT="kup"	# project name
BUGADDR="http://kde-apps.org/content/show.php/Kup+Backup+System?content=147465"	# MSGID-Bugs
WDIR=`pwd`	# working dir
 
 
echo "Preparing rc files"
cd ${BASEDIR}
# we use simple sorting to make sure the lines do not jump around too much from system to system
find . -name 'libgit2*' -prune -o '(' -name '*.rc' -o -name '*.ui' -o -name '*.kcfg' ')' -print | sort > ${WDIR}/rcfiles.list
xargs --arg-file=${WDIR}/rcfiles.list extractrc > ${WDIR}/rc.cpp

#extract strings from .desktop file
intltool-extract --quiet --type=gettext/ini kcm/kcm_kup.desktop.template
cat kcm/kcm_kup.desktop.template.h >> ${WDIR}/rc.cpp
rm kcm/kcm_kup.desktop.template.h
intltool-extract --quiet --type=gettext/ini daemon/kup-daemon.notifyrc.template
cat daemon/kup-daemon.notifyrc.template.h >> ${WDIR}/rc.cpp
rm daemon/kup-daemon.notifyrc.template.h

cd ${WDIR}
echo "Done preparing rc files"
 
 
echo "Extracting messages"
cd ${BASEDIR}
# see above on sorting
find . -name 'libgit2*' -prune -o '(' -name '*.cpp' -o -name '*.h' -o -name '*.c' ')' -print | sort > ${WDIR}/infiles.list
echo "rc.cpp" >> ${WDIR}/infiles.list
cd ${WDIR}
xgettext --from-code=UTF-8 -C -kde -ci18n -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ktr2i18n:1 \
	-kI18N_NOOP:1 -kI18N_NOOP2:1c,2 -kaliasLocale -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
	-kN_:1 --msgid-bugs-address="${BUGADDR}" \
	--files-from=infiles.list -D ${BASEDIR} -D ${WDIR} -o ${PROJECT}.pot || { echo "error while calling xgettext. aborting."; exit 1; }
echo "Done extracting messages"
 
 
echo "Merging translations"
catalogs=`find . -name '*.po'`
for cat in $catalogs; do
  echo $cat
  msgmerge -o $cat.new $cat ${PROJECT}.pot
  mv $cat.new $cat
done
cd ${WDIR}
intltool-merge --quiet --desktop-style ${WDIR} ${BASEDIR}/kcm/kcm_kup.desktop.template ${BASEDIR}/kcm/kcm_kup.desktop
intltool-merge --quiet --desktop-style ${WDIR} ${BASEDIR}/daemon/kup-daemon.notifyrc.template ${BASEDIR}/daemon/kup-daemon.notifyrc
echo "Done merging translations"
 
 
echo "Cleaning up"
cd ${WDIR}
rm rcfiles.list
rm infiles.list
rm rc.cpp
echo "Done"
