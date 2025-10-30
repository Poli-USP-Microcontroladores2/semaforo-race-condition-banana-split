# PSI-Microcontroladores2-Aula06
Atividade: Resolução de Race Condition com Semáforo

- Integrantes: Arthur Londero (16855595) e Anita Cunha.
- Cenário escolhido: simplificação do THERAC-25, máquina de radioterapia.

## Descrição do cenário
O sistema simula uma máquina de radioterapia simplificada, inspirada no caso real do Therac-25.

No sistema, existem duas threads principais:

Operador (operator_thread) – responsável por configurar o modo e a energia do feixe (ELECTRON ou PHOTON) e aplicar o comando.

Hardware (hardware_thread) – responsável por ler os valores do comando e realizar a aplicação do feixe.

Uma terceira thread, monitor_thread, observa o estado do sistema e acende LEDs:

  Verde → operação normal.

  Vermelho → erro detectado (race condition ou leitura inconsistente).


## Descrição da Race Condition

Na versão sem mutex, as threads compartilham a estrutura cmd sem nenhum mecanismo de exclusão mútua.
A sequência de eventos que leva à falha é:

O operador começa a configurar um novo comando:

cmd.mode = 1 (FÓTON)
cmd.energy = 100
cmd.apply = true

Antes que termine, o hardware preempta a CPU e começa a ler cmd enquanto ele ainda está sendo modificado.

O hardware pode ler, por exemplo:

mode = 1 (FÓTON)

energy = 10 (parâmetro editado durante a leitura!)

Essa combinação é fisicamente impossível — um feixe de fótons de baixa energia não existe — mas o sistema executa o comando mesmo assim.

Essa inconsistência representa o mesmo tipo de falha lógica que ocorreu no Therac-25, onde a leitura incorreta de parâmetros resultou em dosagens letais de radiação.
No nosso sistema, a falha é detectada automaticamente e o LED muda para vermelho permanentemente.

## Solução: Exclusão Mútua com k_mutex

Para eliminar a race condition, foi utilizado o mecanismo de mutex do Zephyr (k_mutex).
O acesso à estrutura compartilhada cmd foi protegido com:

k_mutex_lock(&cmd_mutex, K_FOREVER);

(parte sensível aqui)

k_mutex_unlock(&cmd_mutex);



Assim:

- O operador só pode alterar o comando quando o hardware não está lendo.
- O hardware só pode ler quando o operador não está modificando.
- A atomicidade da operação é garantida, impedindo leituras parciais.


## 🎯 Objetivos da Atividade
Nesta atividade, os alunos deverão:
- Retomar o código gerado por IA em atividade anterior que apresenta **condições de corrida (race conditions)**.
- Trabalhar em **duplas ou trios**, com **avaliação cruzada interna** entre os integrantes do grupo.
- Aplicar **testes estruturados** com pré-condição, etapas de teste e pós-condição.
- Demonstrar como o problema de concorrência foi **identificado e resolvido** com uso de semáforo.

## 🧠 Etapas da Atividade

### **1️⃣ Revisão do Código Anterior**
- Cada integrante do grupo deverá **executar o código do colega** que contém a race condition original.
- Documentar:
  - O comportamento incorreto observado.
  - O momento em que o erro ocorre (condição específica, sequência de eventos, etc.).

### **2️⃣ Planejamento de Testes**
Para cada cenário, descreva **três casos de teste** seguindo o formato abaixo:

| Caso de Teste | Pré-condição | Etapas de Teste | Pós-condição Esperada |
|----------------|---------------|------------------|------------------------|
| 1 | ... | ... | ... |
| 2 | ... | ... | ... |
| 3 | ... | ... | ... |

### **3️⃣ Correção e Reteste**
- Corrigir o código para **eliminar a race condition**.
- Reexecutar **os mesmos casos de teste** e registrar:
  - As mudanças feitas.
  - O resultado após a correção com evidências (capturas de tela por exemplo).

### **4️⃣ Avaliação Interna (entre colegas do mesmo grupo)**
Cada integrante deverá:
1. Executar o código original do colega conforme os testes planejados.
2. Executar o código corrigido do colega conforme os testes planejados.
3. Conferir se as condições de corrida foram eliminadas.  
4. Registrar uma **avaliação curta** (pode ser no final do README):
   - O que estava errado antes.  
   - O que mudou com a correção.
   - Se o comportamento agora é estável.  

## 📦 Entregáveis

No repositório do grupo, incluir:
1. `README.md` (este arquivo) contendo:
   - Nome dos integrantes.
   - Cenário escolhido.
   - Casos de teste.
   - Descrição da race condition e da solução.
   - Avaliação de cada colega.
2. Código-fonte organizado (considerando um código original e um corrigido por cada integrante):
   - `codigo_original/`
   - `codigo_corrigido/`
3. Evidências (prints, logs, vídeos curtos, etc.) da execução dos testes.

---

**Repositório:** entregue via GitHub Classroom (um repositório por grupo) e um PDF do markdown final no Moodle.
