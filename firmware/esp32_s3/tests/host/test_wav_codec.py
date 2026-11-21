from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / "components/voice_client"
def test_wav_codec_round_trip_and_bounds(tmp_path):
    binary = tmp_path / "test_wav_codec"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined,address",
        str(COMPONENT / "wav_codec.c"), str(COMPONENT / "test_wav_codec.c"),
        "-I", str(COMPONENT / "include"), "-o", str(binary)], check=True)
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
    assert "all assertions passed" in result.stdout
