# TODO

Marcado à medida que se faz; cada item feito leva o commit.

## Performance

Medido com `bin/k2d_light_benchmark` (correr de dentro de `bin/`; `--dump` grava os frames).

- [x] Atlas de sombras: uma linha polar por luz, todas as arestas em 3 draws, profundidade exacta por texel, arestas só reenviadas quando mudam — 8 luzes / 200 occluders 5.0 → 2.6 ms, 25 600 → 24 draws (`518c11d`)
- [x] Pass de sombras devolve o framebuffer que encontrou (a cena desaparecia no editor) (`1b2491d`)
- [x] Profiler: física fora de `scene.update`, fases `physics.*` (`17e8fd6`)
- [x] Culling da vista no canvas: comandos e luzes fora da câmara; no máximo 8 luzes chegam ao shader (a 9.ª rebentava os arrays) (`2e2cb95`)
- [x] Culling antes do `onRender`: `renderBounds()` em sprite, nine-patch, rect, círculo, cápsula, polígono — 8000 rects em 9 ecrãs 3.6 → 1.3 ms (`ad89d80`)
- [x] Atlas de sombras em textura de profundidade com comparação em hardware: PCF5 em 3 leituras, PCF13 em 7 — 3.3 → 2.8 ms (`db96e97`)
- [x] Contadores no profiler: draw calls, vértices, itens, cortados, luzes, corpos, contactos — no runner (F5), no painel do editor e no MCP `profile` (`51178e0`)
- [x] Custo de visitar objectos: caixa do culling pelo centro e extensões do transform, sem 4 cantos — 8000 objectos 0.70 → 0.56 ms (`1d22520`, `c5bddb0`). Medido: percorrer 8000 custa 0.06 ms, ler caixa + transform 0.21 ms (falhas de cache)
- [ ] Caixas num array contíguo na Scene, actualizado quando um objecto se mexe ou muda de tamanho, para o culling não tocar na memória de cada componente (os ~0.2 ms que sobram; arriscado: um setter esquecido e o objecto some)
- [ ] Vértices de objectos parados: `render.expand` refaz tudo todos os frames (0.23 ms por 2000); guardar e refazer só quando o transform muda
- [x] Sombras paradas: o atlas só se redesenha quando muda uma aresta ou a linha de uma luz — 8 luzes 2.4 → 2.1 ms; sem occluders as linhas limpam-se (antes ficavam as sombras antigas) (`c3d2c03`)
- [ ] Não achatar occluders longe de luzes visíveis (hoje todas as arestas são transformadas todos os frames para detectar mudanças)
- [ ] Medir na web (WebGL2): o runner nunca foi medido no browser
- [ ] Export: texturas comprimidas (Basis/ASTC) e carregamento de níveis em fundo

## Luzes 2D

- [x] Cena de teste `bin/scenes/lights.k2dscene`
- [x] Ambiente da cena (Settings › Scene › ambient); a UI desenha sem luzes nem ambiente (`ad81ea5`)
- [ ] Falloff por luz (hoje a curva é quadrática fixa)
- [ ] Bias e suavidade da sombra por luz (hoje: nenhum / PCF5 / PCF13)
- [ ] Mais de 8 luzes por frame (o Doriax tem 16)

## Editor

- [x] Molduras da câmara, do ecrã e dos limites na vista da cena (`021950c`)
- [x] Settings › Project display: tamanho da janela do runner (`021950c`)
- [x] Game view: pré-visualizar o ecrã do projecto, o painel ou um preset (`021950c`)
- [x] Modos de escala da câmara: Crop, Stretch, Fit Width, Fit Height, Native (`8f4e7c9`)
- [x] Molduras de todas as câmaras (a activa mais forte, com ecrã e limites), resolução 0 = tamanho da janela, posição do objecto em edição, ecrã sem câmara à volta da origem — visto no ecrã com `import -window` (`981cc28`)
- [x] Rótulos das molduras com fundo escuro, legíveis sobre cenas iluminadas (`6284bda`)

## Decisões em aberto

- [ ] `demo/` (ainda ImGui/SDL, fora do build): portar ou apagar
- [ ] `../iGUI` (cópia solta): `git branch backup/pre-rebase-2026-10-13; git reset --hard origin/main`
