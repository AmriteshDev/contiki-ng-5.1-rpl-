import re
import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION 
# ==========================================
SCENARIO_NAME = "normal_avg"  

LOG_FILES = {
    20: "log20E.txt",
    40: "log40E.txt", 
    60: "log60E.txt",  # Aapki nayi log file ka naam yahan updated hai
    # 80: "log80.txt",
    # 100: "log100.txt"
}

OUTPUT_CSV = f"{SCENARIO_NAME}.csv"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1 / Cooja)
# ==========================================
VOLTAGE = 3.0            
I_CPU = 10.0            
I_LPM = 0.023           
I_TX = 17.4             
I_RX = 18.8             

# Cooja mote clock ticks per second (60 seconds = 60000000 ticks)
ENERGEST_SECOND = 1000000  

def parse_and_compute(filename):
    if not os.path.exists(filename):
        print(f"   [!] Missing file: {filename}")
        return None
    
    # Har node ke periodic ticks ko collect aur sum karne ke liye defaultdict
    node_ticks = defaultdict(lambda: {'cpu': 0, 'lpm': 0, 'tx': 0, 'rx': 0})
    
    # Naya Regex pattern jo multi-line Energest logs ko parse karega
    log_pattern = re.compile(
        r"ID:(?P<node_id>\d+)\s+\[INFO:\s+Energest\s*\]\s+"
        r"(?P<type>CPU|LPM|Radio Tx|Radio Rx)\s*:\s+(?P<val>\d+)/"
    )

    with open(filename, 'r', encoding='utf-8', errors='ignore') as file:
        for line in file:
            match = log_pattern.search(line)
            if match:
                node_id = int(match.group("node_id"))
                record_type = match.group("type")
                ticks_val = int(match.group("val"))
                
                # Metrics ko unke respective type ke mutabiq node_ticks me add karna
                if record_type == 'CPU':
                    node_ticks[node_id]['cpu'] += ticks_val
                elif record_type == 'LPM':
                    node_ticks[node_id]['lpm'] += ticks_val
                elif record_type == 'Radio Tx':
                    node_ticks[node_id]['tx'] += ticks_val
                elif record_type == 'Radio Rx':
                    node_ticks[node_id]['rx'] += ticks_val

    total_cpu_mj = total_lpm_mj = total_tx_mj = total_rx_mj = 0.0
    valid_nodes = len(node_ticks)
    
    if valid_nodes == 0:
        return None

    # Step 1: Har node ki total ticks ko Energy (mJ) mein convert karke total me jodna
    for node_id, ticks in node_ticks.items():
        total_cpu_mj += (ticks["cpu"] / ENERGEST_SECOND) * I_CPU * VOLTAGE
        total_lpm_mj += (ticks["lpm"] / ENERGEST_SECOND) * I_LPM * VOLTAGE
        total_tx_mj  += (ticks["tx"]  / ENERGEST_SECOND) * I_TX  * VOLTAGE
        total_rx_mj  += (ticks["rx"]  / ENERGEST_SECOND) * I_RX  * VOLTAGE

    # Step 2: Total network energy ko valid_nodes se DIVIDE karke 'AVERAGE' nikalna
    avg_cpu_mj = total_cpu_mj / valid_nodes
    avg_lpm_mj = total_lpm_mj / valid_nodes
    avg_tx_mj  = total_tx_mj / valid_nodes
    avg_rx_mj  = total_rx_mj / valid_nodes

    # Sabhi average components ko jod kar Node ki Average Total Network Energy nikalna
    avg_network_mj = avg_cpu_mj + avg_lpm_mj + avg_tx_mj + avg_rx_mj

    return (avg_cpu_mj, avg_lpm_mj, avg_tx_mj, avg_rx_mj, avg_network_mj, valid_nodes)

def generate_csv():
    print(f"Creating Average Energy CSV for '{SCENARIO_NAME}'...")
    with open(OUTPUT_CSV, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        
        # CSV Headers
        writer.writerow(["Nodes_Configured", "Valid_Nodes_Found", "Avg_CPU(mJ)", "Avg_LPM(mJ)", "Avg_TX(mJ)", "Avg_RX(mJ)", "Avg_Network_Energy(mJ)"])

        for nodes, filename in LOG_FILES.items():
            result = parse_and_compute(filename)
            if result:
                a_cpu, a_lpm, a_tx, a_rx, avg_total, valid_count = result
                writer.writerow([nodes, valid_count, round(a_cpu, 3), round(a_lpm, 3), round(a_tx, 3), round(a_rx, 3), round(avg_total, 3)])
                print(f"   -> Processed {nodes} nodes (Found: {valid_count}). Avg TX: {round(a_tx, 2)} mJ | Avg Energy per node: {round(avg_total, 2)} mJ")
            else:
                print(f"   -> No valid data extracted for {nodes} nodes ({filename}).")
    
    print(f"\nSaved successfully as '{OUTPUT_CSV}'")

if __name__ == "__main__":
    generate_csv()