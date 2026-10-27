# Plano: tirar o ImGui e o SDL2, ficar só com ct + iGUI + zen_plataform

Decisão proposta: **adaptar o Kinetix2D, não recomeçar.** O motor já é `ct::` quase todo,
o ImGui só toca em 30 linhas do motor, e o editor é o único sítio com trabalho a sério.
A referência de desenho é o prisma3d (`GuiBackend`, `WidgetHost`, `EditorShell`); o Doriax
não serve para este objectivo porque usa ImGui e STL em tudo.

## Levantamento (2026-10-27)

| Módulo | LOC | `std::` (occ / ficheiros) | ImGui (occ / ficheiros) |
|---|---:|---:|---:|
| engine | 125 146 | 730 / 73 | 37 / 5 |
| editor | 15 245 | 210 / 17 | 2 376 / 24 |
| scripting | 8 907 | 106 / 6 | 0 |
| physics2d | 4 686 | 31 / 13 | 0 |
| runner | 575 | 41 / 1 | 0 |
| tools | 626 | 91 / 3 | 0 |
| demo | 6 367 | 27 / 11 | 108 / 14 (já fora do build) |

Dos 730 `std::` do motor, a quase totalidade é `size_t`, `printf`, `fabs`, `sqrt`,
`strcmp`, `snprintf`: libc com prefixo, não STL. O prisma3d proíbe contentores, não a libc,
e aqui segue-se o mesmo critério. Contentores STL a sério, por ficheiro:

| Ficheiro | O que | Para |
|---|---|---|
| `engine/src/TileMapComponent.cpp` | `std::string` ×23, `stringstream` ×1 | `ct::String` |
| `engine/src/KPak.cpp`, `KPak.h` | `string`, `vector`, `array` | `ct::String`, `ct::Vector`, `ct::Array` |
| `engine/src/AudioEngine.cpp` | `unordered_map`, `string`, `vector` | `ct::HashMap`, `ct::String`, `ct::Vector` |
| `engine/src/FileSystem.cpp`, `.h` | `unique_ptr`, `vector` | `ct::Unique`, `ct::Vector` |
| `engine/include/k2d/Skeleton2D.h` | `std::function` ×2 | `ct::Function` |
| `InputActionMap`, `AudioPlayer`, `SceneManager`, `SpriteAtlas` | `string`/`vector` soltos | idem |
| `editor/src/core/EditorApplication.cpp` | `std::filesystem` ×20, `string`, `vector`, `to_string` | `k2d::FileSystem` + fs do zen_plataform |
| `editor/src/panels/AtlasPanel.cpp`, `ScriptEditorPanel.cpp`, `InspectorPanel.cpp`, `AnimatorPanel.cpp`, `TileMapPanel.cpp` | `filesystem`, `string`, `function`, `vector` | idem |
| `runner/src/main.cpp` | `std::filesystem` ×12 | idem |
| `scripting/src/ZenRuntime.cpp`, `ZenScriptComponent.cpp` | `std::filesystem` ×1 cada | idem |
| `tools/k2d_pack.cpp`, `k2d_webserver.cpp` | `string`, `filesystem`, `array` | idem |

Dependências de plataforma no motor: só `Device.cpp` (janela, contexto GL, eventos,
ImGui init/frame/render), `FileBuffer.cpp`/`FileSystem.cpp`/`UserData.cpp` (`SDL_RWops`,
`SDL_GetBasePath`), `Profiler.cpp`/`CanvasRenderer.cpp` (`SDL_GetPerformanceCounter`).
`Device.h` expõe `ImGuiWantsMouse/Keyboard`. `ProfilerUI.cpp` tem 30 chamadas ImGui.

### O que o editor pede ao ImGui e o que o iGUI dá

As 2 376 chamadas concentram-se em ~130 funções. Mapa das que importam (modo imediato,
`ig::Context`):

| ImGui | iGUI |
|---|---|
| `SameLine`, `Button`, `SmallButton`, `Checkbox`, `Text*`, `Separator*`, `Indent` | `sameLine`, `button`, `smallButton`, `checkbox`, `label`/`textColored`/`textDisabled`/`textWrapped`, `separator`/`separatorText`, `indent` |
| `DragFloat/2/4`, `DragInt`, `SliderFloat/Int`, `InputText*`, `InputInt`, `Combo`, `ColorEdit4`, `RadioButton` | `dragFloat/2/4`, `dragInt`, `sliderFloat/Int`, `inputText*`, `inputInt`, `comboBox`, `colorEdit`, `radioButton` |
| `IsItemHovered/Activated/DeactivatedAfterEdit/Clicked/Active`, `SetTooltip` | `isItemHovered/Activated/DeactivatedAfterEdit/Clicked/Active`, `tooltip` |
| `PushID/PopID`, `SetNextItemWidth`, `Begin/EndDisabled`, `PushStyleColor/Var` | `pushId/popId`, `setNextItemWidth`, `beginDisabled/endDisabled`, `pushStyleColor/Var` |
| `BeginMenu*/MenuItem`, `BeginMainMenuBar` | `beginMenu*/menuItem`, `beginMainMenuBar` |
| `BeginDragDropSource/Target`, `Set/AcceptDragDropPayload` | `beginDragSource`, `acceptDragDropTarget` |
| `BeginChild`, `BeginTable/TableSetupColumn/NextColumn`, `TreeNodeEx/TreePop`, `CollapsingHeader`, `Selectable` | `beginChild`, `beginTable/tableHeader/tableNextColumn`, `treeNode`/`treeItem`, `collapsingHeader`, `selectable` |
| `OpenPopup`, `BeginPopupModal`, `BeginPopupContextItem/Window`, `CloseCurrentPopup` | `openPopup`, `beginPopupModal`, `beginContextMenu`, `closeCurrentPopup` |
| `DockSpace`, `DockBuilder*` | `beginDockSpace`, `beginDockPanel`, `setDockSizes`, `captureDockArrangement`, `setWindowStatePath` (.ini com layout) |
| `GetWindowDrawList` + `AddRect/Line/Circle` | `drawRect`, `drawRectFilled`, `drawLine`, `drawCircleFilled`, `drawPolygonFilled`, `drawText` |
| `Image`, `InvisibleButton`, `GetCursorScreenPos`, `GetContentRegionAvail` | `image`, `invisibleButton`, `contentRect`, `availableWidth/Height` |
| `GetIO().WantCapture*`, `IsMouseDown/Clicked/Dragging`, `IsKeyPressed`, `SetMouseCursor` | `wantsPointer/wantsKeyboard`, `isPointerButtonDown`, `isPointerDoubleClicked`, `isKeyPressed`, `setCursor` |
| `ImGuiFileDialog` (19 usos) | `fileDialog` |
| `TextEditor` (ScriptEditorPanel) | `codeEditor` + `ig::syntax` |
| `ImSequencer`, `ImCurveEdit`, `ImGradient` | `sequencer`, `curveEditor`, `gradientEditor` |
| `ImGuiConsole`, toasts próprios | `ConsoleWidget`, `showToast`/`drawToasts` |
| `ImGuiMinimap` (2 usos) | desenhar à mão com `drawRectFilled` |
| `PlotLines` (ProfilerUI) | `plotLines` |
| Gizmo do viewport (feito à mão) | `gizmo2D` ou manter o desenho próprio |

Sem equivalente directo, a confirmar no porte: `GetStateStorage` (2 usos), `IsMouseDragging`
(7; faz-se com `isPointerButtonDown` + posição), DockBuilder programático para o layout
inicial (o iGUI guarda o layout no .ini; o layout por omissão tem de ser expresso com
`setDockSizes`/`beginDockPanel`).

### Versões e compatibilidade

- `external/containers` aqui está em `da30f46` (2026-09-05); o prisma3d usa `d799edb`
  (2026-10-20). Fazer bump antes de tudo. O iGUI traz `third_party/containers` próprio: tem
  de apontar para a cópia de `external/` (o prisma3d faz isso com um target `ct` INTERFACE).
- iGUI é C++14, zen_plataform é C, ct compila em 14 e 17. O motor fica em C++14, o editor já
  está em 17.
- O motor compila shaders `#version 300 es`. O backend OpenGL que vem com o iGUI é
  `#version 330 core`, por isso **não se usa**: escreve-se um `k2d::GuiBackend : ig::Backend`
  que desenha `ig::DrawData` com `k2d::Shader`/`k2d::Texture`, decalcado do
  `prisma3d/libprisma3d/include/prisma3d/gui/GuiBackend.h`. O zen_plataform cria contextos
  GLES na web e no Android e GL no desktop; o spike da fase 2 confirma que o desktop aceita o
  perfil ES 3.0 que o motor usa hoje via SDL.

## Nodes ou componentes

Ficam os **componentes**. Razões:

- Tudo o que há assenta em `GameObject` + `Component`: 125k linhas de motor, 7,5k de
  bindings Zen (`ZenScriptComponent.cpp`), o serializer com `ComponentMatch`/`MatchFn`, o
  inspector (3,9k) e os scripts dos jogos (`projects/tokenkay`). Mudar para nodes é a opção
  "do zero" com outro nome, e não aproxima nada do objectivo (GUI e contentores).
- Os nodes do prisma3d são herança funda à Godot (`Node3D → VisualInstance3D →
  MeshInstance3D`) porque o render e os shaders vieram do Godot. Aqui não há essa cadeia.
- O Doriax, a outra referência 2D, também é componentes (ECS: `Entity` + `SpriteComponent`,
  `Body2DComponent`, `TilemapComponent`...). As duas famílias funcionam para 2D.
- A decisão de 2026-08-29 ("scene is the world": física absorvida no `Scene`/`RigidBody2D`)
  está a meio neste ramo e é toda em componentes.

O que vale a pena trazer do modelo de nodes do prisma3d é só uma ideia: o `NodeClassDB`
(`bindProperties` + `PropertyInfo`/`PropertyValue`), ou seja, reflexão de propriedades por
classe. Aqui seria um `ComponentClassDB` que alimenta o inspector, o serializer e as
propriedades exportadas para Zen a partir de uma única tabela. Fica como fase opcional no fim;
não é pré-requisito de nada.

## Porquê não um prisma2d sobre o prisma

O prisma (`libprisma`, 18,7k linhas, sem `std`, C++14) dá GL 4.6, GLES 3 e Vulkan atrás de
um `Driver` com handles, e shaders escritos uma vez em GLSL 450 compilados no build com
`glslangValidator` e SPIRV-Cross. É a base certa para PBR, sombras e compute. Para 2D é
peso sem retorno:

- O render do Kinetix2D são 3,8k linhas em 6 ficheiros (`Batch`, `CanvasRenderer`, `Shader`,
  `Texture`, `RenderQueue`, `Light2D`) com 68 funções `gl*` distintas, em GLES 3.0. Corre
  igual no desktop, na web e no Android, e o zen_plataform cria esses contextos nos três.
- O gargalo de um 2D é o batching e o fill rate, não o custo da API. Vulkan não acelera um
  sprite batch; só acrescenta pipelines, passes e uma toolchain de shaders ao build.
- O backend GLES do prisma está "working on desktop; Android and web not tested yet". O
  Kinetix2D já tem web a funcionar (`tools/k2d_webserver`).
- Um prisma2d seria o Kinetix2D reescrito sobre prisma + iGUI + zen_plataform + um modelo
  de cena novo: é a opção "do zero", com a camada de render trocada por uma mais pesada do
  que o 2D precisa, e sem os 125k de funcionalidades (tilemaps, skeleton, partículas, luzes,
  navegação, física) que já existem e têm jogos a correr.

Se um dia Vulkan fizer falta no 2D, o passo barato é isolar as 68 chamadas `gl*` atrás de
um `k2d::RenderDevice` próprio: já estão concentradas em 6 ficheiros. Não é para agora.

A alternativa a "um projecto 2D à parte" seria 2D dentro do prisma3d (nodes
`Node2D`/`Sprite2D`, como o Godot faz no mesmo motor). Decidido a 2026-10-27 que não: para
jogos 2D o tamanho e o custo do prisma3d não interessam; o 2D fica neste projecto, à parte
e leve. Isso fecha também a questão nodes contra componentes: ficam os componentes.

## Handles com geração na física 2D

O prisma usa `ct::Handle32` (20 bits de índice, 12 de geração) para tudo o que sai do
driver; por dentro trabalha com arrays densos. A física 2D hoje faz o oposto: ponteiros crus
em todo o lado (`ContactInfo.a/b`, `BodyPair`, `Joint2D::mBodyA/mBodyB`) e, para não ficar
com ponteiros mortos, cada `destroyBody` varre todos os joints, todos os contactos e todos
os pares (`SceneSolver.cpp`). Os ids de corpo já existem (`mId`, reciclados por
`mFreeBodyIds`) mas sem geração, por isso a chave de par pode repetir-se e a limpeza tem de
ser imediata.

Vale a pena, mas só na fronteira do step, não dentro dele:
1. `BodyHandle = ct::Handle32<BodyTag>`: o `mId` passa a índice e ganha uma geração por
   slot. Uma tabela `handle → RigidBody2D*` na cena; o solver continua a correr sobre
   `mBodies` por índice.
2. `ContactInfo`, `BodyPair` e `PairContactState` guardam e são chaveados por handle. O
   `destroyBody` deixa de varrer contactos e pares: uma entrada com geração velha falha a
   validação quando o step a toca e é descartada aí.
3. `Joint2D` guarda handles e resolve-os no início do step; `invalidate()` e a varredura de
   joints em `destroyBody` desaparecem.
4. Resultados de queries (`raycast`, `overlapCircle`, vizinhos) continuam a devolver
   `GameObject*` para os scripts; expor handles ao Zen é trabalho do `ComponentClassDB`.

Precisa do bump de `containers` (o `slotmap32.hpp` só existe na cópia do prisma3d). Medir
com um cenário de spawn/destroy de 200 corpos por frame no `k2d_performance_benchmark`,
que é onde as varreduras de hoje aparecem.

## Pack e binários como no prisma3d

O `KPak` e o `k2d_pack` são o mesmo desenho do `FilePack`/`pack` do prisma3d (cabeçalho de
64 bytes, entradas com CRC e deflate, chave opcional); a diferença está no que o prisma3d
construiu à volta e que aqui não existe:

| prisma3d | Kinetix2D hoje |
|---|---|
| `pack -s`: carimba uma cópia do runner com o pack anexado (`PackStamp` de 32 bytes no fim, com offset, tamanho e chave); `pack -t` lista o pack de um executável | não há |
| `data.rpak` ao lado do runner, encontrado por `fs_get_executable_path` | não há |
| `pack -c`: o pack como fonte C++ para compilar dentro do binário (fonte, ícones, shaders) | não há; o editor carrega tudo do disco |
| `export projecto.json -o out/game [--embed]`: copia o runner, mete manifesto, assets, pastas das cenas e search paths no pack, scripts como bytecode Zen por ordem, shaders pré-compilados | só `export_web.sh` |
| runner arranca em modo "player" quando está carimbado ou tem `data.rpak` ao lado, sem argumentos | runner recebe `cena.k2dscene [projecto.json]` |
| File > Export Game no editor, que lança o `export` com `process_spawn` | não há |
| `FileSystem::mountPack(path, key, offset)` como search path | `KPak` é aberto à parte |

O que se faz, pela ordem em que uma coisa serve à seguinte:
1. `KPak` para `ct` (já na fase 1), `openFromMemory`, `readStamp`/`writeStamp`,
   `sidePackPath`, e `k2d::FileSystem` a montar packs como search path com offset.
2. `k2d_pack` ganha `-s`, `-t` sobre executável e `-c`.
3. `k2d_export projecto.json -o out/jogo [--embed]`: runner copiado e carimbado, manifesto,
   cenas, assets, scripts Zen compilados para bytecode pelo mesmo caminho que o prisma3d usa.
4. Runner: modo player quando está carimbado ou tem `data.kpak` ao lado.
5. Editor novo: File > Export Game lança o `k2d_export`; fonte, ícones e tema do editor
   embebidos com `-c`, um só executável como o ZenHuman.

Trazer o desenho do `FilePack.h` e do `tools/exporter/main.cpp`, não o código: aqui é
`ct::Json`, `k2d::FileSystem` e o zen vendorizado.

## Objectivo de produto: criar jogos e exportar para web, fácil

O export web já existe (`tools/export_web.py`) e funciona, mas pede o Emscripten SDK na
máquina de quem exporta: compila o runner na primeira vez e usa o `file_packager.py` do
emsdk para empacotar os assets num `.data` com preload. Para ser uma ferramenta que alguém
instala e usa, isso tem de desaparecer:

1. **Runner web pré-compilado.** `k2d_runner.js` e `k2d_runner.wasm` vêm no release do
   editor (feitos no CI), em `templates/web/`, como o prisma3d faz com `templates/<platform>/`
   para o runner nativo. Quem exporta não compila nada.
2. **Assets no nosso pack, não no do Emscripten.** O export escreve `data.kpak` e o runner
   web faz `fetch` do pack e abre-o com `openFromMemory` (ponto 1 da secção do pack). O
   `file_packager.py` deixa de ser preciso. Uma barra de progresso enquanto o pack chega é o
   que um jogo web precisa, e é um `fetch` com `onprogress`.
3. **Sem ASYNCIFY.** Hoje o runner web corre com `-sASYNCIFY` porque o loop é nosso. Com o
   zen_plataform o loop é invertido (`app_run` sobre `emscripten_set_main_loop`), o wasm fica
   mais pequeno e mais rápido, e o `emscripten_sleep` do runner desaparece.
4. **Um clique no editor.** Export Web = pack + copiar três ficheiros + escrever
   `index.html` + `k2d_webserver` + abrir o browser. Já é quase isto; o que muda é não ter
   pré-requisitos. Run Web faz o mesmo e abre logo.
5. **Projecto novo a correr em segundos.** Novo projecto cria cena, câmara, um sprite com
   script e Play funciona sem tocar em nada, como o `projects/Default` do prisma3d.

O export nativo (secção do pack) fica a seguir ao web, com o mesmo `k2d_export` e o mesmo
`data.kpak`; o runner nativo e o web lêem o mesmo ficheiro.

## Nota: bugs nos submódulos

`zen`, `ct`, `zen_plataform` e `iGUI` são repositórios nossos. Um bug apanhado em qualquer
um deles corrige-se lá: commit curto, push, e bump do submódulo aqui. Nunca um remendo
local na cópia. Sem linhas `Co-Authored-By` de Claude, Copilot ou de quem for, em nenhum dos
repositórios.

## Nota: limpar comentários

Em cada ficheiro tocado por qualquer fase, apagam-se os comentários. Fica só o mínimo que
for preciso: um porquê que o código não consegue dizer. Referências a outros motores, com
ficheiro e linha, saem do código e vão para `docs/PORTING.md`, como no prisma3d. O
`CanvasRenderer.cpp`, o `Device.cpp` e os joints são os que mais têm.

## Fases

Cada fase termina com a suite verde (`kx_*`, `k2d_*`, de dentro de `bin/`) e o editor a
arrancar. Ordem escolhida para que o trabalho mecânico vá primeiro e o risco (contexto GL,
docking) seja medido cedo com spikes pequenos.

### Fase 0: submódulos e build (feita a 2026-10-27)
1. Bump de `external/containers` para o commit do prisma3d.
2. Submódulos `external/zen_plataform` e `external/iGUI` (os mesmos repos do prisma3d).
3. Target `ct` INTERFACE único em `external/containers`; iGUI a usar esse e não o seu
   `third_party/containers`. Construir só `igui::core`, `igui::igui`, `igui::font`.
4. Copiar `cmake/CheckNoStd.cmake` do iGUI; ligar ao motor, scripting e editor no fim da
   fase 1 (até lá só avisa).

### Fase 1: STL → ct (sem tocar em GUI)
Os ficheiros da tabela acima, nesta ordem: motor (`KPak`, `TileMapComponent`, `AudioEngine`,
`FileSystem`, `Skeleton2D`, `InputActionMap`, `AudioPlayer`, `SceneManager`, `SpriteAtlas`),
depois scripting, runner, tools, editor. `std::filesystem` desaparece por inteiro: o que for
listar directórios, existir, criar pastas e juntar caminhos passa para `k2d::FileSystem`,
que por sua vez usa o `fs_posix.c`/`fs_win32.c` do zen_plataform a partir da fase 2 (até lá
mantém a implementação actual). `std::function` → `ct::Function`. `size_t`, `printf`,
`<cmath>` ficam.

### Fase 2: SDL2 → zen_plataform
Spike primeiro: `hello_x11` do zen_plataform + um shader `#version 300 es` do motor, para
confirmar o perfil no desktop (X11, Wayland e Win32). Depois:
1. `Device.cpp`: janela, contexto GL, bomba de eventos → `k2d::Input`, tempo, clipboard,
   `SDL_GetDisplayUsableBounds`/posição da janela. A API pública de `k2d::Device` não muda,
   excepto `ImGuiWantsMouse/Keyboard` → `uiWantsPointer/uiWantsKeyboard`.
2. `FileBuffer`, `UserData`, `FileSystem` (`SDL_RWops`, `SDL_GetBasePath`) → fs do
   zen_plataform. `Profiler`/`CanvasRenderer` (`SDL_GetPerformanceCounter`) → tempo do
   zen_plataform.
3. Tirar `find_package(SDL2)`, `vcpkg.json` e o `.k2d-tools/vcpkg`. O ImGui ainda fica a
   ser inicializado pelo `Device` nesta fase, com um backend de input provisório; é
   descartado na fase 4.

### Fase 3: backend de GUI
1. `engine/src/GuiBackend.cpp` (`k2d::GuiBackend : ig::Backend`): `measureText` com o
   `ig::FontAtlas` embebido, `render(DrawData)` com `k2d::Shader` ES 3.0, texturas
   `k2d::Texture` ↔ `ig::TextureId`, clipboard via `Device`. Copiar o desenho do
   `GuiBackend` do prisma3d, não o código.
2. `Device` passa a alimentar um `ig::Context` (`pushEvent`, `beginFrame`, `endFrame`) ao
   lado do ImGui. Os dois desenham no mesmo frame durante a fase 4; o input vai para quem
   o pedir (`wantsPointer`).
3. `ProfilerUI.cpp` portado (30 linhas), serve de primeira validação visual.

### Fase 4: editor novo, em iGUI retained
Decisão (2026-10-27): o editor **recomeça de raiz em modo retained**, não se porta painel
a painel. A UI actual é toda ImGui imediato e o retained é outro modelo (árvore de widgets,
sinais, layouts), por isso portar seria reescrever na mesma. O que se reaproveita do editor
actual é o que não é UI: `CommandStack`, `EditorSelection`, `Project`, `EditorSettings`,
`EditorFileSystem`, a lógica de Play/Stop com `mRuntimeScene`, e tudo o que os painéis fazem
à cena por baixo das chamadas ImGui (atlas, sprite editor, tilemap, animator, partículas).
Esse código sai dos painéis para ficheiros sem GUI antes de os painéis serem apagados.

Molde: o editor do prisma3d, que é retained com imediato só onde faz sentido
(`NumberField`, `PropertyWidgets`, `AssetGrid`, viewport). Trazer o desenho, não o código:
- `WidgetHost` (liga `ig::retained::WidgetApp` ao `Device`, texturas, clipboard, fonte).
- `EditorShell` (`DockPanel` + `MenuBar` + `ToolBar` + `StatusBar`, painéis por índice,
  layout por omissão e `.ini`).
- `EditorDocument` (cena aberta, dirty, undo) e `HierarchyActions`.
- `PropertyInspector`/`PropertyRow`/`SectionedInspector`: o inspector genérico, que aqui
  pede a fase 6 (`ComponentClassDB`) para não voltar a ser 3,9k linhas à mão. Por isso a
  fase 6 deixa de ser opcional e passa para antes do inspector.

O esqueleto da aplicação já está feito cinco vezes nas nossas ferramentas, sempre igual:
`ig::zen::App` (janela zen_plataform + GL) → `WidgetApp` → `MainWindow::build(app, window)`
com `MenuBar`, `ToolBar`, `DockPanel`, `StatusBar` → `setFrameCallback` → o viewport GL
desenha para um FBO e aparece num painel como `rt::TextureHandle`/`BuImage`. Ficheiros
que estas ferramentas copiam entre si e que o editor 2D copia também: `MainWindow`,
`MainMenu`, `Panels`, `Settings`, `Icons`, `ViewportPanel`, `McpServer`/`McpTools`.

| Ferramenta | O que dá ao editor 2D |
|---|---|
| `ZenHuman` (`src/app`, `src/panels`) | o shell mais pequeno e limpo (742 linhas de `MainWindow`); `ThumbnailGrid` para assets; `Collapsible` + `SpinSlider` para o inspector; `invalidate()`/redraw preguiçoso; MCP |
| `ZenStage` (`src/seq`, `src/panels/TimelinePanel`) | `rt::Timeline` com tracks, clips, keyframes e sinais (`onKeyframeMoved`...) para o Animator e o `ActionSequence2D`; `LibraryPanel` com drag-drop de clips; `FloatWindow` para editores flutuantes; `ColorButton` |
| `animator` | o único 2D: layers, keyframes, onion skin, sprite sheets, exportar GIF/PNG; referência directa para `SpriteEditor`/`Animation2D` |
| `deform` | `TreeView`/`TreeNode` para a hierarquia; `GizmoMode3D` como modelo do gizmo 2D |
| `coco` (ramo `rewrite` do CocoShape) | a mesma base, serve de segunda referência do shell |

Antes de fazer um painel, ver se um deles já tem o widget. `CocoShape` em `main` é ImGui,
SDL e `std`; não conta.

Performance da GUI: no benchmark headless do iGUI (`bench/README.md`) o modo imediato
custa 1,2 a 1,4 vezes o ImGui a construir a frame, 40 µs contra 30 numa galeria de 130
widgets, com menos draw calls. É irrelevante ao lado do render da cena. O retained repinta
só o que tem `dirty_`, por isso o editor parado custa zero. A única regra: hierarquia e
assets com milhares de entradas não criam um widget por linha; usam as listas e árvores
virtuais do iGUI (`beginVirtualTree`, `beginVirtualList`) ou um `ItemModel` retained.

Contexto GL no editor: as ferramentas pedem `GL_PROFILE_CORE` com shaders `#version 330
core`. O motor é ES 3.0 com `#version 300 es`. O zen_plataform cria `GL_PROFILE_ES` em X11
e Win32, portanto o editor pede ES 3.0 e o motor não muda um shader; o `ig::zen::App`
hardcoda core, por isso ou ganha um campo `profile` no `Config` (o iGUI é nosso) ou o editor
faz o seu próprio arranque com `platform.h` directamente, que são 50 linhas. O `GlRaster`
do iGUI é 330 core e não serve em ES: a GUI retained desenha-se pelo `k2d::GuiBackend`
da fase 3, como o `WidgetHost` do prisma3d faz com o backend dele.

Motor dentro do editor: hoje o `k2d::Device` é dono da janela, do contexto e do loop.
No editor a janela é do `ig::zen::App` e o motor só precisa de um contexto corrente e de um
FBO (o `GameViewport` já existe para mapear a resolução virtual num painel). O `Device`
separa-se em "cria janela e contexto" (runner) e "usa o contexto que está corrente"
(editor); as duas partes ficam no mesmo ficheiro.

Ordem: shell e documento → hierarquia → viewport e game (framebuffer como `ImageView`,
input directo quando a GUI não o quer) → assets → inspector (com `ComponentClassDB`) →
atlas, sprite editor, tilemap → animator, partículas, skeleton → script editor (`CodeEditor`
retained).

### Runtime: onde está hoje (2026-10-27, `bin/k2d_performance_benchmark`, Release)

| Medição | Agosto (`plano.md`) | Hoje |
|---|---:|---:|
| `scene.update`, 2 000 objectos | 0,085 ms | 0,018 ms |
| `scene.render_queue`, 2 000 itens | 0,087 ms | 0,028 ms |
| `physics.step`, 1 000 caixas sobrepostas, 3 783 contactos | 67,4 ms | 9,65 ms (p95 11,1) |

Cena e fila de render estão onde devem estar. A física está ao nível de um Box2D 2.4 num
cenário patológico (bloco sólido de corpos interpenetrados, sem gravidade); um jogo 2D tem
centenas de corpos e não mil sobrepostos. O que falta medir é o que o benchmark não vê:
GPU (sprites, luzes, sombras) e spawn/destroy de corpos por frame. São os dois cenários
que as secções seguintes pedem.

### Luzes 2D: a técnica é a certa, o custo é o pipeline à volta

A dúvida era se o nosso `Light2D` + `LightOccluder2D` presta, porque mata a performance. O
Doriax faz o mesmo que nós (e que o Godot): atlas de sombras polar 1D, uma linha por luz,
`atan` por fragmento, PCF ao longo da linha. A técnica não é o problema. O que custa está
em `CanvasRenderer.cpp`:

1. **`RenderShadowAtlas` corre todos os frames** e, por luz com sombra, faz 4 sectores, e em
   cada sector reconstrói todas as arestas no CPU, faz `glBufferData` e **um `glDrawArrays`
   por aresta**. 8 luzes × 4 × 200 arestas = 6 400 draw calls por frame só para sombras.
   O Doriax desenha todas as arestas de uma luz num único draw (3 vezes, para a costura em
   ±π), numa só linha do atlas, com a intersecção raio/segmento feita no fragment shader.
2. **Não há culling nenhum.** `RenderQueue::AddLight`/`AddOccluder` aceitam tudo: luzes
   fora da câmara são enviadas e sombreadas, e cada luz processa arestas que estão fora do
   seu raio.
3. **Um só shader para tudo.** Cada fragmento de cada sprite corre o loop das 8 luzes
   pontuais e o das 8 direccionais, mesmo em cenas sem luz e mesmo em sprites que a luz não
   alcança. Não há lista de luzes por item (o Godot dá a cada item a máscara das luzes que
   tocam o seu AABB; o Doriax compila variantes `USE_LIGHT2D`/`USE_SHADOWS_2D` por mesh).
4. **PCF13 itera sempre 13 vezes** e usa `continue` para o raio 2; são até 8 × 13 fetches
   por fragmento, mais um `atan` e a aritmética dos sectores por luz.

Plano, por ordem de ganho, com medição antes e depois (cena fixa: N sprites × M luzes com
sombra, ms por frame no desktop GLES e na web):
1. Atlas: uma linha por luz, arestas da luz num único draw com índice, VBO só reenviado quando
   um occluder ou uma luz mudou (stamp de geração, como em `Scene::topologyVersion`), e só
   para luzes visíveis. Tirar os 4 sectores também simplifica o fragment shader.
2. Culling: luzes contra o rectângulo da câmara; arestas contra o raio da luz.
3. Variantes de shader: sem luz, com luz, com luz e sombra. Cenas sem `Light2D` ficam a custo
   zero.
4. Máscara de luzes por item calculada na `RenderQueue` (AABB do item contra raio de cada
   luz) e enviada no atributo que hoje leva só o cull mask; o loop salta as luzes que não
   tocam o item sem calcular nada.
5. PCF com o loop limitado ao raio pedido, 5 taps por omissão.

Isto é trabalho no motor e independente do editor; pode ir antes ou em paralelo com as fases
0–3.

### Fase 5: limpeza
Apagar `external/imgui`, `editor/external/imgui_widgets`, `TextEditor`, `ImGuiMinimap`,
`imgui.ini`; `demo/` ou se porta ou se apaga (está fora do build). `CheckNoStd` a falhar o
build em motor, scripting e editor.

### Fase 6: ComponentClassDB (antes do inspector novo)
Reflexão de propriedades por componente, nos moldes do `NodeClassDB` do prisma3d, a
alimentar o inspector retained genérico, o serializer e `ZenScriptProperties`. Com o editor
a recomeçar, é isto que evita escrever o inspector à mão outra vez; faz-se no motor, com
testes, antes do painel.

## Tamanho

É grande mas limitado: o motor e o scripting quase não mudam, a física não muda nada. O
custo real é o editor (15k linhas, 2,4k chamadas de GUI), e desse custo o inspector é um
quarto. Recomeçar do zero custava isso mais o que já está feito e funciona: comandos e
undo, selecção, serializer, prefabs, atlas, sprite editor, tilemap, viewport com gizmos,
Play/Stop com `mRuntimeScene`. Nenhuma dessas partes depende do ImGui para além das
chamadas de desenho.
