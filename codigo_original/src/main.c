/*
 * Fala meu cria, temos aqui uma simulação baseada no Therac-25
 * Breve história sobre o Therac-25:
 * - Era uma máquina de radioterapia muuito moderna da AECL
 * - Um programador meio vibe coding das ideias não se preocupou com condições de corridas
 * - Semáforo e Mutex? Tô fora...
 * O problema é que isso era UMA MÁQUINA DE RADIOTERAPIA!!!
 * - Tentei simular uma situação onde o operador altera parâmetros durante a leitura do hardware
 * - A lógica final é simples: LED verde -> Tudo na paz, nada deu ruim
 * - Caso o LED fique vermelho, seu paciente provavelmente levou uma sarrafada de MUITOS eV.
 * Ferramentas usadas para forçar a race:
 * - loops de NOP para simular workload
 * - k_yield() para aumentar chance de troca de contexto
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(therac_forced, LOG_LEVEL_INF);

#define LED_VERDE_NODE    DT_ALIAS(led0)
#define LED_VERMELHO_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);

typedef struct { // Declaramos as variáveis do nosso querido THERAC-25
    volatile int seq;        // sequência do comando (incremental)
    volatile int mode;       // 0 = ELÉTRON, 1 = FÓTON
    volatile int energy;     // nível de energia (0..N)
    volatile bool apply;     // sinaliza que o operador terminou de montar o comando
} shared_cmd_t;

static shared_cmd_t cmd = { .seq = 0, .mode = 0, .energy = 0, .apply = false };
static volatile bool erro_detectado = false;

void operator_thread(void *, void *, void *);
void hardware_thread(void *, void *, void *);
void monitor_thread(void *, void *, void *);

K_THREAD_DEFINE(op_id, 1024, operator_thread, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(hw_id, 1024, hardware_thread, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(mon_id, 512, monitor_thread, NULL, NULL, NULL, 2, 0, 0);

// workload: loop de NOP para simular processamento que pode ser preemptado
static inline void workload_nop(int n)
{
    for (volatile int i = 0; i < n; ++i) {
        __asm__ volatile("nop");
        /* force opportunity to be preempted */
        if ((i & 0xFF) == 0) {
            k_yield();
        }
    }
}

int main(void)
{
    if (!gpio_is_ready_dt(&led_verde) || !gpio_is_ready_dt(&led_vermelho)) {
        LOG_ERR("LEDs não prontos no Devicetree. Verifique aliases led0/led1.");
        return 0;
    }

    gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    LOG_INF("Simulação Therac-style race condition iniciada.");
    gpio_pin_set_dt(&led_verde, 1);
    gpio_pin_set_dt(&led_vermelho, 0);

    return 0;
}

/* --- operator_thread
 * Monta o comando em etapas:
 * 1) incrementa seq (novo id)
 * 2) escreve mode
 * 3) trabalho (delay)
 * 4) escreve energy
 * 5) trabalho (delay)
 * 6) escreve apply = true
 *
 * Operador faz várias mudanças rápidas (flip mode) para forçar interleaving.
 */
void operator_thread(void *a, void *b, void *c)
{
    int local_seq = 0;
    int mode = 0;
    int energy = 0;

    while (!erro_detectado) {
        /* prepara novo comando */
        local_seq = cmd.seq + 1;         /* read-modify increment (não atômico intencional) */

        /* etapa 1: publish new seq early (simula implementação defeituosa) */
        cmd.seq = local_seq;

        /* etapa 2: escolhe modo (alternando) */
        mode = (local_seq & 1) ? 1 : 0; /* alterna PHOTON / ELECTRON */
        cmd.mode = mode;

        /* simula cálculo complexo e janela de preempção */
        workload_nop(8000);

        /* etapa 3: escreve energia (valor alto quando modo PHOTON to force hazard) */
        if (mode == 1) {
            energy = 100; /* alto (perigoso) */
        } else {
            energy = 10; /* baixo (seguro) */
        }
        cmd.energy = energy;

        /* mais processamento e oportunidade de preempção */
        workload_nop(6000);

        /* etapa final: aplica o comando */
        cmd.apply = true;

        LOG_INF("Operator: seq=%d mode=%s energy=%d",
                cmd.seq, (cmd.mode ? "PHOTON" : "ELECTRON"), cmd.energy);

        /* intervalo curto; operador pode emitir outro comando rápido */
        k_msleep(5);
    }
}

/* --- hardware_thread
 * Quando detecta apply==true, lê seq/mode/energy em etapas separadas (com delays),
 * e valida consistência: se seq mudou durante leitura → RACE.
 * Também verifica combinação perigosa (FÓTON + alta energia) para simular overdose.
 */
void hardware_thread(void *a, void *b, void *c)
{
    while (!erro_detectado) {
        if (!cmd.apply) {
            /* sem comando pronto, verifica frequentemente */
            k_msleep(1);
            continue;
        }

        /* lê seq no início */
        int seq_before = cmd.seq;

        /* delay na leitura para ampliar chance de interleaving */
        workload_nop(10000);

        /* lê outros campos (cada um com possível preempção) */
        int mode_read = cmd.mode;

        workload_nop(9000);

        int energy_read = cmd.energy;

        /* checa seq de novo */
        int seq_after = cmd.seq;

        LOG_INF("HW: seq_before=%d seq_after=%d mode=%d energy=%d",
                seq_before, seq_after, mode_read, energy_read);

        /* se seq mudou enquanto lia -> inconsistência → RACE */
        if (seq_before != seq_after) {
            LOG_ERR("RACE: comando modificado durante leitura (seq changed)!");
            erro_detectado = true;
            break;
        }

        /* simulate safety check: PHOTON + high energy is dangerous if no additional interlocks */
        if ((mode_read == 1) && (energy_read >= 90)) {
            LOG_ERR("OVERDOSE SIMULADA: Energia do foton=%d", energy_read);
            erro_detectado = true;
            break;
        }

        /* se passou nas checagens, aceita o comando (consumiu) */
        cmd.apply = false;

        /* prepara para próximo ciclo */
        k_msleep(1);
    }
}

/* --- monitor_thread: controla LEDs (verde while ok, vermelho permanently on when error) --- */
void monitor_thread(void *a, void *b, void *c)
{
    while (1) {
        if (erro_detectado) {
            gpio_pin_set_dt(&led_verde, 0);
            gpio_pin_set_dt(&led_vermelho, 1);
            LOG_ERR("LED vermelho acionado permanentemente (seu paciente morreu :/).");
            return; /* mantém vermelho permanentemente */
        } else {
            gpio_pin_set_dt(&led_verde, 1);
            gpio_pin_set_dt(&led_vermelho, 0);
        }
        k_msleep(10);
    }
}
