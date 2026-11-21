#!/bin/sh
# Keep the preview branch's recovery data and single-instance lock separate.
set -u
app=/app/lib/freeship26
export XDG_CONFIG_HOME="$HOME/.var/app/$FLATPAK_ID/config/update-test"
export XDG_DATA_HOME="$HOME/.var/app/$FLATPAK_ID/data/update-test"
export XDG_CACHE_HOME="$HOME/.var/app/$FLATPAK_ID/cache/update-test"
export XDG_STATE_HOME="$HOME/.var/app/$FLATPAK_ID/.local/state/update-test"
unset WINEPREFIX
export FREESHIP_WINE=/app/wine/bin/wine
"$app/FreeShipQt" "$@"
result=$?
if [ "$result" -eq 75 ]; then
    exec /usr/bin/flatpak-spawn --latest-version /app/bin/freeship26
fi
exit "$result"
