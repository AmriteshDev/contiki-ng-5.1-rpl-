import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION
# ==========================================
LOG_FILE = "log40.txt"   # You can change this to log20.txt, log40.txt, etc.
OUTPUT_CSV = f"Time_Series_Attacker_Victim_{LOG_FILE.replace('.txt', '.csv')}"

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
PERIOD_PAT   = re.compile(r'ID:(\d+).*?--- Period summary #(\d+)')
STAT_PAT     = re.compile(r'ID:(\d+).*?(CPU|LPM|Deep LPM|Radio Tx|Radio Rx)\s*:\s+(\d+)/')
ATTACKER_PAT = re.compile(r'ID:(\d+).*?\[INFO: COMPROMISE NODE\]')
VICTIM_PAT   = re.compile(r'Victim Node \(Decimal Node ID:\s*(\d+)\)')

def generate_time_series_csv():
    print(f"Reading '{LOG_FILE}' and extracting time-series data for Attacker and Victim...")
    
    if not os.path.exists(LOG_FILE):
        print(f"[!] Error: File '{LOG_FILE}' not found.")
        return

    attacker_id = None
    victim_id = None
    
    # Nested dictionary: node_data[node_id][period] = {'cpu': 0, 'lpm': 0, ...}
    node_data = defaultdict(lambda: defaultdict(lambda: {'cpu': 0, 'lpm': 0, 'deep_lpm': 0, 'tx': 0, 'rx': 0}))
    
    current_node = None
    current_period = None

    # ---------------------------------------------------------
    # 1. Parse Data from Log File
    # ---------------------------------------------------------
    with open(LOG_FILE, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            # Detect Attacker
            if attacker_id is None:
                m = ATTACKER_PAT.search(line)
                if m: attacker_id = int(m.group(1))
            
            # Detect Victim
            if victim_id is None:
                m = VICTIM_PAT.search(line)
                if m: victim_id = int(m.group(1))
            
            # Detect Period Summary Header
            m_period = PERIOD_PAT.search(line)
            if m_period:
                current_node = int(m_period.group(1))
                current_period = int(m_period.group(2))
                continue
                
            # Extract Energest Stats
            if current_node is not None and current_period is not None:
                m_stat = STAT_PAT.search(line)
                if m_stat:
                    nid = int(m_stat.group(1))
                    if nid == current_node: # Safety check to ensure we are logging for the right node
                        stat_type = m_stat.group(2)
                        ticks = int(m_stat.group(3))
                        
                        if stat_type == 'CPU':
                            node_data[nid][current_period]['cpu'] += ticks
                        elif stat_type == 'LPM':
                            node_data[nid][current_period]['lpm'] += ticks
                        elif stat_type == 'Deep LPM':
                            node_data[nid][current_period]['deep_lpm'] += ticks
                        elif stat_type == 'Radio Tx':
                            node_data[nid][current_period]['tx'] += ticks
                        elif stat_type == 'Radio Rx':
                            node_data[nid][current_period]['rx'] += ticks

    # ---------------------------------------------------------
    # 2. Filter for Target Nodes
    # ---------------------------------------------------------
    target_nodes = []
    if attacker_id is not None: target_nodes.append(('Attacker', attacker_id))
    if victim_id is not None: target_nodes.append(('Victim', victim_id))

    if not target_nodes:
        print("[!] Error: Could not identify Attacker or Victim in the log.")
        return

    # ---------------------------------------------------------
    # 3. Convert Ticks to Energy & Write to CSV
    # ---------------------------------------------------------
    with open(OUTPUT_CSV, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        
        # New Header accommodating time tracking
        writer.writerow([
            "Period", "Time_Seconds", "Node_Role", "Node_ID", 
            "CPU(mJ)", "LPM(mJ)", "Deep_LPM(mJ)", "TX(mJ)", "RX(mJ)", 
            "Period_Total_Energy(mJ)", "Cumulative_Total_Energy(mJ)"
        ])
        
        # Loop through our designated Attacker and Victim
        for node_role, nid in target_nodes:
            periods = sorted(node_data[nid].keys())
            cumulative_energy = 0.0
            
            for p in periods:
                tk = node_data[nid][p]
                
                # Convert to mJ
                cpu_mj      = (tk['cpu'] / ENERGEST_SECOND) * I_CPU * VOLTAGE
                lpm_mj      = (tk['lpm'] / ENERGEST_SECOND) * I_LPM * VOLTAGE
                deep_lpm_mj = (tk['deep_lpm'] / ENERGEST_SECOND) * I_DEEP_LPM * VOLTAGE
                tx_mj       = (tk['tx'] / ENERGEST_SECOND) * I_TX * VOLTAGE
                rx_mj       = (tk['rx'] / ENERGEST_SECOND) * I_RX * VOLTAGE
                
                # Energy consumed specifically in this period
                period_energy = cpu_mj + lpm_mj + deep_lpm_mj + tx_mj + rx_mj
                
                # Running cumulative energy since boot
                cumulative_energy += period_energy
                
                # Estimate simulation time (Assuming 60 seconds per period)
                time_sec = (p + 1) * 60
                
                writer.writerow([
                    p, time_sec, node_role, nid, 
                    round(cpu_mj, 3), round(lpm_mj, 3), round(deep_lpm_mj, 3),
                    round(tx_mj, 3), round(rx_mj, 3), 
                    round(period_energy, 3), round(cumulative_energy, 3)
                ])

    # Print summary to terminal
    print(f"\n✅ Time-series CSV successfully saved as '{OUTPUT_CSV}'")
    print(f"   Attacker Node identified: {attacker_id if attacker_id else 'NA'}")
    print(f"   Victim Node identified:   {victim_id if victim_id else 'NA'}\n")

if __name__ == "__main__":
    generate_time_series_csv()