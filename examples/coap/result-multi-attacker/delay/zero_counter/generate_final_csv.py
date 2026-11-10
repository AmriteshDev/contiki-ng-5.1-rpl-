import csv
import os
import numpy as np
import scipy.stats as stats

# ==========================================
# 1. CONFIGURATION (Updated for Multi-Attacker)
# ==========================================
SCENARIO_NAME = "zero_counter" 
# NODE_COUNTS ki jagah ATTACKERS
ATTACKERS = [1, 3, 5]

FINAL_OUTPUT_CSV = f"{SCENARIO_NAME}_delay_pdr_final.csv"

# ==========================================
# 2. MATH FUNCTION FOR MEAN & CI (97%)
# ==========================================
def calc_mean_ci(data_list):
    n = len(data_list)
    mean_val = np.mean(data_list)
    
    if n > 1:
        se = stats.sem(data_list) 
        t_val = stats.t.ppf(0.985, n - 1)  # 97% CI 
        ci = t_val * se
    else:
        return round(mean_val, 3), np.nan
        
    return round(mean_val, 3), round(ci, 3)

# ==========================================
# 3. CALCULATION LOGIC
# ==========================================
def calculate_metrics_averages():
    print(f"Calculating final metrics averages and 97% CI for Scenario: {SCENARIO_NAME}...\n")
    
    final_data = []
    
    # Process each intermediate metrics file automatically
    for atk_count in ATTACKERS:
        # File name ab attacker count ke hisaab se check hoga
        filename = f"{atk_count}attackers_delay_pdr.csv"
        
        if not os.path.exists(filename):
            print(f"  [!] Missing file: {filename}. Skipping...")
            continue
            
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            
            avg_rtt_vals, total_rtt_vals = [], []
            avg_pdr_vals, total_pdr_vals = [], []
            occ_vals = []
            sent_vals, acked_vals = [], []
            
            run_count = 0
            
            for row in reader:
                avg_rtt_vals.append(float(row["Avg_RTT(ms)"]))
                total_rtt_vals.append(float(row["Total_RTT(ms)"]))
                avg_pdr_vals.append(float(row["Avg_PDR(%)"]))
                total_pdr_vals.append(float(row["Total_PDR(%)"]))
                occ_vals.append(float(row["Buffer_Occupancy(%)"]))
                sent_vals.append(float(row["Total_Sent"]))
                acked_vals.append(float(row["Total_Acked"]))
                
                run_count += 1
                
            if run_count > 0:
                avg_rtt_m, ci_avg_rtt = calc_mean_ci(avg_rtt_vals)
                tot_rtt_m, ci_tot_rtt = calc_mean_ci(total_rtt_vals)
                avg_pdr_m, ci_avg_pdr = calc_mean_ci(avg_pdr_vals)
                tot_pdr_m, ci_tot_pdr = calc_mean_ci(total_pdr_vals)
                occ_m, ci_occ = calc_mean_ci(occ_vals)
                
                sent_m, ci_sent = calc_mean_ci(sent_vals)
                acked_m, ci_acked = calc_mean_ci(acked_vals)

                # Nodes_Configured ki jagah Attacker_Count
                final_data.append([
                    atk_count,
                    run_count,
                    avg_rtt_m, ci_avg_rtt,
                    tot_rtt_m, ci_tot_rtt,
                    avg_pdr_m, ci_avg_pdr,
                    tot_pdr_m, ci_tot_pdr,
                    occ_m, ci_occ,
                    round(sent_m, 1), round(ci_sent, 1) if not np.isnan(ci_sent) else np.nan,
                    round(acked_m, 1), round(ci_acked, 1) if not np.isnan(ci_acked) else np.nan
                ])
                print(f"  -> {atk_count} Attackers: Averaged {run_count} runs successfully with CI.")

    # ==========================================
    # 4. WRITE FINAL OUTPUT
    # ==========================================
    with open(FINAL_OUTPUT_CSV, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        writer.writerow([
            "Attacker_Count", "Valid_Runs_Averaged",
            "Overall_Avg_RTT(ms)", "CI_Avg_RTT(ms)",
            "Overall_Total_RTT(ms)", "CI_Total_RTT(ms)",
            "Overall_Avg_PDR(%)", "CI_Avg_PDR(%)",
            "Overall_Total_PDR(%)", "CI_Total_PDR(%)",
            "Overall_Buffer_Occupancy(%)", "CI_Buffer_Occupancy(%)",
            "Avg_Total_Sent", "CI_Total_Sent",
            "Avg_Total_Acked", "CI_Total_Acked"
        ])
        
        for row in final_data:
            writer.writerow(row)
            
    print(f"\nSaved final summary with Confidence Intervals as '{FINAL_OUTPUT_CSV}'")

if __name__ == "__main__":
    calculate_metrics_averages()