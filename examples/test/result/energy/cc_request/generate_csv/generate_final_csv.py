import csv
import os

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "cc_request"  # Update this if you change scenarios (e.g., "cc_request_avg")
NODE_COUNTS = [20, 40, 60, 80, 100]

FINAL_OUTPUT_CSV = f"{SCENARIO_NAME}_energy_final.csv"

# ==========================================
# 2. CALCULATION LOGIC
# ==========================================
def calculate_averages():
    print(f"Calculating final averages for Scenario: {SCENARIO_NAME}...\n")
    
    final_data = []
    
    # Process each intermediate file automatically
    for nodes in NODE_COUNTS:
        # Dynamically matches the exact output name from your first script
        filename = f"{nodes}_nodes_energy.csv"
        
        if not os.path.exists(filename):
            print(f"  [!] Missing file: {filename}. Skipping...")
            continue
            
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            
            total_cpu = total_lpm = total_deep = total_tx = total_rx = total_network = total_net_total = total_tx_total = total_rx_total = 0.0
            run_count = 0
            
            for row in reader:
                total_cpu += float(row["Avg_CPU(mJ)"])
                total_lpm += float(row["Avg_LPM(mJ)"])
                total_deep += float(row["Avg_Deep_LPM(mJ)"])
                total_tx += float(row["Avg_TX(mJ)"])
                total_rx += float(row["Avg_RX(mJ)"])
                total_network += float(row["Avg_Network_Energy(mJ)"])

               

                total_net_total += float(row["Total_Network_Energy(mJ)"])
                total_tx_total += float(row["Total_TX(mJ)"])
                total_rx_total += float(row["Total_RX(mJ)"])

                run_count += 1
                
            if run_count > 0:
                final_data.append([
                    nodes,
                    run_count,
                    round(total_cpu / run_count, 3),
                    round(total_lpm / run_count, 3),
                    round(total_deep / run_count, 3),
                    round(total_tx / run_count, 3),
                    round(total_rx / run_count, 3),
                    round(total_network / run_count, 3),

                    
                    round(total_net_total / run_count, 3),
                    round(total_tx_total / run_count, 3),
                    round(total_rx_total / run_count, 3)
                ])
                print(f"  -> {nodes} Nodes: Averaged {run_count} runs successfully from {filename}.")

    # ==========================================
    # 3. WRITE FINAL OUTPUT
    # ==========================================
    with open(FINAL_OUTPUT_CSV, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([
            "Nodes_Configured", "Valid_Runs_Averaged",
            "Overall_Avg_CPU(mJ)", "Overall_Avg_LPM(mJ)", "Overall_Avg_Deep_LPM(mJ)",
            "Overall_Avg_TX(mJ)", "Overall_Avg_RX(mJ)", "Overall_Avg_Network_Energy(mJ)",
            "Overall_Total_Network_Energy(mJ)","Overall_Total_TX(mJ)", "Overall_Total_RX(mJ)"
        ])
        for row in final_data:
            writer.writerow(row)
            
    print(f"\nSaved final summary as '{FINAL_OUTPUT_CSV}'")

if __name__ == "__main__":
    calculate_averages()