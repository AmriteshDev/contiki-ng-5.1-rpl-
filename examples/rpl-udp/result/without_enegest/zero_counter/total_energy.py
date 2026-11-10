import re
import csv
import os

# ==========================================
# 1. CONFIGURATION 
# ==========================================
SCENARIO_NAME = "zero_counter"  

LOG_FILES = {
    20: "log20.txt",
    40: "log40.txt",
    # 60: "log60.txt",
    # 80: "log80.txt",
    # 100: "log100.txt"
}

OUTPUT_CSV = f"{SCENARIO_NAME}.csv"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1)
# ==========================================
VOLTAGE = 3.0           
I_CPU = 10.0            
I_LPM = 0.023           
I_TX = 17.4             
I_RX = 18.8             
ENERGEST_SECOND = 1000000  # Ensure this matches your logs

def parse_and_compute(filename):
    if not os.path.exists(filename):
        print(f"  [!] Missing file: {filename}")
        return None
    
    first_readings = {}
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
                    "TIME": int(match.group("time")),
                    "CPU": int(match.group("cpu")),
                    "LPM": int(match.group("lpm")),
                    "TX": int(match.group("tx")),
                    "RX": int(match.group("rx"))
                }
                
                # Agar node pehli baar mila hai, toh first_readings me save karo
                if node_id not in first_readings:
                    first_readings[node_id] = data
                
                # Har baar data aane par last_reading update hoti rahegi
                last_readings[node_id] = data

    total_cpu_mj = total_lpm_mj = total_tx_mj = total_rx_mj = 0.0
    valid_nodes = len(last_readings)
    
    if valid_nodes == 0:
        return None

    # Yahan se safety check hata diya gaya hai, seedha First aur Last reading ka difference nikal rahe hain
    for node_id in last_readings:
        start = first_readings[node_id]
        end = last_readings[node_id]

        total_cpu_mj += ((end["CPU"] - start["CPU"]) / ENERGEST_SECOND) * I_CPU * VOLTAGE
        total_lpm_mj += ((end["LPM"] - start["LPM"]) / ENERGEST_SECOND) * I_LPM * VOLTAGE
        total_tx_mj += ((end["TX"] - start["TX"]) / ENERGEST_SECOND) * I_TX * VOLTAGE
        total_rx_mj += ((end["RX"] - start["RX"]) / ENERGEST_SECOND) * I_RX * VOLTAGE

    # Sabhi components ki total energy jod kar poore network ki total energy nikal li
    total_network_mj = total_cpu_mj + total_lpm_mj + total_tx_mj + total_rx_mj

    return (total_cpu_mj, total_lpm_mj, total_tx_mj, total_rx_mj, total_network_mj, valid_nodes)

def generate_csv():
    print(f"Creating CSV for '{SCENARIO_NAME}' case...")
    with open(OUTPUT_CSV, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        # Headers specifically show 'Total' energy for the network
        writer.writerow(["Nodes_Configured", "Valid_Nodes_Found", "Total_CPU(mJ)", "Total_LPM(mJ)", "Total_TX(mJ)", "Total_RX(mJ)", "Total_Network_Energy(mJ)"])

        for nodes, filename in LOG_FILES.items():
            result = parse_and_compute(filename)
            if result:
                t_cpu, t_lpm, t_tx, t_rx, total, valid_count = result
                writer.writerow([nodes, valid_count, round(t_cpu, 3), round(t_lpm, 3), round(t_tx, 3), round(t_rx, 3), round(total, 3)])
                print(f"  -> Processed {nodes} nodes. Total Network Energy: {round(total, 2)} mJ")
            else:
                print(f"  -> No valid data extracted for {nodes} nodes.")
    
    print(f"\nSaved successfully as '{OUTPUT_CSV}'")

if __name__ == "__main__":
    generate_csv()