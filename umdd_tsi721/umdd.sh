#!/bin/bash

conf=/etc/rapidio/umdd.conf

if [ -z "$UMDD_PATH" ]; then
  dir=/opt/rapidio/rapidio_sw/umdd_tsi721
else
  dir="$UMDD_PATH"
fi

if [ -z "$TMP" ]; then
  if [ -d "$HOME/tmp" ]; then
    TMP="$HOME/tmp";
  else
    TMP="/tmp";
  fi
  export TMP;
fi

pushd "$TMP" &>/dev/null || { echo "Cannot chdir to $TMP" 1>&2; exit 1; }
  rm -fr umdd;
  mkdir umdd;
popd &>/dev/null

TDIR="$TMP/umdd";
  
cd $dir || { echo "Cannot chdir to $dir" 1>&2; exit 1; }

[ -d scripts ] || { echo "Path $dir/scripts does not exist." 1>&2; exit 2; }

[ -f "$conf" ] || { echo "Configuration file $conf does not exist/is not accessible." 1>&2; exit 2; }

R=$RANDOM;
RC="$TDIR/umdd.rc.$R"
trap "rm -f $RC" 0 1 2 15 # cleanup

awk -f umddconf.awk $conf > $RC

# SHM is a harsh mistress. Cleanup first.
rm -f /dev/shm/s[eh]m.DMAChannelSHM* &>/dev/null

$dir/umdd 0 --rc "$RC" $@
