import csv
import os

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "cc_request"  # Aapka scenario name
NODE_COUNTS = [20, 40, 60, 80, 100]

FINAL_OUTPUT_CSV = f"{SCENARIO_NAME}_nodes_delay_pdr_final.csv"

# ==========================================
# 2. CALCULATION LOGIC
# ==========================================
def calculate_metrics_averages():
    print(f"Calculating final metrics averages for Scenario: {SCENARIO_NAME}...\n")
    
    final_data = []
    
    # Process each intermediate metrics file automatically
    for nodes in NODE_COUNTS:
        filename = f"{nodes}_nodes_delay_pdr.csv"
        
        if not os.path.exists(filename):
            print(f"  [!] Missing file: {filename}. Skipping...")
            continue
            
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            
            # --- UPDATE 1: Naye variables yahan define kiye gaye hain ---
            tot_avg_rtt = tot_total_rtt = 0.0
            tot_avg_pdr = tot_total_pdr = 0.0
            tot_occ = 0.0
            tot_sent = tot_acked = 0
            
            run_count = 0
            
            for row in reader:
                # --- UPDATE 2: CSV ke naye headers ke hisaab se data padh rahe hain ---
                tot_avg_rtt   += float(row["Avg_RTT(ms)"])
                tot_total_rtt += float(row["Total_RTT(ms)"])
                tot_avg_pdr   += float(row["Avg_PDR(%)"])
                tot_total_pdr += float(row["Total_PDR(%)"])
                tot_occ       += float(row["Buffer_Occupancy(%)"])
                tot_sent      += float(row["Total_Sent"])
                tot_acked     += float(row["Total_Acked"])
                run_count += 1
                
            if run_count > 0:
                final_data.append([
                    nodes,
                    run_count,
                    # --- UPDATE 3: Sabhi naye metrics ka 10 runs ka overall average nikala gaya ---
                    round(tot_avg_rtt / run_count, 3),       
                    round(tot_total_rtt / run_count, 3),     
                    round(tot_avg_pdr / run_count, 3),       
                    round(tot_total_pdr / run_count, 3),     
                    round(tot_occ / run_count, 3),       
                    round(tot_sent / run_count, 1),      
                    round(tot_acked / run_count, 1)      
                ])
                print(f"  -> {nodes} Nodes: Averaged {run_count} runs successfully from {filename}.")

    # ==========================================
    # 3. WRITE FINAL OUTPUT
    # ==========================================
    with open(FINAL_OUTPUT_CSV, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # --- UPDATE 4: Final CSV ke naye headers ---
        writer.writerow([
            "Nodes_Configured", "Valid_Runs_Averaged",
            "Overall_Avg_RTT(ms)", "Overall_Total_RTT(ms)", 
            "Overall_Avg_PDR(%)", "Overall_Total_PDR(%)", 
            "Overall_Buffer_Occupancy(%)", "Avg_Total_Sent", "Avg_Total_Acked"
        ])
        
        for row in final_data:
            writer.writerow(row)
            
    print(f"\nSaved final summary as '{FINAL_OUTPUT_CSV}'")

if __name__ == "__main__":
    calculate_metrics_averages()