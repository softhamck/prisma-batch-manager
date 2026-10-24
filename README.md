# Prisma Batch Manager

![CI](https://github.com/softhamck/prisma-batch-manager/actions/workflows/ci.yml/badge.svg)

Programa de consola en C++17 que gestiona los baches de una tostadora de café. Dos hilos independientes trabajan al mismo tiempo sobre los mismos datos de baches sin interferir entre sí, y los eventos de creación, edición y consulta se inyectan por consola mediante comandos.

**Descripción**

Cada bache transforma una cantidad de café verde en café tostado y tiene:

- Identificador único
- Peso inicial y peso final (kg)
- Estado: `Created`, `Roasting`, `Cooling`, `Finished` o `Cancelled`

Cada hilo puede crear baches, consultarlos por identificador, cambiar su estado, listar los finalizados y calcular la merma porcentual de un bache.

## Requisitos

- CMake 3.16 o superior
- Compilador con soporte de C++17, incluido `std::from_chars` para `double`. Probado con MSVC (Visual Studio 2022/2026) y GCC 13.
- Conexión a internet la primera vez que se configura el proyecto: CMake descarga GoogleTest para las pruebas. Para compilar sin pruebas: `-DPRISMA_BUILD_TESTS=OFF`.

## Compilación y ejecución

```bash
cmake -S . -B build
cmake --build build --config Debug
```

Ejecutar en modo interactivo:

```bash
# Windows
build\Debug\batch_manager.exe

# Linux
./build/batch_manager
```

Ejecutar el script de ejemplo, que recorre todos los comandos, incluidos casos inválidos:

```bash
# Windows (CMD)
build\Debug\batch_manager.exe < examples\demo.txt

# Windows (PowerShell)
Get-Content examples\demo.txt | .\build\Debug\batch_manager.exe

# Linux
./build/batch_manager < examples/demo.txt
```

## Comandos disponibles

Cada comando se envía a uno de los dos hilos: `t1` o `t2`.


| Comando                                           | Descripción                                                   |
| --------------------------------------------------- | ---------------------------------------------------------------- |
| `<t1|t2> create <peso_inicial_kg>`                | Crea un bache                                                  |
| `<t1|t2> get <id>`                                | Muestra un bache                                               |
| `<t1|t2> state <id> <roasting|cooling|cancelled>` | Cambia el estado                                               |
| `<t1|t2> finish <id> <peso_final_kg>`             | Finaliza el bache registrando su peso final                    |
| `<t1|t2> finished`                                | Lista los baches finalizados                                   |
| `<t1|t2> shrink <id>`                             | Calcula la merma porcentual                                    |
| `help`                                            | Muestra la ayuda                                               |
| `exit`                                            | Espera a que los hilos terminen sus comandos pendientes y sale |

Los comandos no distinguen mayúsculas de minúsculas.

Ejemplo de sesión:

```
t1 create 120
[T1] Created Batch 1 with 120.00 kg
t1 state 1 roasting
[T1] Batch 1 -> Roasting
t2 get 1
[T2] Batch 1 | Roasting | initial 120.00 kg
t1 state 1 cooling
[T1] Batch 1 -> Cooling
t1 finish 1 102
[T1] Batch 1 -> Finished with 102.00 kg
t2 shrink 1
[T2] Batch 1 shrinkage: 15.00 %
t2 state 1 roasting
[T2] Batch 1: Invalid state transition
```

## Diseño

```
                         ┌─────────────┐     ┌─────────┐
                    ┌──► │ Cola de T1  │ ──► │ Hilo T1 │ ──┐
┌────────────────┐  │    └─────────────┘     └─────────┘   │    ┌─────────────────┐
│ Hilo principal │ ─┤                                      ├──► │ BatchRepository │
│ (lee consola)  │  │    ┌─────────────┐     ┌─────────┐   │    │ (shared_mutex)  │
└────────────────┘  └──► │ Cola de T2  │ ──► │ Hilo T2 │ ──┘    └─────────────────┘
```

El hilo principal solo lee y enruta comandos. Cada trabajador tiene su propia cola y ejecuta sus comandos en orden de llegada. El único dato compartido es el repositorio.


| Componente         | Responsabilidad                                                                               |
| -------------------- | ----------------------------------------------------------------------------------------------- |
| `Batch`            | Datos del bache, máquina de estados y cálculo de merma                                      |
| `BatchRepository`  | Dueño de los baches. Toda lectura y escritura pasa por sus métodos, protegidos con un mutex |
| `BlockingQueue<T>` | Cola FIFO segura entre hilos, basada en `std::condition_variable`                             |
| `Worker`           | Dueño de un hilo y su cola; ejecuta comandos contra el repositorio                           |
| `ConsoleLog`       | Serializa la salida para que las líneas de distintos hilos no se mezclen                     |
| `parseLine`        | Convierte una línea de consola en un comando, una petición de ayuda, una salida o un error  |

### Modelo y máquina de estados

```
Created ──► Roasting ──► Cooling ──► Finished
   │            │            │
   └────────────┴────────────┴──────► Cancelled
```

- Solo se avanza una etapa a la vez, como en el proceso físico.
- Un bache activo se puede cancelar en cualquier etapa.
- `Finished` y `Cancelled` son estados terminales.
- Solo se llega a `Finished` con el comando `finish`, que exige el peso final. Así un bache finalizado siempre tiene merma calculable.
- Merma % = (peso inicial − peso final) / peso inicial × 100.

### Concurrencia

- **Encapsulamiento:** los hilos nunca acceden al contenedor ni al mutex directamente, solo a los métodos del repositorio.
- **`std::shared_mutex`:** las escrituras toman un candado exclusivo y las consultas uno compartido, así varias lecturas pueden ocurrir a la vez. Con dos hilos un `std::mutex` también sería correcto; se eligió este porque las consultas son la operación más frecuente.
- **Verificar y modificar bajo el mismo candado:** la validación de la transición y el cambio de estado son una sola operación atómica, lo que evita condiciones de carrera de tipo *check-then-act*.
- **Copias, no referencias:** las consultas devuelven copias de los baches, nunca referencias a datos internos que podrían cambiar después de liberar el candado.
- **RAII:** los candados se liberan solos (`unique_lock`, `shared_lock`, `lock_guard`) y cada `Worker` cierra su cola y hace `join` en su destructor.
- **Apagado limpio:** `exit` cierra las colas; cada hilo termina los comandos pendientes antes de detenerse.

## Pruebas

36 pruebas unitarias con GoogleTest:

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Además de las reglas de negocio y el parser, hay pruebas específicas de concurrencia:

- 8 hilos creando 1000 baches cada uno nunca generan IDs repetidos.
- Si 8 hilos intentan finalizar el mismo bache a la vez, solo uno lo logra.
- Mientras varios hilos escriben, un lector nunca ve un bache finalizado sin peso final.

La integración continua (GitHub Actions) compila y ejecuta las pruebas en cada pull request en Ubuntu (GCC) y Windows (MSVC), y vuelve a ejecutarlas con **ThreadSanitizer** para detectar data races en tiempo de ejecución.

## Decisiones y supuestos

- Los datos viven en memoria durante la ejecución; no hay persistencia.
- Los IDs son enteros consecutivos que empiezan en 1.
- El peso final debe ser mayor que 0 y no mayor que el inicial, porque el tueste solo pierde masa.
- El parser valida el formato de los comandos; las reglas de negocio (pesos, transiciones) las valida el repositorio, sin importar de dónde venga el comando.
- Los errores esperados (comando mal escrito, transición inválida) se reportan con mensajes, no con excepciones.
- No se muestra un prompt `>` porque la salida de los hilos es asíncrona y lo partiría.
- Cuando se envían varios comandos de golpe, por ejemplo con el script de ejemplo, el orden entre la salida de T1 y la de T2 puede variar en cada ejecución. Lo que se garantiza es el orden dentro de cada hilo y la consistencia de los datos.

## Estructura del proyecto

```
include/prisma/   Interfaces públicas
src/              Implementación y main
tests/            Pruebas unitarias (GoogleTest)
examples/         Script de comandos de ejemplo
.github/          Integración continua
```

## Flujo de trabajo

- GitHub Flow: cada cambio en su rama y entra a `main` por pull request.
- Mensajes de commit con Conventional Commits (`feat`, `fix`, `test`, `ci`, `docs`...).
- Formato de código automático con `.clang-format`.
- `main` protegida: solo acepta pull requests con la integración continua en verde.

## Posibles mejoras

- Número de hilos configurable en lugar de `t1`/`t2` fijos.
- Persistencia de los baches (archivo o base de datos).
- Migrar a C++20 para usar `std::jthread`, o a C++23 para `std::expected` en lugar de enums de resultado.
