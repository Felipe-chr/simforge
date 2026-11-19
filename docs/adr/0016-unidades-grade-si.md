# ADR 0016 — Geometria em grade inteira e elétrica em SI

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0008

## Contexto

Misturar pixels, milímetros e volts no mesmo espaço gera erros de escala e
dificulta snapping, diff e determinismo.

## Decisão

- Coordenadas do esquemático usam unidades de grade inteiras (`grid_mm`,
  padrão 1,27 mm).
- Rotação é discreta (0/90/180/270).
- Parâmetros elétricos usam SI: V, A, Ω, F, H, Hz.
- Tempo em ns inteiros.
- Conversão para pixels acontece apenas no renderer.

## Consequências

- Snapping e diff estáveis.
- Sem drift de ponto flutuante no layout.
- O renderer precisa de zoom/pan consistente.
- Componentes com dimensões não múltiplas da grade exigem exceção documentada.

## Alternativas

- Milímetros em `double`: drift e comparações frágeis.
- Pixels: não portátil entre telas.
