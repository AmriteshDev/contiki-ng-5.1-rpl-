import re
import csv
import os

# ==========================================
# 1. CONFIGURATION 
# ==========================================
# Jis case ka log nikal rahe hain, uska naam yahan set karein 
# Yahan 'nodelta' suffix add kiya hai taaki pehchanne me asani ho
SCENARIO_NAME = "cc_request_avg"  

LOG_FILES = {
    20: "log20.txt",
    40: "log40.txt",
    60: "log60.txt",
    80: "log80.txt",
    100: "log100.txt"
}

OUTPUT_CSV = f"{SCENARIO_NAME}.csv"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1 / Sky)
# ==========================================
VOLTAGE = 3.0           
I_CPU = 10.0            
I_LPM = 0.023           
I_TX = 17.4             
I_RX = 18.8             

# [IMPORTANT]: Agar aap Cooja Mote use kar rahe hain, toh ise 1000000 rakhein.
ENERGEST_SECOND = 1000000  

def parse_and_compute(filename):
    if not os.path.exists(filename):
        print(f"  [!] Missing file: {filename}")
        return None
    
    # Bina Delta ke liye hamein sirf last reading chahiye, pehli nahi.
    last_readings = {}
    
    log_pattern = re.compile(
        r"ID:(?P<node_id>\d+).*?\[ENERGY\]\s+(?:Node=\d+\s+)?TIME=(?P<time>\d+)\s+CPU=(?P<cpu>\d+)\s+LPM=(?P<lpm>\d+)\s+TX=(?P<tx>\d+)\s+RX=(?P<rx>\d+)"
    )

    with open(filename, 'r') as file:
        for line in file:
            match = log_pattern.search(line)
            if match:
                node_id = int(match.group("node_id"))
                data = {
                    "CPU": int(match.group("cpu")),
                    "LPM": int(match.group("lpm")),
                    "TX": int(match.group("tx")),
                    "RX": int(match.group("rx"))
                }
                
                # File line-by-line padhi jayegi aur ye dictionary bar-bar update hogi.
                # Aakhir mein sirf sabse last log ki value hi isme bachegi.
                last_readings[node_id] = data

    total_cpu_mj = total_lpm_mj = total_tx_mj = total_rx_mj = 0.0
    valid_nodes = len(last_readings)
    
    if valid_nodes == 0:
        return None

    # Step 1: Har node ki Absolute (Last) energy nikalkar Total mein jodna (No Delta)
    for node_id, last in last_readings.items():
        # Yahan (end - start) hata diya gaya hai. Direct aakhiri ticks ko divide kar rahe hain.
        total_cpu_mj += (last["CPU"] / ENERGEST_SECOND) * I_CPU * VOLTAGE
        total_lpm_mj += (last["LPM"] / ENERGEST_SECOND) * I_LPM * VOLTAGE
        total_tx_mj  += (last["TX"]  / ENERGEST_SECOND) * I_TX  * VOLTAGE
        total_rx_mj  += (last["RX"]  / ENERGEST_SECOND) * I_RX  * VOLTAGE

    # Step 2: Total ko valid_nodes se DIVIDE karke 'AVERAGE' nikalna
    avg_cpu_mj = total_cpu_mj / valid_nodes
    avg_lpm_mj = total_lpm_mj / valid_nodes
    avg_tx_mj  = total_tx_mj / valid_nodes
    avg_rx_mj  = total_rx_mj / valid_nodes

    # Sabhi average components ko jod kar Node ki Average Total Network Energy nikalna
    avg_network_mj = avg_cpu_mj + avg_lpm_mj + avg_tx_mj + avg_rx_mj

    return (avg_cpu_mj, avg_lpm_mj, avg_tx_mj, avg_rx_mj, avg_network_mj, valid_nodes)

def generate_csv():
    print(f"Creating 'No Delta' Average Energy CSV for '{SCENARIO_NAME}'...")
    with open(OUTPUT_CSV, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        
        # Headers for Average Values
        writer.writerow(["Nodes_Configured", "Valid_Nodes_Found", "Avg_CPU(mJ)", "Avg_LPM(mJ)", "Avg_TX(mJ)", "Avg_RX(mJ)", "Avg_Network_Energy(mJ)"])

        for nodes, filename in LOG_FILES.items():
            result = parse_and_compute(filename)
            if result:
                a_cpu, a_lpm, a_tx, a_rx, avg_total, valid_count = result
                writer.writerow([nodes, valid_count, round(a_cpu, 3), round(a_lpm, 3), round(a_tx, 3), round(a_rx, 3), round(avg_total, 3)])
                print(f"  -> Processed {nodes} nodes (Found: {valid_count}). Avg TX: {round(a_tx, 2)} mJ | Avg Energy per node: {round(avg_total, 2)} mJ")
            else:
                print(f"  -> No valid data extracted for {nodes} nodes.")
    
    print(f"\nSaved successfully as '{OUTPUT_CSV}'")

if __name__ == "__main__":
    generate_csv()