import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# FILE CONFIGURATIONS
# ==========================================
CSV_FILES = {
    "Normal": "normal/normal_energy_final.csv",                 
    "Zero Counter": "zero_counter/zero_counter_energy_final.csv", 
    "CC Request": "cc_request/cc_request_energy_final.csv"        
}

OUTPUT_IMAGE = "Total_TX_Energy_Comparison.png"
# Nodes ki jagah attackers
EXPECTED_ATTACKERS = [1, 3, 5]

def get_total_avg_energy(filepath):
    """CSV se Attackers aur Total Energy ko dictionary me extract karta hai"""
    data = {}
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            # Column ka naam "Attacker_Count"
            attackers = int(row["Attacker_Count"])
           
            # total TX energy of network
            energy = float(row["Overall_Total_TX(mJ)"])
            
            data[attackers] = energy
    else:
        print(f"  [Warning] File not found: {filepath}")
    return data

def plot_graph():
    print("Reading CSV files and generating graph...")
    
    # Extract data for all scenarios
    plot_data = {}
    for label, (filepath) in CSV_FILES.items():
        extracted = get_total_avg_energy(filepath)
        # Match with expected attackers, put 0.0 if data is missing
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_ATTACKERS]

    # Set up the bar graph positions
    x = np.arange(len(EXPECTED_ATTACKERS))
    width = 0.25 # Teen bars ke liye 0.25 width theek hai

    fig, ax = plt.subplots(figsize=(10, 6))

    # Plot bars dynamically
    offsets = [-width, 0, width]
    bars_list = []
    
    # Monochrome colors aur hatch patterns
    hatches = ['//', '\\\\', 'xx']
    colors  = ['#2ca02c', '#1f77b4', '#d62728']

    bars_list = []
    for i, (label, (filepath)) in enumerate(CSV_FILES.items()):
        bars = ax.bar(
            x + offsets[i], plot_data[label], width,
            label=label, 
            color='white',         # Background white rahega (koi color fill nahi hoga)
            edgecolor=colors[i],   # Border aur Hatch lines par yeh color apply hoga
            hatch=hatches[i],      
            linewidth=1.5,         # Border aur lines thodi clear dikhane ke liye thickness badha di
            zorder=3
        )
        bars_list.append(bars)

        
    # X-Label update kar diya gaya hai
    ax.set_xlabel('Number of Attackers', fontsize=12, fontweight='bold')
    ax.set_ylabel('Total TX Energy Consumption (mJ)', fontsize=12, fontweight='bold')
    
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_ATTACKERS])
    ax.legend(title="Scenarios", fontsize=10, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    # Add exact values on top of bars
    for bars in bars_list:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                # Text offset ko directly bar ki height ka 2% bana diya
                ax.text(bar.get_x() + bar.get_width()/2., height + (height * 0.02),
                        f'{height:.1f}', ha='center', va='bottom', fontsize=9, rotation=45, fontweight='bold')

    # Save Graph
    ax.margins(y=0.15)
    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"\nGraph successfully saved as '{OUTPUT_IMAGE}'")

if __name__ == "__main__":
    plot_graph()