import pandas as pd
import matplotlib.pyplot as plt

SAMPLING_RATE = 100  
CSV_FILE = 'mpu_data.csv'

# Realiza a leitura do arquivo utilizando pandas (salva os dados em um dataframe)
try:
    df = pd.read_csv(CSV_FILE)
except FileNotFoundError:
    print(f"Erro: Arquivo '{CSV_FILE}' não encontrado.")
    exit()
except Exception as e:
    print(f"Erro ao ler o arquivo CSV: {e}")
    exit()


# Cria coluna de tempo (em segundos)
df['tempo'] = df['num_amostra'] / SAMPLING_RATE

# Configuração dos gráficos
plt.figure(figsize=(12, 8))

# Gráfico de Aceleração
plt.subplot(2, 1, 1)
plt.plot(df['tempo'], df['accel_x'], 'r-', label='Accel X', alpha=0.8)
plt.plot(df['tempo'], df['accel_y'], 'g-', label='Accel Y', alpha=0.8)
plt.plot(df['tempo'], df['accel_z'], 'b-', label='Accel Z', alpha=0.8)

plt.title('Aceleração nos Eixos XYZ')
plt.ylabel('Aceleração (raw)')
plt.grid(True, linestyle=':', alpha=0.7)
plt.legend(loc='upper right')
plt.tight_layout(pad=3.0)

# Gráfico de Giroscópio
plt.subplot(2, 1, 2)
plt.plot(df['tempo'], df['gyro_x'], 'r-', label='Gyro X', alpha=0.8)
plt.plot(df['tempo'], df['gyro_y'], 'g-', label='Gyro Y', alpha=0.8)
plt.plot(df['tempo'], df['gyro_z'], 'b-', label='Gyro Z', alpha=0.8)

plt.title('Giroscópio nos Eixos XYZ')
plt.xlabel('Tempo (s)')
plt.ylabel('Velocidade Angular (raw)')
plt.grid(True, linestyle=':', alpha=0.7)
plt.legend(loc='upper right')

plt.tight_layout()
plt.show()