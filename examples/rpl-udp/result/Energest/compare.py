import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# FILE CONFIGURATIONS
# ==========================================
CSV_FILES = {
    "Normal": ("normal/Node_Details_log60.csv", "#2ca02c"),                 
    "Zero Counter": ("zero_counter/Node_Details_log60.csv", "#1f77b4"), 
    "CC Request": ("cc_request/Node_Details_log60.csv", "#d62728")        
}

OUTPUT_IMAGE = "All_Nodes_TX_Energy_Comparison.png"

def plot_graph():
    print("Reading CSV files and generating node-level comparison graph for ALL nodes...")
    
    # ---------------------------------------------------------
    # 1. Dynamically identify all unique nodes across all CSVs
    # ---------------------------------------------------------
    all_nodes = set()
    for label, (filepath, _) in CSV_FILES.items():
        if os.path.exists(filepath):
            df = pd.read_csv(filepath)
            all_nodes.update(df['Node_ID'].tolist())
            
    if not all_nodes:
        print("[!] No data found. Please check your CSV file paths.")
        return
        
    # Sort them sequentially (1, 2, 3 ... 40)
    nodes_to_plot = sorted(list(all_nodes))
    
    plot_data = {}
    node_labels = {}
    
    # ---------------------------------------------------------
    # 2. Extract Data from CSVs
    # ---------------------------------------------------------
    for label, (filepath, _) in CSV_FILES.items():
        if not os.path.exists(filepath):
            plot_data[label] = [0.0] * len(nodes_to_plot)
            continue
            
        df = pd.read_csv(filepath)
        data = {}
        labels = {}
        
        for _, row in df.iterrows():
            nid = int(row['Node_ID'])
            data[nid] = float(row['TX(mJ)'])
            
            # Check for Attacker/Victim to update x-axis labels
            attacker = str(row['Attacker_NodeID']).strip()
            victim = str(row['Victim_NodeID']).strip()
            
            if attacker != 'NA' and attacker != 'nan':
                labels[nid] = f"{nid}\n(Attacker)"
            elif victim != 'NA' and victim != 'nan':
                labels[nid] = f"{nid}\n(Victim)"
            else:
                labels[nid] = str(nid)
                
        # Store data aligning with the sorted nodes order
        plot_data[label] = [data.get(n, 0.0) for n in nodes_to_plot]
        
        # Ensure Attacker/Victim labels overwrite normal number labels
        for k, v in labels.items():
            if "\n" in v or k not in node_labels:
                node_labels[k] = v

    # Fallback to simple strings just in case
    for n in nodes_to_plot:
        if n not in node_labels:
            node_labels[n] = str(n)

    # Find the maximum energy value to position the bar text properly
    max_energy = max([max(vals) for vals in plot_data.values()])

    # ---------------------------------------------------------
    # 3. Plot the Graph
    # ---------------------------------------------------------
    x = np.arange(len(nodes_to_plot))
    width = 0.25 

    # Dynamic Width: Make the graph wider automatically if there are many nodes
    fig_width = max(15, len(nodes_to_plot) * 0.4)
    fig, ax = plt.subplots(figsize=(fig_width, 6))

    offsets = [-width, 0, width]
    bars_list = []
    
    for i, (label, (_, color)) in enumerate(CSV_FILES.items()):
        bars = ax.bar(x + offsets[i], plot_data[label], width, label=label, color=color, edgecolor='black', zorder=3)
        bars_list.append(bars)

    # Graph Formatting
    # ax.set_title(f'TX Energy Comparison Per Node ({len(nodes_to_plot)} Nodes Scenario)', fontsize=14, fontweight='bold', pad=15)
    ax.set_xlabel('Node ID', fontsize=12, fontweight='bold')
    ax.set_ylabel('Total TX Energy Consumption(mJ)', fontsize=12, fontweight='bold')
    
    # Apply custom labels
    x_labels = [node_labels[n] for n in nodes_to_plot]
    ax.set_xticks(x)
    
    # If 40+ nodes, a smaller font prevents the x-axis text from overlapping
    ax.set_xticklabels(x_labels, fontweight='bold', fontsize=9)
    
    ax.legend(title="Scenarios", fontsize=11, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    # Add exact values on top of bars (Rotated 90 degrees to fit tightly)
    for bars in bars_list:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax.text(bar.get_x() + bar.get_width()/2., height + (max_energy * 0.015),
                        f'{height:.1f}', ha='center', va='bottom', fontsize=7, rotation=90, fontweight='bold')

    # Save Graph
    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"\n✅ Graph successfully saved as '{OUTPUT_IMAGE}'")

if __name__ == "__main__":
    plot_graph()