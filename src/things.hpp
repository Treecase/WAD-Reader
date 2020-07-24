/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _THINGS_H
#define _THINGS_H


struct ThingData
{
    std::string sprite;
    int frames;
    bool cleanloop;
    std::string comment;
};

static std::unordered_map<uint16_t, ThingData> thingdata =
{
    {0xffff, (ThingData){ "----", -1, 0, "(nothing)" }},
    {0x0000, (ThingData){ "----", -1, 0, "(nothing)" }},
    {0x0001, (ThingData){ "PLAY",  0, 0, "Player 1 start (Player 1 start needed on ALL levels)" }},
    {0x0002, (ThingData){ "PLAY",  0, 0, "Player 2 start (Player starts 2-4 are needed in)" }},
    {0x0003, (ThingData){ "PLAY",  0, 0, "Player 3 start (cooperative mode multiplayer games)" }},
    {0x0004, (ThingData){ "PLAY",  0, 0, "Player 4 start" }},
    {0x000b, (ThingData){ "----", -1, 0, "Deathmatch start positions. Should have >= 4/level" }},
    {0x000e, (ThingData){ "----", -1, 0, "Teleport landing. Where players/monsters land when they teleport to the SECT0R containing this thing" }},

    {0x0bbc, (ThingData){ "POSS",  0, 0, "FORMER HUMAN: regular pistol-shooting zombieman" }},
    {0x0054, (ThingData){ "SSWV",  0, 0, "WOLFENSTEIN SS: guest appearance by Wolf3D blue guy" }},
    {0x0009, (ThingData){ "SPOS",  0, 0, "FORMER HUMAN SERGEANT: black armor, shotgunners" }},
    {0x0041, (ThingData){ "CPOS",  0, 0, "HEAVY WEAPON DUDE: red armor, chaingunners" }},
    {0x0bb9, (ThingData){ "TROO",  0, 0, "IMP: brown, hurl fireballs" }},
    {0x0bba, (ThingData){ "SARG",  0, 0, "DEMON: pink, muscular bull-like chewers" }},
    {0x003a, (ThingData){ "SARG",  0, 0, "SPECTRE: invisible version of the DEMON" }},
    {0x0bbe, (ThingData){ "SKUL",  0, 0, "LOST SOUL: flying flaming skulls, they really bite" }},
    {0x0bbd, (ThingData){ "HEAD",  0, 0, "CACODEMON: red one-eyed floating heads. Behold..." }},
    {0x0045, (ThingData){ "BOS2",  0, 0, "HELL KNIGHT: grey-not-pink BARON, weaker" }},
    {0x0bbb, (ThingData){ "BOSS",  0, 0, "BARON OF HELL: cloven hooved minotaur boss" }},
    {0x0044, (ThingData){ "BSPI",  0, 0, "ARACHNOTRON: baby SPIDER, shoots green plasma" }},
    {0x0047, (ThingData){ "PAIN",  0, 0, "PAIN ELEMENTAL: shoots LOST SOULS, deserves its name" }},
    {0x0042, (ThingData){ "SKEL",  0, 0, "REVENANT: Fast skeletal dude shoots homing missles" }},
    {0x0043, (ThingData){ "FATT",  0, 0, "MANCUBUS: Big, slow brown guy shoots barrage of fire" }},
    {0x0040, (ThingData){ "VILE",  0, 0, "ARCH-VILE: Super-fire attack, ressurects the dead!" }},
    {0x0007, (ThingData){ "SPID",  0, 0, "SPIDER MASTERMIND: giant walking brain boss" }},
    {0x0010, (ThingData){ "CYBR",  0, 0, "CYBER-DEMON: robo-boss, rocket launcher" }},

    {0x0058, (ThingData){ "BBRN",  0, 0, "BOSS BRAIN: Horrifying visage of the ultimate demon" }},
    {0x0059, (ThingData){ "-   ", -1, 0, "Boss Shooter: Shoots spinning skull-blocks" }},
    {0x0057, (ThingData){ "-   ", -1, 0, "Spawn Spot: Where Todd McFarlane's guys appear" }},

    {0x07d5, (ThingData){ "CSAW",  1, 0, "Chainsaw" }},
    {0x07d1, (ThingData){ "SHOT",  1, 0, "Shotgun" }},
    {0x0052, (ThingData){ "SGN2",  1, 0, "Double-barreled shotgun" }},
    {0x07d2, (ThingData){ "MGUN",  1, 0, "Chaingun, gatling gun, mini-gun, whatever" }},
    {0x07d3, (ThingData){ "LAUN",  1, 0, "Rocket launcher" }},
    {0x07d4, (ThingData){ "PLAS",  1, 0, "Plasma gun" }},
    {0x07d6, (ThingData){ "BFUG",  1, 0, "Bfg9000" }},
    {0x07d7, (ThingData){ "CLIP",  1, 0, "Ammo clip" }},
    {0x07d8, (ThingData){ "SHEL",  1, 0, "Shotgun shells" }},
    {0x07da, (ThingData){ "ROCK",  1, 0, "A rocket" }},
    {0x07ff, (ThingData){ "CELL",  1, 0, "Cell charge" }},
    {0x0800, (ThingData){ "AMMO",  1, 0, "Box of Ammo" }},
    {0x0801, (ThingData){ "SBOX",  1, 0, "Box of Shells" }},
    {0x07fe, (ThingData){ "BROK",  1, 0, "Box of Rockets" }},
    {0x0011, (ThingData){ "CELP",  1, 0, "Cell charge pack" }},
    {0x0008, (ThingData){ "BPAK",  1, 0, "Backpack: doubles maximum ammo capacities" }},

    {0x07db, (ThingData){ "STIM",  1, 0, "Stimpak" }},
    {0x07dc, (ThingData){ "MEDI",  1, 0, "Medikit" }},
    {0x07de, (ThingData){ "BON1",  4, 1, "Health Potion +1% health" }},
    {0x07df, (ThingData){ "BON2",  4, 1, "Spirit Armor +1% armor" }},
    {0x07e2, (ThingData){ "ARM1",  2, 0, "Green armor 100%" }},
    {0x07e3, (ThingData){ "ARM2",  2, 0, "Blue armor 200%" }},
    {0x0053, (ThingData){ "MEGA",  4, 0, "Megasphere: 200% health, 200% armor" }},
    {0x07dd, (ThingData){ "SOUL",  4, 1, "Soulsphere, Supercharge, +100% health" }},
    {0x07e6, (ThingData){ "PINV",  4, 0, "Invulnerability" }},
    {0x07e7, (ThingData){ "PSTR",  1, 0, "Berserk Strength and 100% health" }},
    {0x07e8, (ThingData){ "PINS",  4, 0, "Invisibility" }},
    {0x07e9, (ThingData){ "SUIT",  1, 0, "Radiation suit - see notes on ! above" }},
    {0x07ea, (ThingData){ "PMAP",  4, 1, "Computer map" }},
    {0x07fd, (ThingData){ "PVIS",  2, 0, "Lite Amplification goggles" }},

    {0x0005, (ThingData){ "BKEY",  2, 0, "Blue keycard" }},
    {0x0028, (ThingData){ "BSKU",  2, 0, "Blue skullkey" }},
    {0x000d, (ThingData){ "RKEY",  2, 0, "Red keycard" }},
    {0x0026, (ThingData){ "RSKU",  2, 0, "Red skullkey" }},
    {0x0006, (ThingData){ "YKEY",  2, 0, "Yellow keycard" }},
    {0x0027, (ThingData){ "YSKU",  2, 0, "Yellow skullkey" }},

    {0x07f3, (ThingData){ "BAR1", -1, 0, "Barrel; not an obstacle after blown up (BEXP sprite)" }},
    {0x0048, (ThingData){ "KEEN", -1, 0, "A guest appearance by Billy" }},

    {0x0030, (ThingData){ "ELEC",  1, 0, "Tall, techno pillar" }},
    {0x001e, (ThingData){ "COL1",  1, 0, "Tall green pillar" }},
    {0x0020, (ThingData){ "COL3",  1, 0, "Tall red pillar" }},
    {0x001f, (ThingData){ "COL2",  1, 0, "Short green pillar" }},
    {0x0024, (ThingData){ "COL5",  2, 0, "Short green pillar with beating heart" }},
    {0x0021, (ThingData){ "COL4",  1, 0, "Short red pillar" }},
    {0x0025, (ThingData){ "COL6",  1, 0, "Short red pillar with skull" }},
    {0x002f, (ThingData){ "SMIT",  1, 0, "Stalagmite: small brown pointy stump" }},
    {0x002b, (ThingData){ "TRE1",  1, 0, "Burnt tree: gray tree" }},
    {0x0036, (ThingData){ "TRE2",  1, 0, "Large brown tree" }},

    {0x07ec, (ThingData){ "COLU",  1, 0, "Floor lamp" }},
    {0x0055, (ThingData){ "TLMP",  4, 0, "Tall techno floor lamp" }},
    {0x0056, (ThingData){ "TLP2",  4, 0, "Short techno floor lamp" }},
    {0x0022, (ThingData){ "CAND",  1, 0, "Candle" }},
    {0x0023, (ThingData){ "CBRA",  1, 0, "Candelabra" }},
    {0x002c, (ThingData){ "TBLU",  4, 0, "Tall blue firestick" }},
    {0x002d, (ThingData){ "TGRE",  4, 0, "Tall green firestick" }},
    {0x002e, (ThingData){ "TRED",  4, 0, "Tall red firestick" }},
    {0x0037, (ThingData){ "SMBT",  4, 0, "Short blue firestick" }},
    {0x0038, (ThingData){ "SMGT",  4, 0, "Short green firestick" }},
    {0x0039, (ThingData){ "SMRT",  4, 0, "Short red firestick" }},
    {0x0046, (ThingData){ "FCAN",  3, 0, "Burning barrel" }},

    {0x0029, (ThingData){ "CEYE",  3, 1, "Evil Eye: floating eye in symbol, over candle" }},
    {0x002a, (ThingData){ "FSKU",  3, 0, "Floating Skull: flaming skull-rock" }},

    {0x0031, (ThingData){ "GOR1",  3, 1, "Hanging victim, twitching" }},
    {0x003f, (ThingData){ "GOR1",  3, 1, "Hanging victim, twitching" }},
    {0x0032, (ThingData){ "GOR2",  1, 0, "Hanging victim, arms out" }},
    {0x003b, (ThingData){ "GOR2",  1, 0, "Hanging victim, arms out" }},
    {0x0034, (ThingData){ "GOR4",  1, 0, "Hanging pair of legs" }},
    {0x003c, (ThingData){ "GOR4",  1, 0, "Hanging pair of legs" }},
    {0x0033, (ThingData){ "GOR3",  1, 0, "Hanging victim, 1-legged" }},
    {0x003d, (ThingData){ "GOR3",  1, 0, "Hanging victim, 1-legged" }},
    {0x0035, (ThingData){ "GOR5",  1, 0, "Hanging leg" }},
    {0x003e, (ThingData){ "GOR5",  1, 0, "Hanging leg" }},
    {0x0049, (ThingData){ "HDB1",  1, 0, "Hanging victim, guts removed" }},
    {0x004a, (ThingData){ "HDB2",  1, 0, "Hanging victim, guts and brain removed" }},
    {0x004b, (ThingData){ "HDB3",  1, 0, "Hanging torso, looking down" }},
    {0x004c, (ThingData){ "HDB4",  1, 0, "Hanging torso, open skull" }},
    {0x004d, (ThingData){ "HDB5",  1, 0, "Hanging torso, looking up" }},
    {0x004e, (ThingData){ "HDB6",  1, 0, "Hanging torso, brain removed" }},

    {0x0019, (ThingData){ "POL1",  1, 0, "Impaled human" }},
    {0x001a, (ThingData){ "POL6",  2, 0, "Twitching impaled human" }},
    {0x001b, (ThingData){ "POL4",  1, 0, "Skull on a pole" }},
    {0x001c, (ThingData){ "POL2",  1, 0, "5 skulls shish kebob" }},
    {0x001d, (ThingData){ "POL3",  2, 0, "Pile of skulls and candles" }},
    {0x000a, (ThingData){ "PLAY",-24, 0, "Bloody mess (an exploded player)" }},
    {0x000c, (ThingData){ "PLAY",-24, 0, "Bloody mess, this thing is exactly the same as 10" }},
    {0x0018, (ThingData){ "POL5",  1, 0, "Pool of blood and flesh" }},
    {0x004f, (ThingData){ "POB1",  1, 0, "Pool of blood" }},
    {0x0050, (ThingData){ "POB2",  1, 0, "Pool of blood" }},
    {0x0051, (ThingData){ "BRS1",  1, 0, "Pool of brains" }},
    {0x000f, (ThingData){ "PLAY",-17, 0, "Dead player" }},
    {0x0012, (ThingData){ "POSS",-15, 0, "Dead former human" }},
    {0x0013, (ThingData){ "SPOS",-15, 0, "Dead former sergeant" }},
    {0x0014, (ThingData){ "TROO",-16, 0, "Dead imp" }},
    {0x0015, (ThingData){ "SARG",-17, 0, "Dead demon" }},
    {0x0016, (ThingData){ "HEAD",-15, 0, "Dead cacodemon" }},
    {0x0017, (ThingData){ "SKUL",-14, 0, "Dead lost soul, invisible (they blow up when killed)" }}
};


#endif

