#!/bin/bash

# ==========================================
# 1. SETUP VARIABLES 
# ==========================================
COOJA_DIR="/home/amritesh/contiki-ng-5.1_rpl/tools/cooja"
CSC_DIR="/home/amritesh/contiki-ng-5.1_rpl/examples/coap/simulations-multi-attacker"

# Base directory for logs[cite: 9]
BASE_LOG_DIR="/home/amritesh/contiki-ng-5.1_rpl/examples/coap/result-multi-attacker/logs"

ATTACKER_DIR="/home/amritesh/contiki-ng-5.1_rpl/examples/coap/coap-example-attacker"
SERVER_DIR="/home/amritesh/contiki-ng-5.1_rpl/examples/coap/coap-example-server" 
CLIENT_DIR="/home/amritesh/contiki-ng-5.1_rpl/examples/coap/coap-example-client" 

SCENARIOS=("normal" "cc_request" "zero_counter")
# Aapke screenshots mein attack1, attack2, attack3 hain, isliye array ko 1, 2, 3 kar diya hai[cite: 9]
ATTACKERS=(1 3 5) 
TOTAL_RUNS=5 # Screenshot mein 5 runs dikh rahe hain[cite: 10]
NODE=60

# ==========================================
# 2. PROGRESS CALCULATION SETUP
# ==========================================
NUM_SCENARIOS=${#SCENARIOS[@]}
NUM_ATTACKERS=${#ATTACKERS[@]}
TOTAL_SIMULATIONS=$((NUM_SCENARIOS * NUM_ATTACKERS * TOTAL_RUNS))
CURRENT_SIM=0

echo "STARTING MULTI-ATTACKER MASS AUTOMATION"
echo "Total Simulations to run: $TOTAL_SIMULATIONS"
echo "========================================================"

# ==========================================
# 3. AUTOMATION LOOP
# ==========================================
for s in "${SCENARIOS[@]}"
do
  # --- AUTO-CHANGE ATTACK MODE ---
  if [ "$s" == "normal" ]; then
    MODE=0
  elif [ "$s" == "zero_counter" ]; then
    MODE=1
  elif [ "$s" == "cc_request" ]; then
    MODE=2
  fi

  echo "========================================================"
  echo "  SCENARIO: $s | Setting ATTACK_MODE: $MODE"
  echo "========================================================"
  
  sed -i "s/#define ATTACK_MODE .*/#define ATTACK_MODE $MODE/" "$ATTACKER_DIR/project-conf.h"

  # Clean old builds
  cd "$ATTACKER_DIR" && make clean TARGET=cooja > /dev/null 2>&1
  cd "$SERVER_DIR" && make clean TARGET=cooja > /dev/null 2>&1
  cd "$CLIENT_DIR" && make clean TARGET=cooja > /dev/null 2>&1
  
  for a in "${ATTACKERS[@]}"
  do
    # Yahan attack1, attack2 folder create hoga[cite: 9]
    mkdir -p "$BASE_LOG_DIR/$s/attack${a}"
    
    for r in $(seq 1 $TOTAL_RUNS)
    do
      CURRENT_SIM=$((CURRENT_SIM + 1))
      PERCENT=$((CURRENT_SIM * 100 / TOTAL_SIMULATIONS))
      
      SEED="${RANDOM}${RANDOM}"
      CSC_FILE="$CSC_DIR/$s/${s}_${a}.csc"
      
      # Exact log file format as requested: log60_1.txt, log60_2.txt[cite: 10]
      LOG_FILE="$BASE_LOG_DIR/$s/attack${a}/log60_${r}.txt"
      
      echo ""
      echo "--------------------------------------------------------"
      echo "PROGRESS: [ $PERCENT% ] completed ($CURRENT_SIM out of $TOTAL_SIMULATIONS)"
      echo "RUNNING: Scenario: [$s] | Attackers: $a | Run: $r/$TOTAL_RUNS"
      echo "--------------------------------------------------------"
      
      if [ ! -f "$CSC_FILE" ]; then
          echo "Error: $CSC_FILE nahi mili! Skipping..."
          continue
      fi

      cd "$COOJA_DIR"
      rm -f COOJA.testlog
      
      ./gradlew run --no-daemon --args="--no-gui --random-seed=$SEED $CSC_FILE" > console_output.txt
      
      if [ -f "COOJA.testlog" ]; then
          mv COOJA.testlog "$LOG_FILE"
          echo "✅ Saved Data to: $s/attack${a}/log60_${r}.txt"
      else
          echo "❌ Error: COOJA.testlog nahi mili!"
      fi
      
    done
  done
done

echo ""
echo "PROGRESS: [ 100% ] - ALL $TOTAL_SIMULATIONS SIMULATIONS COMPLETED!"
