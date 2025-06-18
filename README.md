
# Tarefa: Roteiro de FreeRTOS #2 - EmbarcaTech 2025

Autor: **Gabriel Martins e João Fernandes**

Curso: Residência Tecnológica em Sistemas Embarcados

Instituição: EmbarcaTech - HBr

Campinas, Junho de 2025

---
**Sistema de Semáforo Inteligente com Pedestres**

Este projeto demonstra uma aplicação prática de um sistema embarcado utilizando Raspberry Pi Pico e FreeRTOS, simulando um semáforo de trânsito inteligente com interação de pedestres. O semáforo opera em um ciclo normal, mas pode ser influenciado por solicitações de pedestres, aguardando a confirmação da travessia antes de retornar ao seu estado padrão.

**Funcionalidades**

Semáforo de Veículos Controlado por LEDs (RGB): 
- Verde: Tráfego de veículos liberado (estado padrão).
- Amarelo: Sinal de atenção/transição antes do vermelho.
- Vermelho: Veículos parados, pedestres podem atravessar.
- Solicitação de Pedestre (Botão A): Ao ser pressionado, um pedestre solicita a travessia. Se o semáforo estiver verde, ele encurta o ciclo para amarelo e vermelho, priorizando o pedestre.
- Confirmação de Travessia (Botão B): Uma vez que o semáforo está vermelho para veículos, o pedestre pressiona este botão para indicar que já atravessou. O semáforo aguardará esta confirmação (ou um tempo máximo) antes de retornar ao ciclo verde.
- Sinal Sonoro para Pedestres (Buzzer): Emite bipes curtos e contínuos quando o semáforo está vermelho para veículos, indicando que os pedestres podem atravessar.
- Comunicação Assíncrona: Utiliza FreeRTOS Queues para gerenciar a comunicação entre as tarefas de monitoramento de botões e a tarefa principal do semáforo, garantindo um comportamento responsivo e não-bloqueante.
- Feedback Serial: Mensagens de status detalhadas são impressas no console serial para facilitar a depuração e o entendimento do fluxo do semáforo.

**Hardware Necessário**

- Placa BitDogLab (facilita as conexões com os periféricos integrados).
- LED RGB (conectado aos pinos GPIO 11, 12, 13, ou conforme sua BitDogLab).
- GP13: LED Vermelho
- GP11: LED Verde
- GP12: LED Azul
- Buzzer (conectado ao pino GPIO 10).
- Botão (Botão A) (conectado ao pino GPIO 5).
- Botão (Botão B) (conectado ao pino GPIO 6).

**Configuração do Ambiente**

Este projeto utiliza o Pico SDK e o FreeRTOS. Certifique-se de que seu ambiente de desenvolvimento esteja configurado corretamente.

- Instale o Pico SDK: Siga as instruções oficiais para instalar o Raspberry Pi Pico SDK em seu sistema.
- Configure o FreeRTOS: Integre o FreeRTOS ao seu projeto Pico. Geralmente, isso envolve clonar o repositório do FreeRTOS e configurá-lo no seu CMakeLists.txt.
- Clone este Repositório:
    Bash
    git clone <URL_DO_SEU_REPOSITORIO>
    cd <NOME_DO_SEU_REPOSITORIO>

**Compilação e Gravação**

- Crie um Diretório de Build:
    Bash

mkdir build
cd build

Configure o CMake:
Bash

cmake ..

Compile o Projeto:
Bash

    make

    Isso gerará o arquivo .uf2 na pasta build.

- Grave no Raspberry Pi Pico:
        Com o Pico desligado, pressione e segure o botão BOOTSEL.
        Conecte o Pico ao seu computador via USB.
        Solte o botão BOOTSEL. O Pico deve aparecer como um dispositivo de armazenamento (RPI-RP2).
        Arraste e solte o arquivo main.uf2 (ou o nome que seu executável tiver) da pasta build para o dispositivo RPI-RP2.
        O Pico irá reiniciar e o semáforo começará a funcionar.

**Como Usar**

Após a gravação do firmware, o sistema começará a operar:

- Sem Pedido de Pedestre: O semáforo permanecerá no VERDE indefinidamente, aguardando uma interação. O buzzer estará em silêncio.

- Solicitando Travessia (Botão A):
   - Quando o semáforo estiver VERDE, pressione e solte o Botão A.
   - O semáforo passará de VERDE para AMARELO, e então para VERMELHO.
   - Durante o sinal VERMELHO, o buzzer começará a emitir bipes, indicando que os pedestres podem atravessar.

- Confirmando Travessia (Botão B):
   - Enquanto o semáforo estiver VERMELHO (e o buzzer apitando), pressione e solte o Botão B.
   - Importante: O semáforo aguardará que o sinal VERMELHO permaneça por um tempo mínimo (definido por RED_LIGHT_TIME_MIN) antes de aceitar a confirmação.
   - Uma vez que o tempo mínimo tenha passado e o Botão B for pressionado, o semáforo voltará imediatamente para o VERDE, reiniciando o ciclo para os veículos.

- Tempo Limite para Travessia: Se o Botão B não for pressionado enquanto o semáforo estiver VERMELHO, o sistema esperará por um tempo máximo (definido por RED_LIGHT_TIME_MAX). Após esse tempo, o semáforo forçará o retorno ao VERDE para evitar bloqueios de tráfego.

**Monitoramento Serial**

Para observar o status interno do semáforo e os eventos dos botões, abra um monitor serial (como o Serial Monitor do VS Code, PuTTY, ou screen) conectado à porta serial do seu Raspberry Pi Pico (geralmente /dev/ttyACM0 no Linux/macOS ou COMx no Windows).

Contribuições
Sinta-se à vontade para explorar, modificar e melhorar este projeto. Sugestões e contribuições são sempre bem-vindas!
---

## 📜 Licença
GNU GPL-3.0.
