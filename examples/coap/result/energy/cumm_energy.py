import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# FILE CONFIGURATIONS
# ==========================================
CSV_FILES = {
    "Normal": ("normal/normal_avg.csv", "#2ca02c"),                 # Green
    "Zero Counter": ("zero_counter/zero_counter_avg.csv", "#1f77b4"), # Blue 
    "CC Request": ("cc_request/cc_request_avg.csv", "#d62728")        # Red
}

# Image ka naam update kar diya hai taaki line graph alag se save ho
OUTPUT_IMAGE = "Average_TX_Energy_LineGraph.png"
EXPECTED_NODES = [20, 40, 60, 80, 100] 

# Scenarios ke liye alag-alag marker styles (Circle, Square, Triangle)
MARKERS = {
    "Normal": "o",         # Circle
    "Zero Counter": "s",   # Square
    "CC Request": "^"      # Triangle
}

def get_total_avg_energy(filepath):
    """CSV se Nodes aur Average TX Energy ko dictionary me extract karta hai"""
    data = {}
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            nodes = int(row["Nodes_Configured"])
            energy = float(row["Avg_TX(mJ)"])
            data[nodes] = energy
    else:
        print(f"  [Warning] File not found: {filepath}")
    return data

def plot_graph():
    print("Reading CSV files and generating Line Graph...")
    
    # Extract data for all scenarios
    plot_data = {}
    all_values = [] # Global max nikalne ke liye taaki text offset sahi se set ho sake

    for label, (filepath, _) in CSV_FILES.items():
        extracted = get_total_avg_energy(filepath)
        # Match with expected nodes, put 0.0 if data is missing
        y_vals = [extracted.get(n, 0.0) for n in EXPECTED_NODES]
        plot_data[label] = y_vals
        all_values.extend(y_vals)

    # Agar koi data nahi mila toh default offset bachane ke liye
    max_y = max(all_values) if all_values else 100

    fig, ax = plt.subplots(figsize=(10, 6))

    # Plot lines dynamically
    for label, (filepath, color) in CSV_FILES.items():
        y_values = plot_data[label]
        
        # ax.plot ka use line draw karne ke liye kiya gaya hai
        ax.plot(EXPECTED_NODES, y_values, marker=MARKERS[label], color=color, 
                linewidth=2.5, markersize=8, label=label, zorder=3)

        # Har point ke upar uski exact value likhne ka logic
        for i, y in enumerate(y_values):
            if y > 0:
                # Text ko line ki color me hi rakha hai taaki asani se pehchana ja sake
                ax.text(EXPECTED_NODES[i], y + (max_y * 0.02), f'{y:.1f}', 
                        ha='center', va='bottom', fontsize=9, fontweight='bold', color=color)

    # Graph Formatting
    ax.set_title('Average Transmit (TX) Energy Per Node vs Network Size', fontsize=14, fontweight='bold', pad=15)
    ax.set_xlabel('Number of Nodes', fontsize=12, fontweight='bold')
    ax.set_ylabel('Average TX Energy per Node (mJ)', fontsize=12, fontweight='bold')
    
    ax.set_xticks(EXPECTED_NODES)
    ax.set_xticklabels([str(n) for n in EXPECTED_NODES])
    ax.legend(title="Scenarios", fontsize=10, loc='best') # 'best' position automatic adjust kar lega
    ax.grid(True, linestyle='--', alpha=0.6, zorder=0)    # Dono X aur Y axis par grid apply kiya hai

    # Save Graph
    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"\nGraph successfully saved as '{OUTPUT_IMAGE}'")

if __name__ == "__main__":
    plot_graph()