MODPATH="${0%/*}"

# ensure not running in busybox ash standalone shell
if [ -n "$ASH_STANDALONE" ]; then
    set +o standalone
    unset ASH_STANDALONE
fi

sh $MODPATH/osm0sis.sh -m || exit 1

echo -e "\nDone!"

# warn since KernelSU/APatch's implementation automatically closes if successful
if [ "$KSU" = "true" -o "$APATCH" = "true" ] && [ "$KSU_NEXT" != "true" ] && [ "$WKSU" != "true" ] && [ "$MMRL" != "true" ]; then
    echo -e "\nClosing dialog in 7 seconds ..."
    sleep 20
fi
