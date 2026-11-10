import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# CONFIGURATION
# ==========================================
CSV_FILES = {
    "Normal":       ("normal/normal_avg_buffer.csv",               "#2ca02c"),  # Green
    "Zero Counter": ("zero_counter/zero_counter_avg_buffer.csv",   "#1f77b4"),  # Blue
    "CC Request":   ("cc_request/cc_request_avg_buffer.csv",       "#d62728")   # Red
}

EXPECTED_NODES = [20, 40, 60, 80, 100]

OUTPUT_AVG_GLOBAL    = "Buffer_Avg_Global_Occupancy.png"
OUTPUT_TOTAL_GLOBAL  = "Buffer_Total_Global_Pkts.png"
OUTPUT_AVG_PARENT    = "Buffer_Avg_Parent_Occupancy.png"
OUTPUT_TOTAL_PARENT  = "Buffer_Total_Parent_Pkts.png"
OUTPUT_MAX_GLOBAL    = "Buffer_Max_Global_Seen.png"
OUTPUT_MAX_PARENT    = "Buffer_Max_Parent_Seen.png"


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
# GENERIC BAR PLOTTER
# ==========================================
def plot_bars(plot_data, ylabel, output_file, value_fmt="{:.4f}"):
    x       = np.arange(len(EXPECTED_NODES))
    width   = 0.25
    offsets = [-width, 0, width]

    fig, ax = plt.subplots(figsize=(10, 6))

    bars_list = []
    for i, (label, (filepath, color)) in enumerate(CSV_FILES.items()):
        bars = ax.bar(
            x + offsets[i], plot_data[label], width,
            label=label, color=color, edgecolor='black', zorder=3
        )
        bars_list.append(bars)

    ax.set_xlabel('Number of Nodes', fontsize=12, fontweight='bold')
    ax.set_ylabel(ylabel, fontsize=12, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_NODES])
    ax.legend(title="Scenarios", fontsize=10, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    for bars in bars_list:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax.text(
                    bar.get_x() + bar.get_width() / 2.,
                    height + (height * 0.02),
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
# PLOT 1 — Avg Global Queue Occupancy (%)
# ==========================================
def plot_avg_global():
    print("\n[BUFFER] Plotting Avg Global Occupancy...")
    plot_data = {}
    for label, (filepath, _) in CSV_FILES.items():
        extracted = load_column(filepath, "Avg_Global_Occupancy(%)")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Avg Global Queue Occupancy (%)", OUTPUT_AVG_GLOBAL, "{:.4f}%")


# ==========================================
# PLOT 2 — Total Global Pkts Queued
# ==========================================
def plot_total_global():
    print("\n[BUFFER] Plotting Total Global Pkts Queued...")
    plot_data = {}
    for label, (filepath, _) in CSV_FILES.items():
        extracted = load_column(filepath, "Total_Global_Pkts_Queued")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Total Global Pkts Queued", OUTPUT_TOTAL_GLOBAL, "{:.0f}")


# ==========================================
# PLOT 3 — Avg Parent Queue Occupancy (%)
# ==========================================
def plot_avg_parent():
    print("\n[BUFFER] Plotting Avg Parent Occupancy...")
    plot_data = {}
    for label, (filepath, _) in CSV_FILES.items():
        extracted = load_column(filepath, "Avg_Parent_Occupancy(%)")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Avg Parent Queue Occupancy (%)", OUTPUT_AVG_PARENT, "{:.4f}%")


# ==========================================
# PLOT 4 — Total Parent Pkts Queued
# ==========================================
def plot_total_parent():
    print("\n[BUFFER] Plotting Total Parent Pkts Queued...")
    plot_data = {}
    for label, (filepath, _) in CSV_FILES.items():
        extracted = load_column(filepath, "Total_Parent_Pkts_Queued")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Total Parent Pkts Queued", OUTPUT_TOTAL_PARENT, "{:.0f}")


# ==========================================
# PLOT 5 — Max Global Queue Seen (pkts)
# ==========================================
def plot_max_global():
    print("\n[BUFFER] Plotting Max Global Queue Seen...")
    plot_data = {}
    for label, (filepath, _) in CSV_FILES.items():
        extracted = load_column(filepath, "Max_Global_Seen(pkts)")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Max Global Queue Depth (pkts)", OUTPUT_MAX_GLOBAL, "{:.0f}")


# ==========================================
# PLOT 6 — Max Parent Queue Seen (pkts)
# ==========================================
def plot_max_parent():
    print("\n[BUFFER] Plotting Max Parent Queue Seen...")
    plot_data = {}
    for label, (filepath, _) in CSV_FILES.items():
        extracted = load_column(filepath, "Max_Parent_Seen(pkts)")
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    plot_bars(plot_data, "Max Parent Queue Depth (pkts)", OUTPUT_MAX_PARENT, "{:.0f}")


# ==========================================
# MAIN
# ==========================================
if __name__ == "__main__":
    print("=== Buffer Occupancy Graph Generator ===")
    plot_avg_global()
    plot_total_global()
    plot_avg_parent()
    plot_total_parent()
    plot_max_global()
    plot_max_parent()
    print("\nDone.")