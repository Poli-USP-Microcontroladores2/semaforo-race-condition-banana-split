/*
 * Versão CORRIGIDA com k_mutex (remoção da race condition)
 * 
 * - Agora as seções críticas de leitura e escrita do comando compartilhado
 *   são protegidas por mutex, evitando que o operador altere os dados
 *   enquanto o hardware ainda está lendo.
 * - Viu o que a sua amada nuclear pode fazer com nossos pobres pacientes??
 * - É brincadeira, o problema foi puramente incompetência do engenheiro, tente não replicar.
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(therac_fixed, LOG_LEVEL_INF);

#define LED_VERDE_NODE    DT_ALIAS(led0)
#define LED_VERMELHO_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);

/* Comando compartilhado */
typedef struct {
    int seq;
    int mode;     /* 0 = ELETRON, 1 = FOTON */
    int energy;
    bool apply;
} shared_cmd_t;

static shared_cmd_t cmd = { .seq = 0, .mode = 0, .energy = 0, .apply = false };
static volatile bool erro_detectado = false;

K_MUTEX_DEFINE(cmd_mutex);

void operator_thread(void *, void *, void *);
void hardware_thread(void *, void *, void *);
void monitor_thread(void *, void *, void *);

K_THREAD_DEFINE(op_id, 1024, operator_thread, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(hw_id, 1024, hardware_thread, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(mon_id, 512, monitor_thread, NULL, NULL, NULL, 3, 0, 0);

static inline void workload_nop(int n)
{
    for (volatile int i = 0; i < n; ++i) {
        __asm__ volatile("nop");
        if ((i & 0xFF) == 0) k_yield();
    }
}

/* --- main --- */
int main(void)
{
    if (!gpio_is_ready_dt(&led_verde) || !gpio_is_ready_dt(&led_vermelho)) {
        LOG_ERR("LEDs não prontos no Devicetree. Verifique aliases led0/led1.");
        return 0;
    }

    gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    LOG_INF("Versão protegida com mutex iniciada (sem race condition).");
    gpio_pin_set_dt(&led_verde, 1);
    gpio_pin_set_dt(&led_vermelho, 0);

    return 0;
}

/* --- operator_thread --- */
void operator_thread(void *a, void *b, void *c)
{
    int local_seq = 0;
    int mode = 0;
    int energy = 0;

    while (!erro_detectado) {
        local_seq++;

        /* seção crítica protegida */
        k_mutex_lock(&cmd_mutex, K_FOREVER);

        cmd.seq = local_seq;
        cmd.mode = (local_seq & 1) ? 1 : 0;
        workload_nop(8000);

        if (cmd.mode == 1)
            cmd.energy = 100;
        else
            cmd.energy = 10;

        workload_nop(6000);
        cmd.apply = true;

        k_mutex_unlock(&cmd_mutex);

        LOG_INF("Operator: seq=%d mode=%s energy=%d",
                cmd.seq, (cmd.mode ? "PHOTON" : "ELECTRON"), cmd.energy);

        k_msleep(5);
    }
}

/* --- hardware_thread --- */
void hardware_thread(void *a, void *b, void *c)
{
    while (!erro_detectado) {
        k_mutex_lock(&cmd_mutex, K_FOREVER); /* protege leitura */

        if (!cmd.apply) {
            k_mutex_unlock(&cmd_mutex);
            k_msleep(1);
            continue;
        }

        int seq_before = cmd.seq;
        workload_nop(10000);
        int mode_read = cmd.mode;
        workload_nop(9000);
        int energy_read = cmd.energy;
        int seq_after = cmd.seq;

        cmd.apply = false; /* comando consumido */
        k_mutex_unlock(&cmd_mutex);

        LOG_INF("HW: seq_before=%d seq_after=%d mode=%d energy=%d",
                seq_before, seq_after, mode_read, energy_read);

        /* agora não deve mais ocorrer inconsistência */
        if (seq_before != seq_after) {
            LOG_ERR("RACE: comando modificado durante leitura (seq changed)!");
            erro_detectado = true;
            break;
        }

        if ((mode_read == 1) && (energy_read >= 90)) {
            LOG_WRN("Modo Foton com alta energia OK (seguro)");
        }

        k_msleep(1);
    }
}

void monitor_thread(void *a, void *b, void *c)
{
    while (1) {
        if (erro_detectado) {
            gpio_pin_set_dt(&led_verde, 0);
            gpio_pin_set_dt(&led_vermelho, 1);
            LOG_ERR("LED vermelho acionado permanentemente (erro detectado).");
            return;
        } else {
            gpio_pin_set_dt(&led_verde, 1);
            gpio_pin_set_dt(&led_vermelho, 0);
        }
        k_msleep(10);
    }
}
