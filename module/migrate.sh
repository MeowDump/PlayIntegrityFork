#!/bin/sh

# This file is primarily adapted from PlayIntegrityFork
# Original repository: https://github.com/osm0sis/PlayIntegrityFork
#
# Minor modifications by: https://github.com/MeowDump
#
# Copyright (C) 2026 Original author of PlayIntegrityFork (https://github.com/osm0sis)
#
# Licensed under GNU GPL‑3.0 ~ see LICENSE file for details.

N="
";

case "$1" in
-h|--help|help) echo "sh migrate.sh [-f] [-o] [-a] [in-file] [out-file]"; exit 0;;
-i|--install|install) INSTALL=1; shift;;
*) echo "pixel.txt migration script \
  $N  by osm0sis @ xda-developers $N";;
esac;

item() { echo "- $@"; }
die() { [ "$INSTALL" ] || echo "$N$N! $@"; exit 1; }

until [ -z "$1" -o -f "$1" ]; do
case "$1" in
  -f|--force|force) FORCE=1; shift;;
  -o|--override|override) OVERRIDE=1; shift;;
  -a|--advanced|advanced) ADVANCED=1; shift;;
  *) die "Invalid argument/file not found: $1";;
esac;
done;

grep_get_prop() {
grep -m1 "$1=" "$2" | cut -d= -f2 | cut -d\# -f1 | sed 's/[[:space:]]*$//';
}
grep_check_prop() {
grep -q "$1" "$2" && [ "$(grep_get_prop "$1" "$2")" ];
}
grep_get_config() {
local target="$FILE";
[ -n "$2" ] && target="$2";
grep_get_prop "$1" "$target";
}
grep_check_config() {
local target="$FILE";
[ -n "$2" ] && target="$2";
grep_check_prop "$1" "$target";
}

if [ -f "$1" ]; then
FILE="$1";
DIR="$1";
else
case "$0" in
  *.sh) DIR="$0";;
  *) DIR="$(lsof -p $$ 2>/dev/null | grep -o '/.*migrate.sh$')";;
esac;
fi;
DIR=$(dirname "$(readlink -f "$DIR")");

if [ -z "$FILE" ]; then
[ -f "$DIR/pixel.txt" ] && FILE="$DIR/pixel.txt";
fi;
[ -f "$FILE" ] || die "No config file found";

OUT="$2";
[ -z "$OUT" ] && OUT="$DIR/pixel.txt";

# Preserve existing date comments from input file
RELEASED_ON=""
EXPIRY_DATE=""
if [ -f "$FILE" ]; then
    RELEASED_ON="$(grep -m1 '^# Released On:' "$FILE" | sed 's/^# Released On: //')"
    EXPIRY_DATE="$(grep -m1 '^# Estimated Expiry:' "$FILE" | sed 's/^# Estimated Expiry: //')"
fi

grep_check_config api_level && [ ! "$FORCE" ] && die "No migration required";

[ "$INSTALL" ] || item " ✦ Parsing fields ...";

FPFIELDS="BRAND PRODUCT DEVICE RELEASE ID INCREMENTAL TYPE TAGS";
ALLFIELDS="MANUFACTURER MODEL FINGERPRINT $FPFIELDS SECURITY_PATCH DEVICE_INITIAL_SDK_INT";

for FIELD in $ALLFIELDS; do
eval $FIELD=\"$(grep_get_config $FIELD)\";
done;

if [ -n "$ID" ] && ! grep_check_config build.id; then
item 'Simple entry ID found, changing to ID field and "*.build.id" property ...';
fi;

if [ -z "$ID" ] && grep_check_config BUILD_ID; then
item 'Deprecated entry BUILD_ID found, changing to ID field and "*.build.id" property ...';
ID="$(grep_get_config BUILD_ID)";
fi;

if [ -n "$SECURITY_PATCH" ] && ! grep_check_config security_patch; then
item 'Simple entry SECURITY_PATCH found, changing to SECURITY_PATCH field and "*.security_patch" property ...';
fi;

if grep_check_config VNDK_VERSION; then
item 'Deprecated entry VNDK_VERSION found, changing to "*.vndk.version" property ...';
VNDK_VERSION="$(grep_get_config VNDK_VERSION)";
fi;

if [ -n "$DEVICE_INITIAL_SDK_INT" ] && ! grep_check_config api_level; then
item 'Simple entry DEVICE_INITIAL_SDK_INT found, changing to DEVICE_INITIAL_SDK_INT field and "*api_level" property ...';
fi;

if [ -z "$DEVICE_INITIAL_SDK_INT" ] && grep_check_config FIRST_API_LEVEL; then
item 'Deprecated entry FIRST_API_LEVEL found, changing to DEVICE_INITIAL_SDK_INT field and "*api_level" property ...';
DEVICE_INITIAL_SDK_INT="$(grep_get_config FIRST_API_LEVEL)";
fi;

if [ -z "$RELEASE" -o -z "$INCREMENTAL" -o -z "$TYPE" -o -z "$TAGS" -o "$OVERRIDE" ]; then
if [ "$OVERRIDE" ]; then
  item " ✦ Overriding values for fields derivable from FINGERPRINT ...";
else
  item " ✦ Missing default fields found, deriving from FINGERPRINT ...";
fi;
IFS='/:' read F1 F2 F3 F4 F5 F6 F7 F8 <<EOF
$(grep_get_config FINGERPRINT)
EOF
i=1;
for FIELD in $FPFIELDS; do
  eval [ -z \"\$$FIELD\" -o \"$OVERRIDE\" ] \&\& $FIELD=\"\$F$i\";
  i=$((i+1));
done;
fi;

if [ -z "$SECURITY_PATCH" -o "$SECURITY_PATCH" = "null" ]; then
item 'Missing SECURITY_PATCH value found, setting default ...';
SECURITY_PATCH="2026-06-01"
fi;

if [ -z "$DEVICE_INITIAL_SDK_INT" -o "$DEVICE_INITIAL_SDK_INT" = "null" ]; then
item 'Missing required DEVICE_INITIAL_SDK_INT field and "*api_level" property value found, setting to 25 ...';
DEVICE_INITIAL_SDK_INT=25;
fi;

keep_advanced() {
for SETTING in $ADVSETTINGS; do
  if grep_check_config "$SETTING" "$1"; then
    item " ✦ Retaining existing configuration ...";
    ADVANCED=1;

    for SETTING in $ADVSETTINGS; do
      eval grep_check_config $SETTING \"$1\" \&\& \
        $SETTING=\"$(grep_get_config $SETTING "$1")\";
    done;
    break;
  fi;
done;
}

ADVSETTINGS="verboseLogs spoofApps spoofBuild spoofProps spoofProvider spoofSignature spoofVendingFinger spoofVendingSdk spoofPixel"

# Default advanced values
verboseLogs=0
spoofApps=0
spoofBuild=1
spoofProps=1
spoofProvider=0
spoofSignature=0
spoofVendingFinger=1
spoofVendingSdk=0
spoofPixel=0

# Check if Google Wallet is installed
if pm list packages | grep -q "com.google.android.apps.walletnfcrel"; then
  item " ✦ com.google.android.apps.walletnfcrel is installed, setting spoofProvider to 0";
  spoofProvider=0
else
  item " ✦ com.google.android.apps.walletnfcrel is not installed, keeping spoofProvider as 1";
fi

PXL_FILE="/data/adb/Box-Brain/pixelify"
ADV_FILE="/data/adb/Box-Brain/advanced"
LEGACY_FILE="/data/adb/Box-Brain/legacy"
WIPE_FILE="/data/adb/Box-Brain/wipe"

# Clear ADVANCED by default
ADVANCED=0

# Handle Wipe first
if [ -f "$WIPE_FILE" ]; then
  item " ✦ Wipe file exists, removing advanced properties ..."
  ADVANCED=0
  for SETTING in $ADVSETTINGS; do
      unset $SETTING
  done
  [ -f "$OUT" ] && mv -f "$OUT" "$OUT.bak"

# Pixelify mode
elif [ -f "$PXL_FILE" ]; then
  item " ✦ Applying pixelify profile values ...."
  verboseLogs=0
  spoofApps=0
  spoofBuild=1
  spoofProps=1
  spoofProvider=0
  spoofSignature=1
  spoofVendingFinger=1
  spoofVendingSdk=0
  spoofPixel=1
  ADVANCED=1

# Legacy mode
elif [ -f "$LEGACY_FILE" ]; then
  item " ✦ Applying legacy profile values ...."
  verboseLogs=0
  spoofApps=0
  spoofBuild=1
  spoofProps=1
  spoofProvider=0
  spoofSignature=0
  spoofVendingFinger=0
  spoofVendingSdk=0
  spoofPixel=0
  ADVANCED=1

# Advanced mode
elif [ -f "$ADV_FILE" ]; then
  item " ✦ Applying supreme profile values ...."
  ADVANCED=1
  keep_advanced "$FILE"

# No control file
else
  item " ✦ No profile detected, using default advanced values ..."
  ADVANCED=1
fi

SECURITY_COMMENT=

if [ ! -f "$WIPE_FILE" ]; then
  if [ -f "$OUT" ]; then
      keep_advanced "$OUT";
      item " ✦ Renaming old file to $(basename "$OUT").bak ...";
      mv -f "$OUT" "$OUT.bak";
  else
      case "$FILE" in
          *.txt) ;;
          *) keep_advanced "$FILE";;
      esac;
  fi
else
  item " ✦ Meta mode: skipping Advanced Settings"
fi

[ "$INSTALL" ] || item " ✦ Writing fields and properties to updated pixel.txt ...";
[ "$ADVANCED" ] && item " ✦ Adding Advanced Settings entries ...";

CMNT='#'
MID='='

{
echo "$CMNT Build Fields"
for FIELD in $ALLFIELDS; do
  eval echo "$FIELD$MID\$$FIELD"
done

echo
echo "$CMNT System Properties"
echo "*.build.id$MID$ID"
echo "*.security_patch$MID$SECURITY_PATCH"
[ -z "$VNDK_VERSION" ] || echo "*.vndk.version$MID$VNDK_VERSION"
echo "*api_level$MID$DEVICE_INITIAL_SDK_INT"

if [ "$ADVANCED" ] && [ -n "$ADVSETTINGS" ]; then
    echo
    echo "$CMNT Advanced Settings"
    for SETTING in $ADVSETTINGS; do
        eval VAL=\$$SETTING
        [ -n "$VAL" ] && echo "$SETTING$MID$VAL"
    done
fi

# Append preserved date comments
if [ -n "$RELEASED_ON" ]; then
    echo
    echo "# Released On: $RELEASED_ON"
fi
if [ -n "$EXPIRY_DATE" ]; then
    echo "# Estimated Expiry: $EXPIRY_DATE"
fi
} > "$OUT"

[ "$INSTALL" ] || cat "$OUT";
