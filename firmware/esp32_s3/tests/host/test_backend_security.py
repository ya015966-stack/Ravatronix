from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def test_backend_requires_https_and_runtime_bearer_token():
    source = (ROOT / "components/backend_client/backend_client.c").read_text()
    header = (ROOT / "components/backend_client/include/backend_client.h").read_text()
    assert 'strncmp(https_url, "https://", 8)' in source
    assert "esp_crt_bundle_attach" in source
    assert 'set_header(client, "Authorization", auth)' in source
    assert "const char *bearer_token" in header
    assert "Bearer " not in source
