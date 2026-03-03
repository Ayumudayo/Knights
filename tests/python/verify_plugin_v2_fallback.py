import os
import time
import urllib.request


def _env_list(name: str, default_csv: str) -> list[str]:
    raw = os.getenv(name, default_csv)
    items = [part.strip() for part in raw.split(",")]
    return [item for item in items if item]


METRICS_URLS = _env_list(
    "PLUGIN_METRICS_URLS",
    "http://127.0.0.1:39091/metrics,http://127.0.0.1:39092/metrics",
)

WAIT_TIMEOUT_SEC = float(os.getenv("PLUGIN_WAIT_TIMEOUT_SEC", "30"))
POLL_INTERVAL_SEC = float(os.getenv("PLUGIN_POLL_INTERVAL_SEC", "0.5"))

REQUIRED_PLUGIN_LINES = (
    ('file="10_chat_hook_sample.so"', 'version="v2"'),
    ('file="20_chat_hook_tag.so"', 'version="v1"'),
)


def fetch_metrics(url: str) -> str:
    with urllib.request.urlopen(url, timeout=5) as response:
        return response.read().decode("utf-8", errors="replace")


def wait_for_v2_with_v1_fallback() -> None:
    deadline = time.time() + WAIT_TIMEOUT_SEC
    while time.time() < deadline:
        all_ready = True
        for url in METRICS_URLS:
            text = fetch_metrics(url)
            if "chat_hook_plugin_info" not in text:
                raise RuntimeError(f"missing chat_hook_plugin_info in {url}")

            for file_label, version_label in REQUIRED_PLUGIN_LINES:
                if file_label not in text or version_label not in text:
                    all_ready = False
                    break

            if not all_ready:
                break

        if all_ready:
            return

        time.sleep(POLL_INTERVAL_SEC)

    raise RuntimeError("v2 plugin + v1 fallback plugin state did not converge before timeout")


def main() -> int:
    try:
        wait_for_v2_with_v1_fallback()
        print("PASS: v2 loader and v1 fallback reflected in plugin metrics")
        return 0
    except Exception as exc:
        print(f"FAIL: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
