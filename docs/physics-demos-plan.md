# Plano de Física → Mais Demos (referências: Box2D samples + Chipmunk demos)

Plano **só do motor `kx`**: o que já existe, quais demos dos testbeds de referência estão bloqueadas por cada lacuna, e em que ordem portar as features para desbloquear o máximo de demos pelo menor esforço. Fontes lidas: `/media/projectos/projects/cpp/fisica/box2d/samples` (140+ samples registados por `RegisterSample`) e `/media/projectos/projects/cpp/fisica/Chipmunk2D/demo` (24 demos C).

---

## 0. Estado atual do kx (verificado hoje)

| Área | Estado |
|---|---|
| Colisão | Circle / Polygon / Edge (SAT + clipping, feature ids), ghost vertices, radius, 100% fidelidade vs `phys` (kx_fidelity 75k casos) |
| Solver | Impulso sequencial, warm start (`mImpulseMap` por par), Baumgarte + split-impulse position, atrito, restituição |
| Sleeping | **FEITO** — `UpdateSleeping`, `kSleepVelocity/kSleepAngularVelocity/kTimeToSleep`, `SetAwake` (kx_sleep_tests) |
| Broadphase | Brute + DynamicTree (toggle `SetTreeBroadphase`), pares persistentes |
| Filtros | **FEITO** — `Filter{category,mask,group}` per-shape, `ShouldCollide`, `CollideConnected` |
| Joints | Mouse, Wheel, Revolute (motor+limits), Distance (rigid+spring), Gear (revolute-revolute) |
| Queries | `BodyAtPoint`, `QueryAABB`, `QueryCircle` (brute) — **sem raycast/segment** |
| Shape gen | `CreateMesh` (poly2tri), `CreateFromImage` (Chipmunk march), `AddPolygon` (hull) |
| Utilidades | `Explode`, debug draw (shapes/AABB/contacts/joints) |
| **NÃO tem** | eventos de colisão, sensores, raycast, prismatic/motor/groove joint, joint breaking, CCD/TOI, gravityScale/damping, capsule/chain, surface velocity, rolling resistance, splitting, buoyancy |

## 1. Testbed atual (20 demos)

- **Basic (8):** Single Box, Pyramid Stack, Tilted Stack, Plink, Tumble, Bouncy Hexagons, Pyramid Topple, Convex
- **Joints (10):** Bridge, Gears, Blob, Car, Pendulum, Chain, Springies, Pump, Theo Jansen, Tank
- **World (2):** Planet, Image Shape

## 2. Inventário de referência ainda NÃO portado (por feature que o desbloqueia)

### Bloqueadas por eventos de colisão + sensores + userData  →  **~25 demos**
| Demo | Origem |
|---|---|
| OneWay, Sticky, Player, LogoSmash, Shatter, Crane, Unicycle, ContactGraph | Chipmunk |
| Contact, Persistent Contact, Sensor Funnel, Sensor Hits, Sensor Types, Sensor Bookend, Foot Sensor, Body Move, Joint, Projectile Event, Circle Impulse | Box2D Events |
| Dynamic Mover, Geometric Mover (foot sensor/character) | Box2D Character |

### Bloqueadas por raycast / segment query  →  **~7 demos**
| Demo | Origem |
|---|---|
| Query, Player (visão/aim), Slice (ray de corte) | Chipmunk |
| Ray Cast, Cast World, Shape Cast, Overlap World | Box2D Collision |

### Bloqueadas por joint breaking (maxForce)  →  **~4 demos**
| Demo | Origem |
|---|---|
| Breakable, Breakable Chains | Box2D |
| Sticky, LogoSmash | Chipmunk |

### Bloqueadas por novos joints: Prismatic / Motor / Groove(Slot)  →  **~13 demos**
| Demo | Origem |
|---|---|
| Crane (winch prismatic + dolly groove), Unicycle (groove), Tank (maxForce) | Chipmunk |
| Prismatic, Motor Joint, Doohickey, Scissor Lift, Gear Lift, Driving, Door, Ragdoll, Soft Body, User Constraint | Box2D Joints |

### Bloqueadas por CCD / TOI (continuous)  →  **~20 demos**
| Demo | Origem |
|---|---|
| Chain Drop, Chain Slide, Pinball, Ghost Bumps, Restitution Threshold, Skinny Box, Wedge, Safety Factor, Drop, Bounce House, Bounce Humans, Segment Slide, Speculative Fallback/Ghost/Sliver, Pixel Imperfect | Box2D Continuous |
| StaticVsBulletBug | Box2D Issues |
| Rain, Cast (bullets) | Box2D Benchmark |

### Bloqueadas por gravityScale / damping por corpo  →  **~5 demos**
| Demo | Origem |
|---|---|
| Wind | Box2D Shapes |
| Planet (limpar o hack manual em Step()) | Chipmunk / já temos |
| Weeble, Bad, Pivot | Box2D Bodies |

### Bloqueadas por capsule / ChainShape  →  **~8 demos**
| Demo | Origem |
|---|---|
| Chain Shape, Chain Link, Chain Segment, Capsule Stack | Box2D Shapes/Stacking |
| Segment Slide, Chain Drop | Box2D Continuous |
| Unicycle (calha), Chains (versão chain-shape) | Chipmunk |

### Bloqueadas por surface velocity / rolling resistance  →  **~5 demos**
| Demo | Origem |
|---|---|
| Conveyor Belt, Tangent Speed, Rolling Resistance, Top Down Friction | Box2D Shapes |
| OneWay (plataforma com conveyor) | Chipmunk |

### Bloqueadas por splitting de shapes em runtime  →  **~5 demos**
| Demo | Origem |
|---|---|
| Slice, Shatter, LogoSmash | Chipmunk |
| Modify Geometry, Recreate Static | Box2D Shapes |

### Bloqueadas por buoydancia / fluido  →  **1 demo**
| Demo | Origem |
|---|---|
| Buoyancy | Chipmunk |

---

## 3. Plano por fases (ordem recomendada)

Regra do PLAN.md: **nada especulativo** — cada feature só é feita porque tem consumidor nomeado (as demos acima). Cada fase termina com demos portadas para o testbed como validação.

### Fase 0 — Quick wins: demos que já dão para portar HOJE (sem tocar no motor)
Portar para o testbed, zero código em `kx`:
- **Box2D Stacking:** Vertical Stack, Double Domino, Circle Stack, Arch, Cliff, Confined, Card House
- **Box2D Joints:** Ball & Chain, Cantilever, Distance Joint, Wheel, Revolute, Joint Grid, Bridge (versão b2), Falling Hinges
- **Box2D Bodies/Bench:** Body Type, Kinematic, Set Velocity, Sleep, Wake Touching, Tumbler, Spinner, Smash, Washer, Many Tumblers, Large Pyramid, Compounds, Junkyard
- **Chipmunk:** Bench, Tumble/Pump/Plink/Joints/TheoJansen já temos; acrescentar versões b2 dos mesmos

**Entregável:** +20 a 25 demos no testbed, provam a robustez do solver atual antes de qualquer feature nova.

### Fase 1 — Eventos de colisão + sensores + userData  *(maior ROI, ~25 demos)*
- **O quê:** callbacks `begin/persist/end` por par de bodies + `isSensor` por shape + `userData` por body/shape.
- **Fonte:** Box2D `b2ContactListener` (`box2d/src/dynamics/b2_contact_solver.cpp`/`b2_world.cpp`); Chipmunk `cpArbiter` (já analisado em `chipmunk-ideas.md` item 6.1 — a contabilidade begin/separate é quase grátis por cima do `mImpulseMap` já existente).
- **Esforço:** médio. Mecanismo de callback simples (function pointer por body, sem hashset de collision-type do Chipmunk).
- **Validação:** OneWay, Sticky, Sensor Funnel, Foot Sensor, ContactGraph; depois Player/Crane.

### Fase 2 — Raycast + segment query  *(~7 demos)*
- **O quê:** `Raycast(origin, dir, maxT, out)` e `SegmentQuery(a, b, radius, out{alpha, normal})` (capsule-cast) + `QueryPoint` com distância/gradiente (estilo `cpPointQueryInfo`).
- **Fonte:** Chipmunk `cpSpaceSegmentQuery`/`cpSpacePointQuery` (forma da API, `chipmunk-ideas.md` item 4); math de interseção já existe em `collide.cpp` (SAT/clipping) — falta só o wrapper por shape.
- **Esforço:** médio. Dupla filtragem tree→shape (a `DynamicTree::Query` já existe).
- **Validação:** Query, Player (visão), Explode melhorado; turret/mira.

### Fase 3 — Joint breaking (maxForce)  *(~4 demos)*
- **O quê:** `Joint::SetMaxForce`/`SetMaxTorque` + break quando a reação excede o limite (checar após o solve; precisa de acumulador de impulso por joint — os joints já acumulam internamente).
- **Fonte:** Box2D `Breakable` sample (usa `GetReactionForce` + acumulador por frame); Chipmunk `cpConstraintSetMaxForce` + `Sticky`.
- **Esforço:** baixo.
- **Validação:** Breakable, Breakable Chains, Sticky.

### Fase 4 — Prismatic + Motor + Groove joints  *(~13 demos)*
- **O quê:** `PrismaticJoint` (pistão, com motor/limites, e a variante usada no GearJoint — hoje o Gear é só revolute-revolute), `MotorJoint` (Box2D, ideal para ragdoll/soft body), `GrooveJoint` (Chipmunk, pequeno ~100 linhas).
- **Fonte:** `box2d/src/dynamics/b2_prismatic_joint.cpp`, `b2_motor_joint.cpp`; Chipmunk `cpGrooveJoint.c`.
- **Esforço:** médio (prismatic), baixo-médio (motor), baixo (groove).
- **Validação:** Prismatic, Doohickey, Scissor Lift, Gear Lift, Crane, Unicycle, Ragdoll, Soft Body.

### Fase 5 — Coisas baratas: gravityScale/damping, surface velocity, rolling resistance  *(~6 demos)*
- **O quê:** `Body::SetGravityScale(float)` + `SetLinearDamping/SetAngularDamping` (2 linhas no `IntegrateVelocity`); `Shape::surfaceVelocity` (subtrair `surface_vr` no solve de atrito — ~10 linhas, `chipmunk-ideas.md` item 6.2); rolling resistance (termo angular no atrito).
- **Fonte:** Box2D `b2BodyDef`; Chipmunk `cpShapeSetSurfaceVelocity`.
- **Esforço:** trivial-baixo, cada um isolado.
- **Validação:** Wind, Conveyor Belt, Tangent Speed, Rolling Resistance, Top Down Friction, Planet sem hack.

### Fase 6 — Capsule + ChainShape  *(~8 demos)*
- **O quê:** primitiva `Capsule` (segmento gordo — a collider de segment+circle já existe), `ChainShape`/loop de edges partilhando vértices (kx já tem ghost vertices `vertex0/vertex3` — `chipmunk-ideas.md` item 5 diz que a base está lá).
- **Fonte:** Box2D `b2Capsule`/`b2ChainSegment`/`b2ChainShape` (`box2d/src/collision/b2_shape.c`); Chipmunk fat segments.
- **Esforço:** médio (capsule baixo, chain médio).
- **Validação:** Chain Shape, Chain Link, Chain Segment, Capsule Stack, Segment Slide.

### Fase 7 — CCD / TOI  *(~20 demos, o maior bloco)*
- **O quê:** corpos "bullet" com speculative contacts ou TOI (swept). Há `kSpeculativeDistance` em `common.h` (constante já portada) mas sem wiring.
- **Fonte:** Box2D `b2TimeOfImpact` (`box2d/src/collision/b2_time_of_impact.cpp`) + `b2_contact_solver` speculative branch.
- **Esforço:** alto. Fazer por último; desbloqueia os 16 demos Continuous + robustness (bullets, chain slide rápido).
- **Validação:** Drop (bullets), Chain Slide, Pinball, StaticVsBulletBug.

### Fase 8 — Splitting de shapes em runtime  *(~5 demos)*
- **O quê:** cortar polígono por um segmento (ray) e re-criar o corpo com as metades (usa `AddPolygon`/`CreateMesh` já existentes).
- **Fonte:** Chipmunk `Slice`/`Shatter`/`LogoSmash` (a lógica de corte vive nos demos, não no motor — o motor só precisa de destruir/recriar body sem quebrar; `Destroy` + `ct::Pool` já existem).
- **Esforço:** médio, maioritariamente código de demo (clipping de polígono).
- **Validação:** Slice, Shatter, LogoSmash.

### Fase 9 — Buoyancy / fluido  *(1 demo)*
- **O quê:** forças de flutuação por shape submersa (estilo Chipmunk `Buoyancy.c` — sample, não motor). Modelo de simulação distinto (partículas/LiquidFun) fica como referência futura, não agora.
- **Esforço:** baixo se for só o sample Buoyancy (forças); alto se for um sistema de partículas.
- **Validação:** Buoyancy.

---

## 4. Resumo / prioridade

| Fase | Feature | Esforço | Demos desbloqueadas |
|---|---|---|---|
| 0 | Portar demos (zero motor) | baixo | ~25 |
| 1 | Eventos + sensores + userData | médio | ~25 |
| 2 | Raycast / segment query | médio | ~7 |
| 3 | Joint breaking (maxForce) | baixo | ~4 |
| 4 | Prismatic / Motor / Groove | médio | ~13 |
| 5 | gravityScale / damping / surface vel / rolling | trivial-baixo | ~6 |
| 6 | Capsule + ChainShape | médio | ~8 |
| 7 | CCD / TOI | alto | ~20 |
| 8 | Splitting runtime | médio | ~5 |
| 9 | Buoyancy | baixo-alto | 1 |

**Ordem sugerida de execução:** Fase 0 → 1 → 2 → 3 → 5 → 4 → 6 → 7 → 8 → 9.
(Fases 5 e 3 trocam com a 4 se preferires demos de joints primeiro — a 4 depende de 3 só para o `maxForce`/reação, que é barato.)

**Notas de prioridade estratégica:**
- **Fase 1** é o maior multiplicador: sem eventos, metade dos demos interativos (OneWay/Sticky/Player/sensores/character) não existem.
- **Fase 7 (CCD)** é o único bloco "pesado" de verdade; os demos Continuous são os que mais dependem dele, mas também os mais fáceis de adiar.
- **Chipmunk Buoyancy** é o único demo do Chipmunk que não será portável sem um modelo de fluido — deixar para o fim.
