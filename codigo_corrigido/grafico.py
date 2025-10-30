import serial
import csv
import re
import time
import sys
import argparse
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np

# --- Parser de argumentos ---
parser = argparse.ArgumentParser(description="Captura de dados do acelerômetro via serial")
parser.add_argument("--grafico", type=str, default="True", help="Plotar gráfico 3D (True/False)")
parser.add_argument("--saida", type=str, default="dados.csv", help="Arquivo CSV de saída")
parser.add_argument("--tempo", type=float, default=10, help="Tempo de aquisição em segundos")
parser.add_argument("--porta", type=str, default="COM10", help="Porta serial")
parser.add_argument("--baud", type=int, default=115200, help="Velocidade da porta serial")
args = parser.parse_args()

GRAFICO = args.grafico.lower() == "true"
ARQUIVO_CSV = args.saida
TEMPO_AQUISICAO = args.tempo
PORTA_SERIAL = args.porta
BAUD_RATE = args.baud

# --- Regex para o novo formato de log ---
PADRAO_DADOS = re.compile(
    r'T=(\d+)\s*ms,'
    r'X=(-?\d+\.\d+),Y=(-?\d+\.\d+),Z=(-?\d+\.\d+),'
    r'Vx=(-?\d+\.\d+),Vy=(-?\d+\.\d+),Vz=(-?\d+\.\d+),'
    r'Px=(-?\d+\.\d+),Py=(-?\d+\.\d+),Pz=(-?\d+\.\d+)'
)

def ler_e_salvar_serial():
    try:
        with serial.Serial(PORTA_SERIAL, BAUD_RATE, timeout=1) as ser:
            print("=" * 70)
            print(f"Conectado à porta {PORTA_SERIAL} ({BAUD_RATE} baud)")
            print(f"Arquivo de saída: {ARQUIVO_CSV}")
            print(f"Tempo de aquisição: {TEMPO_AQUISICAO:.1f} s")
            print("=" * 70)
            print("\nAguardando estabilização do microcontrolador (3 s)...")
            time.sleep(3)
            ser.reset_input_buffer()
            print("✓ Iniciando captura de dados... (Ctrl+C para parar)\n")

            with open(ARQUIVO_CSV, 'w', newline='', encoding='utf-8') as f_csv:
                writer = csv.writer(f_csv)
                writer.writerow([
                    'DeltaT_uC (ms)',
                    'Acel_X', 'Acel_Y', 'Acel_Z',
                    'Vx', 'Vy', 'Vz',
                    'Px', 'Py', 'Pz'
                ])

                timestamps = []
                acel_x, acel_y, acel_z = [], [], []
                vx_list, vy_list, vz_list = [], [], []
                px_list, py_list, pz_list = [], [], []

                tempo_inicial = time.time()
                contador = 0

                while True:
                    # Limita pelo tempo de aquisição
                    if TEMPO_AQUISICAO and (time.time() - tempo_inicial) > TEMPO_AQUISICAO:
                        break

                    linha = ser.readline().decode('utf-8', errors='ignore').strip()
                    if not linha:
                        continue

                    match = PADRAO_DADOS.search(linha)
                    if match:
                        delta_ms = int(match.group(1))
                        x, y, z = map(float, match.groups()[1:4])
                        vx, vy, vz = map(float, match.groups()[4:7])
                        px, py, pz = map(float, match.groups()[7:10])

                        writer.writerow([delta_ms, x, y, z, vx, vy, vz, px, py, pz])
                        f_csv.flush()

                        timestamps.append(time.time() - tempo_inicial)
                        acel_x.append(x)
                        acel_y.append(y)
                        acel_z.append(z)
                        vx_list.append(vx)
                        vy_list.append(vy)
                        vz_list.append(vz)
                        px_list.append(px)
                        py_list.append(py)
                        pz_list.append(pz)

                        print(f"[{contador:05d}] T={delta_ms} ms | "
                              f"X={x:.6f}, Y={y:.6f}, Z={z:.6f} | "
                              f"Vx={vx:.6f}, Vy={vy:.6f}, Vz={vz:.6f} | "
                              f"Px={px:.6f}, Py={py:.6f}, Pz={pz:.6f}")
                        contador += 1

        print(f"\n✓ Captura encerrada. Dados salvos em: {ARQUIVO_CSV}")

        if GRAFICO and len(acel_x) > 1:
            print("✓ Gerando gráficos 3D...")

            fig = plt.figure(figsize=(18,5))

            # Aceleração
            ax1 = fig.add_subplot(131, projection='3d')
            ax1.plot(acel_x, acel_y, acel_z, color='r')
            ax1.set_title('Aceleração 3D')
            ax1.set_xlabel('X (m/s²)')
            ax1.set_ylabel('Y (m/s²)')
            ax1.set_zlabel('Z (m/s²)')

            # Velocidade
            ax2 = fig.add_subplot(132, projection='3d')
            ax2.plot(vx_list, vy_list, vz_list, color='g')
            ax2.set_title('Velocidade 3D')
            ax2.set_xlabel('Vx (m/s)')
            ax2.set_ylabel('Vy (m/s)')
            ax2.set_zlabel('Vz (m/s)')

            # Posição
            ax3 = fig.add_subplot(133, projection='3d')
            ax3.plot(px_list, py_list, pz_list, color='b')
            ax3.set_title('Posição 3D')
            ax3.set_xlabel('Px (m)')
            ax3.set_ylabel('Py (m)')
            ax3.set_zlabel('Pz (m)')

            plt.tight_layout()
            plt.show()

    except serial.SerialException as e:
        print(f"\n❌ Erro de comunicação serial: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n✓ Captura interrompida pelo usuário.")
    except Exception as e:
        print(f"\n❌ Erro inesperado: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    try:
        import serial
    except ImportError:
        print("❌ Biblioteca 'pyserial' não instalada.")
        print("   Instale com: pip install pyserial")
        sys.exit(1)

    ler_e_salvar_serial()
