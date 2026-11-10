import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# CONFIGURATION
# ==========================================
CSV_FILES = {
    # Naye structure ke hisaab se paths update kiye
    "Normal":       "normal/normal_delay_pdr_final.csv",  
    "Zero Counter": "zero_counter/zero_counter_delay_pdr_final.csv",  
    "CC Request":   "cc_request/cc_request_delay_pdr_final.csv"   
}

# Nodes ki jagah attackers use honge
EXPECTED_ATTACKERS = [1, 3, 5]

OUTPUT_AVG_PDR   = "Avg_Packet Delivery Ratio.png"
OUTPUT_TOTAL_PDR = "Total_Packet Delivery Ratio.png"

# ==========================================
# DATA LOADER
# ==========================================
def load_column_with_ci(filepath, val_col, ci_col):
    data = {}
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            # Column name "Nodes_Configured" se "Attacker_Count" ho gaya hai
            data[int(row["Attacker_Count"])] = (float(row[val_col]), float(row[ci_col]))
    else:
        print(f"  [Warning] File not found: {filepath}")
    return data


# ==========================================
# GENERIC BAR PLOTTER WITH ERROR BARS
# ==========================================
def plot_bars(plot_data, error_data, ylabel, output_file):
    x       = np.arange(len(EXPECTED_ATTACKERS))
    width   = 0.25
    offsets = [-width, 0, width]
    
    hatches = ['//', '\\\\', 'xx']
    colors  = ['#2ca02c', '#1f77b4', '#d62728']

    fig, ax = plt.subplots(figsize=(10, 6))

    bars_list = []
    for i, (label, filepath) in enumerate(CSV_FILES.items()):
        bars = ax.bar(
            x + offsets[i], plot_data[label], width,
            label=label, 
            color='white',         
            edgecolor=colors[i],   
            hatch=hatches[i],      
            linewidth=1.5,         
            zorder=3
        )
        bars_list.append(bars)

    # X-Label update kiya
    ax.set_xlabel('Number of Attackers', fontsize=12, fontweight='bold')
    ax.set_ylabel(ylabel, fontsize=12, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_ATTACKERS])
    
    ax.set_ylim(0, 115) 
    ax.legend(title="Scenarios", fontsize=10, loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    for i, bars in enumerate(bars_list):
        scenario_key = list(CSV_FILES.keys())[i]
        scenario_cis = error_data[scenario_key]
        
        for j, bar in enumerate(bars):
            height = bar.get_height()
            current_ci = scenario_cis[j]
            if height > 0:
                ci_offset = 0
                text_y = height + ci_offset + (height * 0.02)
                
                ax.text(
                    bar.get_x() + bar.get_width() / 2.,
                    text_y,
                    f'{height:.1f}%',
                    ha='center', va='bottom',
                    fontsize=9, rotation=45, fontweight='bold'
                )

    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    plt.close()
    print(f"  Saved: '{output_file}'")

# ==========================================
# PLOT 1 — Average PDR (%)
# ==========================================
def plot_avg_pdr():
    print("\nPlotting Average Packet Delivery Ratio...")
    plot_data = {}
    error_data = {}
    
    for label, filepath in CSV_FILES.items():
        extracted = load_column_with_ci(filepath, "Overall_Avg_PDR(%)", "CI_Avg_PDR(%)")
        p_data, e_data = [], []
        for n in EXPECTED_ATTACKERS:
            if n not in extracted:
                print(f"[CRITICAL] Missing {n} attackers data in {label}. Check logs.")
                return
            p_data.append(extracted[n][0])
            e_data.append(extracted[n][1])
        plot_data[label] = p_data
        error_data[label] = e_data

    plot_bars(plot_data, error_data, "Average Packet Delivery Ratio (%)", OUTPUT_AVG_PDR)

# ==========================================
# PLOT 2 — Total PDR (%)
# ==========================================
def plot_total_pdr():
    print("\n[PDR] Plotting Total Network PDR...")
    plot_data = {}
    error_data = {}
    
    for label, filepath in CSV_FILES.items():
        extracted = load_column_with_ci(filepath, "Overall_Total_PDR(%)", "CI_Total_PDR(%)")
        p_data, e_data = [], []
        for n in EXPECTED_ATTACKERS:
            if n not in extracted:
                print(f"[CRITICAL] Missing {n} attackers data in {label}. Check logs.")
                return
            p_data.append(extracted[n][0])
            e_data.append(extracted[n][1])
        plot_data[label] = p_data
        error_data[label] = e_data

    plot_bars(plot_data, error_data, "Total Packet Delivery Ratio (%)", OUTPUT_TOTAL_PDR)

if __name__ == "__main__":
    print("=== Packet Delivery Ratio Graph Generator ===")
    plot_avg_pdr()
    plot_total_pdr()
    print("\nDone.")