import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "normal"
ALL_NODE_COUNTS = [20, 40, 60, 80, 100]
TOTAL_RUNS = 3
BASE_LOG_DIR = "../../logs/normal"


# ==========================================
# 2. REGEX PATTERNS
# ==========================================
NODE_INIT_PAT = re.compile(r'ID:(\d+).*?\[INFO: Main\s*\] Starting Contiki')
ATTACKER_PAT  = re.compile(r'ID:(\d+).*?\[INFO: COMPROMISE NODE\]')
VICTIM_PAT    = re.compile(r'Victim Node \(Decimal Node ID:\s*(\d+)\)')

# PDR & Delay (RTT) Patterns
SUCCESS_PAT   = re.compile(r'ID:(\d+).*?\[METRIC\] SUCCESS:\s*node=(\d+)\s*\|\s*RTT:\s*(\d+)\s*ms\s*\|\s*Sent:\s*(\d+)\s*\|\s*Acked:\s*(\d+)')
TIMEOUT_PAT   = re.compile(r'ID:(\d+).*?\[METRIC\] TIMEOUT\s*:\s*node=(\d+)\s*\|\s*Sent:\s*(\d+)\s*\|\s*Acked:\s*(\d+)')

# Buffer Pattern (Focusing on Global Queue as requested)
STATS_PAT     = re.compile(r'ID:(\d+).*?\[STATS\]\s*Node\s*(\d+):\s*Global-Queue=(\d+)/(\d+)\s*pkts')

# ==========================================
# 3. LOG PARSER & COMPUTER
# ==========================================
def parse_and_compute_metrics(filename):
    if not os.path.exists(filename):
        return None

    # Dictionary to store all metrics for a node
    nodes_data = defaultdict(lambda: {
        'rtts': [],
        'final_sent': 0,
        'final_acked': 0,
        'timeouts': 0,
        'g_queue_samples': []
    })
    
    attacker_ids = set()
    victim_ids = set()  
    all_node_ids = set()

    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            m = NODE_INIT_PAT.search(line)
            if m:
                all_node_ids.add(int(m.group(1)))
                continue

            m = ATTACKER_PAT.search(line)
            if m:
                attacker_ids.add(int(m.group(1)))
                continue

            m = VICTIM_PAT.search(line)
            if m:
                victim_ids.add(int(m.group(1))) 
                continue

            # SUCCESS line -> Contains RTT, Sent, and Acked
            m = SUCCESS_PAT.search(line)
            if m:
                node_id = int(m.group(2))
                nodes_data[node_id]['rtts'].append(int(m.group(3)))
                # --- UPDATE: max() ka istemaal overwrite bachane ke liye ---
                nodes_data[node_id]['final_sent'] = max(nodes_data[node_id]['final_sent'], int(m.group(4)))
                nodes_data[node_id]['final_acked'] = max(nodes_data[node_id]['final_acked'], int(m.group(5)))
                continue

            # TIMEOUT line -> Contains Timeouts, Sent, and Acked
            m = TIMEOUT_PAT.search(line)
            if m:
                node_id = int(m.group(2))
                # --- UPDATE: max() ka istemaal overwrite bachane ke liye ---
                nodes_data[node_id]['final_sent'] = max(nodes_data[node_id]['final_sent'], int(m.group(3)))
                nodes_data[node_id]['final_acked'] = max(nodes_data[node_id]['final_acked'], int(m.group(4)))
                nodes_data[node_id]['timeouts'] += 1
                continue

    # Identify Valid Nodes
    total_nodes_in_log = len(all_node_ids)
    valid_node_ids = all_node_ids - attacker_ids
    valid_nodes_count = len(valid_node_ids)

    if valid_nodes_count == 0:
        return None 

    # --- AGGREGATION VARIABLES ---
    all_rtts = []
    total_sent = 0
    total_acked = 0
    all_g_ratios = []
    node_pdrs = [] # --- UPDATE 1: Har node ka alag PDR store karne ke liye naya list ---

    # Calculate only for legitimate nodes
    for nid, d in nodes_data.items():
        if nid in valid_node_ids:
            all_rtts.extend(d['rtts'])
            total_sent += d['final_sent']
            total_acked += d['final_acked']
            
            # --- UPDATE 2: Individual Node PDR calculate karna (Average PDR nikalne ke liye) ---
            if d['final_sent'] > 0:
                node_pdrs.append((d['final_acked'] / d['final_sent']) * 100)
            
            for (cur, mx) in d['g_queue_samples']:
                if mx > 0:
                    all_g_ratios.append(cur / mx * 100)

    # --- UPDATE 3: Naye aur purane metrics ek sath calculate ho rahe hain ---
    avg_rtt = sum(all_rtts) / len(all_rtts) if all_rtts else 0.0
    total_rtt = sum(all_rtts) # NAYA: Total RTT
    
    total_pdr = (total_acked / total_sent * 100) if total_sent > 0 else 0.0 # Puraane network_pdr ka naam change kiya
    avg_pdr = sum(node_pdrs) / len(node_pdrs) if node_pdrs else 0.0 # NAYA: Average PDR
    
    avg_g_occ = sum(all_g_ratios) / len(all_g_ratios) if all_g_ratios else 0.0
    
    atk_str = ";".join(str(a) for a in sorted(attacker_ids)) if attacker_ids else "NA"
    vic_str = ";".join(str(v) for v in sorted(victim_ids)) if victim_ids else "NA"

    # --- UPDATE 4: Return statement mein total_rtt aur avg_pdr add kar diye ---
    return (
        total_nodes_in_log, valid_nodes_count, atk_str, vic_str,
        avg_rtt, total_rtt, avg_pdr, total_pdr, avg_g_occ, total_sent, total_acked
    )

# ==========================================
# 4. CSV GENERATION (Intermediate ONLY)
# ==========================================
def generate_intermediate_metrics_pipeline():
    print(f"=== Generating Intermediate Metrics CSVs for Scenario: {SCENARIO_NAME} ===\n")

    for NODE_COUNT in ALL_NODE_COUNTS:
        intermediate_csv = f"{NODE_COUNT}_nodes_delay_pdr.csv"
        folder_name = f"log{NODE_COUNT}"

        log_files = {}
        for run_id in range(1, TOTAL_RUNS + 1):
            log_files[run_id] = os.path.join(BASE_LOG_DIR, folder_name, f"log{NODE_COUNT}_{run_id}.txt")

        with open(intermediate_csv, 'w', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            # --- UPDATE 5: Headers mein naye columns add kiye ---
            writer.writerow([
                "Run_Number", "Total_Nodes", "Valid_Nodes", "Attacker_ID", "Victim_ID",
                "Avg_RTT(ms)", "Total_RTT(ms)", "Avg_PDR(%)", "Total_PDR(%)", "Buffer_Occupancy(%)", "Total_Sent", "Total_Acked"
            ])

            for run_id, filename in log_files.items():
                result = parse_and_compute_metrics(filename)
                if result:
                    # --- UPDATE 6: Unpacking line mein total_rtt aur avg_pdr add kiye ---
                    tot_nodes, val_nodes, atk_id, vic_id, avg_rtt, total_rtt, avg_pdr, total_pdr, occ, sent, acked = result
                    
                    # --- UPDATE 7: CSV write function mein values add ki ---
                    writer.writerow([
                        run_id, tot_nodes, val_nodes, atk_id, vic_id,
                        round(avg_rtt, 3), round(total_rtt, 3), round(avg_pdr, 3), round(total_pdr, 3), round(occ, 3), sent, acked
                    ])
                    print(f"  -> {filename} parsed successfully.")
                else:
                    print(f"  -> [!] No data for {filename}.")
        
        print(f"Saved Intermediate: '{intermediate_csv}'\n")

if __name__ == "__main__":
    generate_intermediate_metrics_pipeline()