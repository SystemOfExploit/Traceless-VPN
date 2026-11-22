from pathlib import Path
import argparse
import os
import subprocess
import tempfile
import unittest

MIGRATION = Path(__file__).resolve().parents[1] / "server_scripts/awg/upgrade_tp2.sh"

INTERFACE = """[Interface]
PrivateKey = server-private
ListenPort = 51820
S1 = 12
S2 = 12
S3 = 12
S4 = 12
H1 = 100-199
H2 = 200-299
H3 = 300-399
H4 = 400-499
HeaderProtectionKey = header-secret
ContentPaddingAddition = 1-8
MaxHandshakeAttempts = 30-40
"""
FIRST_PEER = """[Peer]
PublicKey = alice
PresharedKey = alice-secret
AllowedIPs = 10.0.0.2/32
"""
SECOND_PEER = """[Peer]
PublicKey = bob
PresharedKey = bob-secret
AllowedIPs = 10.0.0.3/32
"""
THIRD_PEER = """[Peer]
PublicKey = charlie
PresharedKey = charlie-secret
AllowedIPs = 10.0.0.4/32
"""

AWG_STUB = r"""
awg() {
    case "$1" in
        showconf) cat "$root/live" ;;
        addconf)
            next=$(awk -F ' = ' '/^MaxHandshakeAttempts/ { print $2 }' "$3")
            if [ "$next" = 30-40 ] && [ "$MODE" = rollback-fails ]; then
                return 1
            fi
            if [ "$next" = 12-16 ] && [ "$MODE" = concurrent ]; then
                cat "$root/new-peer" >> "$root/awg0.conf"
            fi
            awk -v next_value="$next" '
                /^MaxHandshakeAttempts/ { $0 = "MaxHandshakeAttempts = " next_value }
                { print }
            ' "$root/live" > "$root/live.next"
            mv -f "$root/live.next" "$root/live"
            if [ "$next" = 12-16 ]; then
                case "$MODE" in
                    apply-fails|rollback-fails) return 1 ;;
                    peer-order) cp "$root/reordered" "$root/live" ;;
                esac
            fi
            ;;
        *) return 1 ;;
    esac
}
"""

class UpgradeTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.original = INTERFACE + FIRST_PEER + SECOND_PEER
        self.config = self.root / "awg0.conf"
        self.profile = self.root / "traceless-profile"
        self.live = self.root / "live"
        self.config.write_text(self.original, newline="\n")
        self.live.write_text(self.original, newline="\n")
        (self.root / "new-peer").write_text(THIRD_PEER, newline="\n")
        reordered = INTERFACE.replace("30-40", "12-16") + SECOND_PEER + FIRST_PEER
        (self.root / "reordered").write_text(reordered, newline="\n")
        path = self.root.as_posix().replace("'", "'\"'\"'")
        script = MIGRATION.read_text().replace("root=/opt/amnezia/awg", "root='" + path + "'", 1)
        self.script = self.root / "upgrade.sh"
        self.script.write_text(AWG_STUB + script, newline="\n")

    def run_upgrade(self, mode="success"):
        return subprocess.run(SHELL + [self.script.as_posix()], check=False, text=True, capture_output=True,
                              timeout=15, env={**os.environ, "MODE": mode})

    def test_upgrade_preserves_every_peer_and_is_idempotent(self):
        expected = self.original.replace("30-40", "12-16")
        for _ in range(2):
            result = self.run_upgrade()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stdout.strip(), "TP2_UPGRADED")
            self.assertEqual(self.config.read_text(), expected)
            self.assertEqual(self.live.read_text(), expected)
            self.assertEqual(self.profile.read_text().strip(), "TP2")

    def test_peer_order_does_not_affect_validation(self):
        result = self.run_upgrade("peer-order")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("TP2_UPGRADED", result.stdout)

    def test_partial_apply_is_rolled_back(self):
        result = self.run_upgrade("apply-fails")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config.read_text(), self.original)
        self.assertEqual(self.live.read_text(), self.original)
        self.assertFalse(self.profile.exists())

    def test_rollback_failure_is_reported(self):
        result = self.run_upgrade("rollback-fails")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("TP2_ROLLBACK_FAILED", result.stdout)
        self.assertEqual(self.config.read_text(), self.original)
        self.assertFalse(self.profile.exists())

    def test_concurrent_peer_append_is_preserved(self):
        result = self.run_upgrade("concurrent")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config.read_text(), self.original + THIRD_PEER)
        self.assertEqual(self.live.read_text(), self.original)
        self.assertFalse(self.profile.exists())

    def test_unrecognized_wire_profile_is_refused(self):
        unknown = self.original.replace("S4 = 12", "S4 = 16")
        self.config.write_text(unknown, newline="\n")
        self.live.write_text(unknown, newline="\n")
        result = self.run_upgrade()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config.read_text(), unknown)
        self.assertEqual(self.live.read_text(), unknown)
        self.assertFalse(self.profile.exists())

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--shell", default="sh")
    parser.add_argument("--shell-arg", action="append", default=[])
    args = parser.parse_args()
    SHELL = [args.shell, *args.shell_arg]
    unittest.main(argv=[__file__], verbosity=2)

