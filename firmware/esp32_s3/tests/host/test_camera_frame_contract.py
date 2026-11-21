from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def test_camera_capture_exposes_jpeg_and_explicit_release():
    header = (ROOT / "components/camera_service/include/camera_service.h").read_text()
    source = (ROOT / "components/camera_service/camera_service.c").read_text()
    assert "camera_service_capture_jpeg" in header
    assert "camera_service_release" in header
    assert "PIXFORMAT_JPEG" in source
    assert "esp_camera_fb_return(fb)" in source
    assert "memset(out, 0, sizeof(*out))" in source
