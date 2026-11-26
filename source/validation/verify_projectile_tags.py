"""Verify round identity across native events in a projectile fixture's server logs."""

import argparse
from collections import defaultdict
from pathlib import Path
import re


EVENT = re.compile(r"\[PredateShot\] (fired|effect|damage|stop|object|terrain) (\d+) (\w+) (.*)")
BACKSTOP = "PredateProbe556_Sky"
OPERATOR = "PredateProbe556_Body"
OPEN_AIR = "PredateProbe556_Air"


def verify(lines):
    rounds = defaultdict(list)
    for line in lines:
        match = EVENT.search(line)
        if match:
            event, clock, tag, details = match.groups()
            rounds[tag].append((event, int(clock), details))

    for tag in (BACKSTOP, OPERATOR, OPEN_AIR):
        discharges = [event for event, _, _ in rounds[tag] if event == "fired"]
        assert len(discharges) == 1, f"{tag}: expected one identifiable discharge"
        assert rounds[tag][0][0] == "fired", f"{tag}: an event preceded the discharge"

    assert any(event == "object" and "hit=TreeHard" in detail for event, _, detail in rounds[BACKSTOP]), (
        "The backstop stop event did not carry the discharge tag"
    )
    assert any(event == "effect" and "hit=eAI_" in detail for event, _, detail in rounds[OPERATOR]), (
        "The native collision with an Operator did not carry the discharge tag"
    )
    assert any(event == "damage" for event, _, _ in rounds[OPERATOR]), (
        "The Operator damage callback did not carry the discharge tag"
    )
    air_events = [(kind, clock) for kind, clock, _ in rounds[OPEN_AIR]]
    assert [event for event, _ in air_events] == ["fired", "stop"], (
        "Expected an open-air stop with no contact"
    )
    elapsed_ms = air_events[1][1] - air_events[0][1]
    assert 5500 <= elapsed_ms <= 7000, f"Unexpected 5.56 mm projectile lifetime: {elapsed_ms} ms"
    return elapsed_ms


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server_script_log", type=Path)
    args = parser.parse_args()
    with args.server_script_log.open(encoding="utf-8", errors="replace") as lines:
        elapsed_ms = verify(lines)
    print(f"Three separately tagged rounds verified: backstop, Operator, and open air ({elapsed_ms} ms).")


if __name__ == "__main__":
    main()
