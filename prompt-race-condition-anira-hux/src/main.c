#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

static int shared_counter = 0;

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5

void thread_function(void *arg1, void *arg2, void *arg3)
{
    int thread_id = (int)arg1;
    int temp;
    
    while (1) {
        temp = shared_counter;
        printk("Thread %d: leu valor %d\n", thread_id, temp);
        
        k_msleep(1);
        
        temp++;
        shared_counter = temp;
        printk("Thread %d: escreveu valor %d\n", thread_id, temp);
        
        k_msleep(10);
    }
}

K_THREAD_STACK_DEFINE(thread1_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread2_stack, STACK_SIZE);

struct k_thread thread1_data;
struct k_thread thread2_data;

int main(void)
{
    printk("Iniciando exemplo de race condition\n");
    
    k_thread_create(&thread1_data, thread1_stack,
                    K_THREAD_STACK_SIZEOF(thread1_stack),
                    thread_function,
                    (void *)1, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_create(&thread2_data, thread2_stack,
                    K_THREAD_STACK_SIZEOF(thread2_stack),
                    thread_function,
                    (void *)2, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);
    
    return 0;
}