# Chipmunk2D → Kinetix2D: ideias práticas

Investigação do source real de `/media/projectos/projects/cpp/fisica/Chipmunk2D` (include/chipmunk + src, não docs) contra o estado atual de `kx` (physics/include/kx + physics/src, ver PLAN.md). Cada item: o que o Chipmunk faz (com ficheiro/função), o que kx faz hoje, veredito (adopt/adapt/skip) e esforço aproximado.

Estado atual do kx relevante (para não repetir por item): tree broadphase (dynamictree.h, parcialmente integrado), solver de impulso sequencial com warm start via `ct::HashMap<pairKey, StoredImpulses>` + Baumgarte bias + split-impulse position correction separado (`SolveContactPositions`), `Body{Static,Kinematic,Dynamic}`, multi-shape bodies, `MouseJoint`, `Joint` seam para Distance/Wheel, sem sleeping, sem filtros, sem queries além de `BodyAtPoint`, `Edge` já tem `vertex0/vertex3` (ghost vertices) e `oneSided` — herdado do phys/Box2D.

---

## 1. Solver: biased velocities vs split-impulse separado

**Chipmunk:** `src/cpArbiter.c::cpArbiterApplyImpulse` (linhas ~459-498) e `cpArbiterPreStep` (~416-439). Não separa "solve velocity" de "solve position" em duas passadas como Box2D — mantém dois pares de velocidade por corpo: `v/w` (velocidade real) e `v_bias/w_bias` (velocidade fictícia, só para correção posicional, nunca integrada em posição real nem transferida a joints). Cada iteração do solver resolve as duas em conjunto no mesmo loop: `jbn = (bias - vbn)*nMass` acumulado em `jBias` (clamped ≥0) aplicado via `apply_bias_impulses` a `v_bias/w_bias`; `jn`/`jt` normais acumulados como sempre, aplicados a `v/w` via `apply_impulses`. O bias por contacto é pré-computado uma vez em `cpArbiterPreStep`: `con->bias = -bias*min(0, dist+slop)/dt` (Baumgarte clássico, não NGS/TOI). `cpBody.c` integra posição usando `v + v_bias` (ver `cpBodyUpdatePosition`), depois zera `v_bias` no próximo passo.

**kx hoje:** separa `SolveContactVelocities()` de `SolveContactPositions()` como duas fases distintas de `World::Step` (estilo Box2D moderno: velocity solve N iterações, depois um passo de position solve baseado em push-out de overlap, sem re-tocar velocidades). Já tem Baumgarte constants (`kLinearSlop/kBaumgarte/kVelocityThreshold`) portados do phys.

**Veredito: skip.** O truque do Chipmunk (v_bias) é elegante mas resolve o mesmo problema que o split-impulse position pass do kx já resolve, só que fundido numa única passada por eficiência em 1D. Trocar a arquitetura atual (já testada, 22 testes verdes) por biased-velocities é reescrever o solver todo sem ganho claro de stacking — Box2D's split impulse (que é o que kx já tem) é comprovadamente equivalente ou melhor em qualidade. Não seguir esta pista.

**O que vale a pena olhar aqui:** `surface_vr` em `cpArbiterUpdate` (linha ~396) — velocidade de superfície (conveyor belts) subtraída da velocidade relativa antes do solve de atrito. Isto é uma feature nova (não arquitetura de solver), ver item 6.

---

## 2. Sleeping / idling

**Chipmunk:** `src/cpSpaceComponent.c`, função central `cpSpaceProcessComponents` (linha ~220). Fluxo:
1. Para cada dynamic body: `keThreshold = m * dvsq` onde `dvsq = idleSpeedThreshold² ou |gravity|²*dt²` por default; `idleTime` acumula `dt` se `cpBodyKineticEnergy(body) <= keThreshold`, senão reseta a 0 (linha ~249).
2. Qualquer arbiter tocando um kinematic ou um corpo já sleeping acorda o outro lado (`cpBodyActivate`, linha ~261).
3. **Union-Find via flood-fill no grafo de contactos**, não islands pré-computados: `FloodFillComponent` (linha ~194) faz DFS a partir de cada body sem "root", andando por `CP_BODY_FOREACH_ARBITER` e `CP_BODY_FOREACH_CONSTRAINT`, montando uma lista ligada `sleeping.next` presa a um `sleeping.root`. Kinematic/static nunca entram no grupo (agem como "fronteira").
4. `ComponentActive` (linha ~210): grupo só dorme se **todos** os membros têm `idleTime >= sleepTimeThreshold`.
5. Ao dormir (`cpSpaceDeactivateBody`, linha ~83): remove o body de `dynamicBodies`, migra as suas shapes de `dynamicShapes` para `staticShapes` no spatial index (deixa de ser testado no broadphase dinâmico — só reagrupa se algo dinâmico o tocar), e "congela" os arbiters/constraints copiando os contactos para memória própria (para não perderem warm-start ao acordar).
6. Acordar (`cpBodyActivate`, linha ~119) é o inverso: percorre a lista ligada do componente todo, re-insere no broadphase dinâmico, restaura arbiters.

Nada de Box2D-style "island" pré-alocado com solver corrido por ilha — é simplesmente: acumula tempo de energia baixa por corpo, faz union-find preguiçoso via flood-fill todo o passo, e p'ra os que ficaram abaixo do threshold em bloco.

**kx hoje:** nenhum sleeping. Todo corpo dinâmico integra e é testado no broadphase todos os frames.

**Veredito: adopt.** É consideravelmente mais simples que Box2D islands (Box2D mantém uma estrutura de ilha persistente com stack-based DFS e reordena o array de bodies por ilha para o solver percorrer coeso; Chipmunk resolve tudo lazy, por flood-fill a cada passo, sem reestruturar o solver). Dado que kx já resolve contactos globalmente (não por ilha) — nem Box2D-style islands fariam sentido sem reescrever o solver — o modelo Chipmunk encaixa diretamente: kinetic energy threshold por corpo, flood-fill component via `ContactInfo`/`Joints` (kx já tem essas listas por passo), sleeping bodies saltam integração e saem do broadphase dinâmico (voltam a ficar como "estático" para o tree, exatamente como o dynamictree.h de kx já distingue proxies). Necessário: `mIdleTime` em `Body`, `sleepTimeThreshold` em `World`, skip de `IntegrateVelocity/Position` e do narrowphase para grupos dormentes, acordar em qualquer novo contacto/impulso externo (`ApplyImpulse`, `MouseJoint` grab).

**Esforço:** médio — ~1 sessão. Depende do broadphase tree já estar estável (PENDING WORK #1 do PLAN.md); fazer depois de finalizar a integração da tree, porque sleeping precisa mover proxies entre "ativo"/"dormente" no mesmo mecanismo que a tree já usa para bodies estáticos.

---

## 3. Collision filtering: cpShapeFilter vs b2Filter

**Chipmunk:** `include/chipmunk/cpShape.h` linha ~52:
```c
typedef struct cpShapeFilter {
    cpGroup group;        // mesmo group != 0 em ambos => nunca colide (override)
    cpBitmask categories;  // "o que eu sou"
    cpBitmask mask;         // "o que eu quero atingir"
} cpShapeFilter;
```
Regra de teste (implementada inline onde os filtros são usados, ex. `cpShapesCollide`/broadphase pair test): colide se `(a.group == 0 || a.group != b.group) && (a.categories & b.mask) && (b.categories & a.mask)`. Só um group value (não positivo/negativo) — group é puramente "nunca colidir dentro do mesmo grupo", sem a noção de "sempre colidir" do Box2D.

**Box2D `b2Filter`** (referenciado no PLAN.md item pendente #2): `categoryBits`, `maskBits`, `groupIndex` (i16 assinado) — mesmo groupIndex positivo força colisão sempre, mesmo negativo força nunca, override sobre categorias/máscara.

**kx hoje:** nenhum filtro; pendência #2 do PLAN.md já pede explicitamente o modelo b2Filter (groupIndex assinado + categorias/máscara), com exemplo de "debris ignora outros debris mas colide com chão".

**Veredito: skip a mudança de modelo, mas nota de implementação.** O pedido do utilizador já é explicitamente b2Filter — não há motivo para trocar por cpShapeFilter (grupo unidirecional, sem "sempre colide"). Mas vale adotar UM detalhe do Chipmunk: manter o filtro **por shape** (não só por body) — cpShapeFilter vive em `cpShape`, não em `cpBody`, e o PLAN.md deixou em aberto "body-level first if simpler". Rounded a favor de per-shape desde já evita ter de re-arquitetar Body/Shape depois (custo de portar de body-level para shape-level mais tarde é maior que fazer certo agora, já que `Shape` já é um array dentro de `Body` com dados próprios de densidade — adicionar `Filter filter;` ao struct `Shape` é trivial). Efeito líquido: continuar com b2Filter semantics tal como planeado, só decidir "per-shape" agora.

**Esforço:** baixo (já estava planeado — só a nota per-shape muda algo).

---

## 4. Queries: point / segment / AABB

**Chipmunk API** (`include/chipmunk/cpSpace.h` ~196-218):
- `cpSpacePointQuery(space, point, maxDistance, filter, func, data)` — callback por shape encontrada, devolve `cpPointQueryInfo{shape, point, distance, gradient}` (distância negativa = dentro da shape; `gradient` é o vetor normalizado da signed-distance function, útil para push-out).
- `cpSpacePointQueryNearest(...)` — variante que devolve só a shape mais próxima, sem callback.
- `cpSpaceSegmentQuery` / `cpSpaceSegmentQueryFirst` — raycast espesso (tem `radius`, i.e. capsule-cast, não só raio fino), devolve `alpha` (posição normalizada 0..1 ao longo do segmento) + `normal`.
- `cpSpaceBBQuery` — teste AABB-only, mais barato, usado quando não precisas de exatidão de forma (ex. seleção grosseira).
- `cpSpaceShapeQuery` — testa uma shape arbitrária contra o espaço, devolve `cpContactPointSet` por shape sobreposta.

Cada shape implementa a sua própria `cpShapePointQuery`/`cpShapeSegmentQuery` (`src/cpShape.c` — ver `cpSegmentShapePointQuery`/`cpSegmentShapeSegmentQuery` linha ~408/426, análogas para poly/circle); o Space (`src/cpSpaceQuery.c`) só faz o walk na `cpSpatialIndex`/BBTree e delega para a shape.

**kx hoje:** só `World::BodyAtPoint` (point-in-shape simplificado, sem distância nem gradiente). O PLAN.md M4.5 já pede QueryAABB/QueryCircle (brute-force primeiro) para Explode e mouse picking, e raycast é mencionado na missão como "planned kx QueryAABB/QueryCircle/raycast".

**Veredito: adopt (a forma da API, não o código C).** A separação Chipmunk é o formato certo para o roadmap do kx:
- `QueryAABB` (kx) ≈ `cpSpaceBBQuery` — mais barato, usa a tree existente (`DynamicTree::Query` já tem o método certo, só falta expor um callback de shape/body).
- `QueryCircle`/`QueryPoint` (kx, para Explode/picking) ≈ `cpSpacePointQuery` com `maxDistance` — usar a AABB da query para filtrar via tree primeiro, testar shape real depois (Chipmunk faz exatamente esta dupla filtragem: spatial index depois shape test).
- Raycast futuro (mencionado como "planned") ≈ `cpSpaceSegmentQuery` — vale portar o formato `alpha`+`normal`+`radius` (capsule-cast é praticamente grátis de suportar desde o início e cobre balas/lasers com espessura sem código extra depois).
- Devolver `distance`+`gradient` tipo `cpPointQueryInfo` é diretamente útil para `Explode` (falloff por distância) sem recalcular no chamador.

**Esforço:** médio — a estrutura de callback + a filtragem em duas fases (AABB tree depois shape exato) é o essencial a copiar; a matemática de point-query/segment-query por shape já existe em `collide.cpp`/`shapes.cpp` do kx em formas próximas (SAT, clipping) e só precisa de wrappers "distância a um ponto" por tipo de shape.

---

## 5. cpPolyShape com radius / cpSegmentShape com neighbors — ghost collisions

**Rounded polygons:** `cpPolyShapeInit(poly, body, count, verts, transform, radius)` (`cpPolyShape.h` ~29) — todo polígono no Chipmunk tem um `radius` opcional (arestas "infladas", Minkowski sum com um círculo), exatamente como Box2D's `b2_polygonRadius`. **kx já tem isto**: `Polygon::radius` existe em shapes.h (`kPolygonRadius` default), herdado do phys/Box2D. Nada a fazer aqui — já portado.

**Segment neighbors (o item realmente novo):** `cpSegmentShapeSetNeighbors(shape, prev, next)` (`cpShape.h` ~188, impl `src/cpShape.c` ~539) grava `a_tangent = prev - a`, `b_tangent = next - b` no `cpSegmentShape` (struct em `chipmunk_structs.h` ~209-217: `a,b,n` + `ta,tb,tn` (transformados) + `a_tangent,b_tangent`). O uso está em `src/cpCollision.c` (linhas 560-564, 595-599, 652-654): ao testar colisão perto de um endpoint do segmento, a condição de aceitação do contacto inclui `dot(normal, rotate(a_tangent)) >= 0` (ou `<= 0` no lado B) — ou seja, **rejeita o contacto no "endcap" se a normal encontrada aponta para dentro do ângulo coberto pelo próximo segmento adjacente**. É um teste geométrico local por-par-de-colisão, não uma normal suavizada globalmente como o ghost-vertex clipping do Box2D (que reescreve a normal do manifold usando os vértices fantasma vertex0/vertex3 durante o SAT/clipping da polygon).

**kx hoje:** `Edge` já tem `vertex0`/`vertex3` (ghost vertices) — ver shapes.h linha 88 — herdado fielmente do phys/Box2D (`CollideEdgeAndPolygon` referenciado no PLAN.md M2). Isto é a abordagem Box2D "clássica": os ghost vertices entram no cálculo de separação/clipping da manifold em si.

**Comparação honesta:** as duas técnicas resolvem o mesmo bug (colisão fantasma na junção entre segmentos consecutivos de um terreno, ex. um carro "salta" ao cruzar o vértice entre duas edges coplanares) por caminhos diferentes:
- Box2D/kx: ghost vertices entram no algoritmo de clipping/SAT — mais integrado, mas mais código a manter dentro de `CollideEdgeAndPolygon`.
- Chipmunk: teste de rejeição pós-hoc simples e local (um dot product por endpoint) — mais barato de raciocinar, mas só filtra colisões no *endpoint*, não reescreve a normal como Box2D faz para o caso "quase no vértice mas ainda dentro da face" (onde a normal da manifold pode ficar ligeiramente errada sem clipping consciente da vizinhança).

**Veredito: skip — já resolvido.** kx já tem a versão Box2D (ghost vertices) portada e testada (29 casos incluindo "edge voronoi regions" no `kx_collision_tests.cpp`, conforme PLAN.md). Não vale trocar por uma técnica mais fraca. **Mas** vale registar como referência: se ao portar terrenos longos (várias Edges em cadeia, tipo ChainShape) surgir algum bug residual de "bump" nas junções que o ghost-vertex approach não cobrir bem (ex. quando os dois segmentos não são exatamente coplanares em ângulo raso), o teste de tangente do Chipmunk é um fallback barato e local a acrescentar por cima, sem mexer no clipping.

---

## 6. Outras features notáveis

### 6.1 `cpArbiter` — eventos de colisão (begin/preSolve/postSolve/separate)
`include/chipmunk/cpArbiter.h` + `src/cpArbiter.c` (`cpArbiterCallWildcardBeginA/B`, `PreSolveA/B`, `PostSolveA/B`, `SeparateA/B`, linhas 249-313) e o estado da arbiter em si (`arb->state`: `CP_ARBITER_STATE_FIRST_COLLISION` → begin, `CACHED`/`INVALIDATED` → separate). O handler é resolvido por par de `cpCollisionType` (`cpSpaceLookupHandler`, linha 348) num hashset, com suporte a "wildcard" handlers (um handler que reage a qualquer tipo colidindo com um tipo específico). Callbacks recebem `cpArbiter*` de onde se lê `cpArbiterGetShapes`/`GetBodies`/`GetContactPointSet`/`GetNormal`/`TotalImpulse` (linha 141-247) — dados ricos já persistentes por-par, reaproveitando exatamente o warm-start.

**kx hoje:** zero eventos de colisão. `ContactInfo` existe em `world.h` mas é só um snapshot recalculado todo o frame — sem noção de "begin"/"separate", sem callback de gameplay.

**Veredito: adopt.** Todo jogo precisa disto (som de impacto, dano, trigger de sensor, partículas — o PLAN.md já lista "Particles: debris/effects driven by physics events" em M4.5). kx já tem os dados certos por contacto (`ContactInfo` com body/shape/manifold/normal) e já faz warm-start via `mImpulseMap` keyed por par — a única peça em falta é: (a) por-par um estado First/Persisting/Separated comparando o key do frame atual com o anterior (fácil: `mImpulseMap` já existe como o "cache" — um contacto cujo key não estava no map no frame anterior é "begin", um que estava no anterior mas não está no atual é "separate"); (b) um mecanismo de callback — não precisa do hashset de collision-type do Chipmunk (over-engineering para o tamanho do kx); um `Body::onCollisionBegin/onCollisionEnd` opcional (function pointer ou pequena interface, sem std::function) já cobre 90% do valor sem replicar a máquina de collision-type do Chipmunk.

**Esforço:** baixo-médio — a contabilidade de estado por-par é quase grátis por cima do `mImpulseMap` já existente; o mecanismo de callback é a parte nova.

### 6.2 Surface velocity (conveyor belts) — `cpShapeSetSurfaceVelocity`
`cpArbiter.c` linha ~396: `surface_vr = (b.surfaceV - a.surfaceV)` projetada tangencialmente, subtraída na resolução de atrito. Dá tapetes rolantes / superfícies "arrastando" objetos sem precisar de um joint. **Veredito: adapt, baixa prioridade.** Trivial de acrescentar ao solver de atrito do kx (`SolveContactVelocities`) quando chegar a hora de gameplay 2D com conveyors — não é pedido agora, não building especulativo, mas vale a nota porque o custo de adicionar depois de já ter atrito implementado é ~10 linhas.

### 6.3 Moment/area/centroid helpers
`chipmunk.h` linhas 138-165: `cpMomentForCircle/Segment/Poly/Box`, `cpAreaForCircle/Segment/Poly`, `cpCentroidForPoly`, mais `cpConvexHull` (linha 167). **kx hoje:** `Shape::ComputeMass(density)` por tipo já faz o equivalente embutido no `.cpp` de cada shape (não como funções livres reutilizáveis). **Veredito: skip.** kx já cobre a mesma matemática (portada do phys/Box2D) só que como métodos, não funções livres soltas — não há lacuna funcional, só um estilo de API diferente sem motivo para trocar.

### 6.4 `cpBodyVelocityFunc`/`cpBodyPositionFunc` (custom integration)
`cpBody.h` linhas 44-149: `cpBodySetVelocityUpdateFunc`/`SetPositionUpdateFunc` deixam substituir a integração default por corpo (gravidade custom, damping por corpo, movimento scriptado tipo `KinematicBody` com trajetória própria). **kx hoje:** `IntegrateVelocity`/`IntegratePosition` são métodos fixos de `Body`; Kinematic já integra posição sozinho a partir da própria velocidade (equivalente ao caso mais comum de uso deste hook no Chipmunk). **Veredito: skip por agora.** É uma seam genérica sem consumidor nomeado no roadmap do kx (regra "build to use" do PLAN.md) — WheelJoint/car não precisa disto, é o carro a aplicar torque via joint, não a substituir a integração do corpo.

### 6.5 Gear/Ratchet/Servo/RotaryLimit/Groove/Pin/Slide joints
`cpGearJoint.h`, `cpRatchetJoint.h`, `cpSimpleMotor.h`, `cpRotaryLimitJoint.h`, `cpGrooveJoint.h`, `cpPinJoint.h`, `cpSlideJoint.h` — família completa de constraints 1-DOF. **kx hoje:** `JointType{Distance,Wheel,Mouse}` — só os três já planeados. **Veredito: skip.** Nenhum consumidor nomeado no roadmap atual (carro usa Wheel+Distance/Revolute, não Gear/Ratchet). Regra do PLAN.md é clara: nada especulativo. Se aparecer um caso de uso concreto (ex. guincho/catraca num jogo específico), voltar a este ficheiro como referência — `cpRatchetJoint.c` é pequeno (~100 linhas) e direto de portar quando chegar a hora.

---

## Top-5 priorizado (alinhado ao roadmap: joints/car agora → masks/groups → queries → Explode → CreateMesh → luzes)

1. **Sleeping system (item 2)** — maior ganho de performance/qualidade de vida por esforço médio; kx ainda corre todo corpo todo frame. O modelo Chipmunk (flood-fill lazy por passo, sem reescrever o solver) encaixa diretamente na arquitetura atual do kx sem exigir islands Box2D-style. Fazer logo a seguir a fechar a tree broadphase (PENDING #1), antes ou depois dos joints — é ortogonal ao trabalho de Wheel/Distance.
2. **Collision events begin/separate (item 6.1)** — quase grátis por cima do `mImpulseMap` já existente (a contabilidade de estado por-par já está 90% lá); é o que destrava som/dano/partículas que M4.5 já lista como consumidor. Fazer depois do solver de joints estar fechado, antes de Explode (Explode vai querer notificar impacto também).
3. **Queries em formato Chipmunk — AABB/point com distância+gradiente + raycast capsule (item 4)** — é literalmente o que PENDING WORK #3 e M4.5 já pedem; a contribuição do Chipmunk aqui é a *forma* certa da API (dupla filtragem tree→shape, `alpha`+`normal` no raycast, `distance`+`gradient` no point query) para não reinventar mal. Bloco direto para Explode e mouse picking melhor que o `BodyAtPoint` atual.
4. **Filtro per-shape (item 3, nota de implementação)** — já é trabalho planeado (PENDING #2, b2Filter semantics); a única correção vinda do Chipmunk é fixar "per-shape" agora em vez de "per-body" para não ter de re-arquitetar depois. Esforço quase zero, evita dívida técnica futura.
5. **Surface velocity / conveyor (item 6.2)** — baixa prioridade mas baixo custo; anotar para quando o atrito já estiver maduro (já está) e aparecer o primeiro caso de gameplay com tapete rolante. Não abrir agora (regra "build to use"), só não esquecer que custa ~10 linhas no solver de atrito quando for pedido.

**Fora do top-5 deliberadamente:** biased-velocity solver (item 1) e ghost-vertex-via-tangente (item 5 do corpo do relatório) — ambos são "outra forma de fazer o que kx já faz corretamente", trocar não paga esforço. Gear/Ratchet/Servo/custom-integration-func (6.4/6.5) — sem consumidor nomeado, ficam como referência para quando (se) aparecer um caso concreto.
