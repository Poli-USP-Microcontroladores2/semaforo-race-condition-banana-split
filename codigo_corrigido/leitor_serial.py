
import serial
import csv
import re
import time
import sys
from datetime import datetime

# --- Configurações ---
PORTA_SERIAL = 'COM28'   # ajuste conforme sua porta
BAUD_RATE = 115200
ARQUIVO_CSV = f"dados.csv"

# Exemplo de linha esperada:
# "ΔT=10 ms | X=0.013672 | Y=-0.976562 | Z=0.062500"
PADRAO_DADOS = re.compile(
    r'ΔT\s*=\s*(\d+)\s*ms\s*\|\s*X\s*=\s*(-?\d+\.\d+)\s*\|\s*Y\s*=\s*(-?\d+\.\d+)\s*\|\s*Z\s*=\s*(-?\d+\.\d+)'
)

def ler_e_salvar_serial():
    try:
        with serial.Serial(PORTA_SERIAL, BAUD_RATE, timeout=1) as ser:
            print("=" * 70)
            print(f"Conectado à porta {PORTA_SERIAL} ({BAUD_RATE} baud)")
            print(f"Arquivo de saída: {ARQUIVO_CSV}")
            print("=" * 70)
            print("\nAguardando estabilização do microcontrolador (3 s)...")
            
            time.sleep(3)
            ser.reset_input_buffer()
            
            print("✓ Iniciando captura de dados...")
            print("  Pressione Ctrl+C para parar\n")

            with open(ARQUIVO_CSV, 'w', newline='', encoding='utf-8') as f_csv:
                writer = csv.writer(f_csv)
                writer.writerow([
                    'Timestamp_PC (s)',
                    'DeltaT_uC (ms)',
                    'DeltaT_uC (s)',
                    'Acel_X (m/s²)',
                    'Acel_Y (m/s²)',
                    'Acel_Z (m/s²)'
                ])

                contador = 0
                tempo_inicial = time.time()

                while True:
                    try:
                        linha = ser.readline().decode('utf-8', errors='ignore').strip()
                        if not linha:
                            continue

                        match = PADRAO_DADOS.search(linha)
                        if match:
                            delta_ms = int(match.group(1))
                            x, y, z = map(float, match.groups()[1:])
                            timestamp_pc = time.time() - tempo_inicial

                            writer.writerow([
                                f"{timestamp_pc:.3f}",
                                delta_ms,
                                f"{delta_ms/1000:.3f}",
                                x, y, z
                            ])
                            f_csv.flush()

                            print(f"[{contador:05d}] ΔT={delta_ms:3d} ms | "
                                  f"X={x:+.6f} | Y={y:+.6f} | Z={z:+.6f}")
                            contador += 1
                        else:
                            # Filtra mensagens de inicialização, logs e outros textos
                            if not any(p in linha.lower() for p in ['fxos8700', 'config', 'erro', 'ready']):
                                print(f"(ignorado) {linha}")

                    except KeyboardInterrupt:
                        raise
                    except Exception as e:
                        print(f"⚠️ Erro ao processar linha: {e}")
                        continue

    except serial.SerialException as e:
        print(f"\n❌ Erro de comunicação serial: {e}")
        print("   Verifique se a porta está correta e se o dispositivo está conectado.")
        sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n\n{'='*60}")
        print(f"✓ Captura encerrada pelo usuário.")
        print(f"  Dados salvos em: {ARQUIVO_CSV}")
        print(f"{'='*60}")
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
