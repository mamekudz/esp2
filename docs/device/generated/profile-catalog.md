# ESP][ device profile catalog

Generated from `config/device/profiles/`. **Path-only** - no disk images in Git.

Each catalog profile points at an [Apple ][js](https://www.scullinsteel.com/apple2) media URL (JSON disk). Download into gitignored `local/apple2/` with:

```bash
npx gulp setup:profiles
npx gulp setup:profiles --profile star-blazer --seed star-blazer
```

Commercial / entertainment titles remain **USER_SUPPLIED_ONLY / LOCAL_TEST_ONLY** - website availability is not redistribution permission.

Profiles: **251**. Regenerate: `gulp device:profiles:generate` or `node dev/tools/device-profile-catalog-doc.mjs`.

## Categories

| Category | Count |
| --- | ---: |
| 13 Sector | 12 |
| Demo | 9 |
| Demo (curated) | 3 |
| Diagnostics | 5 |
| Educational | 9 |
| Game | 134 |
| Graphics | 7 |
| Interactive Fiction | 37 |
| Music Video | 1 |
| Personal | 2 |
| Productivity | 7 |
| Programming | 10 |
| System | 5 |
| Utility | 10 |

## Curated demos

| Title | Profile id | Device path | Apple ][js media URL |
| --- | --- | --- | --- |
| Galaxian | `galaxian-demo` | `/esp2/disks/Galaxian.dsk` | https://www.scullinsteel.com/apple2/json/disks/galaxian.json |
| Little Brick Out | `little-brick-out` | `/esp2/disks/LittleBrickOut.dsk` | (user-supplied / not on website catalog) |
| Star Blazer | `star-blazer` | `/esp2/disks/StarBlazer.dsk` | https://www.scullinsteel.com/apple2/json/disks/starblazer.json |

## All profiles (by category)

### 13 Sector

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Boot 13 | `boot-13` | https://www.scullinsteel.com/apple2/json/disks/boot13.json |
| DOS 3.1 Master | `dos-31-master` | https://www.scullinsteel.com/apple2/json/disks/dos31master.json |
| DOS 3.2 Master | `dos-32-master` | https://www.scullinsteel.com/apple2/json/disks/dos32master.json |
| DOS 3.2 Master 2 | `dos-32-master-2` | https://www.scullinsteel.com/apple2/json/disks/dos32master2.json |
| SoftSide 1981-11 | `softside-1981-11` | https://www.scullinsteel.com/apple2/json/disks/ss_1981_11.json |
| SoftSide 1982-03 | `softside-1982-03` | https://www.scullinsteel.com/apple2/json/disks/ss_1982_03.json |
| SoftSide 1982-04 | `softside-1982-04` | https://www.scullinsteel.com/apple2/json/disks/ss_1982_04.json |
| SoftSide 1982-05 | `softside-1982-05` | https://www.scullinsteel.com/apple2/json/disks/ss_1982_05.json |
| SoftSide 30 | `softside-30` | https://www.scullinsteel.com/apple2/json/disks/ss_30.json |
| SoftSide 31 | `softside-31` | https://www.scullinsteel.com/apple2/json/disks/ss_31.json |
| SoftSide 32 | `softside-32` | https://www.scullinsteel.com/apple2/json/disks/ss_32.json |
| SoftSide 33 | `softside-33` | https://www.scullinsteel.com/apple2/json/disks/ss_33.json |

### Demo

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| 1984 Christmas Demo | `1984-christmas-demo` | https://www.scullinsteel.com/apple2/json/disks/xmas_demo.json |
| Apple Presents the //e (1985) | `apple-presents-the-e-1985` | https://www.scullinsteel.com/apple2/json/disks/apple2edemo2.json |
| Apple Presents the //e (1986) | `apple-presents-the-e-1986` | https://www.scullinsteel.com/apple2/json/disks/apple2edemo3.json |
| Apple Presents the Apple IIe | `apple-presents-the-apple-iie` | https://www.scullinsteel.com/apple2/json/disks/apple2edemo1.json |
| Extended 80 Column Demo | `extended-80-column-demo` | https://www.scullinsteel.com/apple2/json/disks/ext80demo2.json |
| Festro | `festro` | https://www.scullinsteel.com/apple2/json/disks/festro.json |
| RGB Color Card (DOS) | `rgb-color-card-dos` | https://www.scullinsteel.com/apple2/json/disks/rgb_dos.json |
| RGB Color Card (Prodos) | `rgb-color-card-prodos` | https://www.scullinsteel.com/apple2/json/disks/rgb_prodos.json |
| Video 7 | `video-7` | https://www.scullinsteel.com/apple2/json/disks/video7.json |

### Demo (curated)

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Galaxian | `galaxian-demo` | https://www.scullinsteel.com/apple2/json/disks/galaxian.json |
| Little Brick Out | `little-brick-out` | - |
| Star Blazer | `star-blazer` | https://www.scullinsteel.com/apple2/json/disks/starblazer.json |

### Diagnostics

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Apple ][+ Diagnostics | `apple-diagnostics` | https://www.scullinsteel.com/apple2/json/disks/diagnostics2plus.json |
| Apple //e Diagnostics 2.0 | `apple-e-diagnostics-20` | https://www.scullinsteel.com/apple2/json/disks/diagnostics2e2.json |
| Apple //e Diagnostics 2.1 | `apple-e-diagnostics-21` | https://www.scullinsteel.com/apple2/json/disks/diagnostics2e.json |
| Audit | `audit` | https://www.scullinsteel.com/apple2/json/disks/audit.json |
| MECC Computer Inspector | `mecc-computer-inspector` | https://www.scullinsteel.com/apple2/json/disks/mecc_a240.json |

### Educational

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Carmen Sandiego 1 | `carmen-sandiego-1` | https://www.scullinsteel.com/apple2/json/disks/carmen1.json |
| Carmen Sandiego 2 | `carmen-sandiego-2` | https://www.scullinsteel.com/apple2/json/disks/carmen2.json |
| Latin Skills I: Cursus Honorum | `latin-skills-i-cursus-honorum` | https://www.scullinsteel.com/apple2/json/disks/latin1.json |
| Latin Skills II: Verb Factory | `latin-skills-ii-verb-factory` | https://www.scullinsteel.com/apple2/json/disks/latin2.json |
| Latin Skills III: Mare Nostrum | `latin-skills-iii-mare-nostrum` | https://www.scullinsteel.com/apple2/json/disks/latin3.json |
| Latin Skills IV: Translat | `latin-skills-iv-translat` | https://www.scullinsteel.com/apple2/json/disks/latin4.json |
| Latin Skills V: Artifex Verborum | `latin-skills-v-artifex-verborum` | https://www.scullinsteel.com/apple2/json/disks/latin5.json |
| Number Squares | `number-squares` | https://www.scullinsteel.com/apple2/json/disks/numbersquares.json |
| Odell Lake | `odell-lake` | https://www.scullinsteel.com/apple2/json/disks/odell_lake.json |

### Game

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Adventure | `adventure` | https://www.scullinsteel.com/apple2/json/disks/adventure.json |
| Airheart | `airheart` | https://www.scullinsteel.com/apple2/json/disks/airheart.json |
| Akalabeth | `akalabeth` | https://www.scullinsteel.com/apple2/json/disks/akalabeth.json |
| Ancient Legends | `ancient-legends` | https://www.scullinsteel.com/apple2/json/disks/ancient_legends_2.json |
| Apple Invaders | `apple-invaders` | https://www.scullinsteel.com/apple2/json/disks/appleinvaders.json |
| Apple Panic | `apple-panic` | https://www.scullinsteel.com/apple2/json/disks/apple_panic.json |
| Apple Trek | `apple-trek` | https://www.scullinsteel.com/apple2/json/disks/appletrek.json |
| Applesoft Games | `applesoft-games` | https://www.scullinsteel.com/apple2/json/disks/applesoft.json |
| Apventure to Atlantis | `apventure-to-atlantis` | https://www.scullinsteel.com/apple2/json/disks/atlantis.json |
| Archon | `archon` | https://www.scullinsteel.com/apple2/json/disks/archon.json |
| Archon II | `archon-ii` | https://www.scullinsteel.com/apple2/json/disks/archon2.json |
| Arkanoid | `arkanoid` | https://www.scullinsteel.com/apple2/json/disks/arkanoid.json |
| Artic Fox | `artic-fox` | https://www.scullinsteel.com/apple2/json/disks/artic_fox.json |
| Asteroid Field | `asteroid-field` | https://www.scullinsteel.com/apple2/json/disks/asteroid.json |
| Autobahn | `autobahn` | https://www.scullinsteel.com/apple2/json/disks/autobahn.json |
| Aztec | `aztec` | https://www.scullinsteel.com/apple2/json/disks/aztec.json |
| Bards Tale Boot | `bards-tale-boot` | https://www.scullinsteel.com/apple2/json/disks/bards_tale_boot.json |
| Bards Tale Character | `bards-tale-character` | https://www.scullinsteel.com/apple2/json/disks/bards_tale_character.json |
| Bards Tale Dungeon | `bards-tale-dungeon` | https://www.scullinsteel.com/apple2/json/disks/bards_tale_dungeon.json |
| Basic Games 1 | `basic-games-1` | https://www.scullinsteel.com/apple2/json/disks/basic_games_1.json |
| Basic Games 2 | `basic-games-2` | https://www.scullinsteel.com/apple2/json/disks/basic_games_2.json |
| Batman | `batman` | https://www.scullinsteel.com/apple2/json/disks/batman2.json |
| Battlezone | `battlezone` | https://www.scullinsteel.com/apple2/json/disks/battlezone.json |
| Bezare | `bezare` | https://www.scullinsteel.com/apple2/json/disks/bezare.json |
| Bolo | `bolo` | https://www.scullinsteel.com/apple2/json/disks/bolo.json |
| Bongo Bash | `bongo-bash` | https://www.scullinsteel.com/apple2/json/disks/bongos_bash.json |
| Boulder Dash | `boulder-dash` | https://www.scullinsteel.com/apple2/json/disks/boulder_dash.json |
| Bubble Bobble 1 | `bubble-bobble-1` | https://www.scullinsteel.com/apple2/json/disks/bubble_bobble1.json |
| Bubble Bobble 2 | `bubble-bobble-2` | https://www.scullinsteel.com/apple2/json/disks/bubble_bobble2.json |
| Cannonball Blitz | `cannonball-blitz` | https://www.scullinsteel.com/apple2/json/disks/cannonball.json |
| Castle Wolfenstein | `castle-wolfenstein` | https://www.scullinsteel.com/apple2/json/disks/wolfenstein.json |
| Cavern Creatures | `cavern-creatures` | https://www.scullinsteel.com/apple2/json/disks/caverncreatures.json |
| Chivalry | `chivalry` | https://www.scullinsteel.com/apple2/json/disks/chivalry.json |
| Choplifter | `choplifter` | https://www.scullinsteel.com/apple2/json/disks/choplifter.json |
| Chuck Yeager 1 | `chuck-yeager-1` | https://www.scullinsteel.com/apple2/json/disks/yeager1.json |
| Chuck Yeager 2 | `chuck-yeager-2` | https://www.scullinsteel.com/apple2/json/disks/yeager2.json |
| Crossfire | `crossfire` | https://www.scullinsteel.com/apple2/json/disks/crossfire.json |
| Dangerous Dave | `dangerous-dave` | https://www.scullinsteel.com/apple2/json/disks/dangerous_dave.json |
| Dino Eggs | `dino-eggs` | https://www.scullinsteel.com/apple2/json/disks/dino_eggs.json |
| Donkey Kong | `donkey-kong` | https://www.scullinsteel.com/apple2/json/disks/donkey_kong.json |
| Dungeon Adventure | `dungeon-adventure` | https://www.scullinsteel.com/apple2/json/disks/dungeon.json |
| Elite | `elite` | https://www.scullinsteel.com/apple2/json/disks/elite.json |
| Falcons | `falcons` | https://www.scullinsteel.com/apple2/json/disks/falcons.json |
| Flapple Bird | `flapple-bird` | https://www.scullinsteel.com/apple2/json/disks/flapple.json |
| Flight Simulator | `flight-simulator` | https://www.scullinsteel.com/apple2/json/disks/flightsimulator.json |
| Galaxian | `galaxian` | https://www.scullinsteel.com/apple2/json/disks/galaxian.json |
| Galaxy | `galaxy` | https://www.scullinsteel.com/apple2/json/disks/galaxy.json |
| Gorgons | `gorgons` | https://www.scullinsteel.com/apple2/json/disks/gorgon.json |
| Hard Hat Mac | `hard-hat-mac` | https://www.scullinsteel.com/apple2/json/disks/burgertime.json |
| Horizon V | `horizon-v` | https://www.scullinsteel.com/apple2/json/disks/horizonv.json |
| Interlude | `interlude` | https://www.scullinsteel.com/apple2/json/disks/interlude.json |
| Jungle Hunt | `jungle-hunt` | https://www.scullinsteel.com/apple2/json/disks/jungle_hunt.json |
| Karateka | `karateka` | https://www.scullinsteel.com/apple2/json/disks/karateka.json |
| Kings Bounty | `kings-bounty` | https://www.scullinsteel.com/apple2/json/disks/kings_bounty2.json |
| Lambchops | `lambchops` | https://www.scullinsteel.com/apple2/json/disks/lambchops.json |
| Last Ninja (Disk 1) | `last-ninja-disk-1` | https://www.scullinsteel.com/apple2/json/disks/lastninja1.json |
| Last Ninja (Disk 2) | `last-ninja-disk-2` | https://www.scullinsteel.com/apple2/json/disks/lastninja2.json |
| Last Ninja (Disk 3) | `last-ninja-disk-3` | https://www.scullinsteel.com/apple2/json/disks/lastninja3.json |
| Lode Runner | `lode-runner` | https://www.scullinsteel.com/apple2/json/disks/lode_runner.json |
| Marble Madness 1 | `marble-madness-1` | https://www.scullinsteel.com/apple2/json/disks/marble_madness1.json |
| Marble Madness 2 | `marble-madness-2` | https://www.scullinsteel.com/apple2/json/disks/marble_madness2.json |
| Mario Bros | `mario-bros` | https://www.scullinsteel.com/apple2/json/disks/mariobros.json |
| Midnight Madness | `midnight-madness` | https://www.scullinsteel.com/apple2/json/disks/midnightmadness.json |
| Midnight Magic | `midnight-magic` | https://www.scullinsteel.com/apple2/json/disks/midnightmagic.json |
| Might and Magic A | `might-and-magic-a` | https://www.scullinsteel.com/apple2/json/disks/mightmagic1.json |
| Might and Magic B | `might-and-magic-b` | https://www.scullinsteel.com/apple2/json/disks/mightmagic2.json |
| Might and Magic C | `might-and-magic-c` | https://www.scullinsteel.com/apple2/json/disks/mightmagic3.json |
| Might and Magic D | `might-and-magic-d` | https://www.scullinsteel.com/apple2/json/disks/mightmagic4.json |
| Might and Magic II Disk A | `might-and-magic-ii-disk-a` | https://www.scullinsteel.com/apple2/json/disks/mightandmagicii-a.json |
| Might and Magic II Disk B | `might-and-magic-ii-disk-b` | https://www.scullinsteel.com/apple2/json/disks/mightandmagicii-b.json |
| Might and Magic II Disk C | `might-and-magic-ii-disk-c` | https://www.scullinsteel.com/apple2/json/disks/mightandmagicii-c.json |
| Might and Magic II Disk D | `might-and-magic-ii-disk-d` | https://www.scullinsteel.com/apple2/json/disks/mightandmagicii-d.json |
| Might and Magic II Disk E | `might-and-magic-ii-disk-e` | https://www.scullinsteel.com/apple2/json/disks/mightandmagicii-e.json |
| Might and Magic II Disk F | `might-and-magic-ii-disk-f` | https://www.scullinsteel.com/apple2/json/disks/mightandmagicii-f.json |
| Millenium Leaper | `millenium-leaper` | https://www.scullinsteel.com/apple2/json/disks/millenium.json |
| Miner 2049er | `miner-2049er` | https://www.scullinsteel.com/apple2/json/disks/miner_2049er.json |
| Mini Dung Beetles | `mini-dung-beetles` | https://www.scullinsteel.com/apple2/json/disks/dung.json |
| Montezuma's Revenge | `montezumas-revenge` | https://www.scullinsteel.com/apple2/json/disks/montezuma.json |
| Moon Patrol | `moon-patrol` | https://www.scullinsteel.com/apple2/json/disks/moonpatrol.json |
| Ms Pacman | `ms-pacman` | https://www.scullinsteel.com/apple2/json/disks/mspacman.json |
| Neuromancer (Disk 1) | `neuromancer-disk-1` | https://www.scullinsteel.com/apple2/json/disks/neuromancer1.json |
| Neuromancer (Disk 2) | `neuromancer-disk-2` | https://www.scullinsteel.com/apple2/json/disks/neuromancer2.json |
| Neuromancer (Disk 3) | `neuromancer-disk-3` | https://www.scullinsteel.com/apple2/json/disks/neuromancer3.json |
| Neuromancer (Disk 4) | `neuromancer-disk-4` | https://www.scullinsteel.com/apple2/json/disks/neuromancer4.json |
| Night Flight | `night-flight` | https://www.scullinsteel.com/apple2/json/disks/nightflight.json |
| Night Mission | `night-mission` | https://www.scullinsteel.com/apple2/json/disks/nightmission.json |
| Nightmare Gallery | `nightmare-gallery` | https://www.scullinsteel.com/apple2/json/disks/nightmaregallery.json |
| North Atlantic 86 | `north-atlantic-86` | https://www.scullinsteel.com/apple2/json/disks/atlantic86.json |
| Odyssey: The Complete Apventure | `odyssey-the-complete-apventure` | https://www.scullinsteel.com/apple2/json/disks/odyssey.json |
| Operation Sabotage | `operation-sabotage` | https://www.scullinsteel.com/apple2/json/disks/operation_sabotage.json |
| Oregon Trail | `oregon-trail` | https://www.scullinsteel.com/apple2/json/disks/oregon_trail.json |
| Pacman | `pacman` | https://www.scullinsteel.com/apple2/json/disks/pacman.json |
| Penny Arcade | `penny-arcade` | https://www.scullinsteel.com/apple2/json/disks/pennyarcade.json |
| Phantom 5 | `phantom-5` | https://www.scullinsteel.com/apple2/json/disks/phantom5.json |
| Pinball Construction Set | `pinball-construction-set` | https://www.scullinsteel.com/apple2/json/disks/pcs.json |
| Pitfall II | `pitfall-ii` | https://www.scullinsteel.com/apple2/json/disks/pitfallii.json |
| Prince of Persia Boot | `prince-of-persia-boot` | https://www.scullinsteel.com/apple2/json/disks/prince_of_persia_boot.json |
| Prince of Persia Disk A | `prince-of-persia-disk-a` | https://www.scullinsteel.com/apple2/json/disks/prince_of_persia_a.json |
| Prince of Persia Disk B | `prince-of-persia-disk-b` | https://www.scullinsteel.com/apple2/json/disks/prince_of_persia_b.json |
| Q-Bert | `q-bert` | https://www.scullinsteel.com/apple2/json/disks/qbert.json |
| qix | `qix` | https://www.scullinsteel.com/apple2/json/disks/qix.json |
| Repton | `repton` | https://www.scullinsteel.com/apple2/json/disks/repton.json |
| Rescue on Fractalus | `rescue-on-fractalus` | https://www.scullinsteel.com/apple2/json/disks/fractalus.json |
| Rescue Raiders | `rescue-raiders` | https://www.scullinsteel.com/apple2/json/disks/rescue.json |
| Return of Heracles | `return-of-heracles` | https://www.scullinsteel.com/apple2/json/disks/returnofhercules.json |
| Robotron | `robotron` | https://www.scullinsteel.com/apple2/json/disks/robotron.json |
| Rogue | `rogue` | https://www.scullinsteel.com/apple2/json/disks/rogue.json |
| Sabotage | `sabotage` | https://www.scullinsteel.com/apple2/json/disks/sabotage.json |
| Sea Dragon | `sea-dragon` | https://www.scullinsteel.com/apple2/json/disks/seadragon.json |
| Skyfox | `skyfox` | https://www.scullinsteel.com/apple2/json/disks/skyfox.json |
| Sneakers | `sneakers` | https://www.scullinsteel.com/apple2/json/disks/sneakers.json |
| SoftSide 34 | `softside-34` | https://www.scullinsteel.com/apple2/json/disks/softside34.json |
| Spy's Demise | `spys-demise` | https://www.scullinsteel.com/apple2/json/disks/spysdemise.json |
| Star Trek | `star-trek` | https://www.scullinsteel.com/apple2/json/disks/startrek.json |
| Star Wars | `star-wars` | https://www.scullinsteel.com/apple2/json/disks/starwars.json |
| Starmaze | `starmaze` | https://www.scullinsteel.com/apple2/json/disks/starmaze.json |
| Stellar 7 | `stellar-7` | https://www.scullinsteel.com/apple2/json/disks/stellar7.json |
| Super Quest | `super-quest` | https://www.scullinsteel.com/apple2/json/disks/superquest.json |
| Super Zaxxon | `super-zaxxon` | https://www.scullinsteel.com/apple2/json/disks/super_zaxxon.json |
| Swashbuckler | `swashbuckler` | https://www.scullinsteel.com/apple2/json/disks/swashbuckler.json |
| Taipan | `taipan` | https://www.scullinsteel.com/apple2/json/disks/taipan.json |
| Tennis Championship | `tennis-championship` | https://www.scullinsteel.com/apple2/json/disks/tennischampionship.json |
| Test Drive - Disk 1 | `test-drive-disk-1` | https://www.scullinsteel.com/apple2/json/disks/testdrive1.json |
| Test Drive - Disk 2 | `test-drive-disk-2` | https://www.scullinsteel.com/apple2/json/disks/testdrive2.json |
| Tetris | `tetris` | https://www.scullinsteel.com/apple2/json/disks/tetris.json |
| Thexder | `thexder` | https://www.scullinsteel.com/apple2/json/disks/thexder2.json |
| Threshold | `threshold` | https://www.scullinsteel.com/apple2/json/disks/threshold.json |
| Tron | `tron` | https://www.scullinsteel.com/apple2/json/disks/tron.json |
| TSR Dungeon | `tsr-dungeon` | https://www.scullinsteel.com/apple2/json/disks/tsr_dungeon.json |
| Twerps | `twerps` | https://www.scullinsteel.com/apple2/json/disks/spaceeggs.json |
| Warriors of Ra 1: Dunzhin | `warriors-of-ra-1-dunzhin` | https://www.scullinsteel.com/apple2/json/disks/warriorofras1-dunzhin.json |
| Wings of Fury | `wings-of-fury` | https://www.scullinsteel.com/apple2/json/disks/wingsoffury2.json |
| Winter Games | `winter-games` | https://www.scullinsteel.com/apple2/json/disks/winter2.json |
| Zaxxon | `zaxxon` | https://www.scullinsteel.com/apple2/json/disks/zaxxon.json |

### Graphics

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Beagle Graphics | `beagle-graphics` | https://www.scullinsteel.com/apple2/json/disks/beagle_graphics.json |
| Complete Graphics System | `complete-graphics-system` | https://www.scullinsteel.com/apple2/json/disks/cgsii.json |
| Dazzle Draw | `dazzle-draw` | https://www.scullinsteel.com/apple2/json/disks/dazzledraw.json |
| FantaVision 1 | `fantavision-1` | https://www.scullinsteel.com/apple2/json/disks/fantavision1.json |
| FantaVision 2 | `fantavision-2` | https://www.scullinsteel.com/apple2/json/disks/fantavision2.json |
| Koala Illustrator | `koala-illustrator` | https://www.scullinsteel.com/apple2/json/disks/koala.json |
| The Graphics Magician | `the-graphics-magician` | https://www.scullinsteel.com/apple2/json/disks/graphics_magician.json |

### Interactive Fiction

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Adam's Adventures 1 | `adams-adventures-1` | https://www.scullinsteel.com/apple2/json/disks/adams1.json |
| Adam's Adventures 2 | `adams-adventures-2` | https://www.scullinsteel.com/apple2/json/disks/adams2.json |
| Coveted Mirror 1 | `coveted-mirror-1` | https://www.scullinsteel.com/apple2/json/disks/coveted1.json |
| Coveted Mirror 2 | `coveted-mirror-2` | https://www.scullinsteel.com/apple2/json/disks/coveted2.json |
| Crimson Crown 1 | `crimson-crown-1` | https://www.scullinsteel.com/apple2/json/disks/crimson1.json |
| Crimson Crown 2 | `crimson-crown-2` | https://www.scullinsteel.com/apple2/json/disks/crimson2.json |
| Deadline | `deadline` | https://www.scullinsteel.com/apple2/json/disks/deadline.json |
| HGTTG | `hgttg` | https://www.scullinsteel.com/apple2/json/disks/hhgttg.json |
| Incredible Hulk 1 | `incredible-hulk-1` | https://www.scullinsteel.com/apple2/json/disks/hulk1.json |
| Incredible Hulk 2 | `incredible-hulk-2` | https://www.scullinsteel.com/apple2/json/disks/hulk2.json |
| Lurking Horror | `lurking-horror` | https://www.scullinsteel.com/apple2/json/disks/lurking.json |
| Maniac Mansion - Disk 1 | `maniac-mansion-disk-1` | https://www.scullinsteel.com/apple2/json/disks/maniacmansion1.json |
| Maniac Mansion - Disk 2 | `maniac-mansion-disk-2` | https://www.scullinsteel.com/apple2/json/disks/maniacmansion2.json |
| Mission Asteroid | `mission-asteroid` | https://www.scullinsteel.com/apple2/json/disks/mission_asteroid.json |
| Mystery House | `mystery-house` | https://www.scullinsteel.com/apple2/json/disks/mystery_house.json |
| Oo-topos 1 | `oo-topos-1` | https://www.scullinsteel.com/apple2/json/disks/oo-topos1.json |
| Oo-topos 2 | `oo-topos-2` | https://www.scullinsteel.com/apple2/json/disks/oo-topos2.json |
| Planetfall | `planetfall` | https://www.scullinsteel.com/apple2/json/disks/planetfall.json |
| Space Quest 1 - Disk 1 - Side 1 | `space-quest-1-disk-1-side-1` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-1-1.json |
| Space Quest 1 - Disk 1 - Side 2 | `space-quest-1-disk-1-side-2` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-1-2.json |
| Space Quest 1 - Disk 2 - Side 1 | `space-quest-1-disk-2-side-1` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-2-1.json |
| Space Quest 1 - Disk 2 - Side 2 | `space-quest-1-disk-2-side-2` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-2-2.json |
| Space Quest 1 - Disk 3 - Side 1 | `space-quest-1-disk-3-side-1` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-3-1.json |
| Space Quest 1 - Disk 3 - Side 2 | `space-quest-1-disk-3-side-2` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-3-2.json |
| Space Quest 1 - Disk 4 - Side 1 | `space-quest-1-disk-4-side-1` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-4-1.json |
| Space Quest 1 - Disk 4 - Side 2 | `space-quest-1-disk-4-side-2` | https://www.scullinsteel.com/apple2/json/disks/spacequest1-4-2.json |
| Starcross | `starcross` | https://www.scullinsteel.com/apple2/json/disks/starcross.json |
| Suspended | `suspended` | https://www.scullinsteel.com/apple2/json/disks/suspended.json |
| Tass Times in Tonetown | `tass-times-in-tonetown` | https://www.scullinsteel.com/apple2/json/disks/tasstimes2.json |
| Transylvania | `transylvania` | https://www.scullinsteel.com/apple2/json/disks/transylvania.json |
| Zork I | `zork-i` | https://www.scullinsteel.com/apple2/json/disks/zork_1.json |
| Zork II | `zork-ii` | https://www.scullinsteel.com/apple2/json/disks/zork_2.json |
| Zork III | `zork-iii` | https://www.scullinsteel.com/apple2/json/disks/zork_3.json |
| Zork Zero - Disk 1 | `zork-zero-disk-1` | https://www.scullinsteel.com/apple2/json/disks/zorkzero1.json |
| Zork Zero - Disk 2 | `zork-zero-disk-2` | https://www.scullinsteel.com/apple2/json/disks/zorkzero2.json |
| Zork Zero - Disk 3 | `zork-zero-disk-3` | https://www.scullinsteel.com/apple2/json/disks/zorkzero3.json |
| Zork Zero - Disk 4 | `zork-zero-disk-4` | https://www.scullinsteel.com/apple2/json/disks/zorkzero4.json |

### Music Video

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Jed | `jed` | https://www.scullinsteel.com/apple2/json/disks/jed.json |

### Personal

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| X-Mas Card | `x-mas-card` | https://www.scullinsteel.com/apple2/json/disks/xmas-card.json |
| X-Mas Card 2 | `x-mas-card-2` | https://www.scullinsteel.com/apple2/json/disks/xmas-card2.json |

### Productivity

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Apple Pie | `apple-pie` | https://www.scullinsteel.com/apple2/json/disks/applepie.json |
| Appleworks 3.0 - Disk 1 | `appleworks-30-disk-1` | https://www.scullinsteel.com/apple2/json/disks/appleworks3-1.json |
| Appleworks 3.0 - Disk 2 | `appleworks-30-disk-2` | https://www.scullinsteel.com/apple2/json/disks/appleworks3-2.json |
| Appleworks Disk 1 | `appleworks-disk-1` | https://www.scullinsteel.com/apple2/json/disks/appleworks1.json |
| Appleworks Disk 2 | `appleworks-disk-2` | https://www.scullinsteel.com/apple2/json/disks/appleworks2.json |
| AppleWriter | `applewriter` | https://www.scullinsteel.com/apple2/json/disks/applewriter.json |
| Easywriter | `easywriter` | https://www.scullinsteel.com/apple2/json/disks/easywriter.json |

### Programming

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Apple LOGO | `apple-logo` | https://www.scullinsteel.com/apple2/json/disks/logo.json |
| Apple LOGO Sample Programs | `apple-logo-sample-programs` | https://www.scullinsteel.com/apple2/json/disks/logo_toolkit_samples.json |
| Apple LOGO Tool Kit | `apple-logo-tool-kit` | https://www.scullinsteel.com/apple2/json/disks/logo_toolkit.json |
| Apple LOGO Tool Kit Documentation | `apple-logo-tool-kit-documentation` | https://www.scullinsteel.com/apple2/json/disks/logo_toolkit_docs.json |
| Apple Pascal - Disk 0 | `apple-pascal-disk-0` | https://www.scullinsteel.com/apple2/json/disks/pascal0.json |
| Apple Pascal - Disk 1 | `apple-pascal-disk-1` | https://www.scullinsteel.com/apple2/json/disks/pascal1.json |
| Apple Pascal - Disk 2 | `apple-pascal-disk-2` | https://www.scullinsteel.com/apple2/json/disks/pascal2.json |
| Apple Pascal - Disk 3 | `apple-pascal-disk-3` | https://www.scullinsteel.com/apple2/json/disks/pascal3.json |
| Graforth | `graforth` | https://www.scullinsteel.com/apple2/json/disks/graforth.json |
| Turtles | `turtles` | https://www.scullinsteel.com/apple2/json/disks/turtles.json |

### System

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| DOS 3.3 Master | `dos-33-master` | https://www.scullinsteel.com/apple2/json/disks/dos33master.json |
| DOS 3.3 Master 2 | `dos-33-master-2` | https://www.scullinsteel.com/apple2/json/disks/dos33master2.json |
| DOS 3.3 Sample | `dos-33-sample` | https://www.scullinsteel.com/apple2/json/disks/dos33sample.json |
| ProDOS | `prodos` | https://www.scullinsteel.com/apple2/json/disks/prodos.json |
| ProDOS 8 | `prodos-8` | https://www.scullinsteel.com/apple2/json/disks/prodos8.json |

### Utility

| Title | Profile id | Apple ][js media URL |
| --- | --- | --- |
| Alpha Plot | `alpha-plot` | https://www.scullinsteel.com/apple2/json/disks/alpha_plot.json |
| Beagle Basic | `beagle-basic` | https://www.scullinsteel.com/apple2/json/disks/beagle_basic.json |
| Copy ][+ | `copy` | https://www.scullinsteel.com/apple2/json/disks/c2plus.json |
| Davex | `davex` | https://www.scullinsteel.com/apple2/json/disks/davex.json |
| DOS Boss | `dos-boss` | https://www.scullinsteel.com/apple2/json/disks/dos_boss.json |
| FreeWriter | `freewriter` | https://www.scullinsteel.com/apple2/json/disks/freewriter.json |
| Locksmith | `locksmith` | https://www.scullinsteel.com/apple2/json/disks/locksmith.json |
| Thunderclock 1 | `thunderclock-1` | https://www.scullinsteel.com/apple2/json/disks/thunderclock1.json |
| Utility City | `utility-city` | https://www.scullinsteel.com/apple2/json/disks/utility_city.json |
| VideoTerm Utilities | `videoterm-utilities` | https://www.scullinsteel.com/apple2/json/disks/videoterm.json |

