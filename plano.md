# Plano de melhoria de performance

## Objetivo

Reduzir o custo dos hot paths de física e cena sem alterar o comportamento público. As mudanças devem ser guiadas por medições reproduzíveis e manter os testes funcionais existentes.

## Fase 1: medir antes de alterar

- [ ] Criar um benchmark reproduzível para `Scene::update()` e `Scene::buildRenderQueue()`.
- [ ] Criar cenários de física com corpos, shapes, contatos, joints e tilemaps em diferentes escalas.
- [ ] Medir separadamente:
  - [ ] `scene.update`
  - [ ] `scene.render_queue`
  - [ ] `render.sort`
  - [ ] `physics.step`
  - [ ] broadphase, narrowphase, solver de velocidade e solver de posição
- [ ] Registrar tempo médio, p95, número de contatos, pares, componentes, itens renderizados e alocações.
- [x] Registrar os executáveis de teste no CTest.
- [x] Garantir que o build de testes registre os testes no CTest após reconfiguração.

### Baseline funcional

Executado em 2026-08-26:

```bash
ctest --test-dir build --output-on-failure -E 'physics_reference_test'
```

- 33 testes executados.
- 31 passaram.
- 2 falharam antes das otimizações:
  - `k2d_pixmap_tests`: `save` e `load`.
  - `k2d_prefab_tests`: `round_trip` e `cached_not_reread`.
- Os testes de física, `physics2d` e cena passaram.
- `physics_reference_test` foi mantido fora da suíte funcional por ser um benchmark extremo de longa duração; deve ser executado separadamente para medições de performance.

### Baseline de performance

Benchmark criado em `tests/k2d_performance_benchmark.cpp` e executado em 2026-08-26:

```bash
cmake --build build --target k2d_performance_benchmark -j2
./bin/k2d_performance_benchmark
```

Cenário fixo: 2.000 objetos com componentes de update/render, 1.000 corpos físicos, 30 frames de aquecimento e 120 amostras.

| Métrica | Média | P95 |
| --- | ---: | ---: |
| `scene.update` | 0.0849 ms | 0.0904 ms |
| `scene.render_queue` | 0.0870 ms | 0.0912 ms |
| `physics.step` | 67.4138 ms | 74.1808 ms |

Resultado do cenário: 3.340 contatos físicos. Esses valores são a referência antes das otimizações.

### Bloqueio atual

Antes de continuar, inicializar os submódulos na raiz do repositório:

```bash
git submodule update --init --recursive
```

Depois repetir a configuração e a validação:

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

## Fase 2: Scene

### 2.1 Evitar compactação por frame

- [x] Adicionar uma flag de lista suja em `Scene`.
- [x] Marcar a flag em `unregisterComponent()`.
- [x] Executar `compactComponentLists()` somente quando houver remoções.
- [x] Evitar compactar duas vezes no mesmo frame entre `update()` e `buildRenderQueue()`.
- [x] Medir o custo de cenas sem remoções e com destruição em massa.

No cenário de 2.000 objetos e sem remoções, o benchmark passou de `0.0850 ms` para `0.0670 ms` no update e de `0.0874 ms` para `0.0729 ms` na render queue. A suíte ampla passou 31/31 testes, excluindo o benchmark extremo e as duas falhas preexistentes.

### 2.2 Separar inicialização de atualização

- [ ] Manter uma lista de componentes que precisam receber `onStart()`.
- [ ] Criar uma lista de atualização contendo apenas componentes com `ComponentEventUpdate`.
- [ ] Preservar a semântica atual de `onStart()` para componentes render-only e componentes sem update.
- [ ] Validar componentes adicionados ou removidos durante callbacks.

### 2.3 Cache da hierarquia

- [ ] Adicionar estado cacheado para `activeInHierarchy` e `visibleInHierarchy`.
- [ ] Atualizar o estado apenas quando houver mudança em `setActive`, `setVisible`, attach ou reparent.
- [ ] Propagar mudanças somente quando o valor efetivo mudar.
- [ ] Comparar contra a implementação atual em cenas profundas.

### 2.4 Transformações

- [ ] Criar uma atualização interna conjunta de posição e rotação para a sincronização física.
- [ ] Fazer uma única invalidação de transform por corpo.
- [ ] Evitar invalidação quando posição e rotação não mudaram.
- [ ] Avaliar versionamento de transforms para substituir a caminhada recursiva em `invalidateChildren()`.
- [ ] Validar subárvores com transform, rotação e escala herdadas.

### 2.5 Render queue

- [ ] Medir o custo real de `ct::sort` em cenas com e sem y-sort.
- [ ] Reservar capacidade da fila com base no pico observado.
- [ ] Avaliar ordenação por grupos de `zIndex` e ordenação somente dos grupos que usam y-sort.
- [ ] Confirmar que a ordem visual permanece idêntica.

## Fase 3: Physics2D

### 3.1 Rebuild incremental

- [x] Transformar `markDirty()` em entrada de uma fila de `RigidBody2D` sujos.
- [x] Evitar inserir o mesmo corpo mais de uma vez na fila.
- [x] Fazer `rebuildChanged()` processar somente essa fila.
- [ ] Manter o scan atual temporariamente como fallback/debug, se necessário.
- [ ] Medir cenas sem alterações e cenas com alterações concentradas.

Implementação validada com a suíte funcional ampla: 31/31 testes passaram. A medição específica de performance da camada `PhysicsWorld2D` ainda precisa de um cenário próprio.

### 3.2 Sincronização de corpos

- [x] Eliminar o lookup redundante `objectFor(rigidBody->mBody)` nos loops de push/pull.
- [x] Usar o owner diretamente no componente na sincronização.
- [ ] Reservar `mBodies`, `mPending` e buffers temporários quando o tamanho esperado estiver disponível.
- [ ] Validar attach, detach, destroy e reparent durante a simulação.

A sincronização agora usa diretamente `RigidBody2D::owner()`. Build e os 2 testes de `physics2d` passaram. O ganho percentual ainda não foi medido porque o benchmark atual cobre `kx::World`, não a camada `PhysicsWorld2D`.

Corrigido também um caso de múltiplos mundos: `Collider2D::markDirty()` agora notifica o `PhysicsWorld2D` proprietário de cada rigid body, em vez de usar somente o mundo global ativo. O teste `collider_world` cobre esse caso e a suíte ampla passou 31/31 testes, excluindo o benchmark extremo e as duas falhas preexistentes.

Próximo candidato de cena: cachear `activeInHierarchy` e `visibleInHierarchy`. A implementação deve atualizar o cache em `setActive`, `setVisible`, attach e reparent, com testes específicos para mudanças de pai e propagação para subárvores.

### 3.3 Tilemaps

- [ ] Medir o custo de reconstrução de tilemaps por tamanho e densidade de células sólidas.
- [ ] Reconstruir somente regiões alteradas, quando suportado pela representação do collider.
- [ ] Evitar recriar todos os corpos do tilemap para pequenas alterações.
- [ ] Verificar ownership e destruição dos colliders criados com `new`.

## Fase 4: Physics

### 4.1 AABB e narrowphase

- [x] Calcular o tight AABB do corpo uma vez por sincronização de proxies.
- [x] Reutilizar o AABB no filtro de pares e na narrowphase.
- [ ] Avaliar cache de AABB por shape para corpos estáticos ou não modificados.
- [ ] Invalidar o cache quando posição, rotação ou geometria mudarem.

### 4.2 Joints

- [ ] Criar um índice por chave de par para joints com `CollideConnected == false`.
- [ ] Atualizar o índice na criação, alteração e destruição de joints.
- [ ] Substituir a varredura linear de `JointsAllowCollision()` por lookup constante.
- [ ] Adicionar testes para múltiplos joints entre os mesmos corpos.

### 4.3 Broadphase

- [ ] Reutilizar o buffer de hits de `FindNewPairs()` para evitar alocações por step.
- [ ] Reservar capacidade de `mDeadPairs`, `mMoveBuffer` e `mBulletSweeps` com base no pico.
- [ ] Medir a quantidade de proxies movidos e candidatos por step.
- [ ] Manter `UpdateContactsBrute()` apenas para diagnóstico e testes controlados.
- [ ] Adicionar uma métrica ou aviso quando o modo brute-force for usado com muitos corpos.

### 4.4 Solver

- [x] Pré-separar contatos dinâmicos e contatos com corpo estático antes das iterações do solver de velocidade.
- [x] Reutilizar as listas de contatos ativos no solver de posição.
- [x] Medir separadamente o custo de joints/preparação e contatos em cada iteração.
- [ ] Tornar as iterações configuráveis por mundo/cenário.
- [ ] Comparar 4, 6 e 8 iterações em estabilidade, penetração e tempo.
- [ ] Avaliar redução adaptativa somente após medir impacto na estabilidade.

### Perfil atual do solver

Com o benchmark de 1.000 corpos e 3.340 contatos:

- `solve_velocity`: 10.0901 ms.
- contatos nas iterações: 8.2989 ms.
- joints e preparação restante: 1.7913 ms.

O próximo alvo deve ser o loop de contatos. A instrumentação foi validada com 14/14 testes focados de física, `physics2d` e cena.

### Pré-filtro de contatos ativos

- [x] Pré-filtrar contatos inativos antes das iterações do solver de velocidade.
- [x] Remover o teste repetido de atividade dentro de cada iteração.
- [x] Validar com 14/14 testes focados passando.
- [x] Tornar o benchmark físico determinístico e repetir a medição. O cenário agora usa 1.000 caixas em grade fixa, sem gravidade, com sobreposição controlada.

Baseline controlada atual:

- 3.783 contatos em três execuções consecutivas.
- `physics.step_avg_ms`: 37.7830 ms.
- `physics_step_p95_ms`: 45.9742 ms.
- `solve_velocity_contacts_ms`: 8.6845 ms.
- O benchmark compilou e os 14 testes focados passaram.

### Warm start de contatos

- [x] Reutilizar as listas de contatos ativos no `WarmStartContacts()`.
- [x] Eliminar a varredura de todos os contatos e os filtros repetidos no warm start.
- [x] Validar com 14/14 testes focados passando.

Comparação no cenário determinístico com 3.783 contatos:

| Métrica | Antes | Depois |
| --- | ---: | ---: |
| `physics.step` médio | 37.7830 ms | 36.5114 ms |
| `solve_velocity_contacts` | 8.6845 ms | 8.5346 ms |

O ganho medido foi de 3,37% no step total e 1,73% no custo de contatos.

### Solver de posição

- [x] Remover as duas varreduras completas de `mContacts` em cada iteração.
- [x] Preservar três iterações e a ordem de resolução de contatos dinâmicos antes dos estáticos.
- [x] Validar com 14/14 testes focados passando.

Comparação no cenário determinístico com 3.783 contatos:

| Métrica | Antes | Depois |
| --- | ---: | ---: |
| `solve_position` | 5.0048 ms | 4.8244 ms |
| `physics.step` médio | 36.5114 ms | 35.5959 ms |

O ganho medido foi de 3,60% no solver de posição e 2,51% no step total.

### Resultado da primeira otimização do solver

Após a separação dos contatos, no mesmo cenário de 1.000 corpos e 3.340 contatos:

| Métrica | Antes | Depois |
| --- | ---: | ---: |
| `physics.step` médio | 40.8273 ms | 27.5211 ms |
| `solve_velocity` | 16.9827 ms | 9.8799 ms |
| `solve_position` | 7.5545 ms | 4.4695 ms |

Os 14 testes focados de física, `physics2d` e cena passaram após a mudança.

## Fase 5: validação

- [ ] Executar todos os testes funcionais de cena, física e physics2d.
- [ ] Comparar resultados determinísticos antes/depois.
- [ ] Comparar tempo médio e p95 nos benchmarks.
- [ ] Verificar número de alocações no hot path.
- [ ] Testar cenas pequenas, médias e extremas.
- [ ] Confirmar que a melhoria não aumenta o custo de criação, attach ou destruição de forma desproporcional.
- [ ] Documentar ganhos, regressões e cenários não cobertos.

## Ordem recomendada

1. Instrumentação e benchmark.
2. Flag para evitar compactação por frame.
3. Fila incremental de rebuild em `PhysicsWorld2D`.
4. Reuso de buffers temporários da broadphase.
5. Cache de AABBs.
6. Índice de joints que bloqueiam colisão.
7. Atualização conjunta e cache de transforms.
8. Cache de atividade/visibilidade hierárquica.
9. Otimizações de render queue e solver orientadas pelos resultados.

## Critério de aceite

Uma mudança só deve ser mantida quando:

- reduz o tempo medido no cenário afetado;
- não aumenta significativamente o custo dos demais cenários;
- preserva os testes funcionais e a ordem visual/física esperada;
- não introduz alocações recorrentes no hot path;
- possui uma métrica ou benchmark que permita detectar regressão futura.
