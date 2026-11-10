import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "normal_avg"

LOG_FILES = {
    20: "log20_2.txt",
    40: "log40_2.txt",
    60: "log60_2.txt",
    80: "log80_2.txt",
    100: "log100_2.txt"
}


# LOG_FILES = {
#     20:  "logs/normal/log20.txt",
#     40:  "logs/normal/log40.txt",
#     60:  "logs/normal/log60.txt",
#     80:  "logs/normal/log80.txt",
#     100: "logs/normal/log100.txt"
# }


OUTPUT_CSV = f"{SCENARIO_NAME}.csv"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1 / Cooja)
# ==========================================
VOLTAGE = 3.0
I_CPU   = 10.0
I_LPM   = 0.023
I_TX    = 17.4
I_RX    = 18.8
I_DEEP_LPM = 0.001  # Deep LPM (Sleep) current in mA

ENERGEST_SECOND = 1000000  # Cooja: 1,000,000 ticks = 1 second

# ==========================================
# 3. REGEX PATTERNS
# ==========================================
CPU_PAT      = re.compile(r'ID:(\d+).*?\bCPU\b\s*:\s+(\d+)/')
LPM_PAT      = re.compile(r'ID:(\d+).*?\] LPM\s*:\s+(\d+)/')
DEEP_LPM_PAT = re.compile(r'ID:(\d+).*?\] Deep LPM\s*:\s+(\d+)/')
TX_PAT       = re.compile(r'ID:(\d+).*?Radio Tx\s*:\s+(\d+)/')
RX_PAT       = re.compile(r'ID:(\d+).*?Radio Rx\s*:\s+(\d+)/')

# Note: Attacker and Victim regex patterns are removed for the normal scenario.

def parse_and_compute(filename):
    if not os.path.exists(filename):
        print(f"  [!] Missing file: {filename}")
        return None

    node_ticks = defaultdict(lambda: {'cpu': 0, 'lpm': 0, 'deep_lpm': 0, 'tx': 0, 'rx': 0})
    
    # --- Hardcoded to NA because all nodes act normally in this scenario ---
    attacker_id = "NA"
    victim_id = "NA"

    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            
            # CPU line
            m = CPU_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['cpu'] += int(m.group(2))
                continue

            # Deep LPM line (Hamesha pehle Deep LPM check karein)
            m = DEEP_LPM_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['deep_lpm'] += int(m.group(2))
                continue    

            # Normal LPM line
            m = LPM_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['lpm'] += int(m.group(2))
                continue

            # Radio Tx line
            m = TX_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['tx'] += int(m.group(2))
                continue

            # Radio Rx line
            m = RX_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['rx'] += int(m.group(2))
                continue

    valid_nodes = len(node_ticks)
    if valid_nodes == 0:
        return None

    # -------------------------------------------------------
    # Energy conversion: ticks → mJ
    # -------------------------------------------------------
    total_cpu_mj = total_lpm_mj = total_deep_lpm_mj = total_tx_mj = total_rx_mj = 0.0

    for nid, tk in node_ticks.items():
        total_cpu_mj      += (tk['cpu'] / ENERGEST_SECOND) * I_CPU * VOLTAGE
        total_lpm_mj      += (tk['lpm'] / ENERGEST_SECOND) * I_LPM * VOLTAGE
        total_deep_lpm_mj += (tk['deep_lpm'] / ENERGEST_SECOND) * I_DEEP_LPM * VOLTAGE
        total_tx_mj       += (tk['tx']  / ENERGEST_SECOND) * I_TX  * VOLTAGE
        total_rx_mj       += (tk['rx']  / ENERGEST_SECOND) * I_RX  * VOLTAGE

    # Average across all nodes
    avg_cpu_mj      = total_cpu_mj / valid_nodes
    avg_lpm_mj      = total_lpm_mj / valid_nodes
    avg_deep_lpm_mj = total_deep_lpm_mj / valid_nodes
    avg_tx_mj       = total_tx_mj  / valid_nodes
    avg_rx_mj       = total_rx_mj  / valid_nodes
    
    avg_network_mj = avg_cpu_mj + avg_lpm_mj + avg_deep_lpm_mj + avg_tx_mj + avg_rx_mj

    # Returning attacker_id and victim_id as NA
    return (attacker_id, victim_id, avg_cpu_mj, avg_lpm_mj, avg_deep_lpm_mj, avg_tx_mj, avg_rx_mj, avg_network_mj, valid_nodes)


def generate_csv():
    print(f"Creating Average Energy CSV for '{SCENARIO_NAME}'...")
    with open(OUTPUT_CSV, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        
        # CSV header matches the previous structure
        writer.writerow([
            "Nodes_Configured", "Valid_Nodes_Found",
            "Attacker_NodeID", "Victim_NodeID",
            "Avg_CPU(mJ)", "Avg_LPM(mJ)", "Avg_Deep_LPM(mJ)",
            "Avg_TX(mJ)", "Avg_RX(mJ)",
            "Avg_Network_Energy(mJ)"
        ])

        for nodes, filename in LOG_FILES.items():
            result = parse_and_compute(filename)
            if result:
                # Unpacking variables
                atk_id, vic_id, a_cpu, a_lpm, a_deep_lpm, a_tx, a_rx, avg_total, valid_count = result
                writer.writerow([
                    nodes, valid_count,
                    atk_id, vic_id,
                    round(a_cpu, 3), round(a_lpm, 3), round(a_deep_lpm, 3),
                    round(a_tx,  3), round(a_rx,  3),
                    round(avg_total, 3)
                ])
                print(f"  -> Nodes: {nodes} | Found: {valid_count} | Attacker: {atk_id} | Victim: {vic_id} | "
                      f"Avg TX: {round(a_tx, 2)} mJ | "
                      f"Avg Total: {round(avg_total, 2)} mJ")
            else:
                print(f"  -> No valid data for {nodes} nodes.")

    print(f"\nSaved as '{OUTPUT_CSV}'")


if __name__ == "__main__":
    generate_csv()