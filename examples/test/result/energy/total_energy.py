import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# FILE CONFIGURATIONS
# ==========================================
# CSV_FILES = {
#     "Normal": ("normal/normal_avg.csv", "#2ca02c"),                 
#     "Zero Counter": ("zero_counter/zero_counter_avg.csv", "#1f77b4"), 
#     "CC Request": ("cc_request/cc_request_avg.csv", "#d62728")        
# }


CSV_FILES = {
    "Normal": "normal/generate_csv/normal_energy_final.csv",                 
    "Zero Counter": "zero_counter/generate_csv/zero_counter_energy_final.csv", 
    "CC Request": "cc_request/generate_csv/cc_request_energy_final.csv"        
}

OUTPUT_IMAGE = "Total_TX_Energy_Comparison.png"
EXPECTED_NODES = [20, 40, 60, 80, 100] 

def get_total_avg_energy(filepath):
    """CSV se Nodes aur Average Energy ko dictionary me extract karta hai"""
    data = {}
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            # [FIXED] Column names ab pichli script ke hisaab se theek kar diye hain
            nodes = int(row["Nodes_Configured"])
           
            # toal energy      
          
            # total energy of network

            # energy = float(row["Overall_Total_Network_Energy(mJ)"])

            # total TX energy of network
            energy = float(row["Overall_Total_TX(mJ)"])

           

            
            data[nodes] = energy
    else:
        print(f"  [Warning] File not found: {filepath}")
    return data

def plot_graph():
    print("Reading CSV files and generating graph...")
    
    # Extract data for all scenarios
    plot_data = {}
    for label, (filepath) in CSV_FILES.items():
        extracted = get_total_avg_energy(filepath)
        # Match with expected nodes, put 0.0 if data is missing
        plot_data[label] = [extracted.get(n, 0.0) for n in EXPECTED_NODES]

    # Set up the bar graph positions
    x = np.arange(len(EXPECTED_NODES))
    width = 0.25 # Teen bars ke liye 0.25 width theek hai

    fig, ax = plt.subplots(figsize=(10, 6))

    # Plot bars dynamically
    offsets = [-width, 0, width]
    bars_list = []
    
        # --- UPDATE: Naye monochrome colors aur hatch patterns ---
    hatches = ['//', '\\\\', 'xx']
    colors  = ['#2ca02c', '#1f77b4', '#d62728']

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

        
    # ax.set_title('Average Transmit (TX) Energy Per Node vs Network Size', fontsize=14, fontweight='bold', pad=15)
    ax.set_xlabel('Number of Nodes', fontsize=12, fontweight='bold')
    ax.set_ylabel('Total TX Energy Consumption (mJ)', fontsize=12, fontweight='bold')

    
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_NODES])
    ax.legend(title="Scenarios", fontsize=10, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    # [FIXED] Add exact values on top of bars (Bina "CC Request" pe depend hue)
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