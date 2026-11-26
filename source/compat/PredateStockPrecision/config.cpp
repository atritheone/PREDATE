// Zero spread overrides for the DayZ and Workshop weapon configs used by PREDATE.
class CfgPatches
{
    class PredateStockPrecision
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DayZExpansion_Dta_Core",
            "DZ_Nonlethal_Shockpistol",
            "DZ_Pistols_1911",
            "DZ_Pistols_CZ75",
            "DZ_Pistols_DE",
            "DZ_Pistols_Derringer",
            "DZ_Pistols_Encore",
            "DZ_Pistols_Flaregun",
            "DZ_Pistols_FNX45",
            "DZ_Pistols_Glock",
            "DZ_Pistols_Magnum",
            "DZ_Pistols_mkii",
            "DZ_Pistols_Nasdara_Luger",
            "DZ_Pistols_p1",
            "DZ_Pistols_pb6p9",
            "DZ_Pistols_pmm",
            "DZ_Pistols_Red9",
            "DZ_Weapons_Firearms_AK101",
            "DZ_Weapons_Firearms_AK74",
            "DZ_Weapons_Firearms_AKM",
            "DZ_Weapons_Firearms_aug",
            "DZ_Weapons_Firearms_B95",
            "DZ_Weapons_Firearms_CZ527",
            "DZ_Weapons_Firearms_CZ550",
            "DZ_Weapons_Firearms_cz61",
            "DZ_Weapons_Firearms_FAL",
            "DZ_Weapons_Firearms_Famas",
            "DZ_Weapons_Firearms_groza",
            "DZ_Weapons_Firearms_IZH18",
            "DZ_Weapons_Firearms_M14",
            "DZ_Weapons_Firearms_M16A2",
            "DZ_Weapons_Firearms_M249Para",
            "DZ_Weapons_Firearms_M4",
            "DZ_Weapons_Firearms_MosinNagant",
            "DZ_Weapons_Firearms_MP5",
            "DZ_Weapons_Firearms_pm73rak",
            "DZ_Weapons_Firearms_PP19",
            "DZ_Weapons_Firearms_Repeater",
            "DZ_Weapons_Firearms_Ruger1022",
            "DZ_Weapons_Firearms_SaigaK",
            "DZ_Weapons_Firearms_SCARH",
            "DZ_Weapons_Firearms_Scout",
            "DZ_Weapons_Firearms_SKS",
            "DZ_Weapons_Firearms_SSG82",
            "DZ_Weapons_Firearms_SV98",
            "DZ_Weapons_Firearms_SVD",
            "DZ_Weapons_Firearms_Trumpet",
            "DZ_Weapons_Firearms_UMP",
            "DZ_Weapons_Firearms_VSS",
            "DZ_Weapons_Firearms_Winchester70",
            "DZ_Weapons_Launchers_LAW",
            "DZ_Weapons_Launchers_M79",
            "DZ_Weapons_Launchers_RPG7",
            "DZ_Weapons_Muzzles",
            "DZ_Weapons_Nasdara_Firearms_LeeEnfield",
            "DZ_Weapons_Nasdara_Firearms_mp18",
            "DZ_Weapons_Nasdara_Firearms_SCARL",
            "DZ_weapons_nonlethal_DartGun",
            "DZ_Weapons_Projectiles",
            "DZ_Weapons_Shotguns_Izh18Shotgun",
            "DZ_Weapons_Shotguns_Izh43",
            "DZ_Weapons_Shotguns_MP133",
            "DZ_Weapons_Shotguns_RemingtonR12",
            "DZ_Weapons_Supports"
        };
    };
};

class WeaponMode_Base;
class Mode_SemiAuto: WeaponMode_Base { dispersion = 0; };
class Mode_Single: Mode_SemiAuto { dispersion = 0; };
class Mode_Burst: Mode_SemiAuto { dispersion = 0; };
class Mode_Double: Mode_SemiAuto { dispersion = 0; };
class Mode_FullAuto: Mode_SemiAuto { dispersion = 0; };

class CfgWeapons
{
    class BoltActionRifle_ExternalMagazine_Base;
    class BoltActionRifle_InnerMagazine_Base;
    class FNX45;
    class Izh18;
    class Launcher_Base;
    class NonlethalPistol;
    class Pistol_Base;
    class Rifle_Base;
    class Shotgun_Base;
    class AK101_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class AK74_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class AKM_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class AKS74U: AK74_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Aug_Base: Rifle_Base
    {
        class Burst: Mode_Burst { dispersion = 0; };
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class AugShort: Aug_Base
    {
        class Burst: Mode_Burst { dispersion = 0; };
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class B95_Base: Rifle_Base
    {
        class Muzzle_Base;
        class Double: Mode_Double { dispersion = 0; };
        class SecondMuzzle: Muzzle_Base
        {
            class Double: Mode_Double { dispersion = 0; };
            class Single: Mode_Single { dispersion = 0; };
        };
        class Single: Mode_Single { dispersion = 0; };
    };
    class Colt1911_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class CZ527_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class CZ550_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class CZ61_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class CZ75_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class DartGun: NonlethalPistol
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class Deagle_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Derringer_Base: Pistol_Base
    {
        class Muzzle_Base;
        class SecondMuzzle: Muzzle_Base
        {
            class Single: Mode_Single { dispersion = 0; };
        };
        class Single: Mode_Single { dispersion = 0; };
    };
    class FNX45_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Expansion_LobotomyDartGun_Base: FNX45_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class FAL_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Famas_Base: Rifle_Base
    {
        class Burst: Mode_Burst { dispersion = 0; };
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Flaregun_Base: Pistol_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class FNX45_arrow: FNX45 { dispersion = 0; };
    class Glock19_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Groza_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Izh18_Base: Rifle_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class Izh18Shotgun_Base: Shotgun_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class Izh43Shotgun_Base: Shotgun_Base
    {
        class Muzzle_Base;
        class Double: Mode_Double { dispersion = 0; };
        class SecondMuzzle: Muzzle_Base
        {
            class Double: Mode_Double { dispersion = 0; };
            class Single: Mode_Single { dispersion = 0; };
        };
        class Single: Mode_Single { dispersion = 0; };
    };
    class LAWBase: Launcher_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class LeeEnfield_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class LongHorn_Base: Pistol_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class Luger_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class M14_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class M16A2_Base: Rifle_Base
    {
        class Burst: Mode_Burst { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class M249_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
    };
    class M4A1_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class M79_Base: Rifle_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class Magnum_Base: Pistol_Base
    {
        class Muzzle_Base;
        class SecondMuzzle: Muzzle_Base
        {
            class Single: Mode_Single { dispersion = 0; };
        };
        class Single: Mode_Single { dispersion = 0; };
    };
    class MakarovIJ70_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class MakarovPB_Base: Pistol_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class MKII_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Mosin9130_Base: BoltActionRifle_InnerMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class Mp133Shotgun_Base: Shotgun_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class MP18_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
    };
    class MP5K_Base: Rifle_Base
    {
        class Burst: Mode_Burst { dispersion = 0; };
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class P1_Base: Pistol_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class PM73Rak_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
    };
    class PP19_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class R12_Base: Shotgun_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Red9_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Repeater_Base: Rifle_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class RPG7Base: Launcher_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class Ruger1022_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Saiga_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class SawedoffB95: B95_Base
    {
        class Double: Double { dispersion = 0; };
        class SecondMuzzle: SecondMuzzle
        {
            class Double: Double { dispersion = 0; };
            class Single: Single { dispersion = 0; };
        };
        class Single: Single { dispersion = 0; };
    };
    class SawedoffIzh18: Izh18
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class SawedoffIzh18Shotgun: Izh18Shotgun_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class SawedoffIzh43Shotgun: Izh43Shotgun_Base
    {
        class Double: Mode_Double { dispersion = 0; };
        class SecondMuzzle: SecondMuzzle
        {
            class Double: Mode_Double { dispersion = 0; };
            class Single: Mode_Single { dispersion = 0; };
        };
        class Single: Mode_Single { dispersion = 0; };
    };
    class SawedoffMagnum: Magnum_Base
    {
        class SecondMuzzle: SecondMuzzle
        {
            class Single: Single { dispersion = 0; };
        };
        class Single: Single { dispersion = 0; };
    };
    class SawedoffMosin9130_Base: Mosin9130_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class SCARH_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class SCARL_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Scout_Base: BoltActionRifle_ExternalMagazine_Base
    {
        dispersionModifier = 0;
        class Single: Mode_Single { dispersion = 0; };
    };
    class Shockpistol_Base: NonlethalPistol
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class SKS_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class SSG82_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class SV98_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class SVD_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Trumpet_Base: Rifle_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class UMP45_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class UnderSlugGrenadeM4: Launcher_Base
    {
        dispersionModifier = 0;
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class VSS_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Vikhr: VSS_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class Winchester70_Base: BoltActionRifle_InnerMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
};

class CfgAmmo
{
    class Bullet_12GaugeSlug;
    class BulletCore;
    class ShotgunCore;
    class Bullet_12GaugeBeanbag: Bullet_12GaugeSlug { dispersion = 0; };
    class Shotgun_Base: ShotgunCore { coefGravity = 0; };
    class Bullet_12GaugePellets: Shotgun_Base { dispersion = 0; };
    class Bullet_Base: BulletCore { coefGravity = 0; };
#include "predate-rounds.hpp"
};

class CfgVehicles
{
    class Inventory_Base;
    class ItemSuppressor;
    class AK_Suppressor: ItemSuppressor { dispersionModifier = 0; };
    class AtlasBipod: Inventory_Base { dispersionModifier = 0; };
    class Groza_Barrel_Suppressor: ItemSuppressor { dispersionModifier = 0; };
    class Groza_LowerReceiver: Inventory_Base { dispersionModifier = 0; };
    class GrozaGL_LowerReceiver: Inventory_Base { dispersionModifier = 0; };
    class M249_Bipod: Inventory_Base { dispersionModifier = 0; };
    class M249_Hndgrd: Inventory_Base { dispersionModifier = 0; };
    class M249_RisHndgrd: Inventory_Base { dispersionModifier = 0; };
    class M4_Suppressor: ItemSuppressor { dispersionModifier = 0; };
    class PistolSuppressor: ItemSuppressor { dispersionModifier = 0; };
    class Red9Bttstck: Inventory_Base { dispersionModifier = 0; };
};
