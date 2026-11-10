import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION
# ==========================================
LOG_FILE = "log80.txt"   # You can change this to log20.txt, log40.txt, etc.
OUTPUT_CSV = f"Node_Details_{LOG_FILE.replace('.txt', '.csv')}"

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

def generate_node_csv():
    print(f"Reading '{LOG_FILE}' and extracting Energy data for ALL nodes...\n")
    
    if not os.path.exists(LOG_FILE):
        print(f"[!] Error: File '{LOG_FILE}' not found.")
        return

    node_ticks = defaultdict(lambda: {'cpu': 0, 'lpm': 0, 'deep_lpm': 0, 'tx': 0, 'rx': 0})
    attacker_id = None
    victim_id = None

    # ---------------------------------------------------------
    # 1. Parse Data from Log File
    # ---------------------------------------------------------
    with open(LOG_FILE, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            # Detect Attacker
            m = ATTACKER_PAT.search(line)
            if m and attacker_id is None:
                attacker_id = int(m.group(1))
            
            # Detect Victim
            m = VICTIM_PAT.search(line)
            if m and victim_id is None:
                victim_id = int(m.group(1))

            # Accumulate Ticks for ALL nodes
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
        print(f"[!] No energy data found in {LOG_FILE}.")
        return

    # ---------------------------------------------------------
    # 2. Convert Ticks to Energy & Write to CSV
    # ---------------------------------------------------------
    with open(OUTPUT_CSV, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        
        # --- MODIFIED: Write CSV Header with two new columns ---
        writer.writerow([
            "Node_ID", "Attacker_NodeID", "Victim_NodeID", 
            "CPU(mJ)", "LPM(mJ)", "Deep_LPM(mJ)", 
            "TX(mJ)", "RX(mJ)", "Total_Energy(mJ)"
        ])

        # Sort nodes sequentially (1, 2, 3 ... 60)
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

            # # --- MODIFIED: Assign values for the two columns ---
            # row_attacker = nid if nid == attacker_id else "NA"
            # row_victim   = nid if nid == victim_id else "NA"
            
 # --- MODIFIED: Assign values for the two columns ---
            row_attacker =  "NA"
            row_victim   =  "NA"

            # --- MODIFIED: Write row for this specific node ---
            writer.writerow([
                nid, row_attacker, row_victim,
                round(cpu_mj, 3), round(lpm_mj, 3), round(deep_lpm_mj, 3),
                round(tx_mj, 3), round(rx_mj, 3), round(total_mj, 3)
            ])

    # Print summary to terminal
    print(f"✅ CSV successfully saved as '{OUTPUT_CSV}'")
    print(f"   Total Nodes processed: {len(sorted_nodes)}")
    print(f"   Attacker Node: {attacker_id if attacker_id else 'NA'}")
    print(f"   Victim Node: {victim_id if victim_id else 'NA'}\n")

if __name__ == "__main__":
    generate_node_csv()