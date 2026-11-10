import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# FILE CONFIGURATIONS
# ==========================================
CSV_FILES = {
    # Naye structure ke mutabiq paths
    "Normal": "normal/normal_energy_final.csv",                 
    "Zero Counter": "zero_counter/zero_counter_energy_final.csv", 
    "CC Request": "cc_request/cc_request_energy_final.csv"        
}

OUTPUT_IMAGE = "Average_TX_Energy_Comparison_with_CI.png"
# Nodes ki jagah attackers array
EXPECTED_ATTACKERS = [1, 3, 5] 

def get_total_avg_energy(filepath):
    """CSV se Attackers, Average Energy aur CI ko dictionary me extract karta hai"""
    data = {}
    if os.path.exists(filepath):
        df = pd.read_csv(filepath)
        for _, row in df.iterrows():
            # Ab column ka naam "Attacker_Count" hai
            attackers = int(row["Attacker_Count"])
           
            # Energy ke saath CI bhi read karna
            energy = float(row["Overall_Avg_TX(mJ)"]) 
            ci = float(row["CI_TX(mJ)"]) 

            # Tuple format mein (energy, ci) save karenge
            data[attackers] = (energy, ci)
    else:
        print(f"  [Warning] File not found: {filepath}")
    return data

def plot_graph():
    print("Reading CSV files and generating graph...")
    
    plot_data = {}
    error_data = {} 
    
    # Extract data for all scenarios
    for label, filepath in CSV_FILES.items():
        extracted = get_total_avg_energy(filepath)
        
        p_data = []
        e_data = []
        
        # Ab n variable attackers ke liye hai
        for n in EXPECTED_ATTACKERS:
            # STRICT CHECK: Agar kisi attacker case ka data nahi mila, to plotting rok do
            if n not in extracted:
                print(f"\n[CRITICAL ERROR] '{label}' scenario mein {n} attackers ka data MISSING hai!")
                print("Missing data ko 0.0 dikhana galat hai, isliye graph generation rok diya gaya hai.")
                print("Kripya apne simulation logs check karein.")
                return  # Program yahin ruk jayega
            
            p_data.append(extracted[n][0])
            e_data.append(extracted[n][1])
            
        plot_data[label] = p_data
        error_data[label] = e_data

    # Set up the bar graph positions
    x = np.arange(len(EXPECTED_ATTACKERS))
    width = 0.25 # Teen bars ke liye 0.25 width

    fig, ax = plt.subplots(figsize=(10, 6))

    offsets = [-width, 0, width]
    hatches = ['//', '\\\\', 'xx']
    colors  = ['#2ca02c', '#1f77b4', '#d62728']

    bars_list = []
    
    # Plot bars dynamically
    for i, (label, filepath) in enumerate(CSV_FILES.items()):
        bars = ax.bar(
            x + offsets[i], plot_data[label], width,
            yerr=error_data[label],   # Error bars ki height
            capsize=4,                # Error bar ki upper aur lower line (cap) ka size
            error_kw={'elinewidth': 1.5, 'capthick': 1.5, 'ecolor': 'black'}, # Error bar styling
            label=label, 
            color='white',         
            edgecolor=colors[i],   
            hatch=hatches[i],      
            linewidth=1.5,         
            zorder=3
        )
        bars_list.append(bars)
 
    # X-Label update kar diya gaya hai
    ax.set_xlabel('Number of Attackers', fontsize=12, fontweight='bold')
    ax.set_ylabel('Average TX Energy Consumption (mJ)', fontsize=12, fontweight='bold')
    
    ax.set_xticks(x)
    ax.set_xticklabels([str(n) for n in EXPECTED_ATTACKERS])
    ax.legend(title="Scenarios", fontsize=10, loc='upper left')
    ax.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)

    # Values likhne ke liye loop
    for i, bars in enumerate(bars_list):
        scenario_key = list(CSV_FILES.keys())[i]
        scenario_cis = error_data[scenario_key]
        
        for j, bar in enumerate(bars):
            height = bar.get_height()
            current_ci = scenario_cis[j]
            if height > 0:
                # Agar CI 'NaN' hai (jaise single run case mein), to text position ke liye ci_offset = 0 lenge
                ci_offset = 0 if np.isnan(current_ci) else current_ci
                
                # Text ko error bar ke cap ke thoda upar rakhenge
                text_y = height + ci_offset + (height * 0.02)
                ax.text(bar.get_x() + bar.get_width()/2., text_y,
                        f'{height:.1f}', ha='center', va='bottom', fontsize=9, rotation=45, fontweight='bold')

    # Save Graph
    ax.margins(y=0.15)
    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"\nGraph successfully saved as '{OUTPUT_IMAGE}'")

if __name__ == "__main__":
    plot_graph()