// Zero spread overrides for the DayZ and Workshop weapon configs used by PREDATE.
class CfgPatches
{
    class PredateBucaPrecision
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "BC_Ammo_127x55",
            "BC_Ammo_20mm",
            "BC_Ammo_300BO",
            "BC_Ammo_338",
            "BC_Ammo_408",
            "BC_Ammo_408AP",
            "BC_Ammo_46x30mm",
            "BC_Ammo_50Beowulf",
            "BC_Ammo_50Cal",
            "BC_Ammo_FN57x28mm",
            "BC_Ammo_Rail",
            "A2CHEY_Suppressor",
            "BC_338_Suppressor",
            "BC_408_Suppressor",
            "BC_408_Suppressor_2",
            "BC_50BMGDeagle",
            "BC_50Cal_Suppressor",
            "BC_50Cal_Suppressor_2",
            "BC_50Cal_Suppressor_3",
            "BC_A2CHEY",
            "BC_A2CHEY_Scope",
            "BC_ACR17S",
            "BC_AEK971",
            "BC_AK47T",
            "BC_AKDRACO",
            "BC_AMB17",
            "BC_ANZIO",
            "BC_AR15Beowulf",
            "BC_AR50",
            "BC_ARS12",
            "BC_ARS_Suppressor",
            "BC_ARX160",
            "BC_AS50",
            "BC_ASH12",
            "BC_ASVAL",
            "BC_AWM",
            "BC_AWP",
            "BC_AX50",
            "BC_BarrettM107A1",
            "BC_BarrettM107A1_Suppressor",
            "BC_BarrettM82A1",
            "BC_BarrettM95",
            "BC_BFG",
            "BC_Bipod_AtlasBipod",
            "BC_Calibre_Sons",
            "BC_C14",
            "BC_CDX50",
            "BC_CheytacPaladin",
            "BC_Cordura_Suppressor",
            "BC_CSR50",
            "BC_CZScorpion",
            "BC_CZTSR",
            "BC_DELTA",
            "BC_DesertTechHTI",
            "BC_DesertTechSRSA2",
            "BC_DesertTechSRSA2_Suppressor",
            "BC_DSR",
            "BC_DVLM1",
            "BC_DVLM2",
            "BC_FAMASF1",
            "BC_FNEVOLYS",
            "BC_FNFAL",
            "BC_FNFiveSeven",
            "BC_FNP90",
            "BC_G36",
            "BC_GEVAR",
            "BC_GEVAR338_Suppressor",
            "BC_GM6Lynx",
            "BC_HK416D",
            "BC_HK417",
            "BC_HKG28",
            "BC_HKG36C",
            "BC_HKG36K",
            "BC_HKPSG1",
            "BC_HoneyBadger",
            "BC_HS50M1",
            "BC_HT76",
            "BC_JK_Armament_155_GOAT_556_Suppressor",
            "BC_K14",
            "BC_KAC_PRS_Universal_Suppressor",
            "BC_KORD",
            "BC_KSVK",
            "BC_M100",
            "BC_M1000X",
            "BC_M110",
            "BC_M1A",
            "BC_M200",
            "BC_M2010",
            "BC_M240",
            "BC_M249",
            "BC_M300",
            "BC_M300T",
            "BC_M32Milkor",
            "BC_M4A1",
            "BC_M60",
            "BC_M82A2",
            "BC_M98B",
            "BC_M99",
            "BC_MCXRAPTOR",
            "BC_MCXSPEAR338",
            "BC_MCXVIRTUS",
            "BC_MDR",
            "BC_MG338",
            "BC_MG338_Suppressor",
            "BC_MK18Mjolnir",
            "BC_MP7A1",
            "BC_NoreenULR50",
            "BC_NTW",
            "BC_OSV96",
            "BC_PBS4_Suppressor",
            "BC_PKM",
            "BC_PKP",
            "BC_PTRD",
            "BC_R700A7",
            "BC_Revolver50",
            "BC_RN50",
            "BC_Rogue",
            "BC_RPK16",
            "BC_RSASS",
            "BC_Rsh12",
            "BC_SA58",
            "BC_Sabatti",
            "BC_SCARH",
            "BC_Scout",
            "BC_SERBU",
            "BC_SigMPX",
            "BC_SNIPEX",
            "BC_SnipexM100",
            "BC_SnowOwl",
            "BC_SR25",
            "BC_SV98",
            "BC_T95",
            "BC_TAC21",
            "BC_TAC95",
            "BC_VictrixCorvoCS",
            "BC_VictrixCorvoCS_Suppressor",
            "BC_VSK94",
            "BC_VSK94_Suppressor",
            "BC_VSSK",
            "BC_Wrapped_Suppressor",
            "BC_Zafir"
        };
    };
};

class CfgAmmo
{
#include "predate-rounds.hpp"
};

class Mode_FullAuto;
class Mode_SemiAuto;
class Mode_Single;

#include "distant_audio.hpp"

#define PREDATE_BUCA_308_SUPPRESSED(Report) \
    {Report, "BC_Cal_308_SilencerTail_SoundSet", "BC_Cal_308_SilencerInteriorTail_SoundSet"}
#define PREDATE_BUCA_556_SUPPRESSED(Report) \
    {Report, "BC_Cal_556x45_SilencerTail_SoundSet", "BC_Cal_556x45_SilencerInteriorTail_SoundSet"}
#define PREDATE_BUCA_762X54_SUPPRESSED(Report) \
    {Report, "BC_Cal_762x54_SilencerTail_SoundSet", "BC_Cal_762x54_SilencerInteriorTail_SoundSet"}

class CfgWeapons
{
    class BoltActionRifle_ExternalMagazine_Base;
    class BoltActionRifle_InnerMagazine_Base;
    class famas;
    class Pistol_Base;
    class Rifle_Base;
    class BC_50BMGDeagle_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_A2CHEY_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_ACR17S_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AEK971_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AK47T_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AKDRACO_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AMB17_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_ANZIO_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AR15Beowulf_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AR50_Base: BoltActionRifle_InnerMagazine_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class BC_ARS12_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_ARX160_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AS50_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_ASH12_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_ASVAL_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AWM_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AWP_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_AX50_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_BarrettM107A1_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_BarrettM82A1_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_BarrettM95_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_BFG_Base: BoltActionRifle_InnerMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_C14_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_CDX50_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_CheytacPaladin_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_CSR50_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_CZScorpion_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_CZTSR_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_DELTA_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_DesertTechHTI_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_DesertTechSRSA2_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_DSR_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_DVLM1_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_DVLM2_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_FAMASF1_Base: famas
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_FNEVOLYS_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_FNFAL_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_FNFiveSeven_Base: Pistol_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_FNP90_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_G36_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_G36_Admin: BC_G36_Base
    {
        class FullAuto: Mode_FullAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_308_SUPPRESSED("PredateBucaFALSuppressed")};
        };
    };
    class BC_GEVAR_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_GM6Lynx_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HK416D_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HK417_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HKG28_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HKG36C_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HKG36K_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HKPSG1_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_308_SUPPRESSED("PredateBucaPSG1Suppressed"),
                PREDATE_BUCA_308_SUPPRESSED("PredateBucaPSG1Improvised")};
        };
    };
    class BC_HoneyBadger_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HS50M1_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_HT76_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_K14_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_KORD_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_KSVK_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M1000X_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M100_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M110_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M1A_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_308_SUPPRESSED("PredateBucaFALSuppressed")};
        };
    };
    class BC_M200_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M2010_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M240_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_762X54_SUPPRESSED("PKP_sup_SoundSet"),
                PREDATE_BUCA_762X54_SUPPRESSED("PKP_sup_SoundSet")};
        };
        class SemiAuto: Mode_SemiAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_762X54_SUPPRESSED("PKP_sup_SoundSet"),
                PREDATE_BUCA_762X54_SUPPRESSED("PKP_sup_SoundSet")};
        };
    };
    class BC_M249_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_556_SUPPRESSED("BC_M4A1_sup_SoundSet"),
                PREDATE_BUCA_556_SUPPRESSED("BC_M4A1_sup_SoundSet")};
        };
        class SemiAuto: Mode_SemiAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_556_SUPPRESSED("BC_M4A1_sup_SoundSet"),
                PREDATE_BUCA_556_SUPPRESSED("BC_M4A1_sup_SoundSet")};
        };
    };
    class BC_M300_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M300T_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M32Milkor_Base: Rifle_Base
    {
        class Muzzle_Base;
        class SecondMuzzle: Muzzle_Base
        {
            class Single: Mode_SemiAuto { dispersion = 0; };
        };
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M4A1_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M60_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M82A2_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M98B_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_M99_Base: BoltActionRifle_InnerMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_MCXRAPTOR_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_MCXSPEAR338_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_MCXVIRTUS_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_MDR_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_MG338_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_MK18Mjolnir_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_MP7A1_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_NoreenULR50_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_NTW_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_OSV96_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_PKM_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_762X54_SUPPRESSED("PredateBucaPKMSuppressed"),
                PREDATE_BUCA_762X54_SUPPRESSED("PredateBucaPKMImprovised")};
        };
        class SemiAuto: Mode_SemiAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_762X54_SUPPRESSED("PredateBucaPKMSuppressed"),
                PREDATE_BUCA_762X54_SUPPRESSED("PredateBucaPKMImprovised")};
        };
    };
    class BC_PKP_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_PTRD_Base: BoltActionRifle_InnerMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_R700SA_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_Revolver50_Base: Pistol_Base
    {
        class Muzzle_Base;
        class SecondMuzzle: Muzzle_Base
        {
            class Single: Mode_Single { dispersion = 0; };
        };
        class Single: Mode_Single { dispersion = 0; };
    };
    class BC_RN50_Base: Rifle_Base
    {
        class Single: Mode_Single { dispersion = 0; };
    };
    class BC_Rogue_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_RPK16_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_RSASS_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_Rsh12_Base: Pistol_Base
    {
        class Muzzle_Base;
        class SecondMuzzle: Muzzle_Base
        {
            class Single: Mode_Single { dispersion = 0; };
        };
        class Single: Mode_Single { dispersion = 0; };
    };
    class BC_SA58_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_Sabatti_Base: BoltActionRifle_InnerMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SCARH_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_Scout_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SERBU_Base: Rifle_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SigMPX_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SNIPEX_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SnipexM100_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SnowOwl_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SR25_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_SV98_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_T95_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_308_SUPPRESSED("PredateBucaFALSuppressed")};
        };
        class SemiAuto: Mode_SemiAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_308_SUPPRESSED("PredateBucaFALSuppressed")};
        };
    };
    class BC_TAC21_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_TAC95_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_VictrixCorvoCS_Base: BoltActionRifle_ExternalMagazine_Base
    {
        class Single: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_VSK94_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto { dispersion = 0; };
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_VSSK_Base: Rifle_Base
    {
        class SemiAuto: Mode_SemiAuto { dispersion = 0; };
    };
    class BC_Zafir_Base: Rifle_Base
    {
        class FullAuto: Mode_FullAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_762X54_SUPPRESSED("PKP_sup_SoundSet")};
        };
        class SemiAuto: Mode_SemiAuto
        {
            dispersion = 0;
            soundSetShotExt[] = {PREDATE_BUCA_762X54_SUPPRESSED("PKP_sup_SoundSet")};
        };
    };
};

#undef PREDATE_BUCA_308_SUPPRESSED
#undef PREDATE_BUCA_556_SUPPRESSED
#undef PREDATE_BUCA_762X54_SUPPRESSED

class CfgVehicles
{
    class BC_Picatinny_Attachments_Base;
    class ItemOptics_Base;
    class ItemSuppressor;
    class BC_338_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_408_Suppressor_2_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_408_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_50Cal_Suppressor_2_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_50Cal_Suppressor_3_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_50Cal_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_A2CHEY_Scope_Base: ItemOptics_Base { dispersionModifier = 0; };
    class BC_A2CHEY_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_ARS_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_BarrettM107A1_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_Bipod_AtlasBipod: BC_Picatinny_Attachments_Base { dispersionModifier = 0; };
    class BC_Cordura_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_DesertTechSRSA2_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_GEVAR338_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_JK_Armament_155_GOAT_556_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_KAC_PRS_Universal_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_MG338_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_PBS4_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_VictrixCorvoCS_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_VSK94_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
    class BC_Wrapped_Suppressor_Base: ItemSuppressor { dispersionModifier = 0; };
};
