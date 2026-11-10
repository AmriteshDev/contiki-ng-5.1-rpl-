import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION (Change this for 40, 60, 80, 100)
# ==========================================
SCENARIO_NAME = "cc_request_avg"
ALL_NODE_COUNTS = [20, 40, 60, 80, 100]  # Defines all node cases at once

# Yahan aap apne total runs define kar sakte hain
TOTAL_RUNS = 8

# --- DIRECTORY CONFIGURATION ---
# Since your script is in: .../result/energy/cc_request/generate_csv/
# And logs are in:         .../result/logs/cc_request/
# We go up 3 levels (../../..) to reach the 'result' folder, then into logs.
BASE_LOG_DIR = "../../../logs/cc_request"


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
NODE_INIT_PAT = re.compile(r'ID:(\d+).*?\[INFO: Main\s*\] Starting Contiki')
CPU_PAT      = re.compile(r'ID:(\d+).*?\bCPU\b\s*:\s+(\d+)/')
LPM_PAT      = re.compile(r'ID:(\d+).*?\] LPM\s*:\s+(\d+)/')
DEEP_LPM_PAT = re.compile(r'ID:(\d+).*?\] Deep LPM\s*:\s+(\d+)/')
TX_PAT       = re.compile(r'ID:(\d+).*?Radio Tx\s*:\s+(\d+)/')
RX_PAT       = re.compile(r'ID:(\d+).*?Radio Rx\s*:\s+(\d+)/')

ATTACKER_PAT = re.compile(r'ID:(\d+).*?\[INFO: COMPROMISE NODE\]')
VICTIM_PAT   = re.compile(r'Victim Node \(Decimal Node ID:\s*(\d+)\)')


def parse_and_compute(filename):
    if not os.path.exists(filename):
        print(f"  [!] Missing file: {filename}")
        return None

    node_ticks = defaultdict(lambda: {'cpu': 0, 'lpm': 0, 'deep_lpm': 0, 'tx': 0, 'rx': 0})
    attacker_ids = set()
    victim_ids = set()  
    all_node_ids = set()

    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            # 1. Capture all Node IDs starting up
            m = NODE_INIT_PAT.search(line)
            if m:
                all_node_ids.add(int(m.group(1)))
                continue

            m = ATTACKER_PAT.search(line)
            if m:
                attacker_ids.add(int(m.group(1)))
                continue

            m = VICTIM_PAT.search(line)
            if m :
                victim_ids.add(int(m.group(1))) 
                continue

            m = CPU_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['cpu'] += int(m.group(2))
                continue

            m = DEEP_LPM_PAT.search(line)
            if m:
                node_ticks[int(m.group(1))]['deep_lpm'] += int(m.group(2))
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

    # ==========================================
    # Logic: Total Nodes - Attackers = Valid Nodes
    # ==========================================
    total_nodes_in_log = len(all_node_ids)
    valid_node_ids = all_node_ids - attacker_ids
    valid_nodes_count = len(valid_node_ids)

    if valid_nodes_count == 0:
        return None 

    total_cpu_mj = total_lpm_mj = total_deep_lpm_mj = total_tx_mj = total_rx_mj = total_network_mj = 0.0

    for nid, tk in node_ticks.items():
        if nid in valid_node_ids:
            total_cpu_mj      += (tk['cpu'] / ENERGEST_SECOND) * I_CPU * VOLTAGE
            total_lpm_mj      += (tk['lpm'] / ENERGEST_SECOND) * I_LPM * VOLTAGE
            total_deep_lpm_mj += (tk['deep_lpm'] / ENERGEST_SECOND) * I_DEEP_LPM * VOLTAGE
            total_tx_mj       += (tk['tx']  / ENERGEST_SECOND) * I_TX  * VOLTAGE
            total_rx_mj       += (tk['rx']  / ENERGEST_SECOND) * I_RX  * VOLTAGE
    
    # NEW: Total Energy Calculation (Without dividing by nodes)
    total_network_mj = total_cpu_mj + total_lpm_mj + total_deep_lpm_mj + total_tx_mj + total_rx_mj


    avg_cpu_mj      = total_cpu_mj / valid_nodes_count
    avg_lpm_mj      = total_lpm_mj / valid_nodes_count
    avg_deep_lpm_mj = total_deep_lpm_mj / valid_nodes_count
    avg_tx_mj       = total_tx_mj  / valid_nodes_count
    avg_rx_mj       = total_rx_mj  / valid_nodes_count
    
    avg_network_mj = avg_cpu_mj + avg_lpm_mj + avg_deep_lpm_mj + avg_tx_mj + avg_rx_mj

    attacker_str = ";".join(str(a) for a in sorted(attacker_ids)) if attacker_ids else "NA"
    victim_str = ";".join(str(v) for v in sorted(victim_ids)) if victim_ids else "NA"
    return (total_nodes_in_log, attacker_str, victim_str, avg_cpu_mj, avg_lpm_mj, avg_deep_lpm_mj, 
            avg_tx_mj, avg_rx_mj, avg_network_mj, total_network_mj, total_tx_mj, total_rx_mj, valid_nodes_count)
            

def generate_all_intermediate_csvs():
    print(f"Creating Intermediate CSVs for Scenario: {SCENARIO_NAME}...")
    
    # Loop through all node counts automatically
    for NODE_COUNT in ALL_NODE_COUNTS:
        output_csv = f"{NODE_COUNT}_nodes_energy.csv"
        
       # Dynamically build the path: ../../../logs/cc_request/log20/log20(1).txt
        folder_name = f"log{NODE_COUNT}"

        log_files = {}
        for run_id in range(1, TOTAL_RUNS + 1):
            log_files[run_id] = os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_{run_id}.txt")

        with open(output_csv, 'w', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow([
                "Run_Number", "Nodes_Configured", "Valid_Nodes_Found",
                "Attacker_NodeID", "Victim_NodeID",
                "Avg_CPU(mJ)", "Avg_LPM(mJ)", "Avg_Deep_LPM(mJ)",
                "Avg_TX(mJ)", "Avg_RX(mJ)", "Avg_Network_Energy(mJ)",
                "Total_Network_Energy(mJ)","Total_TX(mJ)", "Total_RX(mJ)"
            ])

            for run_id, filename in log_files.items():
                result = parse_and_compute(filename)
                if result:
                    total_in_log, atk_id, vic_id, a_cpu, a_lpm, a_deep_lpm, a_tx, a_rx, avg_total, total_network, t_tx, t_rx, valid_count = result
                    writer.writerow([
                        run_id, total_in_log, valid_count, atk_id, vic_id,
                        round(a_cpu, 3), round(a_lpm, 3), round(a_deep_lpm, 3),
                        round(a_tx,  3), round(a_rx,  3), round(avg_total, 3), round(total_network, 3),round(t_tx, 3), round(t_rx, 3)
                    ])
                    print(f"  -> {filename} parsed successfully. (Total: {total_in_log} | Valid: {valid_count})")
                else:
                    print(f"  -> [!] No valid data for {filename}.")
        print(f"Saved: '{output_csv}'\n")


if __name__ == "__main__":
    generate_all_intermediate_csvs()