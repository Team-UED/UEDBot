<div align="center">

# UEDBot

### A StarCraft II bot that turtles just long enough to teleport a Battlecruiser into your mineral line.

Built in C++ using the StarCraft II API.

**57 wins. 3 draws. 0 losses.**

</div>

## What is this?

UEDBot is a competitive Terran bot for StarCraft II.

It builds a wall, stabilizes its economy, rushes through the Terran tech tree, and sends a Battlecruiser across the map before the opponent is ready for it.

This is not a machine-learning project.

There is no neural network trying to discover what a Supply Depot is.

UEDBot is a collection of explicit systems, heuristics, timings, threat calculations, and unit controllers designed to play one strategy extremely well.

The result was an undefeated first-place finish in a tournament of 11 bots:

```text
57 wins
3 draws
0 losses
```

## The strategy

The plan is straightforward:

1. Seal the main ramp.
2. Keep the economy running.
3. Build Barracks, Factory, Starport, and Fusion Core.
4. Produce Marines, Siege Tanks, and Battlecruisers.
5. Tactical Jump into the enemy base.
6. Keep applying pressure until there is nothing left to target.

Simple plan, significantly less simple implementation.

The bot still needs to understand where it can build, which workers are available, when it is under attack, what can threaten a Battlecruiser, when a unit should retreat, and how to find an opponent hiding their last structure in a corner of the map.

That is what the rest of the repository handles.

## Features

### Dynamic ramp wall

UEDBot analyzes the map and places Supply Depots and a production structure around the main ramp.

The wall is not just decorative. Supply Depots are actively raised and lowered to control movement while blocking early aggression.

If part of the wall is destroyed, the bot can attempt to rebuild it.

### Fast Battlecruiser timing

The build progresses through:

```text
Supply Depot
    ↓
Barracks
    ↓
Factory
    ↓
Starport
    ↓
Fusion Core
    ↓
Battlecruiser
```

The first attack can begin while the initial Battlecruiser is still being produced.

Once available, Tactical Jump sends healthy Battlecruisers directly toward the enemy starting location.

### Threat-aware Battlecruiser micro

Battlecruisers do not blindly attack the nearest object.

The controller:

* assigns threat values to dangerous enemy units,
* calculates nearby anti-air pressure,
* prioritizes targets,
* avoids overwhelming turret coverage,
* kites nearby threats,
* retreats damaged Battlecruisers,
* waits for them to recover before sending them back,
* avoids jumping away while the home base is under attack.

Target selection prioritizes meaningful threats before workers, static defenses, supply structures, and miscellaneous buildings.

### Marines and Siege Tanks

The ground army supports the Battlecruiser attack and protects the bot at home.

Marines respond to enemies near friendly bases.

Siege Tanks are split between attacking and defending groups, with part of the tank force remaining behind during a push.

Units rally near their production structures until the bot decides it has enough army to move out.

### Automated economy

The economy manager handles:

* continuous SCV production,
* mineral and gas worker assignment,
* idle worker recovery,
* refinery construction,
* Supply Depot construction,
* expansion timing,
* worker reassignment,
* MULE usage,
* scans against cloaked enemies,
* damaged builder detection,
* stalled construction detection.

SCV production scales with the ideal worker count across completed bases, plus additional workers for construction and emergencies.

### Expansion management

UEDBot calculates expansion locations using the SC2 API and creates additional Command Centers when its economy and game state allow it.

Each completed base becomes part of the bot's tracked base set and receives its own worker and defensive management.

### Reactive defense

Defense runs in two layers.

**Early defense** detects visible enemies near friendly bases and sends Marines toward the highest-priority local target.

**Late defense** uses Engineering Bays to place Missile Turrets near mineral lines and important base locations.

The bot can continue executing its offensive plan while maintaining a smaller defensive force at home.

### Scouting and cleanup

UEDBot tracks possible enemy starting locations and generates pathable scouting points across the map.

When the enemy's known base is destroyed, the bot:

1. searches for visible enemy units,
2. checks remembered snapshot locations,
3. switches into cleanup mode,
4. fans units across the map to locate hidden structures.

Because winning the main fight does not matter if there is still a Pylon hiding somewhere.

## Architecture

The bot is intentionally split by responsibility instead of putting the entire game inside one enormous `OnStep()` function.

```text
UEDBot
├── BasicSc2Bot.cpp
│   ├── lifecycle callbacks
│   ├── game-state initialization
│   ├── event handling
│   └── main update loop
├── Build_Order.cpp
│   ├── production structure progression
│   ├── add-on construction
│   └── building swaps
├── Build_Units.cpp
│   └── unit production
├── Economy.cpp
│   ├── workers
│   ├── resources
│   ├── supply
│   └── expansions
├── Defense.cpp
│   ├── base defense
│   └── Missile Turret placement
├── Offence.cpp
│   ├── attack timing
│   ├── army movement
│   └── cleanup behavior
├── ControlBattlecruisers.cpp
│   ├── Tactical Jump
│   ├── threat evaluation
│   ├── targeting
│   ├── kiting
│   └── retreat logic
├── ControlMarines.cpp
│   └── Marine micro
├── ControlSiegeTanks.cpp
│   └── Siege Tank micro
├── ControlSCVs.cpp
│   └── worker control and repair
├── MapInfo.cpp
│   ├── ramps
│   ├── placement grids
│   └── map geometry
├── Helper.cpp
│   └── shared utilities
└── main.cpp
    └── ladder and local-game entry point
```

The core update loop is effectively:

```cpp
ManageEconomy();
ExecuteBuildOrder();
ManageProduction();
ControlUnits();
Defense();
Offense();
```

Each system runs repeatedly and makes decisions from the latest SC2 observation state.

## Requirements

You will need:

* StarCraft II
* CMake 3.6 or newer
* A compiler with C++14 support
* StarCraft II map packs
* Git, with submodule support

The project uses [`cpp-sc2`](https://github.com/cpp-sc2/cpp-sc2) as a Git submodule.

The original testing setup uses maps from the `Ladder 2017 Season 1` map pack.

## Clone the repository

The `--recursive` flag matters.

Without it, the SC2 API submodule will not be downloaded.

```bash
git clone --recursive https://github.com/Team-UED/UEDBot.git
cd UEDBot
```

Already cloned without submodules?

```bash
git submodule update --init --recursive
```

## Build

Build artifacts are written to:

```text
build/bin
```

### Windows

Install Visual Studio 2022 with C++ development tools.

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
```

Open the generated solution:

```powershell
start UEDBot.sln
```

Build the `UEDBot` target in Visual Studio.

You can also build from the command line:

```powershell
cmake --build . --config Release
```

The executable should be available at:

```text
build/bin/UEDBot.exe
```

### macOS

UEDBot must be compiled with the Apple-provided version of Clang.

```bash
mkdir build
cd build

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

The executable should be available at:

```text
build/bin/UEDBot
```

If StarCraft II crashes before opening, try changing the computer name in macOS Sharing settings to a single-word name.

### Linux

The Linux StarCraft II client is headless, so games will run without a visible game window.

Place the Linux StarCraft II installation at:

```text
/home/<USER>/StarCraftII/
```

Rename the maps directory:

```bash
mv /home/<USER>/StarCraftII/Maps /home/<USER>/StarCraftII/maps
```

Create the compatibility directory expected by the SC2 API:

```bash
mkdir "/home/<USER>/StarCraft II"
```

Create `ExecuteInfo.txt` with the path to your SC2 executable:

```bash
echo "executable = /home/<USER>/StarCraftII/Versions/Base75689/SC2_x64" \
  > "/home/<USER>/StarCraft II/ExecuteInfo.txt"
```

Replace `<USER>` with your Linux username.

The `Base75689` directory may be different for your installation. Check:

```bash
ls /home/<USER>/StarCraftII/Versions/
```

Then build normally:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Play against the built-in AI

UEDBot uses the ladder interface included in the repository, which supports both local games and bot-versus-bot ladder games.

### Windows

```powershell
.\build\bin\UEDBot.exe -c -a zerg -d Hard -m CactusValleyLE.SC2Map
```

### macOS and Linux

```bash
./build/bin/UEDBot -c -a zerg -d Hard -m CactusValleyLE.SC2Map
```

This launches a game against the built-in Zerg AI on Hard difficulty using `CactusValleyLE`.

### Arguments

```text
-c              Run against the built-in computer
-a <race>       Opponent race
-d <difficulty> Opponent difficulty
-m <map>        Map filename
```

Example races:

```text
terran
zerg
protoss
random
```

Make sure the selected map exists inside your StarCraft II maps directory.

## Ladder play

The bot can also run through an SC2 ladder server.

`main.cpp` passes `BasicSc2Bot` into the repository's ladder interface:

```cpp
RunBot(argc, argv, new BasicSc2Bot(), sc2::Race::Terran);
```

The ladder server supplies the game connection, opponent, map, and match configuration.

For local bot-versus-bot testing, see the documentation for the SC2 Ladder Server:

https://github.com/Cryptyc/Sc2LadderServer

## Development

Most behavior begins in `BasicSc2Bot::OnStep()`.

```cpp
void BasicSc2Bot::OnStep() {
    ++step_counter;

    if (step_counter < 10) {
        return;
    }

    if (step_counter == 10) {
        on_start();
    }

    current_gameloop = Observation()->GetGameLoop();

    if (step_counter > 10) {
        depot_control();
        ManageEconomy();
        ExecuteBuildOrder();
        ManageProduction();
        ControlUnits();
        Defense();
        Offense();
    }
}
```

When changing the strategy, start with these files:

| Goal                             | File                        |
| -------------------------------- | --------------------------- |
| Change the opening               | `Build_Order.cpp`           |
| Change unit composition          | `Build_Units.cpp`           |
| Change worker or expansion logic | `Economy.cpp`               |
| Change attack timing             | `Offence.cpp`               |
| Change Battlecruiser targeting   | `ControlBattlecruisers.cpp` |
| Change defensive reactions       | `Defense.cpp`               |
| Change building placement        | `MapInfo.cpp`               |
| Add lifecycle behavior           | `BasicSc2Bot.cpp`           |

## Design philosophy

UEDBot does not attempt to solve all of StarCraft II.

It has a specific win condition and organizes the entire game around reaching it consistently.

That means:

* defensive construction buys time,
* the economy feeds the tech rush,
* production creates a mixed army,
* scouting identifies where to attack,
* micro keeps expensive units alive,
* cleanup logic closes games.

Every subsystem exists to support the same plan.

The strategy is predictable.

The execution is not.

## Known limitations

UEDBot is a tournament strategy bot, not a general-purpose StarCraft II agent.

Some assumptions are embedded directly in the implementation:

* it always plays Terran,
* parts of the attack timing are map-specific,
* the build is centered around Battlecruisers,
* several decisions use fixed resource thresholds,
* some placement logic assumes conventional ladder-map geometry,
* it is not designed to learn from previous games,
* unusual maps or custom game rules may require adjustments.

That is also what makes the repository useful.

The behavior is explicit, inspectable, and easy to experiment with.

## Tournament result

UEDBot finished first in a tournament featuring 11 StarCraft II bots.

```text
Wins:   57
Draws:   3
Losses:  0
```

Turns out teleporting a capital ship into someone's economy is a reasonably effective strategy.

## License

UEDBot is released under the MIT License.

See [`LICENSE`](./LICENSE) for details.
