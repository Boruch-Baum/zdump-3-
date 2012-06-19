#!/bin/bash
# create_locales.sh
# Boruch Baum <zdump@gmx.com>

# At some point I lost all my locales
# My best guess is that an apt-get update
# updated the locale archive file
#
# If this should hapen again, run this script
# with administrator privileges to recreate
# the listed locale definitions


locales=( \
he_IL Hebrew Israel \
de_DE German Germany \
pt_BR Portugese Brazil \
ru_RU Russian Russia \
ar_MA Arabic Morocco \
es_AR Spanish Argentina \
es_MX Spanish Mexico \
fr_FR French France \
fr_CA French Canada \
hi_IN Hindi India \
fa_IR Farsi Iran \
ja_JP Japanese Japan \
ko_KR Korean Korea     \
hy_AM Armenian Armenia \
ka_GE Georgian Georgia \
am_ET Amharic Ethiopia \
zz
)

echo -e "\e[33mCreating locale definitions. This may take a while..."
i=0
while [[ "${locales[$i]}" != "zz" ]]; do
   echo -en "\e[33mCreating locale for \e[01;00m${locales[$i]} ${locales[$i+1]} ${locales[$i+2]}... "
   localedef -f UTF-8 -i ${locales[$i]} ${locales[$i]}.UTF-8
   echo -e "\e[33mCompleted."
   i=$i+3
   done
