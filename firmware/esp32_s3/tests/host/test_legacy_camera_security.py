from pathlib import Path
import re
import unittest


REPO_ROOT = Path(__file__).resolve().parents[4]
SKETCH = REPO_ROOT / "firmware/esp32_s3/camera_wifi_ap/camera_wifi_ap.ino"


class LegacyCameraSecurityContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SKETCH.read_text(encoding="utf-8")

    def test_ap_password_must_come_from_local_secrets_header(self):
        self.assertIn('#include "camera_secrets.h"', self.source)
        self.assertIn("#ifndef RAVA_CAMERA_AP_PASSWORD", self.source)
        self.assertIn("#error", self.source)
        self.assertIn("RAVA_CAMERA_AP_PASSWORD", self.source)
        self.assertNotRegex(
            self.source,
            r'AP_PASSWORD\s*=\s*"[^"]{8,}"',
            "A real AP password must never be committed in the sketch.",
        )

    def test_ap_password_has_a_strong_minimum_length(self):
        self.assertRegex(
            self.source,
            r"static_assert\s*\(\s*sizeof\(AP_PASSWORD\)\s*-\s*1\s*>=\s*16",
        )

    def test_ap_password_has_a_maximum_wpa2_length(self):
        self.assertRegex(
            self.source,
            r"static_assert\\s*\\(\\s*sizeof\\(AP_PASSWORD\\)\\s*-\\s*1\\s*<=\\s*63",
        )

    def test_ap_password_is_never_written_to_serial_logs(self):
        self.assertNotRegex(
            self.source,
            r"Serial\.(?:print|printf|println)\s*\([^;]*AP_PASSWORD",
        )

    def test_camera_stream_does_not_enable_wildcard_cors(self):
        self.assertNotIn("Access-Control-Allow-Origin", self.source)
        self.assertNotIn('"*"', self.source)

    def test_access_point_remains_wpa2_protected_and_ap_only(self):
        self.assertIn("WiFi.mode(WIFI_AP)", self.source)
        self.assertRegex(
            self.source,
            r"WiFi\.softAP\s*\(\s*AP_SSID\s*,\s*AP_PASSWORD\s*\)",
        )


if __name__ == "__main__":
    unittest.main()
