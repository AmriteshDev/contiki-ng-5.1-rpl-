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

OUTPUT_AVG_RTT   = "Avg_End-to-End Delay.png"
OUTPUT_TOTAL_RTT = "Total_End-to-End Delay.png"

# ==========================================
# DATA LOADER
# ==========================================
def load_column_with_ci(filepath, val_col, ci_col):
    data = {}
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            # Returns a tuple of (value, confidence_interval)
            data[int(row["Nodes_Configured"])] = (float(row[val_col]), float(row[ci_col]))
    else:
        print(f"  [Warning] File not found: {filepath}")
    return data

# ==========================================
# GENERIC BAR PLOTTER WITH ERROR BARS
# ==========================================
def plot_bars(plot_data, error_data, ylabel, output_file, value_fmt="{:.1f}"):
    x       = np.arange(len(EXPECTED_NODES))
    width   = 0.25
    offsets = [-width, 0, width]

    hatches = ['//', '\\\\', 'xx']
    colors  = ['#2ca02c', '#1f77b4', '#d62728']

    fig, ax = plt.subplots(figsize=(10, 6))

    bars_list = []
    
    for i, (label, filepath) in enumerate(CSV_FILES.items()):
        bars = ax.bar(
            x + offsets[i], plot_data[label], width,
            yerr=error_data[label],  # Error bars add kiye
            capsize=4,               # Caps add kiye
            error_kw={'elinewidth': 1.5, 'capthick': 1.5, 'ecolor': 'black'},
            label=label, 
            color='white',         
            edgecolor=colors[i],   
            hatch=hatches[i],      
            linewidth=1.5,         
            zorder=3
        )
        bars_list.append(bars)

    ax.set_xlabel('Number of Nodes', fontsize=12, fontweight='bold')
    ax.set_ylabel(ylabel, fontsize=12, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_NODES])

    ax.ticklabel_format(style='plain', axis='y')
    ax.legend(title="Scenarios", fontsize=10, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    # Offset formatting for CI handling
    for i, bars in enumerate(bars_list):
        scenario_key = list(CSV_FILES.keys())[i]
        scenario_cis = error_data[scenario_key]
        
        for j, bar in enumerate(bars):
            height = bar.get_height()
            current_ci = scenario_cis[j]
            
            if height > 0:
                ci_offset = 0 if np.isnan(current_ci) else current_ci
                text_y = height + ci_offset + (height * 0.02)
                
                ax.text(
                    bar.get_x() + bar.get_width() / 2.,
                    text_y,
                    value_fmt.format(height),
                    ha='center', va='bottom',
                    fontsize=9, rotation=45, fontweight='bold'
                )

    ax.margins(y=0.15)
    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    plt.close()
    print(f"  Saved: '{output_file}'")

# ==========================================
# PLOT 1 — Average RTT (ms)
# ==========================================
def plot_avg_rtt():
    print("\nPlotting Avg End-to-End Delay...")
    plot_data = {}
    error_data = {}
    
    for label, filepath in CSV_FILES.items():
        extracted = load_column_with_ci(filepath, "Overall_Avg_RTT(ms)", "CI_Avg_RTT(ms)") 
        p_data, e_data = [], []
        for n in EXPECTED_NODES:
            if n not in extracted:
                print(f"[CRITICAL] Missing {n} node data in {label}. Check simulation logs.")
                return
            p_data.append(extracted[n][0])
            e_data.append(extracted[n][1])
        plot_data[label] = p_data
        error_data[label] = e_data

    plot_bars(plot_data, error_data, "Average End-to-End Delay (ms)", OUTPUT_AVG_RTT)

# ==========================================
# PLOT 2 — Total RTT (ms)
# ==========================================
def plot_total_rtt():
    print("\nPlotting Total End-to-End Delay...")
    plot_data = {}
    error_data = {}
    
    for label, filepath in CSV_FILES.items():
        extracted = load_column_with_ci(filepath, "Overall_Total_RTT(ms)", "CI_Total_RTT(ms)") 
        p_data, e_data = [], []
        for n in EXPECTED_NODES:
            if n not in extracted:
                print(f"[CRITICAL] Missing {n} node data in {label}. Check simulation logs.")
                return
            p_data.append(extracted[n][0])
            e_data.append(extracted[n][1])
        plot_data[label] = p_data
        error_data[label] = e_data

    plot_bars(plot_data, error_data, "Total End-to-End Delay (ms)", OUTPUT_TOTAL_RTT)

if __name__ == "__main__":
    print("End-to-End Delay Graph Generator")
    plot_avg_rtt()
    plot_total_rtt()
    print("\nDone.")