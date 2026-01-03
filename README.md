# Sistema Integrado de Gestao Hospitalar (SIGH)

## Descrição
Este projeto consiste numa simulação avançada de um ambiente hospitalar desenvolvida em linguagem C para sistemas Linux. O sistema utiliza uma arquitetura multiprocesso para gerir a admissão de pacientes e serviços hospitalares.

O projeto foca-se na aplicacao pratica de conceitos de Sistemas Operativos, nomeadamente:
* **Gestao de Processos e Threads:** Ciclo de vida de pacientes e servicos hospitalares.
* **IPC (Inter-Process Communication):** Message Queues, Shared Memory, Named Pipes e Sinais.
* **Sincronizacao:** Mutexes, Semaforos POSIX e Variaveis de Condicao para gestao de recursos partilhados e prevencao de *deadlocks*.

## Funcionalidades
* **5 Processos Principais:** Main, Triagem, Bloco Operatorio, Farmacia e Laboratorio.
* **Simulacao de Pacientes:** Cada paciente e uma *thread* independente com estabilidade que decai com o tempo.
* **Farmacia Inteligente:** Gestao de stock com reposicao automatica (threads monitoras) e espera passiva (Condition Variables).
* **Laboratorio Sequencial:** Suporte para exames complexos (PREOP) que exigem execucao sequencial em maquinas diferentes.
* **Monitorizacao:** Logs detalhados, snapshots estatisticos e tratamento gracioso de encerramento (`SIGINT`).

## Requisitos
* Sistema Operativo Linux (ou compativel POSIX).
* Compilador `gcc`.
* Ferramenta `make`.
* Bibliotecas: `pthread` e `rt`.

## Compilação

### Para compilar o projeto, execute o seguinte comando na raiz:


make

### Isto ira gerar:

    O executavel hospital_system.

    A pasta obj/ com os ficheiros objeto.

    As pastas results/ e logs/ para outputs do sistema.

### Para limpar os ficheiros compilados e remover recursos IPC (em caso de erro):


make clean

### Como Executar

    Iniciar o Sistema: No terminal principal, execute:

    ./hospital_system

    Interagir com o Sistema: O sistema cria um Named Pipe em /tmp/input_pipe. Deve abrir um segundo terminal para enviar comandos.

### Lista de Comandos de Teste

### Admissão de Paciente (Urgente):


echo "EMERGENCY PAC_MARIA init: 0 triage: 1 stability: 90 tests: [HEMO] meds: [ANALGESICO_A]" > /tmp/input_pipe

### Admissão de Paciente (Normal):


echo "EMERGENCY PAC_JOAO init: 0 triage: 4 stability: 100 tests: [] meds: [VITAMINA_N]" > /tmp/input_pipe

### Agendar Cirurgia Complexa (Requer Lab Sequencial):


echo "SURGERY PAC_CARDIO init: 0 type: CARDIO scheduled: 10 urgency: HIGH tests: [PREOP] meds: [ANESTESICO_C]" > /tmp/input_pipe

### Agendar Consulta:


echo "APPOINTMENT PAC_CONSULTA init: 0 scheduled: 50 doctor: ORTHO tests: []" > /tmp/input_pipe

### Verificar Estatísticas (Consola):


echo "STATUS ALL" > /tmp/input_pipe

### Pedido Manual de Reposição (Farmacia):


echo "RESTOCK ANALGESICO_A quantity: 500" > /tmp/input_pipe

### Sinais do Sistema

    SIGINT (Ctrl+C): Encerramento gracioso (limpeza de memoria partilhada, filas e semaforos).

    SIGUSR1: Imprime as estatisticas atuais na consola.
    
        kill -SIGUSR1 $(pidof hospital_system)

    SIGUSR2: Gera um ficheiro de snapshot na pasta results/.


        kill -SIGUSR2 $(pidof hospital_system)

    SIGCHLD: Monitoriza a terminacao de processos filhos para evitar zombies.

### Estrutura de Ficheiros

    src/: Codigo fonte (.c).

    include/: Cabecalhos (.h) organizados modularmente.

    config/: Ficheiro de configuracao.

    Makefile: Automacao da compilacao.

### Autoria

Trabalho realizado no ambito da unidade curricular de Sistemas Operativos.

Tiago Polónio - 30039
Gonçalo Fernandes - 29996
