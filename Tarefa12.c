#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/bootrom.h"
#include "ssd1306.h"

#include "ff.h"
#include "diskio.h"
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"
#include "sd_card.h"

/**
 * Definições de I2C para comunicação com o sensor 
 */
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

/**
 * Definições de I2C para comunicação com o display 
 */
#define I2C_PORT_DISP i2c1
#define SDA_DISP 14
#define SCL_DISP 15
#define ADDR_DISP 0x3C  

#define BUTTON_A 5 //Botão A
#define BUTTON_B 6 //Botão B
#define BUTTON_J 22 //Botão do Joystick

//Definição de Pinagem para os LEDS
#define RED 13
#define GREEN 11
#define BLUE 12

//Definição de pinagem, wrap e diviser e slice para uso do buzzer com PWM
#define BUZZER 10
#define WRAP 1000
#define DIVISER 62.5
uint slice;


//Variaveis e Macro para Debouncing
#define DEBOUNCE_TIME_MS 500
absolute_time_t current_time, last_time = 0;

//Endereço do sensor MPU6050
static int addr = 0x68;

//Flags para controle de ações com arquivos 
bool mount_sd_card = false; 
bool capturing_data = false;
bool show_file = false;
bool open_file = false;
bool mounted = false;
//ID para as amostras
uint data_index = 0;

ssd1306_t ssd;

static FIL file_global;

static char filename[20] = "mpu_data.csv";

/**
 * Protótipos de funções
 */
void show_message(char *text);
void clear_display();
void on_off_leds(bool red, bool green, bool blue); 
void init_buzzer();
void start_stop_buzzer(bool start);

/**
 * @brief Inicializa os leds RGB
 */
void init_leds()
{
    gpio_init(RED);
    gpio_set_dir(RED, GPIO_OUT);

    gpio_init(GREEN);
    gpio_set_dir(GREEN, GPIO_OUT);
    
    gpio_init(BLUE);
    gpio_set_dir(BLUE, GPIO_OUT);
}

static void mpu6050_reset()
{
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(100);
    buf[1] = 0x00;
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(10);
}

/**
 * @brief Faz a leitura do sensor MPU6050
 */
static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp)
{
    uint8_t buffer[6];
    uint8_t val = 0x3B;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        accel[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];

    val = 0x43;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        gyro[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];

    val = 0x41;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 2, false);
    *temp = (buffer[0] << 8) | buffer[1];
}

static sd_card_t *sd_get_by_name(const char *const name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}
static FATFS *sd_get_fs_by_name(const char *name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return &sd_get_by_num(i)->fatfs;
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

/**
 * @brief Monta o cartão SD
 */
static void run_mount()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_mount(p_fs, arg1, 1);
    if (FR_OK != fr)
    {
        start_stop_buzzer(true);
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = true;
    printf("Processo de montagem do SD ( %s ) concluído\n", pSD->pcName);
}

/**
 * @brief Desmonta o cartão SD
 */
static void run_unmount()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_unmount(arg1);
    if (FR_OK != fr)
    {
        start_stop_buzzer(true);
        printf("f_unmount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = false;
    pSD->m_Status |= STA_NOINIT; // in case medium is removed
    printf("SD ( %s ) desmontado\n", pSD->pcName);
}

/**
 * @brief Captura os dados do MPU6050 e os escreve no arquivo .csv
 */
FRESULT capture_data()
{
    FRESULT res;

    int16_t aceleracao[3], gyro[3], temp;
    mpu6050_read_raw(aceleracao, gyro, &temp);
    char buffer[1024];
    data_index++;
    sprintf(buffer, "%d,%d,%d,%d,%d,%d,%d,%d\n", data_index, 
            aceleracao[0], aceleracao[1], aceleracao[2], 
            gyro[0], gyro[1], gyro[2], temp);
    UINT bw;
    res = f_write(&file_global, buffer, strlen(buffer), &bw);
    return res;
}

/**
 * @brief Lê o conteúdo de um arquivo e o escreve no terminal
 */
void read_file(const char *filename)
{
    FIL file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK)
    {
        start_stop_buzzer(true);
        printf("[ERRO] Não foi possível abrir o arquivo para leitura. Verifique se o Cartão está montado ou se o arquivo existe.\n");

        return;
    }
    char buffer[1024];
    UINT br;
    printf("Conteúdo do arquivo %s:\n", filename);
    while (f_read(&file, buffer, sizeof(buffer) - 1, &br) == FR_OK && br > 0)
    {
        buffer[br] = '\0';
        printf("%s", buffer);
    }
    f_close(&file);
    printf("\nLeitura do arquivo %s concluída.\n\n", filename);
}

/**
 * @brief Função de callback para tratamento dos botões
 */
void gpio_irq_handler(uint gpio, uint32_t events)
{
    current_time = to_ms_since_boot(get_absolute_time());

    //Realiza o debounce para tratamento dos acionamentos dos botões
    if ((current_time - last_time) > DEBOUNCE_TIME_MS)
    {
        if(gpio == BUTTON_A){
            //Define se o cartão deve ser montado (true) ou desmontando (false)
            mount_sd_card = !mount_sd_card;
        }else if(gpio == BUTTON_B){
            //Define se os dados devem ser capturados (true) ou não (false)
            capturing_data = !capturing_data;
        }else if (gpio == BUTTON_J){
            //Aciona a função de exibir o arquivo no terminal quando definida como true
            show_file = !show_file;
        }

        last_time = current_time;
    }
}

int main()
{
    stdio_init_all();
    
    /**
     * Inicialização dos Botões
     */
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_init(BUTTON_J);
    gpio_set_dir(BUTTON_J, GPIO_IN);
    gpio_pull_up(BUTTON_J);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_J, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    init_leds();
    init_buzzer();

    sleep_ms(5000);
    time_init();

    /**
     * Inicializa o I2C e o Display ssd1306
     */
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_DISP);
    gpio_pull_up(SCL_DISP);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ADDR_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    // 
    // Limpa o display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    /**
     * Inicialização do I2C para comunicação com o sensor
     */
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    mpu6050_reset();

    printf("Iniciando Programa...\n");
    printf("\033[2J\033[H"); // Limpa tela
    printf("\n> ");
    stdio_flush();
    
    while (true)
    {
        //Acende o led Verde caso o cartão SD esteja montado (pode salvar dados) vermelho caso contrário
        (mounted && !capturing_data) ? on_off_leds(false, true, false) : on_off_leds(true, false, false);
        
        /**
         * Monta ou Desmonta o cartão SD, quando o botão A é pressionado
         */
        if (mount_sd_card == true && !mounted)
        {
            show_message("Montando sd");
            printf("\nIniciando Montagem do Cartão SD. Aguarde....\n");
            run_mount();
            mounted = true;
        }else if (mount_sd_card == false && mounted)
        {
            show_message("Desmontando sd");
            printf("\nIniciando Desmontagem do Cartão SD. Aguarde....\n");
            run_unmount();
            mounted = false;
        }

        /**
         * Inicia / Para a captura de dados (e salva o arquivo no final) quando o Botão B é pressionado
         */
        if (capturing_data && !open_file)
        {
            show_message("Abrindo Arquivo");
            printf("\nCriando Arquivo...\n");
            FRESULT res = f_open(&file_global, filename, FA_WRITE | FA_CREATE_ALWAYS);
            if (res != FR_OK)
            {
                start_stop_buzzer(true);
                printf("\n[ERRO] Não foi possível abrir o arquivo para escrita. Monte o Cartao.\n");
                capturing_data = false;
                show_message("Erro ao abrir");
            }else {
                open_file = true;
                 /**
                 * Escreve o cabeçalho do arquivo
                 */
                UINT bw;
                char buffer[500];
                sprintf(buffer, "num_amostra,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,temp\n");
                res = f_write(&file_global, buffer, strlen(buffer), &bw);
                
                if (res != FR_OK)
                {
                    start_stop_buzzer(true);
                    printf("\n[ERRO] Não foi possível escrever no arquivo. Monte o Cartao.\n");
                    f_close(&file_global);
                    capturing_data = false;
                    open_file = false;
                    show_message("Erro ao escrever");
                }
                show_message("Arquivo Aberto");
            }

        }else if (capturing_data && open_file) {
            printf("\nCapturando dados do MPU6050. Pressione o botão B para finalizar...\n");
            FRESULT res = capture_data();
            if (res != FR_OK)
            {
                start_stop_buzzer(true);
                printf("[ERRO] Não foi possível escrever no arquivo. Monte o Cartao.\n");
                f_close(&file_global);
                capturing_data = false;
                open_file = false;
                show_message("Erro ao Escrever");
            }
            show_message("Capturando dados");
            on_off_leds(true, true, false);
        }else if (!capturing_data && open_file)
        {
            f_close(&file_global);
            printf("\nDados do MPU6050 salvos no arquivo %s.\n\n", filename);    
            open_file = false;
            data_index = 0;
            show_message("Dados Salvos");
        }

        /**
         * Exibe o conteúdo do arquivo .csv
         */
        if (show_file)
        {
            show_message("Exibindo Arquivo");
            read_file(filename);
            show_file = false;

        }
        sleep_ms(500);
    }
    return 0;
}

/**
 * @brief Exibe uma mensagem no display
 */
void show_message(char *text)
{
    ssd1306_fill(&ssd, false);                            // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);        // Desenha um retângulo
    ssd1306_draw_string(&ssd, text, 14, 31);           // Escreve o texto no display  
    ssd1306_send_data(&ssd);
}

/**
 * @brief Limpa o conteúdo do display
 */
void clear_display()
{
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

/**
 * @brief Acende / Apaga os leds indicados
 */
void on_off_leds(bool red, bool green, bool blue)
{
    gpio_put(RED, red);
    gpio_put(GREEN, green);
    gpio_put(BLUE, blue);
}

/**
 * @brief Inicializa o PWM para uso do buzzer
 */
void init_buzzer()
{
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);

    slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_clkdiv(slice, DIVISER);
    pwm_set_wrap(slice, WRAP);
    pwm_set_gpio_level(BUZZER, 500);
    pwm_set_enabled(slice, false);
}

/**
 * @brief Dispara um alerta sonoro com o buzzer
 */
void start_stop_buzzer(bool start)
{
    if (start)
    {    
        pwm_set_enabled(slice, true);
        sleep_ms(200);
        pwm_set_enabled(slice, false);
    }else {
        pwm_set_enabled(slice, false);

    }
}