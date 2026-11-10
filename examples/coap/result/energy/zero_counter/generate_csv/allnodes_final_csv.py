import csv
import os
from collections import defaultdict

# ==========================================
# 1. CONFIGURATION
# ==========================================
SCENARIO_NAME = "cc_request"  
NODE_COUNTS = [20, 40, 60, 80, 100]

# ==========================================
# 2. CALCULATION LOGIC (Node-Level Averaging)
# ==========================================
def calculate_node_averages():
    print(f"Calculating final node averages for Scenario: {SCENARIO_NAME}...\n")
    
    # Process each node configuration file (20, 40, 60, 80, 100)
    for nodes in NODE_COUNTS:
        input_file = f"{nodes}_nodes_all_runs.csv"
        output_file = f"{nodes}_nodes_final_averages.csv"
        
        if not os.path.exists(input_file):
            print(f"  [!] Missing file: {input_file}. Skipping...")
            continue
            
        # Dictionary to accumulate energy for each Node ID
        # Format: node_data[Node_ID] = {'cpu': sum, 'tx': sum, ...}
        node_data = defaultdict(lambda: {
            'cpu': 0.0, 'lpm': 0.0, 'deep_lpm': 0.0, 
            'tx': 0.0, 'rx': 0.0, 'total': 0.0,
            'run_count': 0,
            'attacker_status': "NA",
            'victim_status': "NA"
        })
        
        # Read the intermediate multi-run CSV
        with open(input_file, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            
            for row in reader:
                nid = int(row["Node_ID"])
                
                # Add up energy for this specific node
                node_data[nid]['cpu'] += float(row["CPU(mJ)"])
                node_data[nid]['lpm'] += float(row["LPM(mJ)"])
                node_data[nid]['deep_lpm'] += float(row["Deep_LPM(mJ)"])
                node_data[nid]['tx'] += float(row["TX(mJ)"])
                node_data[nid]['rx'] += float(row["RX(mJ)"])
                node_data[nid]['total'] += float(row["Total_Energy(mJ)"])
                node_data[nid]['run_count'] += 1
                
                # Keep track of attacker/victim labels (if any run flags them)
                if row["Attacker_NodeID"] != "NA":
                    node_data[nid]['attacker_status'] = row["Attacker_NodeID"]
                if row["Victim_NodeIDs"] != "NA":
                    node_data[nid]['victim_status'] = row["Victim_NodeIDs"]
                    
        # Write the final averaged node data
        with open(output_file, 'w', newline='', encoding='utf-8') as out_f:
            writer = csv.writer(out_f)
            # CSV Header
            writer.writerow([
                "Node_ID", "Attacker_NodeID", "Victim_NodeIDs", 
                "Avg_CPU(mJ)", "Avg_LPM(mJ)", "Avg_Deep_LPM(mJ)", 
                "Avg_TX(mJ)", "Avg_RX(mJ)", "Avg_Total_Energy(mJ)",
                "Valid_Runs_Averaged"
            ])
            
            # Write data sorted by Node ID (1, 2, 3...)
            for nid in sorted(node_data.keys()):
                d = node_data[nid]
                rc = d['run_count']
                
                # Avoid division by zero just in case
                if rc > 0:
                    writer.writerow([
                        nid, 
                        d['attacker_status'], 
                        d['victim_status'],
                        round(d['cpu'] / rc, 3),
                        round(d['lpm'] / rc, 3),
                        round(d['deep_lpm'] / rc, 3),
                        round(d['tx'] / rc, 3),
                        round(d['rx'] / rc, 3),
                        round(d['total'] / rc, 3),
                        rc
                    ])
        print(f"  -> {nodes} Nodes: Averaged {rc} runs. Saved to '{output_file}'.")
        
    print("\nAll final node averages generated successfully!")

if __name__ == "__main__":
    calculate_node_averages()