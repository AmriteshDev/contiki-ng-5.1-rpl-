import csv
import os
import numpy as np
import scipy.stats as stats

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "cc_request"  # Update this if you change scenarios (e.g., "cc_request_avg")
NODE_COUNTS = [20, 40, 60, 80, 100]

FINAL_OUTPUT_CSV = f"{SCENARIO_NAME}_energy_final.csv"

# ==========================================
# 2. MATH FUNCTION FOR MEAN & CI (95%)
# ==========================================
def calc_mean_ci(data_list):
    """List of runs se Average aur 97% Confidence Interval calculate karta hai"""
    n = len(data_list)
    mean_val = np.mean(data_list)
    
    # Agar 1 se zyada run hain tabhi CI niklega
    if n > 1:
        # Standard Error of Mean (SD / sqrt(n))
        se = stats.sem(data_list) 
        # t-value for 95% confidence (two-tailed) based on runs (n)
        # t_val = stats.t.ppf(0.975, n - 1)
        # t-value for 97% confidence (two-tailed) based on runs (n)
        t_val = stats.t.ppf(0.985, n - 1) 
        ci = t_val * se
    else:
        # Agar sirf 1 run hai to CI calculation possible nahi hai
        return round(mean_val, 3), np.nan
        
    return round(mean_val, 3), round(ci, 3)


# ==========================================
# 3. CALCULATION LOGIC
# ==========================================
def calculate_averages():
    print(f"Calculating final averages and 97% CI for Scenario: {SCENARIO_NAME}...\n")
    
    final_data = []
    
    # Process each intermediate file automatically
    for nodes in NODE_COUNTS:
        filename = f"{nodes}_nodes_energy.csv"
        
        if not os.path.exists(filename):
            print(f"  [!] Missing file: {filename}. Skipping...")
            continue
            
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            
            # --- CHANGE: Variables ab sum ki jagah lists hain ---
            tx_vals, rx_vals, cpu_vals, lpm_vals, deep_vals = [], [], [], [], []
            net_vals, total_net_vals, total_tx_vals, total_rx_vals = [], [], [], []
            
            run_count = 0
            
            # Har run ka data array (list) mein add ho raha hai
            for row in reader:
                cpu_vals.append(float(row["Avg_CPU(mJ)"]))
                lpm_vals.append(float(row["Avg_LPM(mJ)"]))
                deep_vals.append(float(row["Avg_Deep_LPM(mJ)"]))
                tx_vals.append(float(row["Avg_TX(mJ)"]))
                rx_vals.append(float(row["Avg_RX(mJ)"]))
                net_vals.append(float(row["Avg_Network_Energy(mJ)"]))
                
                total_net_vals.append(float(row["Total_Network_Energy(mJ)"]))
                total_tx_vals.append(float(row["Total_TX(mJ)"]))
                total_rx_vals.append(float(row["Total_RX(mJ)"]))

                run_count += 1
                
            if run_count > 0:
                # --- CHANGE: Har list par Average aur CI calculate ho raha hai ---
                avg_cpu, ci_cpu = calc_mean_ci(cpu_vals)
                avg_lpm, ci_lpm = calc_mean_ci(lpm_vals)
                avg_deep, ci_deep = calc_mean_ci(deep_vals)
                avg_tx, ci_tx = calc_mean_ci(tx_vals)
                avg_rx, ci_rx = calc_mean_ci(rx_vals)
                avg_net, ci_net = calc_mean_ci(net_vals)
                
                avg_tot_net, ci_tot_net = calc_mean_ci(total_net_vals)
                avg_tot_tx, ci_tot_tx = calc_mean_ci(total_tx_vals)
                avg_tot_rx, ci_tot_rx = calc_mean_ci(total_rx_vals)

                # Final data store karna (Mean ke just baad uski CI value)
                final_data.append([
                    nodes, run_count,
                    avg_cpu, ci_cpu,
                    avg_lpm, ci_lpm,
                    avg_deep, ci_deep,
                    avg_tx, ci_tx,
                    avg_rx, ci_rx,
                    avg_net, ci_net,
                    avg_tot_net, ci_tot_net,
                    avg_tot_tx, ci_tot_tx,
                    avg_tot_rx, ci_tot_rx
                ])
                print(f"  -> {nodes} Nodes: Averaged {run_count} runs successfully with CI.")

    # ==========================================
    # 4. WRITE FINAL OUTPUT
    # ==========================================
    with open(FINAL_OUTPUT_CSV, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # --- CHANGE: CSV Header mein nayi CI columns add kar di hain ---
        writer.writerow([
            "Nodes_Configured", "Valid_Runs_Averaged",
            "Overall_Avg_CPU(mJ)", "CI_CPU(mJ)", 
            "Overall_Avg_LPM(mJ)", "CI_LPM(mJ)", 
            "Overall_Avg_Deep_LPM(mJ)", "CI_Deep_LPM(mJ)",
            "Overall_Avg_TX(mJ)", "CI_TX(mJ)", 
            "Overall_Avg_RX(mJ)", "CI_RX(mJ)", 
            "Overall_Avg_Network_Energy(mJ)", "CI_Network_Energy(mJ)",
            "Overall_Total_Network_Energy(mJ)", "CI_Total_Network_Energy(mJ)",
            "Overall_Total_TX(mJ)", "CI_Total_TX(mJ)", 
            "Overall_Total_RX(mJ)", "CI_Total_RX(mJ)"
        ])
        for row in final_data:
            writer.writerow(row)
            
    print(f"\nSaved final summary with Confidence Intervals as '{FINAL_OUTPUT_CSV}'")

if __name__ == "__main__":
    calculate_averages()