import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# FILE CONFIGURATIONS
# ==========================================
CSV_FILES = {
    "Normal": "normal/normalattacker_victim.csv",                 
    "Zero Counter": "zero_counter/zero_counter_attacker_victim.csv", 
    "CC Request": "cc_request/cc_request_attacker_victim.csv"        
}

# 👇 Yahan aap decide kar sakte hain ki kaunse scenario ka graph banana hai
SELECTED_SCENARIO = "CC Request"  # Isko "Normal" ya "Zero Counter" karke dusre graphs bana sakte hain

TARGET_CSV = CSV_FILES[SELECTED_SCENARIO]
OUTPUT_IMAGE = f"Attacker_vs_Victim_TX_{SELECTED_SCENARIO.replace(' ', '_')}.png"

EXPECTED_NODES = [20, 40, 60, 80, 100] 

# Scenarios / Entities ki mapping (Column Name, Color, Node ID Column)
ENTITIES = {
    "Attacker Node": ("Attacker_TX(mJ)", "#d62728", "Attacker_NodeID"), # Red
    "Victim Node":   ("Victim_TX(mJ)", "#1f77b4", "Victim_NodeID")      # Blue
}

def get_attacker_victim_data(filepath):
    """CSV se Nodes, Energy aur unke Node IDs extract karta hai aur console pe print karta hai"""
    data = {"Attacker Node": {}, "Victim Node": {}}
    node_ids = {"Attacker Node": {}, "Victim Node": {}}
    
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            nodes = int(row["Nodes_Configured"])
            
            # Energy extract kar rahe hain
            data["Attacker Node"][nodes] = float(row["Attacker_TX(mJ)"])
            data["Victim Node"][nodes] = float(row["Victim_TX(mJ)"])
            
            # Node IDs extract kar rahe hain
            atk_id = int(row["Attacker_NodeID"])
            vic_id = int(row["Victim_NodeID"])
            node_ids["Attacker Node"][nodes] = atk_id
            node_ids["Victim Node"][nodes] = vic_id
            
            # ==========================================
            # EXACT CONSOLE OUTPUT FORMAT
            # ==========================================
            print(f"  ✅ Nodes = {nodes}")
            print(f"     Attacker (Node {atk_id}): TX={row['Attacker_TX(mJ)']} mJ | Total={row['Attacker_Total(mJ)']} mJ")
            print(f"     Victim   (Node {vic_id}): TX={row['Victim_TX(mJ)']} mJ | Total={row['Victim_Total(mJ)']} mJ")
            print()
            
    else:
        print(f"  [!] Error: File not found: {filepath}")
        
    return data, node_ids

def plot_graph():
    print(f"Reading '{TARGET_CSV}' for scenario '{SELECTED_SCENARIO}'...\n")
    
    # Extract data & IDs for both entities
    extracted_data, extracted_ids = get_attacker_victim_data(TARGET_CSV)
    
    plot_data = {}
    plot_ids = {}
    for label in ENTITIES.keys():
        # Missing data ko 0 se replace karte hue sequence banana
        plot_data[label] = [extracted_data[label].get(n, 0.0) for n in EXPECTED_NODES]
        plot_ids[label]  = [extracted_ids[label].get(n, 0) for n in EXPECTED_NODES]

    # Set up the bar graph positions
    x = np.arange(len(EXPECTED_NODES))
    width = 0.35 

    fig, ax = plt.subplots(figsize=(10, 6))

    # Plot bars dynamically
    offsets = [-width/2, width/2]
    bars_list = []
    
    for i, (label, (col_name, color, id_col)) in enumerate(ENTITIES.items()):
        bars = ax.bar(x + offsets[i], plot_data[label], width, label=label, color=color, edgecolor='black', zorder=3)
        bars_list.append((label, bars))

    # Graph Formatting
    ax.set_title(f'Attacker vs Victim-[{SELECTED_SCENARIO}] ', fontsize=14, fontweight='bold', pad=15)
    ax.set_xlabel('Number of Nodes', fontsize=12, fontweight='bold')
    ax.set_ylabel('Total TX Energy Consumption (mJ)', fontsize=12, fontweight='bold')
    
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_NODES])
    ax.legend(title="Node Type", fontsize=10, loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    # Label Text ke liye y-axis ko thoda badhana taaki text graph se baahar na jaaye
    all_heights = [bar.get_height() for _, bars in bars_list for bar in bars]
    max_h = max(all_heights) if all_heights else 100
    ax.set_ylim(0, max_h * 1.15)  # 15% extra headroom for multi-line text

    # Add exact values AND Node IDs on top of bars
    for label, bars in bars_list:
        for j, bar in enumerate(bars):
            height = bar.get_height()
            if height > 0:
                nid = plot_ids[label][j] # CSV se uthaya gaya Node ID
                
                # Format: "241.8 \n (Node 20)"
                text_str = f'{height:.1f}\n(Node {nid})'
                
                # Height + small uniform offset
                ax.text(bar.get_x() + bar.get_width()/2., height + (max_h * 0.015),
                        text_str, ha='center', va='bottom', fontsize=9, rotation=0, fontweight='bold')

    # Save Graph
    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Graph successfully saved as '{OUTPUT_IMAGE}'")

if __name__ == "__main__":
    plot_graph()