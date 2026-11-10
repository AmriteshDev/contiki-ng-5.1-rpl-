import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "normalattacker_victim"

LOG_FILES = {
    20:  "log20.txt",
    40:  "log40.txt",
    60:  "log60.txt",
    80:  "log80.txt",
    100: "log100.txt"
}

OUTPUT_CSV = f"{SCENARIO_NAME}.csv"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1 / Cooja)
# ==========================================
VOLTAGE    = 3.0
I_CPU      = 10.0
I_LPM      = 0.023
I_TX       = 17.4
I_RX       = 18.8
I_DEEP_LPM = 0.001

ENERGEST_SECOND = 1000000  # Cooja: 1,000,000 ticks = 1 second

# ==========================================
# 3. REGEX PATTERNS
# ==========================================

# ── Energy patterns (same as your format) ──
CPU_PAT      = re.compile(r'ID:(\d+).*?\bCPU\b\s*:\s+(\d+)/')
LPM_PAT      = re.compile(r'ID:(\d+).*?\] LPM\s*:\s+(\d+)/')
DEEP_LPM_PAT = re.compile(r'ID:(\d+).*?\] Deep LPM\s*:\s+(\d+)/')
TX_PAT       = re.compile(r'ID:(\d+).*?Radio Tx\s*:\s+(\d+)/')
RX_PAT       = re.compile(r'ID:(\d+).*?Radio Rx\s*:\s+(\d+)/')

# ── Attacker/Victim patterns ──
# Log line: "ID:20  [INFO: COMPROMISE NODE] Sending request..."
# → [INFO: COMPROMISE NODE] tag wala node = Attacker (Compromise Node)
ATTACKER_PAT = re.compile(r'ID:(\d+).*?\[INFO: COMPROMISE NODE\]')

# Log line: "ID:20   Victim Node (Decimal Node ID: 2)"
# → Decimal Node ID number = Victim node ka ID
# Directly from c code: LOG_INFO_(" Victim Node (Decimal Node ID: %u)\n", parent_ipaddr->u8[15])
VICTIM_PAT   = re.compile(r'Victim Node \(Decimal Node ID:\s*(\d+)\)')

# ── Tx/Rx packet count pattern ──
# Log line: "ID:20  [INFO:...] Tx/Rx/MissedTx: 140/140/0"
TXRX_PAT     = re.compile(r'ID:(\d+).*?Tx/Rx/MissedTx:\s*(\d+)/(\d+)/(\d+)')


def parse_and_compute(filename):
    if not os.path.exists(filename):
        print(f"  [!] Missing file: {filename}")
        return None

    # ── Attacker / Victim trackers ──
    attacker_id = None   # [INFO: COMPROMISE NODE] wala node
    victim_id   = None   # Victim Node (Decimal Node ID) se mila node

    # ── Energy ticks (SUM of all Energest periods) ──
    node_ticks = defaultdict(lambda: {
        'cpu': 0, 'lpm': 0, 'deep_lpm': 0, 'tx': 0, 'rx': 0
    })

    # ── Packet stats (last seen value per node) ──
    last_txrx = {}  # { node_id: {tx_pkt, rx_pkt, missed} }

    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:

            # ── Attacker detection ──────────────────────────────────
            # "ID:20  [INFO: RPL] Targeted Parent(Victim Node): ..."
            # Jo node ye print kare = attacker
            m = ATTACKER_PAT.search(line)
            if m and attacker_id is None:
                attacker_id = int(m.group(1))

            # ── Victim detection ────────────────────────────────────
            # "ID:20   Victim Node (Decimal Node ID: 2)"
            # Decimal mein jo number aata hai = victim node ID
            m = VICTIM_PAT.search(line)
            if m and victim_id is None:
                victim_id = int(m.group(1))

            # ── Tx/Rx packet stats ──────────────────────────────────
            # "ID:20  Tx/Rx/MissedTx: 140/140/0"
            m = TXRX_PAT.search(line)
            if m:
                nid = int(m.group(1))
                last_txrx[nid] = {
                    'tx_pkt' : int(m.group(2)),
                    'rx_pkt' : int(m.group(3)),
                    'missed' : int(m.group(4))
                }

            # ── Energest energy ticks ───────────────────────────────
            # Deep LPM pehle check karo (LPM se overlap avoid karne ke liye)
            m = DEEP_LPM_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['deep_lpm'] += int(m.group(2))
                continue

            m = CPU_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['cpu'] += int(m.group(2))
                continue

            m = LPM_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['lpm'] += int(m.group(2))
                continue

            m = TX_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['tx'] += int(m.group(2))
                continue

            m = RX_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['rx'] += int(m.group(2))
                continue

    # ── Validation ──────────────────────────────────────────────────
    if attacker_id is None or victim_id is None:
        print(f"  [!] Attacker/Victim not found in {filename}")
        return None

    if len(node_ticks) == 0:
        print(f"  [!] No energy data in {filename}")
        return None

    # ── Energy conversion: ticks → mJ ───────────────────────────────
    def node_energy(nid):
        tk = node_ticks.get(nid, {'cpu':0,'lpm':0,'deep_lpm':0,'tx':0,'rx':0})
        cpu_mj      = (tk['cpu']      / ENERGEST_SECOND) * I_CPU      * VOLTAGE
        lpm_mj      = (tk['lpm']      / ENERGEST_SECOND) * I_LPM      * VOLTAGE
        deep_lpm_mj = (tk['deep_lpm'] / ENERGEST_SECOND) * I_DEEP_LPM * VOLTAGE
        tx_mj       = (tk['tx']       / ENERGEST_SECOND) * I_TX       * VOLTAGE
        rx_mj       = (tk['rx']       / ENERGEST_SECOND) * I_RX       * VOLTAGE
        total_mj    = cpu_mj + lpm_mj + deep_lpm_mj + tx_mj + rx_mj
        return {
            'cpu_mj'      : round(cpu_mj,      3),
            'lpm_mj'      : round(lpm_mj,      3),
            'deep_lpm_mj' : round(deep_lpm_mj, 3),
            'tx_mj'       : round(tx_mj,       3),
            'rx_mj'       : round(rx_mj,       3),
            'total_mj'    : round(total_mj,    3)
        }

    attacker_energy = node_energy(attacker_id)
    victim_energy   = node_energy(victim_id)

    attacker_pkt = last_txrx.get(attacker_id, {'tx_pkt':0,'rx_pkt':0,'missed':0})
    victim_pkt   = last_txrx.get(victim_id,   {'tx_pkt':0,'rx_pkt':0,'missed':0})

    return (
        attacker_id, victim_id,
        attacker_energy, attacker_pkt,
        victim_energy,   victim_pkt
    )


def generate_csv():
    print(f"Creating Attacker-Victim Energy CSV for '{SCENARIO_NAME}'...\n")

    with open(OUTPUT_CSV, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)

        writer.writerow([
            "Nodes_Configured",
            "Attacker_NodeID",        "Victim_NodeID",
            # Attacker energy
            "Attacker_CPU(mJ)",       "Attacker_LPM(mJ)",
            "Attacker_Deep_LPM(mJ)",  "Attacker_TX(mJ)",
            "Attacker_RX(mJ)",        "Attacker_Total(mJ)",
            # Attacker packets
            "Attacker_TX_Packets",    "Attacker_RX_Packets",   "Attacker_Missed_TX",
            # Victim energy
            "Victim_CPU(mJ)",         "Victim_LPM(mJ)",
            "Victim_Deep_LPM(mJ)",    "Victim_TX(mJ)",
            "Victim_RX(mJ)",          "Victim_Total(mJ)",
            # Victim packets
            "Victim_TX_Packets",      "Victim_RX_Packets",     "Victim_Missed_TX"
        ])

        for nodes, filename in LOG_FILES.items():
            result = parse_and_compute(filename)
            if result is None:
                continue

            atk_id, vic_id, A, A_pkt, V, V_pkt = result

            writer.writerow([
                nodes,
                atk_id,              vic_id,
                A['cpu_mj'],         A['lpm_mj'],
                A['deep_lpm_mj'],    A['tx_mj'],
                A['rx_mj'],          A['total_mj'],
                A_pkt['tx_pkt'],     A_pkt['rx_pkt'],     A_pkt['missed'],
                V['cpu_mj'],         V['lpm_mj'],
                V['deep_lpm_mj'],    V['tx_mj'],
                V['rx_mj'],          V['total_mj'],
                V_pkt['tx_pkt'],     V_pkt['rx_pkt'],     V_pkt['missed']
            ])

            print(f"  ✅ Nodes = {nodes}")
            print(f"     Attacker (Node {atk_id}): TX={A['tx_mj']} mJ | Total={A['total_mj']} mJ")
            print(f"     Victim   (Node {vic_id}): TX={V['tx_mj']} mJ | Total={V['total_mj']} mJ | Missed_TX={V_pkt['missed']}")
            print()

    print(f"Saved as '{OUTPUT_CSV}'")


if __name__ == "__main__":
    generate_csv()