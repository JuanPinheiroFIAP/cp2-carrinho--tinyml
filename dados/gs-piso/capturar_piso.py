"""Coleta vibração do piso com o carrinho e salva no formato do Edge Impulse.

Fala com o sketch src/arduino/gs_coleta_piso pela serial. Para cada
"corrida" o carrinho apita 3 vezes, anda reto alguns segundos e manda
as leituras do MPU-6050 (200 por segundo). Cada corrida vira um arquivo:

    amostras/<classe>.<numero>.csv

O Edge Impulse pega o rótulo do nome do arquivo (parte antes do 1º ponto).

Uso (feche o Monitor Serial da IDE antes):
    python capturar_piso.py --classe liso --corridas 10
    python capturar_piso.py --classe irregular --corridas 10
    python capturar_piso.py --classe degrau --corridas 10
    python capturar_piso.py --classe parado --corridas 5

A classe "parado" não liga os motores. Nas outras, o carrinho anda para a
frente gravando e depois volta de ré sozinho até o começo (a volta não é
gravada). Com --sem-volta ele só anda para a frente e você leva ele de volta.
"""

import argparse
import re
import sys
import time
from pathlib import Path

import serial

CABECALHO = "timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ"

# O começo (arrancada) e o fim (freada) da corrida não representam o piso
DESCARTAR_INICIO_MS = 700
DESCARTAR_FIM_MS = 300

LINHA_DADOS = re.compile(r"^\d+(,-?\d+\.\d+){6}$")
LINHA_FIM = re.compile(r"^#FIM amostras=(\d+) falhas=(\d+)")

# andar: 0 = parado, 1 = só frente, 2 = frente e volta de ré até o começo
MODO_PARADO = 0
MODO_FRENTE = 1
MODO_IDA_E_VOLTA = 2


def esperar_pronto(ser, tempo_maximo_s=20):
    """Abrir a porta reinicia a placa: espera ela responder ao "P"."""
    fim = time.time() + tempo_maximo_s
    ultimo_envio = 0.0

    while time.time() < fim:
        if time.time() - ultimo_envio > 1.0:
            ser.write(b"P\n")
            ultimo_envio = time.time()

        linha = ser.readline().decode("utf-8", errors="ignore").strip()

        if "OK GS-COLETA" in linha:
            return True

    return False


def coletar_uma(ser, duracao_ms, modo):
    """Pede uma corrida e devolve (linhas_de_dados, amostras, falhas).

    No modo ida e volta, espera o carrinho voltar de ré ao começo antes
    de devolver (a volta não é gravada).
    """
    ser.reset_input_buffer()
    ser.write(f"R,{duracao_ms},{modo}\n".encode())

    linhas = []
    fim = time.time() + duracao_ms / 1000 + 15
    resultado = None

    while time.time() < fim:
        linha = ser.readline().decode("utf-8", errors="ignore").strip()

        if LINHA_DADOS.match(linha):
            linhas.append(linha)
            continue

        achou = LINHA_FIM.match(linha)

        if achou:
            resultado = (linhas, int(achou.group(1)), int(achou.group(2)))
            break

    if resultado is None:
        raise RuntimeError("o carrinho nao terminou a coleta a tempo")

    if modo == MODO_IDA_E_VOLTA:
        fim = time.time() + duracao_ms / 1000 + 8

        while time.time() < fim:
            linha = ser.readline().decode("utf-8", errors="ignore").strip()

            if linha == "#VOLTOU":
                break
        else:
            print("  aviso: nao recebi o aviso de que o carrinho voltou")

    return resultado


def recortar(linhas, duracao_ms, descartar_inicio_ms, descartar_fim_ms):
    """Tira arrancada e freada e recomeça o tempo do zero."""
    manter = []

    for linha in linhas:
        tempo = int(linha.split(",", 1)[0])

        if descartar_inicio_ms <= tempo < duracao_ms - descartar_fim_ms:
            manter.append(linha)

    if not manter:
        return []

    inicio = int(manter[0].split(",", 1)[0])

    saida = []

    for linha in manter:
        tempo, resto = linha.split(",", 1)
        saida.append(f"{int(tempo) - inicio},{resto}")

    return saida


def proximo_numero(pasta, classe):
    numeros = []

    for arquivo in pasta.glob(f"{classe}.*.csv"):
        achou = re.fullmatch(rf"{re.escape(classe)}\.(\d+)\.csv", arquivo.name)

        if achou:
            numeros.append(int(achou.group(1)))

    return max(numeros, default=0) + 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--classe", required=True)
    parser.add_argument("--corridas", type=int, default=10)
    parser.add_argument("--duracao", type=int, default=5000, help="ms por corrida")
    parser.add_argument("--pausa", type=float, default=2.0, help="segundos entre corridas")
    parser.add_argument("--porta", default="COM8")
    parser.add_argument(
        "--descartar-inicio",
        type=int,
        default=DESCARTAR_INICIO_MS,
        help="ms do começo a jogar fora (arrancada); use 0 para não cortar",
    )
    parser.add_argument(
        "--descartar-fim",
        type=int,
        default=DESCARTAR_FIM_MS,
        help="ms do fim a jogar fora (freada); use 0 para não cortar",
    )
    parser.add_argument(
        "--sem-volta",
        action="store_true",
        help="so anda para a frente (voce leva o carrinho de volta na mao)",
    )
    args = parser.parse_args()

    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.classe):
        sys.exit("A classe so pode ter letras, numeros, - e _ (sem pontos nem espacos).")

    pasta = Path(__file__).resolve().parent / "amostras"
    pasta.mkdir(exist_ok=True)

    if args.classe == "parado":
        modo = MODO_PARADO
    elif args.sem_volta:
        modo = MODO_FRENTE
    else:
        modo = MODO_IDA_E_VOLTA

    try:
        ser = serial.Serial(args.porta, 460800, timeout=0.5)
    except serial.SerialException as erro:
        print(f"ERRO ao abrir {args.porta}: {erro}")
        print("Feche o Monitor Serial da IDE e tente de novo.")
        sys.exit(1)

    with ser:
        if not esperar_pronto(ser):
            sys.exit("A placa nao respondeu. O sketch gs_coleta_piso esta gravado?")

        print(f"Placa pronta. Classe '{args.classe}', {args.corridas} corridas.")

        for i in range(args.corridas):
            if i > 0:
                time.sleep(args.pausa)

            linhas, amostras, falhas = coletar_uma(ser, args.duracao, modo)

            dados = recortar(
                linhas, args.duracao, args.descartar_inicio, args.descartar_fim
            )

            numero = proximo_numero(pasta, args.classe)

            arquivo = pasta / f"{args.classe}.{numero:03d}.csv"

            arquivo.write_text(CABECALHO + "\n" + "\n".join(dados) + "\n", encoding="utf-8")

            aviso = ""

            if amostras and falhas / amostras > 0.02:
                aviso = "  <-- MUITAS FALHAS DE LEITURA (fio solto?)"

            print(
                f"[{i + 1}/{args.corridas}] {arquivo.name}: "
                f"{len(dados)} linhas, {falhas} falhas de leitura{aviso}"
            )

    print("Pronto.")


if __name__ == "__main__":
    main()
