import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from capture_uart import parse_row, summarize  # noqa: E402


def test_parse_row():
    assert parse_row("1000,24.5,41.0,0,1\n") == (1000, 24.5, 41.0, 0, 1)
    assert parse_row("tick_ms,temperature_c,humidity_pct,alert,latency_ms") is None
    assert parse_row("1,2,3") is None


def test_summarize():
    rows = [(0, 20.0, 40.0, 0, 1), (2000, 31.0, 40.0, 1, 3)]
    s = summarize(rows)
    assert s["samples"] == 2 and s["alerts"] == 1
    assert s["latency_min_ms"] == 1 and s["latency_max_ms"] == 3
    assert s["latency_mean_ms"] == 2
    assert summarize([]) is None
