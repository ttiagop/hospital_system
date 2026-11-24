#!/bin/bash
PIPE=/tmp/input_pipe
CMD_FILE="tests/sample_commands/commands_basic.txt"

echo "=== TESTE BÁSICO (Operação Normal + Prioridade) ==="
rm -f $PIPE

# Iniciar Hospital
./hospital_system &
PID=$!
sleep 2

if [ -p $PIPE ]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        # Ignora linhas vazias ou comentários
        [[ $line =~ ^#.*$ ]] || [[ -z $line ]] && continue
        
        echo "A enviar: $line"
        echo "$line" > $PIPE
        sleep 1 # Pausa segura entre comandos básicos
    done < "$CMD_FILE"
    
    echo ">> Comandos enviados. A aguardar 20 segundos..."
    sleep 20
    echo "STATUS" > $PIPE
    sleep 1
else
    echo "ERRO: Pipe não existe."
fi

kill -SIGINT $PID
wait $PID