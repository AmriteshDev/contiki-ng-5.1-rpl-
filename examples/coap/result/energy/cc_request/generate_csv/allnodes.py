import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "cc_request"  
ALL_NODE_COUNTS = [20, 40, 60, 80, 100]  

# --- DIRECTORY CONFIGURATION ---
BASE_LOG_DIR = "../../../logs/cc_request"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1)
# ==========================================
VOLTAGE = 3.0
I_CPU   = 10.0
I_LPM   = 0.023
I_TX    = 17.4
I_RX    = 18.8
I_DEEP_LPM = 0.001

ENERGEST_SECOND = 1000000

# ==========================================
# 3. REGEX PATTERNS
# ==========================================
CPU_PAT      = re.compile(r'ID:(\d+).*?\bCPU\b\s*:\s+(\d+)/')
LPM_PAT      = re.compile(r'ID:(\d+).*?\] LPM\s*:\s+(\d+)/')
DEEP_LPM_PAT = re.compile(r'ID:(\d+).*?\] Deep LPM\s*:\s+(\d+)/')
TX_PAT       = re.compile(r'ID:(\d+).*?Radio Tx\s*:\s+(\d+)/')
RX_PAT       = re.compile(r'ID:(\d+).*?Radio Rx\s*:\s+(\d+)/')

ATTACKER_PAT = re.compile(r'ID:(\d+).*?\[INFO: COMPROMISE NODE\]')
VICTIM_PAT   = re.compile(r'Victim Node \(Decimal Node ID:\s*(\d+)\)')


def extract_node_data(filename):
    if not os.path.exists(filename):
        print(f"  [!] Missing file: {filename}")
        return None

    node_ticks = defaultdict(lambda: {'cpu': 0, 'lpm': 0, 'deep_lpm': 0, 'tx': 0, 'rx': 0})
    attacker_id = None
    victim_ids  = set() 

    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            m = ATTACKER_PAT.search(line)
            if m and attacker_id is None:
                attacker_id = int(m.group(1))
            
            m = VICTIM_PAT.search(line)
            if m:
                victim_ids.add(int(m.group(1))) 

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

    if len(node_ticks) == 0:
        return None

    results = []
    sorted_nodes = sorted(node_ticks.keys())
    
    for nid in sorted_nodes:
        tk = node_ticks[nid]
        
        # Convert to mJ
        cpu_mj      = (tk['cpu']      / ENERGEST_SECOND) * I_CPU      * VOLTAGE
        lpm_mj      = (tk['lpm']      / ENERGEST_SECOND) * I_LPM      * VOLTAGE
        deep_lpm_mj = (tk['deep_lpm'] / ENERGEST_SECOND) * I_DEEP_LPM * VOLTAGE
        tx_mj       = (tk['tx']       / ENERGEST_SECOND) * I_TX       * VOLTAGE
        rx_mj       = (tk['rx']       / ENERGEST_SECOND) * I_RX       * VOLTAGE
        total_mj    = cpu_mj + lpm_mj + deep_lpm_mj + tx_mj + rx_mj

        row_attacker = nid if nid == attacker_id else "NA"
        row_victim   = nid if nid in victim_ids else "NA"
        
        results.append([
            nid, row_attacker, row_victim,
            round(cpu_mj, 3), round(lpm_mj, 3), round(deep_lpm_mj, 3),
            round(tx_mj, 3), round(rx_mj, 3), round(total_mj, 3)
        ])
        
    return results


def generate_all_detailed_node_csvs():
    print(f"Creating Detailed Node CSVs for Scenario: {SCENARIO_NAME}...\n")
    
    for NODE_COUNT in ALL_NODE_COUNTS:
        output_csv = f"{NODE_COUNT}_nodes_all_runs.csv"
        folder_name = f"log{NODE_COUNT}"
        
        # Mapping exactly to the 3 runs you currently have active
        log_files = {
            1: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_1.txt"),
            2: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_2.txt"),
            3: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_3.txt"),
           
            4: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_4.txt"),
            5: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_5.txt"),
            6: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_6.txt"),
            7: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_7.txt"),
            8: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_8.txt"),
           
            9: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_9.txt"),
            10: os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_10.txt")
        }

        with open(output_csv, 'w', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            
            writer.writerow([
                "Run_Number", "Node_ID", "Attacker_NodeID", "Victim_NodeIDs", 
                "CPU(mJ)", "LPM(mJ)", "Deep_LPM(mJ)", 
                "TX(mJ)", "RX(mJ)", "Total_Energy(mJ)"
            ])

            for run_id, filepath in log_files.items():
                node_results = extract_node_data(filepath)
                
                if node_results:
                    for row in node_results:
                        writer.writerow([run_id] + row)
                    print(f"  -> {filepath} (Processed {len(node_results)} nodes successfully)")
                else:
                    print(f"  -> [!] No valid data for {filepath}")
                    
        print(f"Saved: '{output_csv}'\n")


if __name__ == "__main__":
    generate_all_detailed_node_csvs()