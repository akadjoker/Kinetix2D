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
  - [x] Comandos movidos para a fila em vez de copiados: `render_queue` de 2000 rects 0.32 → 0.26 ms, 8000 em nove ecrãs 0.73 → 0.51 (`187b7ef`)
  - [x] Comparação da água no batching só quando ligada: era 6.6 % do frame no `perf`; `expand` 0.222 → 0.209 ms (`acb23e9`)
  - `perf` (2000 rects): `EmitPolygon` 17 %, `RectShape::onRender` 11 %, `DrawItems` 8 %, `AddItem` 6 %. O tempo do `onRender` está na primeira leitura do componente (falha de cache), não no código: o que sobra é memória espalhada. O passo seguinte a sério é o modo retido (itens guardados por componente, refeitos só quando mudam), com o risco de um setter esquecido
  - tentado e desfeito: saltar no canvas o culling dos itens que a cena já testou, e tirar as multiplicações de matrizes; o `expand` ficou 0.03 ms pior sem explicação. Voltar a isto com `perf` (`sudo sysctl kernel.perf_event_paranoid=1`)
- [x] Sombras paradas: o atlas só se redesenha quando muda uma aresta ou a linha de uma luz — 8 luzes 2.4 → 2.1 ms; sem occluders as linhas limpam-se (antes ficavam as sombras antigas) (`c3d2c03`)
- [ ] Não achatar occluders longe de luzes visíveis (hoje todas as arestas são transformadas todos os frames para detectar mudanças)
- [ ] Medir na web (WebGL2): o benchmark de luzes já corre no Chrome (`cmake --build build-web --target k2d_light_benchmark`, página em `build-web/bench/`); o `glFinish` do WebGL não espera pela GPU, mede-se com a leitura de um pixel. Falta o runner com um jogo a sério
  - limites no Chrome/ANGLE: 1024 vectores de uniform no fragment, blocos de 64 KB, textura 16384; o mínimo de GLES3/WebGL2 é 224 vectores e 16 KB
- [x] Medir em Android (Redmi Pad 2, Mali-G57 MC2, Android 16; `K2D_APK_TARGET=k2d_light_benchmark tools/android/build-apk.sh -`), alvo de 1280x720, contra o desktop (Radeon):
  - 2000 rects sem luzes 7.8 ms (1.3); com 8 luzes 16.5 (1.9); 8 com sombra e 200 occluders 19.6 (2.6); 16 com sombra, 12 visíveis, 27.2 (2.8); PCF13 29.6 (3.5)
  - o custo é o fragment shader: cada luz ~1.1 ms a 720p, as sombras ~3 ms no total. Limites: 4096 vectores de uniform no fragment, texturas de 16383
  - CPU (`k2d_performance_benchmark`): física 1000 caixas 20.2 ms (9.6 no desktop); `scene.update` de 2000 objectos 0.60 ms (0.018), provavelmente num núcleo pequeno
- [ ] Android: desenhar o jogo à resolução do projecto e escalar para o ecrã (2560x1600 são 4,4 vezes os píxeis de 720p, e as luzes pagam por píxel)
- [ ] Android: não desenhar com a app em segundo plano (hoje chama o GL sem superfície)
- [ ] Export: texturas comprimidas (Basis/ASTC) e carregamento de níveis em fundo

## Luzes 2D

- [x] Cena de teste `bin/scenes/lights.k2dscene`
- [x] Ambiente da cena (Settings › Scene › ambient); a UI desenha sem luzes nem ambiente (`ad81ea5`)
- [x] Falloff por luz: expoente da curva radial, 2 por omissão (a quadrática de antes) (`053af71`)
- [x] Bias e suavidade da sombra por luz, pontuais e direccionais: bias em profundidade normalizada (0.002 por omissão), suavidade multiplica o espaçamento do PCF (`053af71`)
- [x] 16 luzes pontuais por frame: luzes num uniform block, atlas com uma linha por luz (24), o que corrigiu as luzes 4–7 e os sóis a desenharem nas mesmas linhas; até 8 luzes os frames são iguais ao pixel. Para voltar a 8: `kMaxPointLights` ou `git revert` (`a08c054`)
  - desktop (Radeon, GLES 3.2): 8 luzes com sombra 2.2 → 2.3 ms (ruído); 16 com sombra, 12 visíveis, 2.6 ms
  - Chrome (WebGL2, mediana de 3): 8 com sombra 2.7 → 3.0 ms (ruído; vai nos dois sentidos); 16 com sombra, 12 visíveis, 3.1 → 3.9 ms
- [ ] Mais de 16 luzes visíveis: hoje passam as primeiras 16 da fila, não as mais próximas da câmara

## Editor

- [x] Nada se perde sem perguntar: Image Editor e Sprite Editor perguntam antes de trocar de imagem; sair (menu, Ctrl+Q, X da janela) e mudar de projecto perguntam pela cena, imagem, regiões e scripts; zen_plataform `efec44c` (o fecho recusado fica recusado) (`f91f216`)
- [x] Menu Help: manual (F1), referência de scripting e C++ no browser; os pacotes levam o manual em `bin/docs` (`7234a65`)
- [x] Transições de cena: `load_scene(path, transition, seconds)` com fade, slide, zoom e wipe, a imagem da cena antiga copiada na GPU; o Play do editor passou a correr os fades (`b155f95`)
- [x] Autotile nos TileMaps: terrenos com os vizinhos que cada tile liga (16 só lados, 47 blob, qualquer layout), pincel Terrain e marcação na paleta, `set_terrain` nos scripts (`e3afc1f`, `8886605`)
- [x] Gamepad: 4 pads, acções ligadas a botões e eixos, `action_strength`, as acções por omissão no motor (o Play do editor não as tinha) (`bfcc2ba`); na web ainda não (stub no zen_plataform)
- [x] Inspector: campos de ficheiro com o nome e pré-visualização no tooltip; imagem largada numa linha da Hierarchy vai para o Sprite/TileMap do objecto (`80471d6`)
- [x] Add Component por categorias (`5702065`)
- [x] Camadas de colisão com nome: Settings, `category`/`mask` por nomes, `collision_layer()` (`b96fef8`)
- [x] Tileset partilhado (`.k2dtileset`): textura, corte, terrenos, tiles sólidos e animados, usado por vários mapas; editar num mapa grava o ficheiro; empacotado no export (`82d51d5`, `55dffe5`). Falta: mudar tiles sólidos em Play não refaz as formas da física
- [x] Debugger de scripts: breakpoints, variáveis, passo a passo
  - [x] VM (zenpy `ed1ced0`, `82eabd8`): nomes dos locais por função, `set_debug_hook` só no `zen_debug`, leitura de frames e locais; `d719248`: um local só aparece depois de ter valor
  - [x] Runner: `k2d_runner_debug --debug=porta`, pára nos breakpoints e fala JSON por TCP local (`9f066e4`, `e63f0d6`)
  - [x] iGUI (`5989d8d`): margem de breakpoints e linha actual no CodeEditor
  - [x] Editor: breakpoints no Script Editor, painel Debugger (pilha, locais, Continue/Pause/Over/In/Out/Stop), F9/F10/F11/Shift+F11, ferramentas MCP `breakpoint` e `debug`
  - [ ] Ver e mudar o valor de uma variável; breakpoints com condição; guardar os breakpoints no projecto
- [ ] Timeline genérica: animar qualquer propriedade de qualquer objecto, com eventos
- [x] Assets: criar pastas e scripts, renomear, mover (arrastar para uma pasta ou `..`) e apagar; cenas, prefabs, tilesets, sprites, o projecto, a cena aberta e o seu undo acompanham, a imagem leva o `.sprite.json`/`.atlas.json`; scripts só avisados; MCP `move_asset` e `asset_references`
- [ ] Gamepad na web (Gamepad API no backend web do zen_plataform)
- [ ] Tutoriais de jogos pequenos (colisão, plataformas…) no manual
- [ ] Android
  - [x] Runner como NativeActivity (`libk2d_runner.so`, arm64-v8a, API 26): o backend Android do zen_plataform, o `data.kpak` lido do APK; `tools/android/build-apk.sh` faz o APK sem Gradle nem Java (aapt2, zipalign, apksigner)
  - [x] Correr num dispositivo: ecrã inteiro, modo imersivo (zen_plataform `7a8fcac`), toque como rato, pausa e retoma (`6f31b2e`)
  - [ ] Áudio no dispositivo
  - [ ] Export Android no editor e no `k2d_export`; ícone e orientação por projecto; armeabi-v7a e x86_64

- [x] Molduras da câmara, do ecrã e dos limites na vista da cena (`021950c`)
- [x] Settings › Project display: tamanho da janela do runner (`021950c`)
- [x] Game view: pré-visualizar o ecrã do projecto, o painel ou um preset (`021950c`)
- [x] Modos de escala da câmara: Crop, Stretch, Fit Width, Fit Height, Native (`8f4e7c9`)
- [x] Molduras de todas as câmaras (a activa mais forte, com ecrã e limites), resolução 0 = tamanho da janela, posição do objecto em edição, ecrã sem câmara à volta da origem — visto no ecrã com `import -window` (`981cc28`)
- [x] Rótulos das molduras com fundo escuro, legíveis sobre cenas iluminadas (`6284bda`)

### Lacunas encontradas ao escrever o manual (`documentation/editor/`)

- [x] Trabalho perdido sem aviso (`f91f216`)
- [ ] Regiões do Sprite Editor sem undo
- [ ] Image Editor grava sempre PNG, mesmo num `.jpg`/`.bmp`/`.tga`
- [ ] `set_collision` de um TileMap em Play não chega à física (as formas só se fazem quando o corpo nasce)
- [ ] Componentes Ui* adicionam-se mas não têm secção no Inspector; MotionTween2D e MotionStreak2D estão no ClassDB mas não no Add Component
- [ ] Add Component sem descrições (o ClassDB não tem campo para elas)
- [x] Assets sem criar pasta, renomear ou apagar
- [ ] Animator: sem UI para eventos, offsets e pontos dos frames; marcar para arrancar um clip ainda sem frames deixa-o parado
- [ ] Skeleton: duplo clique, mute/lock e Delete na timeline não mexem no clip; apagar o clip do `autoplay` deixa o nome; renomear um osso deixa as pistas órfãs
- [ ] Atlas: imagens com o mesmo nome em pastas diferentes perdem a região em silêncio
- [ ] `docs/editor-workflow.md` é do editor ImGui e contradiz o actual em 10 pontos: substituir por um apontador para o manual

## Decisões em aberto

- [ ] `demo/` (ainda ImGui/SDL, fora do build): portar ou apagar
- [ ] `../iGUI` (cópia solta): `git branch backup/pre-rebase-2026-10-13; git reset --hard origin/main`
