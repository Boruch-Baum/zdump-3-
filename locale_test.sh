#!/bin/bash
# locale_test.sh
# Boruch Baum <zdump@gmx.com>
# execute an arbitrary command from a list of locales
# note: the locales must first be defined using localedef(1). This usually requires administrator privileges

# for colorization
#define CODE_BOLD_VIDEO    "%c[1m", 27
#define CODE_REVERSE_VIDEO "%c[7m", 27
#define CODE_RESTORE_VIDEO "%c[m", 27
#define CODE_BLACK         "%c[30m", 27
#define CODE_LIGHT_RED     "%c[31m", 27
#define CODE_LIGHT_GREEN   "%c[32m", 27
#define CODE_LIGHT_BROWN   "%c[33m", 27
#define CODE_DARK_BLUE     "%c[34m", 27
#define CODE_LIGHT_PURPLE  "%c[35m", 27
#define CODE_LIGHT_AQUA    "%c[36m", 27
#define CODE_LIGHT_GREY    "%c[37m", 27
#define CODE_BOLD_GREY     "%c[1;30m", 27
#define CODE_BOLD_RED      "%c[1;31m", 27
#define CODE_BOLD_GREEN    "%c[1;32m", 27
#define CODE_BOLD_YELLOW   "%c[1;33m", 27
#define CODE_BOLD_BLUE     "%c[1;34m", 27
#define CODE_BOLD_PURPLE   "%c[1;35m", 27
#define CODE_BOLD_AQUA     "%c[1;36m", 27
#define CODE_BOLD_WHITE    "%c[1;37m", 27

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

echo -e "\e[33mcommand: $*\n"
i=0
while [[ "${locales[$i]}" != "zz" ]]; do
#  echo -e "\e[48;5;234mcommand: $*, locale: ${locales[$i]} ${locales[$i+1]} ${locales[$i+2]}\e[01;00m"
   echo -en "\e[33mlocale: ${locales[$i]} ${locales[$i+1]} ${locales[$i+2]}: \e[01;00m"
   env LC_ALL=${locales[$i]}.UTF-8 $*
   i=$i+3
   done
