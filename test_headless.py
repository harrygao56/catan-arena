#!/usr/bin/env python3
"""
Headless mode smoke test.
Plays a scripted first round, then a simple greedy strategy:
- Always roll dice
- Place a settlement if a legal spot exists
- Otherwise end turn
- Reject all trades
- Discard greedily from cheapest resources first
"""

import subprocess
import json
import sys

proc = subprocess.Popen(
    ['./headless_catan'],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL,   # suppress game log (goes to stderr)
    text=True,
    bufsize=1
)

# Scripted first-round placements (matches run_first_round() positions)
# Order: RED→BLUE→YELLOW→YELLOW→BLUE→RED
FIRST_ROUND = [
    ("RED",    "first_round_settlement", {"action":"place_settlement","vertex":8}),
    ("RED",    "first_round_road",       {"action":"place_road","edge":13}),
    ("BLUE",   "first_round_settlement", {"action":"place_settlement","vertex":31}),
    ("BLUE",   "first_round_road",       {"action":"place_road","edge":46}),
    ("YELLOW", "first_round_settlement", {"action":"place_settlement","vertex":14}),
    ("YELLOW", "first_round_road",       {"action":"place_road","edge":15}),
    ("YELLOW", "first_round_settlement", {"action":"place_settlement","vertex":21}),
    ("YELLOW", "first_round_road",       {"action":"place_road","edge":23}),
    ("BLUE",   "first_round_settlement", {"action":"place_settlement","vertex":47}),
    ("BLUE",   "first_round_road",       {"action":"place_road","edge":62}),
    ("RED",    "first_round_settlement", {"action":"place_settlement","vertex":20}),
    ("RED",    "first_round_road",       {"action":"place_road","edge":31}),
]
fr_idx = 0

turn_count = 0
MAX_TURNS  = 1000

def send(msg):
    proc.stdin.write(json.dumps(msg) + '\n')
    proc.stdin.flush()

def log(s):
    print(s, file=sys.stderr)

winner = None

for raw_line in proc.stdout:
    raw_line = raw_line.strip()
    if not raw_line:
        continue

    try:
        msg = json.loads(raw_line)
    except json.JSONDecodeError:
        log(f"[non-json]: {raw_line}")
        continue

    mtype  = msg.get("type", "")
    phase  = msg.get("phase", "")
    player = msg.get("player", "?")

    # ── game over ──────────────────────────────────────────────────────────
    if mtype == "game_over":
        winner = msg.get("winner", "?")
        print(f"\n✅  Game over! Winner: {winner}")
        break

    # ── trade request from another player ─────────────────────────────────
    if mtype == "trade_request":
        log(f"  [trade] {msg['from']} → {player}: rejecting")
        send({"action": "trade_response", "accept": False})
        continue

    # ── action request ────────────────────────────────────────────────────
    if mtype != "action_request":
        continue

    # ── first round settlement / road ─────────────────────────────────────
    if phase in ("first_round_settlement", "first_round_road"):
        if fr_idx < len(FIRST_ROUND):
            exp_player, exp_phase, action = FIRST_ROUND[fr_idx]
            # Verify it's the expected player+phase (sanity check)
            if player == exp_player and phase == exp_phase:
                fr_idx += 1
                log(f"  [first_round] {player} {phase} → {action}")
                send(action)
            else:
                log(f"  [first_round] MISMATCH expected {exp_player}/{exp_phase} got {player}/{phase}")
                # still send it to avoid deadlock
                send(action)
        else:
            log(f"  [first_round] EXTRA prompt {player}/{phase}, using legal[0]")
            legal = msg.get("legal_vertices", msg.get("legal_edges", []))
            if phase == "first_round_settlement" and legal:
                send({"action":"place_settlement","vertex":legal[0]})
            elif legal:
                send({"action":"place_road","edge":legal[0]})
        continue

    # ── discard on 7 ──────────────────────────────────────────────────────
    if phase == "discard":
        must      = msg["must_discard"]
        resources = msg["resources"]
        discard   = {"action":"discard","wood":0,"clay":0,"sheep":0,"wheat":0,"stone":0}
        remaining = must
        for r in ["wood","clay","sheep","wheat","stone"]:
            take = min(resources.get(r, 0), remaining)
            discard[r] = take
            remaining -= take
            if remaining == 0:
                break
        log(f"  [{player}] discard {must}: {discard}")
        send(discard)
        continue

    # ── pre-roll ──────────────────────────────────────────────────────────
    if phase == "pre_roll":
        turn_count += 1
        if turn_count > MAX_TURNS:
            log(f"Reached max turns ({MAX_TURNS}), stopping test")
            break
        vp = {p["color"]: p["vp"] for p in msg["game_state"]["players"]}
        log(f"Turn {turn_count:3d}: [{player}] rolling  VP={vp}")
        send({"action":"roll_dice"})
        continue

    # ── post-roll ─────────────────────────────────────────────────────────
    if phase == "post_roll":
        dice   = msg.get("dice", 0)
        legal  = msg.get("legal_actions", {})
        settlements = legal.get("place_settlement", [])
        roads       = legal.get("place_road", [])
        can_buy     = legal.get("buy_dev_card", False)

        # Get current player's resources from game state
        res = {}
        for p in msg["game_state"]["players"]:
            if p["color"] == player:
                res = p["resources"]
                break

        can_settle = bool(settlements) and res.get("wood",0)>=1 and res.get("clay",0)>=1 and res.get("sheep",0)>=1 and res.get("wheat",0)>=1
        can_road   = bool(roads)       and res.get("wood",0)>=1 and res.get("clay",0)>=1

        if can_settle:
            log(f"         [{player}] dice={dice}  → place_settlement at {settlements[0]}")
            send({"action":"place_settlement","vertex":settlements[0]})
        elif can_road:
            log(f"         [{player}] dice={dice}  → place_road at {roads[0]}")
            send({"action":"place_road","edge":roads[0]})
        elif can_buy:
            log(f"         [{player}] dice={dice}  → buy_dev_card")
            send({"action":"buy_dev_card"})
        else:
            log(f"         [{player}] dice={dice}  → end_turn")
            send({"action":"end_turn"})
        continue

proc.stdin.close()
proc.wait()

if winner:
    print(f"Test passed ✅  — winner: {winner}")
else:
    print("Test ended without game_over ❌ (hit turn limit or crash)")
    sys.exit(1)
