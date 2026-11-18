# Corridas descartadas

Estas corridas foram tiradas de `amostras/` porque o carrinho **não andou reto**:
o desvio padrão do giroscópio no eixo Z ficou acima de 8 graus/s (nas corridas boas
fica abaixo de 5). Isso indica que ele desviou ou raspou na parede, e a vibração
medida não representa só o piso.

O critério usa o giroscópio, e não o nível de vibração, para não escolher os dados
pela resposta que se espera do modelo.

| Arquivo | Desvio do giro em Z (graus/s) | Observação |
|---|---|---|
| liso.001-bateu-na-parede.csv | - | pico de 8,2 g no eixo X: batida na parede |
| liso.008-carrinho-girou.csv | 22,5 | girou e desviou |
| liso.009-carrinho-girou.csv | 40,2 | girou e desviou |
| liso.010-carrinho-girou.csv | 35,3 | girou e desviou |
| irregular.005-carrinho-girou.csv | 17,8 | girou e desviou |
| irregular.007-carrinho-girou.csv | 17,3 | girou e desviou (pico de 1,49 g) |
| degrau.005-carrinho-girou.csv | 10,8 | girou e o pico foi fraco (0,95 g) |
