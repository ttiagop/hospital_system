#!/bin/bash
PIPE=/tmp/input_pipe
CMD_FILE="tests/sample_commands/commands_stress.txt"

echo "=== STRESS TEST (Carga Rápida) ==="
rm -f $PIPE

# Redireciona stdout para null para focar na performance, ou mantém para debug
./hospital_system > /dev/null 2>&1 &
PID=$!
sleep 2

if [ -p $PIPE ]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ $line =~ ^#.*$ ]] || [[ -z $line ]] && continue
        
        # Sem sleep! Envia o mais rápido possível
        echo "$line" > $PIPE
    done < "$CMD_FILE"
    
    echo ">> Carga máxima enviada. A processar (30s)..."
    sleep 30
    echo "STATUS" > $PIPE
    sleep 1
fi

kill -SIGINT $PID
wait $PID
echo "Stress test concluído."