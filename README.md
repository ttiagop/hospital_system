1| # Sistema Integrado de Gestão Hospitalar (SIGH)
2| 
3| ## Descrição
4| Este projeto consiste numa simulação avançada de um ambiente hospitalar desenvolvida em linguagem C para sistemas Linux. O sistema utiliza uma arquitetura multiprocesso para gerir a admissão de pacientes e serviços hospitalares.
5| 
6| O projeto foca-se na aplicação prática de conceitos de Sistemas Operativos, nomeadamente:
7| * **Gestão de Processos e Threads:** Ciclo de vida de pacientes e serviços hospitalares.
8| * **IPC (Inter-Process Communication):** Message Queues, Shared Memory, Named Pipes e sinais.
9| * **Sincronização:** Mutexes, Semáforos POSIX e Variáveis de Condição para gestão de recursos partilhados e prevenção de *deadlocks*.
10| 
11| ## Funcionalidades
12| * **5 Processos Principais:** Main, Triagem, Bloco Operatório, Farmácia e Laboratório.
13| * **Simulação de Pacientes:** Cada paciente é uma *thread* independente com estabilidade que decai com o tempo.
14| * **Farmácia Inteligente:** Gestão de stock com reposição automática (threads monitoras) e espera passiva (condition variables).
15| * **Laboratório Sequencial:** Suporte para exames complexos (PREOP) que exigem execução sequencial em máquinas diferentes.
16| * **Monitorização:** Logs detalhados, snapshots estatísticos e tratamento gracioso de encerramento (`SIGINT`).
17| 
18| ## Requisitos
19| * Sistema operativo Linux (ou compatível com POSIX).
20| * Compilador `gcc`.
21| * Ferramenta `make`.
22| * Bibliotecas: `pthread` e `rt`.
23| 
24| ## Compilação
25| 
26| ### Para compilar o projeto, execute o seguinte comando na raiz:
27| 
28| make
29| 
30| ### Isto irá gerar:
31| 
32|     O executável hospital_system.
33| 
34|     A pasta obj/ com os ficheiros objeto.
35| 
36|     As pastas results/ e logs/ para outputs do sistema.
37| 
38| ### Para limpar os ficheiros compilados e remover recursos IPC (em caso de erro):
39| 
40| make clean
41| 
42| ### Como Executar
43| 
44|     Iniciar o sistema: No terminal principal, execute:
45| 
46|     ./hospital_system
47| 
48|     Interagir com o sistema: O sistema cria um named pipe em /tmp/input_pipe. Deve abrir um segundo terminal para enviar comandos.
49| 
50| ### Lista de Comandos de Teste
51| 
52| ### Admissão de Paciente (Urgente):
53| 
54| echo "EMERGENCY PAC_MARIA init: 0 triage: 1 stability: 90 tests: [HEMO] meds: [ANALGESICO_A]" > /tmp/input_pipe
55| 
56| ### Admissão de Paciente (Normal):
57| 
58| echo "EMERGENCY PAC_JOAO init: 0 triage: 4 stability: 100 tests: [] meds: [VITAMINA_N]" > /tmp/input_pipe
59| 
60| ### Agendar Cirurgia Complexa (Requer Lab Sequencial):
61| 
62| echo "SURGERY PAC_CARDIO init: 0 type: CARDIO scheduled: 10 urgency: HIGH tests: [PREOP] meds: [ANESTESICO_C]" > /tmp/input_pipe
63| 
64| ### Agendar Consulta:
65| 
66| echo "APPOINTMENT PAC_CONSULTA init: 0 scheduled: 50 doctor: ORTHO tests: []" > /tmp/input_pipe
67| 
68| ### Verificar Estatísticas (consola):
69| 
70| echo "STATUS ALL" > /tmp/input_pipe
71| 
72| ### Pedido Manual de Reposição (Farmácia):
73| 
74| echo "RESTOCK ANALGESICO_A quantity: 500" > /tmp/input_pipe
75| 
76| ### Sinais do Sistema
77| 
78|     SIGINT (Ctrl+C): Encerramento gracioso (limpeza de memória partilhada, filas e semáforos).
79| 
80|     SIGUSR1: Imprime as estatísticas atuais na consola.
81|     
82|         kill -SIGUSR1 $(pidof hospital_system)
83| 
84|     SIGUSR2: Gera um ficheiro de snapshot na pasta results/.
85| 
86|         kill -SIGUSR2 $(pidof hospital_system)
87| 
88|     SIGCHLD: Monitoriza a terminação de processos filhos para evitar zombies.
89| 
90| ### Estrutura de Ficheiros
91| 
92|     src/: Código-fonte (.c).
93| 
94|     include/: Cabeçalhos (.h) organizados modularmente.
95| 
96|     config/: Ficheiro de configuração.
97| 
98|     Makefile: Automação da compilação.
99| 
100| ### Autoria
101| 
102| Trabalho realizado no âmbito da unidade curricular de Sistemas Operativos.
103| 
104| Tiago Polónio - 30039
105| Gonçalo Fernandes - 29996
106| 
