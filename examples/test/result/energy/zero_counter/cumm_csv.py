import re
import csv
import os

# ==========================================
# 1. CONFIGURATION 
# ==========================================
SCENARIO_NAME = "zero_counter_cumulative_tx"  

LOG_FILES = {
    20: "log20.txt",
    # 40: "log40.txt",
    # 60: "log60.txt",
    # 80: "log80.txt",
    # 100: "log100.txt"
}

OUTPUT_CSV = f"{SCENARIO_NAME}.csv"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1)
# ==========================================
VOLTAGE = 3.0           
I_TX = 17.4             # Sirf TX current ki zaroorat hai
ENERGEST_SECOND = 1000000  

def parse_and_compute(filename):
    if not os.path.exists(filename):
        print(f"  [!] Missing file: {filename}")
        return None
    
    first_readings = {}
    last_readings = {}
    
    # Regex mein sirf TX nikalna zaroori hai, par pura pattern match karne ke liye baki bhi rakhe hain
    log_pattern = re.compile(
        r"ID:(?P<node_id>\d+).*?\[ENERGY\]\s+(?:Node=\d+\s+)?TIME=(?P<time>\d+)\s+CPU=(?P<cpu>\d+)\s+LPM=(?P<lpm>\d+)\s+TX=(?P<tx>\d+)\s+RX=(?P<rx>\d+)"
    )

    with open(filename, 'r') as file:
        for line in file:
            match = log_pattern.search(line)
            if match:
                node_id = int(match.group("node_id"))
                data = {
                    "TX": int(match.group("tx"))  # Ab hum dictionary mein sirf TX ticks save kar rahe hain
                }
                
                if node_id not in first_readings:
                    first_readings[node_id] = data
                last_readings[node_id] = data

    cumulative_tx_mj = 0.0
    valid_nodes = len(last_readings)
    
    if valid_nodes == 0:
        return None

    # Poore network ka sirf TX (Transmit) CUMULATIVE nikalein
    for node_id in last_readings:
        start = first_readings[node_id]
        end = last_readings[node_id]

        cumulative_tx_mj += ((end["TX"] - start["TX"]) / ENERGEST_SECOND) * I_TX * VOLTAGE

    return (cumulative_tx_mj, valid_nodes)

def generate_csv():
    print(f"Creating Cumulative TX Energy CSV for '{SCENARIO_NAME}' case...")
    with open(OUTPUT_CSV, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        
        # CSV Header mein ab sirf 3 columns hain
        writer.writerow(["Nodes_Configured", "Valid_Nodes_Found", "Cumulative_TX(mJ)"])

        for nodes, filename in LOG_FILES.items():
            result = parse_and_compute(filename)
            if result:
                c_tx, valid_count = result
                writer.writerow([nodes, valid_count, round(c_tx, 3)])
                print(f"  -> Processed {nodes} nodes (Found: {valid_count}). Cumulative TX Energy: {round(c_tx, 2)} mJ")
            else:
                print(f"  -> No valid data extracted for {nodes} nodes.")
    
    print(f"\nSaved successfully as '{OUTPUT_CSV}'")

if __name__ == "__main__":
    generate_csv()