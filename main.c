#include "FreeRTOS.h"      // Biblioteca principal do FreeRTOS
#include "task.h"          // Gerenciamento de tarefas
#include "queue.h"         // Para comunicação de eventos de botões
#include <stdio.h>         // Entrada/Saída padrão (para printf)
#include "pico/stdlib.h"   // Pico SDK padrão
#include <stdint.h>        // Tipos de dados padronizados
#include <stdbool.h>       // Tipo booleano
#include "hardware/gpio.h" // Interface GPIO

// --- Definições de Hardware (Pinos GPIO) ---
// **IMPORTANTE:** Ajuste estes pinos se a sua BitDogLab tiver uma configuração diferente!
#define LED_RGB_RED_PIN     13 // LED Vermelho
#define LED_RGB_GREEN_PIN   11 // LED Verde
#define LED_RGB_BLUE_PIN    12 // LED Azul
#define BUZZER_PIN          10 // Buzzer
#define BUTTON_A_PIN        5  // Botão A (Pedido de Pedestre)
#define BUTTON_B_PIN        6  // Botão B (Pedestre ja Atravessou)

// --- Tempos do Semáforo (em milissegundos) ---
#define GREEN_LIGHT_TIME_MS     5000 // Tempo do sinal verde para veículos
#define YELLOW_LIGHT_TIME_MS    1500 // Tempo do sinal amarelo para veículos
#define RED_LIGHT_TIME_MIN      4000 // Tempo MÍNIMO do sinal vermelho (pedestres)
#define RED_LIGHT_TIME_MAX      15000 // Tempo MÁXIMO do sinal vermelho (para não travar)

// --- Variáveis Globais e Filas ---
static QueueHandle_t xPedestrianRequestQueue = NULL; // Fila para sinalizar um pedido de pedestre (Botão A)
static QueueHandle_t xPedestrianCrossedQueue = NULL; // Fila para sinalizar que o pedestre atravessou (Botão B)

// --- Handles de Tarefas (opcional para controle mais fino) ---
TaskHandle_t xBuzzerSignalTaskHandle = NULL;

// --- Funções Auxiliares de GPIO ---
static void gpio_configure_output(uint gpio_pin) {
    gpio_init(gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_OUT);
}

static void gpio_configure_input_pullup(uint gpio_pin) {
    gpio_init(gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_IN);
    gpio_pull_up(gpio_pin);
}

static void rgb_off() {
    gpio_put(LED_RGB_RED_PIN, 0);
    gpio_put(LED_RGB_GREEN_PIN, 0);
    gpio_put(LED_RGB_BLUE_PIN, 0);
}

static void set_rgb_color(bool red, bool green, bool blue) {
    rgb_off(); // Garante que outras cores estejam desligadas
    gpio_put(LED_RGB_RED_PIN, red);
    gpio_put(LED_RGB_GREEN_PIN, green);
    gpio_put(LED_RGB_BLUE_PIN, blue);
}

// --- Tarefa: Sinal Sonoro do Buzzer ---
// Esta tarefa controla o som do buzzer para pedestres.
void buzzer_signal_task(void *pvParameters) {
    gpio_configure_output(BUZZER_PIN);

    while (true) {
        // O buzzer só toca se o LED estiver VERMELHO (sinal de pedestre)
        if (gpio_get(LED_RGB_RED_PIN) && !gpio_get(LED_RGB_GREEN_PIN) && !gpio_get(LED_RGB_BLUE_PIN)) {
            // Som de "pode atravessar" (bipes curtos)
            gpio_put(BUZZER_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(150));
            gpio_put(BUZZER_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(150));
        } else {
            gpio_put(BUZZER_PIN, 0); // Buzzer desligado em outros estados
            vTaskDelay(pdMS_TO_TICKS(100)); // Pequeno delay para não consumir CPU
        }
    }
}

// --- Tarefa: Semáforo de Veículos (LED RGB) ---
// Esta tarefa gerencia o ciclo de cores do semáforo e reage a pedidos de pedestres.
void traffic_light_task(void *pvParameters) {
    bool pedestrian_request_signal; // Variável local para receber da fila (Botão A)
    bool pedestrian_crossed_signal;   // Variável local para receber da fila (Botão B)

    // Configura os pinos do LED RGB como saídas
    gpio_configure_output(LED_RGB_RED_PIN);
    gpio_configure_output(LED_RGB_GREEN_PIN);
    gpio_configure_output(LED_RGB_BLUE_PIN);

    while (true) {
        // 1. Sinal VERDE para veículos
        set_rgb_color(0, 1, 0); // Verde
        printf("SEMAFORO: VERDE\n");

        // Espera pelo tempo normal do verde OU por um pedido de pedestre via fila.
        // Se um pedido chegar, o 'xQueueReceive' retorna pdPASS e a espera é interrompida.
        BaseType_t green_status = xQueueReceive(xPedestrianRequestQueue, &pedestrian_request_signal, pdMS_TO_TICKS(GREEN_LIGHT_TIME_MS));

        // AQUI ESTÁ A LÓGICA CRÍTICA: O semáforo SÓ passa para amarelo/vermelho se um pedido de pedestre foi feito.
        if (green_status == pdPASS && pedestrian_request_signal) {
            printf("PEDESTRE: Pedido detectado. Encurtando VERDE para AMARELO.\n");
            
            // 2. Transição para AMARELO
            set_rgb_color(1, 1, 0); // Amarelo
            printf("SEMAFORO: AMARELO\n");
            vTaskDelay(pdMS_TO_TICKS(YELLOW_LIGHT_TIME_MS));

            // 3. Sinal VERMELHO para veículos (pedestres atravessam)
            set_rgb_color(1, 0, 0); // Vermelho
            printf("SEMAFORO: VERMELHO (Pedestres podem atravessar. Aguardando CONFIRMACAO).\n");

            TickType_t red_start_time = xTaskGetTickCount();
            bool has_pedestrian_crossed = false;

            // Loop principal no estado VERMELHO:
            // Permanece vermelho até que o Botão B seja pressionado (confirmando travessia)
            // OU até o tempo máximo de vermelho expirar.
            while (!has_pedestrian_crossed && ((xTaskGetTickCount() - red_start_time) < pdMS_TO_TICKS(RED_LIGHT_TIME_MAX))) {
                // Tenta receber o sinal de "já atravessou" da fila (Botão B)
                // Espera por 100ms para o sinal, permitindo outras tarefas.
                if (xQueueReceive(xPedestrianCrossedQueue, &pedestrian_crossed_signal, pdMS_TO_TICKS(100)) == pdPASS) {
                    if (pedestrian_crossed_signal) { // Certifica-se de que o sinal é verdadeiro
                        // Garante que o tempo mínimo de vermelho seja respeitado antes de sair.
                        while ((xTaskGetTickCount() - red_start_time) < pdMS_TO_TICKS(RED_LIGHT_TIME_MIN)) {
                            vTaskDelay(pdMS_TO_TICKS(50));
                        }
                        printf("PEDESTRE: Sinal 'Ja Atravessei' recebido. Voltando ao ciclo.\n");
                        has_pedestrian_crossed = true; // Sai do loop do vermelho
                    }
                }
            }

            // Lógica para saída do vermelho
            if (!has_pedestrian_crossed) {
                printf("SEMAFORO: Tempo maximo de VERMELHO expirou. Voltando ao ciclo normal.\n");
            }

        } else {
            // Se green_status == pdFAIL, significa que o tempo do verde expirou e NENHUM pedido foi feito.
            // O semáforo permanece no verde e o loop recomeça.
            printf("SEMAFORO: Sem pedido de pedestre, mantendo VERDE.\n");
            // A tarefa só voltará a verificar um pedido após o próximo GREEN_LIGHT_TIME_MS
            // que já foi esperado pelo xQueueReceive anterior.
            // Não precisa de vTaskDelay aqui pois o xQueueReceive já introduziu o delay.
        }
    }
}

// --- Tarefa: Monitoramento de Botões ---
// Monitora os botões e envia sinais para as outras tarefas.
void button_monitor_task(void *pvParameters) {
    gpio_configure_input_pullup(BUTTON_A_PIN);
    gpio_configure_input_pullup(BUTTON_B_PIN);

    bool button_a_last_state = false;
    bool button_b_last_state = false;

    while (true) {
        bool button_a_current = !gpio_get(BUTTON_A_PIN); // Invertido: HIGH = não pressionado, LOW = pressionado
        bool button_b_current = !gpio_get(BUTTON_B_PIN);

        // --- Botão A: Pedido de Pedestre ---
        if (button_a_current && !button_a_last_state) {
            // Borda de subida (botão A pressionado)
            printf("BOTAO A: Pedido de pedestre acionado.\n");
            bool signal = true;
            // Envia um sinal para a fila (não bloqueia, 0 de timeout).
            // Se a fila estiver cheia (já tem um pedido), ignora o novo para evitar spam.
            xQueueSend(xPedestrianRequestQueue, &signal, 0);
        }

        // --- Botão B: Pedestre Já Atravessou ---
        if (button_b_current && !button_b_last_state) {
            // Borda de subida (botão B pressionado)
            // Este sinal só é relevante se o semáforo estiver VERMELHO para pedestres.
            // No entanto, para simplificar o ButtonMonitor, ele sempre envia o sinal.
            // A lógica de consumir o sinal será na traffic_light_task.
            printf("BOTAO B: Pedestre confirmou travessia (sinal enviado).\n");
            bool signal = true;
            xQueueSend(xPedestrianCrossedQueue, &signal, 0);
        }

        button_a_last_state = button_a_current;
        button_b_last_state = button_b_current;

        vTaskDelay(pdMS_TO_TICKS(50)); // Delay para debounce
    }
}

// --- Tarefa: Feedback Serial ---
// Imprime o status atual do sistema no console.
void status_feedback_task(void *pvParameters) {
    while (true) {
        printf("\n--- STATUS DO SISTEMA ---\n");
        printf("Pedido Pedestre Pendente (A): %s\n", uxQueueMessagesWaiting(xPedestrianRequestQueue) > 0 ? "SIM" : "NAO");
        printf("Sinal 'Ja Atravessei' (B) na fila: %s\n", uxQueueMessagesWaiting(xPedestrianCrossedQueue) > 0 ? "SIM" : "NAO");
        printf("-------------------------\n");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Atualiza a cada 2 segundos
    }
}

// --- Função Principal ---
int main() {
    stdio_init_all(); // Inicializa a comunicação serial (UART)

    printf("\n=== Inicializando Sistema de Semaforo Inteligente (v4) ===\n");
    printf("Configurando FreeRTOS...\n");

    // Cria a fila para pedidos de pedestre.
    // Capacidade para 1 item (um bool), pois só precisamos sinalizar um pedido pendente.
    xPedestrianRequestQueue = xQueueCreate(1, sizeof(bool));
    if (xPedestrianRequestQueue == NULL) {
        printf("ERRO: Falha ao criar a fila de pedidos de pedestre\n");
        for(;;);
    }

    // Cria a fila para sinal de "pedestre já atravessou".
    // Capacidade para 1 item, pois só precisamos do último estado do botão B.
    xPedestrianCrossedQueue = xQueueCreate(1, sizeof(bool));
    if (xPedestrianCrossedQueue == NULL) {
        printf("ERRO: Falha ao criar a fila de 'pedestre ja atravessou'\n");
        for(;;);
    }

    // Criação das Tarefas
    BaseType_t xReturned;

    // Tarefa para o Semáforo (LED RGB)
    xReturned = xTaskCreate(traffic_light_task, "TrafficLight", 256, NULL, 2, NULL);
    if (xReturned != pdPASS) {
        printf("ERRO: Falha ao criar a tarefa TrafficLight\n");
        for(;;);
    }

    // Tarefa para o Sinal Sonoro do Buzzer
    xReturned = xTaskCreate(buzzer_signal_task, "BuzzerSignal", 256, NULL, 2, &xBuzzerSignalTaskHandle);
    if (xReturned != pdPASS) {
        printf("ERRO: Falha ao criar a tarefa BuzzerSignal\n");
        for(;;);
    }

    // Tarefa para Monitorar os Botões (maior prioridade para resposta rápida)
    xReturned = xTaskCreate(button_monitor_task, "ButtonMonitor", 256, NULL, 3, NULL);
    if (xReturned != pdPASS) {
        printf("ERRO: Falha ao criar a tarefa ButtonMonitor\n");
        for(;;);
    }

    // Tarefa para Feedback Serial
    xReturned = xTaskCreate(status_feedback_task, "StatusFeedback", 512, NULL, 1, NULL);
    if (xReturned != pdPASS) {
        printf("ERRO: Falha ao criar a tarefa StatusFeedback\n");
        for(;;);
    }

    printf("Todas as tarefas criadas com sucesso!\n");
    printf("Semaforo operando. Pressione Botao A para pedir passagem (pedestre).\n");
    printf("Pressione Botao B para sinalizar 'Pedestre ja atravessou' DURANTE O SINAL VERMELHO.\n");

    // Inicia o escalonador do FreeRTOS.
    vTaskStartScheduler();

    // Este ponto nunca deve ser alcançado em um sistema FreeRTOS em operação normal.
    printf("ERRO FATAL: O escalonador FreeRTOS parou inesperadamente!\n");
    for(;;); // Loop infinito de segurança.
}
