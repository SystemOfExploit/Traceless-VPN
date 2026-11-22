set -eu
umask 077

root=/opt/amnezia/awg
config="$root/awg0.conf"
profile="$root/traceless-profile"
lock="$root/.traceless-upgrade"
target=12-16

[ -f "$config" ] && [ ! -L "$config" ] && [ ! -L "$profile" ] || exit 1
if ! mkdir "$lock"; then
    exit 1
fi

work=
applied=0
persisted=0
committed=0

cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    rollback_failed=0
    if [ "$committed" = 0 ]; then
        if [ "$persisted" = 1 ]; then
            if ! cmp -s "$config" "$work/expected" || ! mv -f "$work/original" "$config"; then
                rollback_failed=1
            fi
        fi
        if [ "$applied" = 1 ]; then
            if ! awg addconf awg0 "$work/rollback.conf" >/dev/null 2>&1 \
                    || ! awg showconf awg0 > "$work/after" \
                    || [ "$(interface_value "$work/after" MaxHandshakeAttempts)" != "$running" ]; then
                rollback_failed=1
            fi
        fi
    fi
    if [ "$rollback_failed" = 1 ]; then
        printf '%s\n' TP2_ROLLBACK_FAILED
        result=1
    fi
    if [ -n "$work" ]; then
        rm -f "$work/original" "$work/live" "$work/next" "$work/upgrade.conf" "$work/rollback.conf" \
            "$work/profile" "$work/after" "$work/before.state" "$work/after.state" "$work/expected" \
            "$work/before.state.unsorted" "$work/after.state.unsorted"
        rmdir "$work"
    fi
    rmdir "$lock"
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

interface_value() {
    awk -F= -v name="$2" '
        /^\[/ { in_interface = ($0 == "[Interface]") }
        in_interface && $1 ~ ("^[[:space:]]*" name "[[:space:]]*$") {
            value = substr($0, index($0, "=") + 1)
            gsub(/[[:space:]]/, "", value)
            print value
        }' "$1"
}

stable_settings() {
    awk '
        /^\[Interface\]$/ { section = "interface" }
        /^\[Peer\]$/ { section = ""; next }
        /^[[:space:]]*PublicKey[[:space:]]*=/ { section = $0 }
        /^[[:space:]]*(Endpoint|MaxHandshakeAttempts)[[:space:]]*=/ { next }
        NF { print section "|" $0 }
    ' "$1" > "$2.unsorted" || return 1
    LC_ALL=C sort "$2.unsorted" > "$2" || return 1
    rm -f "$2.unsorted"
}

work=$(mktemp -d "$root/.tp2.XXXXXXXX")
cp -p "$config" "$work/original"
awg showconf awg0 > "$work/live"

for field in PrivateKey ListenPort HeaderProtectionKey H1 H2 H3 H4 S1 S2 S3 S4; do
    stored=$(interface_value "$work/original" "$field")
    running=$(interface_value "$work/live" "$field")
    [ -n "$stored" ] && [ "$stored" = "$running" ] || exit 1
done

[ "$(interface_value "$work/original" S4)" = 12 ] || exit 1
[ "$(interface_value "$work/original" ContentPaddingAddition)" = 1-8 ] || exit 1
[ "$(interface_value "$work/live" ContentPaddingAddition)" = 1-8 ] || exit 1
case "$(interface_value "$work/original" DisableCookies)" in ''|off|false|no|0) ;; *) exit 1 ;; esac
case "$(interface_value "$work/live" DisableCookies)" in ''|off|false|no|0) ;; *) exit 1 ;; esac

stored=$(interface_value "$work/original" MaxHandshakeAttempts)
running=$(interface_value "$work/live" MaxHandshakeAttempts)
case "$stored" in 30-40|"$target") ;; *) exit 1 ;; esac
case "$running" in 30-40|"$target") ;; *) exit 1 ;; esac

if [ -f "$profile" ] && [ "$(cat "$profile")" != TP2 ]; then
    exit 1
fi

printf '[Interface]\nMaxHandshakeAttempts = %s\n' "$target" > "$work/upgrade.conf"
printf '[Interface]\nMaxHandshakeAttempts = %s\n' "$running" > "$work/rollback.conf"
printf 'TP2\n' > "$work/profile"

if [ "$stored" != "$target" ]; then
    awk -v target="$target" '
        /^\[/ { in_interface = ($0 == "[Interface]") }
        in_interface && /^[[:space:]]*MaxHandshakeAttempts[[:space:]]*=/ {
            sub(/=[[:space:]]*30-40/, "= " target)
        }
        { print }
    ' "$work/original" > "$work/next"
    [ "$(interface_value "$work/next" MaxHandshakeAttempts)" = "$target" ] || exit 1
fi

if [ "$running" != "$target" ]; then
    applied=1
    awg addconf awg0 "$work/upgrade.conf" >/dev/null 2>&1
fi

awg showconf awg0 > "$work/after"
[ "$(interface_value "$work/after" MaxHandshakeAttempts)" = "$target" ] || exit 1
stable_settings "$work/live" "$work/before.state"
stable_settings "$work/after" "$work/after.state"
cmp -s "$work/before.state" "$work/after.state" || exit 1
cmp -s "$config" "$work/original" || exit 1

if [ "$stored" != "$target" ]; then
    cp -p "$work/next" "$work/expected"
    mv -f "$work/next" "$config"
    persisted=1
fi
mv -f "$work/profile" "$profile"
committed=1
printf '%s\n' TP2_UPGRADED

