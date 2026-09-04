1. Create MountSeats.ini in your system

2. MountSeats.ini should be like this: 

; MountSeats.ini
;
; IMF_NATIVE_BONE_OR_PRESET_FINAL_R12
;
; Enabled=0
;   -> DLL does not override this mount.
;
; Enabled=1 + SeatBone=ValidBone
;   -> use that bone. X/Y/Z are ignored in bone mode.
;
; Enabled=1 + SeatBone=
;   -> native no-bone preset mode.
;   -> X/Y/Z are written into UE2 bHardAttach/HardRelMatrix translation.
;
; If a non-empty SeatBone is requested but AttachToBone returns 0,
; the DLL automatically falls back to the same native X/Y/Z preset mode.
;
[General]
MountCount=14
LogPresetHits=1
MaxInitialHitLogs=20
PeriodicHitLogEvery=500

[MountList]
Mount0=41000

[Mount.41000]
Enabled=1
CodeName=Rider.gray_horse
Mesh=Riders.gray_horse_m00
SeatBone=
X=-6.0
Y=0.0
Z=35.0


3. Adapt it on engine.dll ( i did it with CFF Explorer> import table)

4. After Launch the l2.exe should be created file in system with name InterludeMountFix.log

5. in InterludeMountFix.log you will see this READY.

[2026-09-04 12:14:37.306] ============================================================
[2026-09-04 12:14:37.307] InterludeMountFix - INI rider SeatBone selector
[2026-09-04 12:14:37.308] Build tag = IMF_NATIVE_BONE_OR_PRESET_FINAL_R12
[2026-09-04 12:14:37.309] Hook = AActor::AttachToBone(FName)
[2026-09-04 12:14:37.310] Mode = custom bone OR native bHardAttach/HardRelMatrix preset
[2026-09-04 12:14:37.311] ============================================================
[2026-09-04 12:14:38.313] Engine timestamp = 0x46DBE989
[2026-09-04 12:14:38.313] Engine image size = 0x01B0D000
[2026-09-04 12:14:38.314] Original RiderEnter bone = Bone15 | FName=0x00002014
[2026-09-04 12:14:38.323] INI = C:\Clients\Lineage II - Replica\system\MountSeats.ini
[2026-09-04 12:14:38.323] Declared MountCount = 14
[2026-09-04 12:14:38.325] ACTIVE Mount.41000 | NPC=41000 | Name=Rider.gray_horse | Mesh=Riders.gray_horse_m00 | SeatBone=<empty / Engine preset> | FName=0x00000000 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.326] ACTIVE Mount.41001 | NPC=41001 | Name=Rider.tawny_maned_lion | Mesh=Riders.tawny_maned_lion_m00 | SeatBone=Dummy01 | FName=0x00000001 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.327] ACTIVE Mount.41002 | NPC=41002 | Name=Rider.steam_sledge | Mesh=Riders.steam_sledge_m00 | SeatBone=Dummy_man | FName=0x00000001 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.329] ACTIVE Mount.41003 | NPC=41003 | Name=Rider.br_z_bike | Mesh=Riders.br_z_bike_m00 | SeatBone=<empty / Engine preset> | FName=0x00000000 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.330] ACTIVE Mount.41004 | NPC=41004 | Name=Rider.br_g_ant_princess | Mesh=Riders.g_ant_princess_m00 | SeatBone=Bone15 | FName=0x00000001 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.331] ACTIVE Mount.41005 | NPC=41005 | Name=Rider.br_g_black_bear | Mesh=Riders.g_black_bear | SeatBone=Dummy01 | FName=0x00000001 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.332] ACTIVE Mount.41006 | NPC=41006 | Name=Rider.br_g_halloween_flying_broom | Mesh=Riders.g_halloween_flying_broom_m00 | SeatBone=Dummy01 | FName=0x00000001 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.333] ACTIVE Mount.41007 | NPC=41007 | Name=Rider.illusion_vehicle | Mesh=Riders.illusion_vehicle_m00 | SeatBone=Dummy_seat | FName=0x00000001 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.334] ACTIVE Mount.41008 | NPC=41008 | Name=Rider.vehicle_lindvior | Mesh=Riders.vehicle_lindvior_m00 | SeatBone=Dummy01 | FName=0x00000001 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.335] ACTIVE Mount.41009 | NPC=41009 | Name=Rider.craft_vehicle_dwarf | Mesh=Riders.craft_vehicle_dwarf_m00 | SeatBone=<empty / Engine preset> | FName=0x00000000 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.337] SKIP Mount.41010 | NPC=41010 | Rider.eligor_vehicle | disabled
[2026-09-04 12:14:38.338] ACTIVE Mount.41011 | NPC=41011 | Name=Rider.elder_pegasus_vehicle | Mesh=Riders.elder_pegasus_vehicle_m00 | SeatBone=<empty / Engine preset> | FName=0x00000000 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.339] SKIP Mount.41012 | NPC=41012 | Rider.wing_hound_vehicle | disabled
[2026-09-04 12:14:38.340] ACTIVE Mount.41013 | NPC=41013 | Name=Rider.sp_griffin_vehicle | Mesh=Riders.sp_griffin_vehicle_m00 | SeatBone=<empty / Engine preset> | FName=0x00000000 | XYZ=(0.000, 0.000, 0.000) [bone-local offset]
[2026-09-04 12:14:38.340] Active mount presets = 12
[2026-09-04 12:14:38.341] AActor::LocalToWorld implementation = 00A777B0
[2026-09-04 12:14:38.342] Expected AActor::LocalToWorld implementation = 00A777B0
[2026-09-04 12:14:38.343] Engine base = 00A50000
[2026-09-04 12:14:38.343] AttachToBone export = 00A55F79
[2026-09-04 12:14:38.344] AttachToBone implementation = 00C7E3E0
[2026-09-04 12:14:38.345] Expected implementation = 00C7E3E0
[2026-09-04 12:14:38.346] SUCCESS: AActor::AttachToBone(FName) hook installed.
[2026-09-04 12:14:38.347] RiderEnter export = 00A5E06B
[2026-09-04 12:14:38.348] RiderEnter implementation = 00D790E0
[2026-09-04 12:14:38.348] Expected RiderEnter implementation = 00D790E0
[2026-09-04 12:14:38.349] SUCCESS: APawn::RiderEnter hook installed.
[2026-09-04 12:14:38.349] READY.
[2026-09-04 12:14:38.350] SeatBone is applied in AttachToBone; X/Y/Z are applied AFTER original RiderEnter completes.
[2026-09-04 12:14:38.350] Duplicate SeatBone values are allowed.
[2026-09-04 12:14:38.351] Any positive NPC ID listed in [MountList] is supported.
[2026-09-04 12:14:38.351] ============================================================



6. After New Launch InterludeMountFix.log Delete and Create New One so InterludeMountFix.log will never be more than 5KB
