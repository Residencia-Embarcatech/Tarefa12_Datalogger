# Datalogger de Movimento com IMU (MPU6050)

Este projeto consiste em um datalogger embarcado desenvolvido com o Raspberry Pi Pico W (RP2040), capaz de capturar dados de movimento a partir de um sensor IMU (MPU6050), armazenando os dados em um cartão microSD no formato `.csv`. O sistema utiliza periféricos como display OLED, LEDs RGB, botões físicos e buzzer para interação e feedback ao usuário.

---

## Objetivo

Capturar dados de aceleração, giroscópio e temperatura via MPU6050, armazená-los em um arquivo `.csv` em um cartão microSD e exibir as operações no terminal e display OLED.

---

## Funcionalidades

- **Montagem/Desmontagem do cartão SD** com o botão A.
- **Captura contínua de dados do sensor MPU6050** com o botão B.
- **Visualização do conteúdo do arquivo `.csv`** no terminal com o botão do Joystick.
- Feedback visual via **LEDs RGB**:
  - **Vermelho**: Cartão SD não montado.
  - **Verde**: Cartão SD montado, pronto para captura.
  - **Amarelo (Verde + Vermelho)**: Captura de dados em andamento.
- Feedback sonoro com o **buzzer** em caso de erro (como falha ao montar o SD ou escrever no arquivo).
- Exibição de mensagens no **display OLED**.

---

## Lógica do Sistema

- O código principal roda em loop infinito (`main`), monitorando as flags que são modificadas por interrupções nos botões.
- O sensor MPU6050 é lido a cada ciclo de captura e os dados são salvos no arquivo `mpu_data.csv` com o seguinte formato:

```csv
num_amostra,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,temp
1,124,435,211,-18,304,765,30
...
```

- Ao final da captura (quando o botão B é pressionado novamente), o arquivo é automaticamente salvo.

---

## Execução

1. Pressione **Botão A** para montar o cartão SD.
2. Pressione **Botão B** para iniciar a captura dos dados.
3. Pressione novamente **Botão B** para encerrar a captura e salvar o arquivo.
4. Pressione **Botão do Joystick** para exibir os dados capturados no terminal.
5. Pressione novamente o **Botão A** para desmontar o cartão SD.
---

## Análise Externa dos Dados

1. Copie os dados exibidos no terminal ao visualizar o arquivo.
2. Cole no arquivo `.csv` localizado em `ArquivosDados/`.
3. Execute o script Python disponível na mesma pasta para visualizar os gráficos com os dados capturados.

## Compilação e Execução

1. **Pré-requisitos**:
   - Ter o ambiente de desenvolvimento para o Raspberry Pi Pico configurado (compilador, SDK, etc.).
   - CMake instalado.

2. **Compilação**:
   - Clone o repositório ou baixe os arquivos do projeto.
   - Navegue até a pasta do projeto e crie uma pasta de build:
     ```bash
     mkdir build
     cd build
     ```
   - Execute o CMake para configurar o projeto:
     ```bash
     cmake ..
     ```
   - Compile o projeto:
     ```bash
     make
     ```

3. **Upload para a placa**:
   - Conecte o Raspberry Pi Pico ao computador.
   - Copie o arquivo `.uf2` gerado para a placa.


