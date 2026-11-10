import re
import os
import matplotlib.pyplot as plt

# ==========================================
# 1. FILE CONFIGURATION (YAHAN EDIT KAREIN)
# ==========================================
# Apni teeno log files ka exact path yahan dalein. 
# Agar files same folder mein hain, toh sirf file ka naam dein.
FILE_PATHS = {
    "Normal Scenario": "normal/log40.txt",         # E.g., "log40_normal.txt"
    "Zero Counter Attack": "zero_counter/log40.txt", # E.g., "log40_zerocounter.txt"
    "CC Request Attack": "cc_request/log40.txt"    # E.g., "log40_ccrequest.txt"
}

OUTPUT_IMAGE = "Victim_Energy_Comparison_LineGraph.png"

# ==========================================
# 2. HARDWARE PROFILE (Zolertia Z1)
# ==========================================
VOLTAGE = 3.0
I_TX    = 17.4
ENERGEST_SECOND = 1000000

# ==========================================
# 3. REGEX PATTERNS
# ==========================================
VICTIM_PAT = re.compile(r'Victim Node \(Decimal Node ID:\s*(\d+)\)')
PERIOD_PAT = re.compile(r'ID:(\d+).*?Period summary #(\d+)')
TX_PAT     = re.compile(r'ID:(\d+).*?Radio Tx\s*:\s+(\d+)/')

# Styles matching your requested image
STYLES = {
    "Normal Scenario": {"style": "-", "marker": "o"},
    "Zero Counter Attack": {"style": "--", "marker": "s"},
    "CC Request Attack": {"style": "-.", "marker": "^"}
}

def parse_victim_cumulative_energy(filename):
    """Log file ko padhkar Victim node ki cumulative energy aur time nikalta hai."""
    if not os.path.exists(filename):
        print(f"  [❌] MISSING: Cannot find file -> '{filename}'")
        return [], []

    victim_id = None
    
    # Step 1: Find Victim ID
    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            m = VICTIM_PAT.search(line)
            if m:
                victim_id = int(m.group(1))
                break
                
    if victim_id is None:
        print(f"  [⚠️] WARNING: 'Victim Node' tag not found in '{filename}'. Please check log format.")
        return [], []

    print(f"  [✅] File '{filename}' loaded. Detected Victim Node: {victim_id}")

    # Step 2: Extract Period-wise TX Energy
    period_energies = {}
    current_period = None
    
    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            m_period = PERIOD_PAT.search(line)
            if m_period and int(m_period.group(1)) == victim_id:
                current_period = int(m_period.group(2))
                continue
                
            m_tx = TX_PAT.search(line)
            if m_tx and int(m_tx.group(1)) == victim_id and current_period is not None:
                tx_ticks = int(m_tx.group(2))
                
                # Formula: E(J) = (Ticks / 1000000) * I * V / 1000
                energy_joules = ((tx_ticks / ENERGEST_SECOND) * I_TX * VOLTAGE) / 1000.0
                period_energies[current_period] = period_energies.get(current_period, 0.0) + energy_joules
                current_period = None 

    # Step 3: Cumulative Calculation
    periods = sorted(list(period_energies.keys()))
    if not periods:
        print(f"  [⚠️] WARNING: No period data found for Victim in '{filename}'.")
        return [], []

    times_sec = [0]
    cumulative_energies = [0.0]
    current_cumulative = 0.0
    
    for p in periods:
        current_cumulative += period_energies[p]
        time_s = (p + 1) * 60  
        times_sec.append(time_s)
        cumulative_energies.append(current_cumulative)
        
    return times_sec, cumulative_energies

def plot_line_graph():
    print("--- Reading Log Files ---\n")
    
    fig, ax = plt.subplots(figsize=(10, 6))
    data_plotted = False
    
    for label, filepath in FILE_PATHS.items():
        times, energies = parse_victim_cumulative_energy(filepath)
        
        if times and energies:
            data_plotted = True
            ax.plot(times, energies, color='black', 
                    linestyle=STYLES[label]["style"], marker=STYLES[label]["marker"], 
                    linewidth=1.5, markersize=7, label=label)
            print(f"       -> Extracted {len(times)-1} minutes of data.\n")

    if not data_plotted:
        print("=========================================================")
        print("[!] ERROR: No graph generated because no valid data was found.")
        print("Please check Section 1 in the code and verify your file names/paths.")
        print("=========================================================")
        return

    # ==========================================
    # GRAPH FORMATTING
    # ==========================================
    ax.set_xlabel('Time (seconds)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Energy Consumption (Joules)', fontsize=14, fontweight='bold')
    
    ax.tick_params(axis='both', which='major', labelsize=12)
    ax.grid(True, linestyle='-', alpha=0.7)
    ax.legend(fontsize=12, loc='upper left')

    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"✅ Graph successfully saved as '{OUTPUT_IMAGE}'")

if __name__ == "__main__":
    plot_line_graph()