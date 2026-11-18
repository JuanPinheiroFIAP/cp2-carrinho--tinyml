# Roteiro do vídeo da Global Solution (até 6 minutos)

[← Voltar ao README](../README.md)

O enunciado pede um vídeo de **no máximo 6 minutos** que apresente o problema no contexto de CitizenTech, explique a solução e **demonstre o robô funcionando**, evidenciando o **sensor adicional**, a **integração com a IA** e as principais funcionalidades.

## Antes de gravar
- Carrinho **ligado em piso plano**, com as pilhas novas ou carregadas e (de preferência) alimentado por **power bank**, sem cabo USB.
- Um **trecho de teste montado**: caminho livre, um obstáculo (caixa) no meio, uma régua atravessada (degrau), um trecho de piso irregular (tapete ou papelão) e uma rampa de madeira.
- Notebook com o **Monitor Serial aberto** (115200), para mostrar as ocorrências e o relatório. O cabo USB pode ficar ligado só nesse plano.
- Controle de PS4 já pareado e **enquadrado na câmera**: a luz que pisca é o aviso principal. Se possível, uma segunda pessoa filma o controle de perto.
- **Ligue ou reinicie o carrinho sempre em piso plano e parado.** Ele calibra o nível nos primeiros 3 segundos; se for calibrado torto, ele aparece como `TOMBADO` e não anda (reinicie plano, ou aperte **Share** no controle).
- Um **obstáculo baixo** (livro fino, por exemplo) para a cena da batida, além da caixa para a cena do obstáculo.
- Grave em **vários planos curtos**: se um erra, você regrava só aquele.

## Roteiro

| Tempo | O que falar | O que mostrar |
|---|---|---|
| **0:00 – 0:40** | **Problema.** Obstáculos, degraus, pisos irregulares e rampas íngremes demais impedem a circulação de pessoas com deficiência e idosos, e só são conhecidos quando alguém reclama. Cidadão: pessoas com mobilidade reduzida. Território: calçadas e passagens urbanas. Ligação com os ODS 11, 10 e 9. | rosto do grupo ou imagens de calçadas; o nome dos integrantes |
| **0:40 – 1:30** | **Solução.** Um robô de baixo custo percorre o trecho sozinho e registra cada barreira. Apresente os componentes e destaque o **sensor novo, o MPU-6050**. | o carrinho de perto, apontando ESP32, HC-SR04 e MPU-6050 |
| **1:30 – 2:30** | **Como a IA funciona.** O MPU-6050 mede a vibração do carrinho; o grupo coletou dados em pisos liso, irregular e degrau; o modelo, treinado no Edge Impulse, classifica 1 segundo de vibração em 4 classes e roda dentro da ESP32. Resultado: 97,6% na validação e **77,3% no teste com dados que ele nunca viu**, com o piso irregular como ponto fraco. | tela do Edge Impulse (matriz de confusão e teste) e o gráfico dos dados |
| **2:30 – 4:30** | **Demonstração.** (1) Modo manual com o PS4: luz e vibração no controle. (2) Aperte △: **patrulha**. O carrinho anda sozinho e para diante da caixa, gira 90° e segue. (3) Passa pela régua: **degrau**, e a **luz do controle pisca em magenta**, o controle vibra e o buzzer dá 2 bipes. (4) Passa pelo tapete: **piso irregular** (luz amarela). (5) Na rampa: alerta de inclinação acima de 8,33% (luz laranja, 3 bipes). (6) Bate num obstáculo baixo: **batida**, o carrinho dá ré e vira (luz branca). (7) Incline o carrinho: **tombo** e motores desligados. (8) **Options**: relatório. Explique que **o aviso chega pelo controle, sem cabo e sem computador**. | o carrinho em ação e **o controle de PS4 bem no quadro** (para a luz aparecer), com o Monitor Serial ao lado mostrando as linhas `OCORRENCIA #...` e o relatório |
| **4:30 – 5:15** | **Testes e falhas.** O que deu errado e o que o grupo fez: motor em curto, buzzer ligado errado, **fio do ECHO com defeito** (o sensor de distância lia "sem eco" e só um teste isolado mostrou que o programa estava certo), carrinho batendo na parede, corridas descartadas pelo giroscópio, duas bibliotecas do Edge Impulse que não convivem. | tabela de falhas de [docs/09](09-gs-robo-inspetor.md#falhas-que-aconteceram-e-como-foram-resolvidas) |
| **5:15 – 5:50** | **Impacto e próximos passos.** Indicadores (barreiras por trecho, metros inspecionados, acerto do modelo). Mais dados de piso, um relatório por Wi-Fi e a integração com o restante do produto do grupo. | README e a Ficha Técnica |
| **5:50 – 6:00** | Fechamento, com os nomes de todos. | tela com o link do repositório |

## Depois de gravar
1. Suba o vídeo no **YouTube como "Público"** (os links precisam estar públicos e funcionando durante a correção, que vai até uma semana depois).
2. Cole o link no README, na linha **Vídeo demonstrativo**, e dê um `git push`.
3. Confira que o link abre **sem login**, de uma janela anônima.
4. Entre as 23h55 de **19/11**, **um integrante** envia o link do repositório no Teams da disciplina.
