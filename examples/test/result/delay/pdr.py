import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# CONFIGURATION
# ==========================================
CSV_FILES = {
    "Normal":       "normal/normal_nodes_delay_pdr_final.csv",  
    "Zero Counter": "zero_counter/zero_counter_nodes_delay_pdr_final.csv",  
    "CC Request":   "cc_request/cc_request_nodes_delay_pdr_final.csv"   
}

EXPECTED_NODES = [20, 40, 60, 80, 100]

OUTPUT_AVG_PDR   = "Avg_Packet Delivery Ratio.png"
OUTPUT_TOTAL_PDR = "Total_Packet Delivery Ratio.png"

# ==========================================
# DATA LOADER
# ==========================================
def load_column(filepath, col):
    data = {}
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            data[int(row["Nodes_Configured"])] = float(row[col])
    else:
        print(f"  [Warning] File not found: {filepath}")
    return data


# ==========================================
# GENERIC BAR PLOTTER (Hatch & Monochrome Style)
# ==========================================
def plot_bars(plot_data, ylabel, output_file):
    x       = np.arange(len(EXPECTED_NODES))
    width   = 0.25
    offsets = [-width, 0, width]
    
    # --- UPDATE: Naye monochrome colors aur hatch patterns ---
    hatches = ['//', '\\\\', 'xx']
    colors  = ['#2ca02c', '#1f77b4', '#d62728']

    fig, ax = plt.subplots(figsize=(10, 6))

    bars_list = []
    # --- UPDATE: loop mein hatch aur naya color apply kiya gaya hai ---
    for i, (label, (filepath)) in enumerate(CSV_FILES.items()):
        bars = ax.bar(
            x + offsets[i], plot_data[label], width,
            label=label, 
            color='white',         # <--- CHANGE: Background white rahega (koi color fill nahi hoga)
            edgecolor=colors[i],   # <--- CHANGE: Border aur Hatch lines par yeh color apply hoga
            hatch=hatches[i],      
            linewidth=1.5,         # Border aur lines thodi clear dikhane ke liye thickness badha di
            zorder=3
        )
        bars_list.append(bars)

    ax.set_xlabel('Number of Nodes', fontsize=12, fontweight='bold')
    ax.set_ylabel(ylabel, fontsize=12, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_NODES])
    
    # Y-axis limit for percentages
    ax.set_ylim(0, 115) 
    ax.legend(title="Scenarios", fontsize=10, loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    for bars in bars_list:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax.text(
                    bar.get_x() + bar.get_width() / 2.,
                    height + (height * 0.02),
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
    for label, (filepath) in CSV_FILES.items():
        extracted = load_column(filepath, "Overall_Avg_PDR(%)")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Average Packet Delivery Ratio (%)", OUTPUT_AVG_PDR)

# ==========================================
# PLOT 2 — Total PDR (%)
# ==========================================
def plot_total_pdr():
    print("\n[PDR] Plotting Total Network PDR...")
    plot_data = {}
    for label, (filepath) in CSV_FILES.items():
        extracted = load_column(filepath, "Overall_Total_PDR(%)")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Total Packet Delivery Ratio (%)", OUTPUT_TOTAL_PDR)

# ==========================================
# MAIN
# ==========================================
if __name__ == "__main__":
    print("=== Packet Delivery Ratio Graph Generator ===")
    plot_avg_pdr()
    plot_total_pdr()
    print("\nDone.")