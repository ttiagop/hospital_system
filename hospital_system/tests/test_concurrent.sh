#!/bin/bash
PIPE=/tmp/input_pipe
CMD_FILE="tests/sample_commands/commands_complex.txt"

echo "=== TESTE CONCORRÊNCIA (Stock + Cirurgias Múltiplas) ==="
rm -f $PIPE

./hospital_system &
PID=$!
sleep 2

if [ -p $PIPE ]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ $line =~ ^#.*$ ]] || [[ -z $line ]] && continue
        
        echo "A enviar: $line"
        echo "$line" > $PIPE
        # Pausa muito curta para simular concorrência (chegam quase juntos)
        sleep 0.1 
    done < "$CMD_FILE"
    
    echo ">> A aguardar resolução de bloqueios (25s)..."
    sleep 25
    echo "STATUS" > $PIPE
    sleep 1
fi

kill -SIGINT $PID
wait $PID