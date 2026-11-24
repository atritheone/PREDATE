// Reference samples already shipped with Buca Weapons. The existing sound
// shaders keep their distance limits, volume and close/mid transitions.
#define PRE_BUCA_FAR(Shader, Clip) class Shader { samples[] = {{Clip, 1}}; };
#define PRE_BUCA_DERIVED_FAR(Shader, Base, Clip) class Shader: Base { samples[] = {{Clip, 1}}; };
#define PRE_M4A1_FAR "BUCA_WEAPONS\Weapons\Rifles\M4A1\Sounds\M4A1_far"
#define PRE_HK416D_FAR "BUCA_WEAPONS\Weapons\Rifles\HK416D\Sounds\HK416_far"
#define PRE_HK416D_SUP "BUCA_WEAPONS\Weapons\Rifles\HK416D\Sounds\HK416_sup_far"
#define PRE_SCARH_FAR "BUCA_WEAPONS\Weapons\Rifles\SCARH\Sounds\SCAR_H_shot_01_far"
#define PRE_SCARH_SUP "BUCA_WEAPONS\Weapons\Rifles\SCARH\Sounds\SCAR_H_shot_01_sup_far"
#define PRE_M110_FAR "BUCA_WEAPONS\Weapons\Rifles\M110\Sounds\M110_Shot_far"
#define PRE_M110_SUP "BUCA_WEAPONS\Weapons\Rifles\M110\Sounds\M110_Shot_sup_far"
#define PRE_AK47_FAR "BUCA_WEAPONS\Weapons\Rifles\AK47\Sounds\AK47T_far"
#define PRE_AK47_SUP "BUCA_WEAPONS\Weapons\Rifles\AK47\Sounds\AK47T_sup_far"
#define PRE_AEK971_FAR "BUCA_WEAPONS\Weapons\Rifles\AEK971\Sounds\AEK971_far"
#define PRE_AEK971_SUP "BUCA_WEAPONS\Weapons\Rifles\AEK971\Sounds\AEK971_sup_far"
#define PRE_PKP_FAR "BUCA_WEAPONS\Weapons\LMGs\PKP\Sounds\PKP_unsuppressed_far"
#define PRE_PKP_SUP "BUCA_WEAPONS\Weapons\LMGs\PKP\Sounds\PKP_sup_far"
#define PRE_ASVAL_FAR "BUCA_WEAPONS2\Weapons\Rifles\ASVAL\Sounds\ASVAL_far"
#define PRE_HONEY_BADGER_FAR "BUCA_WEAPONS2\Weapons\Rifles\HoneyBadger\sounds\HoneyBadger_Sup_Far"
#define PRE_AWM_FAR "BUCA_WEAPONS\Weapons\Snipers\AWM\Sounds\AWM_far"
#define PRE_AWM_SUP "BUCA_WEAPONS\Weapons\Snipers\AWM\Sounds\AWM_Sup_far"
#define PRE_GEVAR_FAR "BUCA_WEAPONS2\Weapons\Rifles\GEVAR\Sounds\Gevar_far"
#define PRE_GEVAR_SUP "BUCA_WEAPONS2\Weapons\Rifles\GEVAR\Sounds\Gevar_sup_far"
#define PRE_M200_FAR "BUCA_WEAPONS\Weapons\Snipers\M200\Sounds\m200_far"
#define PRE_M200_SUP "BUCA_WEAPONS\Weapons\Snipers\M200\Sounds\m200_sup_far"
#define PRE_CHEYTAC_AP_FAR "BUCA_WEAPONS\Weapons\Snipers\A2CHEY\Sounds\A2CHEY_Far"
#define PRE_CHEYTAC_AP_SUP "BUCA_WEAPONS\Weapons\Snipers\A2CHEY\Sounds\A2CHEY_close_shot_01suppressed_far"
#define PRE_M82A1_FAR "BUCA_WEAPONS\Weapons\Snipers\M82A1\Sounds\m82_far"
#define PRE_M82A1_SUP "BUCA_WEAPONS\Weapons\Snipers\M82A1\Sounds\m82_sup_far"
#define PRE_M95_FAR "BUCA_WEAPONS3\Weapons\Snipers\BarrettM95\Sounds\BarrettM95_far"
#define PRE_M95_SUP "BUCA_WEAPONS3\Weapons\Snipers\BarrettM95\Sounds\BarrettM95_sup_far"
#define PRE_DEAGLE_FAR "BUCA_WEAPONS\Weapons\Pistols\50BMGDeagle\Sounds\deaglefun_far"
#define PRE_ANZIO_FAR "BUCA_WEAPONS\Weapons\Snipers\Anzio\Sounds\anzio_far"
#define PRE_MP7_FAR "BUCA_WEAPONS\Weapons\SMGs\MP7A1\Sounds\mp7a1_shot_far"
#define PRE_MP7_SUP "BUCA_WEAPONS\Weapons\SMGs\MP7A1\Sounds\mp7a1_shot_sup_far"
#define PRE_P90_FAR "BUCA_WEAPONS2\Weapons\SMGs\FNP90\Sounds\FNP90_Far"
#define PRE_P90_SUP "BUCA_WEAPONS2\Weapons\SMGs\FNP90\Sounds\FNP90_Sup_Far"

class CfgSoundShaders
{
    class base_distShot_SoundShader;

    // 5.56 NATO, standard rifles and support weapons.
    PRE_BUCA_DERIVED_FAR(BC_M249distShot_SoundShader, base_distShot_SoundShader, PRE_M4A1_FAR)

    // 5.56 NATO, compact rifles.
    PRE_BUCA_FAR(BC_FAMASF1_SilfarShot_SoundShader, PRE_HK416D_SUP)
    PRE_BUCA_DERIVED_FAR(BC_FAMASF1distShot_SoundShader, base_distShot_SoundShader, PRE_HK416D_FAR)
    PRE_BUCA_FAR(BC_HKG36K_SilfarShot_SoundShader, PRE_HK416D_SUP)
    PRE_BUCA_FAR(BC_HKG36K_distShot_SoundShader, PRE_HK416D_FAR)
    PRE_BUCA_FAR(BC_MDR_SilfarShot_SoundShader, PRE_HK416D_SUP)
    PRE_BUCA_FAR(BC_MDR_distShot_SoundShader, PRE_HK416D_FAR)

    // .308 Win / 7.62 NATO, battle rifles.
    PRE_BUCA_FAR(BC_ACR17S_SilfarShot_SoundShader, PRE_SCARH_SUP)
    PRE_BUCA_FAR(BC_ACR17S_distShot_SoundShader, PRE_SCARH_FAR)
    PRE_BUCA_FAR(FNEVOLYS_SilfarShot_SoundShader, PRE_SCARH_SUP)
    PRE_BUCA_FAR(FNEVOLYS_farShot_SoundShader, PRE_SCARH_FAR)
    PRE_BUCA_FAR(BC_FNFAL_SilfarShot_SoundShader, PRE_SCARH_SUP)
    PRE_BUCA_FAR(BC_FNFAL_distShot_SoundShader, PRE_SCARH_FAR)
    PRE_BUCA_FAR(BC_G36Shot_SilfarShot_SoundShader, PRE_SCARH_SUP)
    PRE_BUCA_DERIVED_FAR(BC_G36ShotdistShot_SoundShader, base_distShot_SoundShader, PRE_SCARH_FAR)
    PRE_BUCA_FAR(BC_HK417_SilfarShot_SoundShader, PRE_SCARH_SUP)
    PRE_BUCA_FAR(BC_HK417_distShot_SoundShader, PRE_SCARH_FAR)
    PRE_BUCA_FAR(BC_MCXRAPTOR_SilfarShot_SoundShader, PRE_SCARH_SUP)
    PRE_BUCA_FAR(BC_MCXRAPTOR_distShot_SoundShader, PRE_SCARH_FAR)
    PRE_BUCA_FAR(BC_SA58_SilfarShot_SoundShader, PRE_SCARH_SUP)
    PRE_BUCA_FAR(BC_SA58_distShot_SoundShader, PRE_SCARH_FAR)
    PRE_BUCA_DERIVED_FAR(BC_T95distShot_SoundShader, base_distShot_SoundShader, PRE_SCARH_FAR)

    // .308 Win / 7.62 NATO, marksman rifles.
    PRE_BUCA_FAR(BC_HKG28_SilfarShot_SoundShader, PRE_M110_SUP)
    PRE_BUCA_FAR(BC_HKG28_distShot_SoundShader, PRE_M110_FAR)
    PRE_BUCA_DERIVED_FAR(BC_HKPSG1distShot_SoundShader, base_distShot_SoundShader, PRE_M110_FAR)
    PRE_BUCA_DERIVED_FAR(BC_M1AdistShot_SoundShader, base_distShot_SoundShader, PRE_M110_FAR)
    PRE_BUCA_FAR(BC_RSASS_SilfarShot_SoundShader, PRE_M110_SUP)
    PRE_BUCA_FAR(BC_RSASS_distShot_SoundShader, PRE_M110_FAR)
    PRE_BUCA_FAR(BC_SR25_SilfarShot_SoundShader, PRE_M110_SUP)
    PRE_BUCA_FAR(BC_SR25_distShot_SoundShader, PRE_M110_FAR)

    // 7.62x39.
    PRE_BUCA_FAR(BC_AKDRACO_SilfarShot_SoundShader, PRE_AK47_SUP)
    PRE_BUCA_DERIVED_FAR(BC_AKDRACO_SoundShaderLong, base_distShot_SoundShader, PRE_AK47_FAR)
    PRE_BUCA_FAR(BC_ARX160_SilfarShot_SoundShader, PRE_AK47_SUP)
    PRE_BUCA_FAR(BC_ARX160_distShot_SoundShader, PRE_AK47_FAR)

    // 5.45x39.
    PRE_BUCA_FAR(BC_RPK16_SilfarShot_SoundShader, PRE_AEK971_SUP)
    PRE_BUCA_DERIVED_FAR(BC_RPK16_SoundShaderLong, base_distShot_SoundShader, PRE_AEK971_FAR)

    // 7.62x54R as configured by Buca.
    PRE_BUCA_FAR(BC_PKM_farShot_SoundShader, PRE_PKP_FAR)
    PRE_BUCA_DERIVED_FAR(BC_M60distShot_SoundShader, base_distShot_SoundShader, PRE_PKP_FAR)
    PRE_BUCA_FAR(M60_SilfarShot_SoundShader, PRE_PKP_SUP)
    PRE_BUCA_DERIVED_FAR(BC_M240distShot_SoundShader, base_distShot_SoundShader, PRE_PKP_FAR)
    PRE_BUCA_DERIVED_FAR(BC_ZafirdistShot_SoundShader, base_distShot_SoundShader, PRE_PKP_FAR)

    // Subsonic 9x39.
    PRE_BUCA_FAR(BC_AMB17_SoundShaderLong, PRE_ASVAL_FAR)
    PRE_BUCA_FAR(BC_VSK94_SilfarShot_SoundShader, PRE_ASVAL_FAR)
    PRE_BUCA_FAR(BC_VSK94_distShot_SoundShader, PRE_ASVAL_FAR)

    // .300 Blackout, suppressed.
    PRE_BUCA_DERIVED_FAR(BC_MCXVIRTUSdistShot_SoundShader, base_distShot_SoundShader, PRE_HONEY_BADGER_FAR)

    // .338 Lapua, bolt-action rifles.
    PRE_BUCA_FAR(BC_CZTSR_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(BC_CZTSR_distShot_SoundShader, PRE_AWM_FAR)
    PRE_BUCA_FAR(BC_DesertTechSRSA2_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(BC_DesertTechSRSA2_distShot_SoundShader, PRE_AWM_FAR)
    PRE_BUCA_FAR(BC_DVLM1_SoundShaderLong, PRE_AWM_FAR)
    PRE_BUCA_FAR(BC_DVLM2_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(BC_DVLM2_distShot_SoundShader, PRE_AWM_FAR)
    PRE_BUCA_FAR(C14_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(C14_farShot_SoundShader, PRE_AWM_FAR)
    PRE_BUCA_FAR(delta_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(delta_farShot_SoundShader, PRE_AWM_FAR)
    PRE_BUCA_FAR(M2010_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(M2010_farShot_SoundShader, PRE_AWM_FAR)
    PRE_BUCA_FAR(BC_R700SA_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(BC_R700SA_farShot_SoundShader, PRE_AWM_FAR)
    PRE_BUCA_FAR(BC_sv98_SilfarShot_SoundShader, PRE_AWM_SUP)
    PRE_BUCA_FAR(BC_sv98_farShot_SoundShader, PRE_AWM_FAR)

    // .338 Lapua, self-loading rifles.
    PRE_BUCA_FAR(BC_HT76_SilfarShot_SoundShader, PRE_GEVAR_SUP)
    PRE_BUCA_DERIVED_FAR(BC_HT76distShot_SoundShader, base_distShot_SoundShader, PRE_GEVAR_FAR)
    PRE_BUCA_FAR(BC_MK18Mjolnir_SilfarShot_SoundShader, PRE_GEVAR_SUP)
    PRE_BUCA_FAR(BC_MK18Mjolnir_distShot_SoundShader, PRE_GEVAR_FAR)
    PRE_BUCA_FAR(BC_MCXSPEAR338_farShot_SoundShader, PRE_GEVAR_FAR)

    // .408 CheyTac.
    PRE_BUCA_FAR(K14_SilfarShot_SoundShader, PRE_M200_SUP)
    PRE_BUCA_FAR(K14_farShot_SoundShader, PRE_M200_FAR)
    PRE_BUCA_FAR(m1000x_SilfarShot_SoundShader, PRE_M200_SUP)
    PRE_BUCA_FAR(m1000x_farShot_SoundShader, PRE_M200_FAR)
    PRE_BUCA_FAR(m300_SilfarShot_SoundShader, PRE_M200_SUP)
    PRE_BUCA_FAR(m300_farShot_SoundShader, PRE_M200_FAR)
    PRE_BUCA_FAR(BC_scout_SilfarShot_SoundShader, PRE_M200_SUP)
    PRE_BUCA_FAR(BC_scout_farShot_SoundShader, PRE_M200_FAR)
    PRE_BUCA_FAR(BC_M100_SoundShaderLong, PRE_M200_SUP)

    // .408 CheyTac AP.
    PRE_BUCA_FAR(BC_CheytacPaladin_SilfarShot_SoundShader, PRE_CHEYTAC_AP_SUP)
    PRE_BUCA_FAR(BC_CheytacPaladin_farShot_SoundShader, PRE_CHEYTAC_AP_FAR)
    PRE_BUCA_FAR(BC_M300T_SilfarShot_SoundShader, PRE_CHEYTAC_AP_SUP)
    PRE_BUCA_FAR(BC_M300T_distShot_SoundShader, PRE_CHEYTAC_AP_FAR)

    // .50 BMG, self-loading rifles and support weapons.
    PRE_BUCA_FAR(BC_AS50_SilfarShot_SoundShader, PRE_M82A1_SUP)
    PRE_BUCA_FAR(BC_AS50_farShot_SoundShader, PRE_M82A1_FAR)
    PRE_BUCA_FAR(BC_GM6Lynx_SilfarShot_SoundShader, PRE_M82A1_SUP)
    PRE_BUCA_FAR(BC_GM6Lynx_farShot_SoundShader, PRE_M82A1_FAR)
    PRE_BUCA_FAR(BC_M107A1_SilfarShot_SoundShader, PRE_M82A1_SUP)
    PRE_BUCA_FAR(BC_M107A1_farShot_SoundShader, PRE_M82A1_FAR)
    PRE_BUCA_FAR(BC_M82A2_farShot_SoundShader, PRE_M82A1_FAR)
    PRE_BUCA_FAR(KORD_farShot_SoundShader, PRE_M82A1_FAR)

    // .50 BMG, bolt-action and single-shot rifles.
    PRE_BUCA_FAR(BC_DesertTechHTI_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(BC_DesertTechHTI_distShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(BC_HS50M1_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(BC_HS50M1_distShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(BC_KSVK_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(BC_NoreenULR50_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(BC_RN50_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(BC_SnipexM100_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(BC_SnipexM100_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(BC_VictrixCorvoCS_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(BC_VictrixCorvoCS_distShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(awp_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(awp_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(ax50_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(ax50_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(CDX50_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(CDX50_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(dsr_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(dsr_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(M98B_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(M98B_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(Rogue_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(Rogue_farShot_SoundShader, PRE_M95_FAR)
    PRE_BUCA_FAR(BC_SnowOwl_SoundShaderLong, PRE_M95_FAR)
    PRE_BUCA_FAR(TAC21_SilfarShot_SoundShader, PRE_M95_SUP)
    PRE_BUCA_FAR(TAC21_farShot_SoundShader, PRE_M95_FAR)

    // .50 BMG, handguns.
    PRE_BUCA_FAR(BC_Revolver50_farShot_SoundShader, PRE_DEAGLE_FAR)

    // 20 mm rifles.
    PRE_BUCA_FAR(BC_Sabatti_farShot_SoundShader, PRE_ANZIO_FAR)
    PRE_BUCA_FAR(BC_BFG_SoundShaderLong, PRE_ANZIO_FAR)
    PRE_BUCA_FAR(BC_CSR50_SoundShaderLong, PRE_ANZIO_FAR)
    PRE_BUCA_FAR(BC_M99_SoundShaderLong, PRE_ANZIO_FAR)
    PRE_BUCA_FAR(BC_NTW_SoundShaderLong, PRE_ANZIO_FAR)
    PRE_BUCA_FAR(OSV96_farShot_SoundShader, PRE_ANZIO_FAR)
    PRE_BUCA_FAR(BC_PTRD_SoundShaderLong, PRE_ANZIO_FAR)
    PRE_BUCA_FAR(Serbu_farShot_SoundShader, PRE_ANZIO_FAR)

    // 4.6x30.
    PRE_BUCA_FAR(BC_CZScorpion_SilfarShot_SoundShader, PRE_MP7_SUP)
    PRE_BUCA_DERIVED_FAR(BC_CZScorpiondistShot_SoundShader, base_distShot_SoundShader, PRE_MP7_FAR)

    // 5.7x28.
    PRE_BUCA_FAR(BC_FNFiveSeven_SilfarShot_SoundShader, PRE_P90_SUP)
    PRE_BUCA_FAR(BC_FNFiveSeven_distShot_SoundShader, PRE_P90_FAR)
};

#undef PRE_BUCA_FAR
#undef PRE_BUCA_DERIVED_FAR
#undef PRE_M4A1_FAR
#undef PRE_HK416D_FAR
#undef PRE_HK416D_SUP
#undef PRE_SCARH_FAR
#undef PRE_SCARH_SUP
#undef PRE_M110_FAR
#undef PRE_M110_SUP
#undef PRE_AK47_FAR
#undef PRE_AK47_SUP
#undef PRE_AEK971_FAR
#undef PRE_AEK971_SUP
#undef PRE_PKP_FAR
#undef PRE_PKP_SUP
#undef PRE_ASVAL_FAR
#undef PRE_HONEY_BADGER_FAR
#undef PRE_AWM_FAR
#undef PRE_AWM_SUP
#undef PRE_GEVAR_FAR
#undef PRE_GEVAR_SUP
#undef PRE_M200_FAR
#undef PRE_M200_SUP
#undef PRE_CHEYTAC_AP_FAR
#undef PRE_CHEYTAC_AP_SUP
#undef PRE_M82A1_FAR
#undef PRE_M82A1_SUP
#undef PRE_M95_FAR
#undef PRE_M95_SUP
#undef PRE_DEAGLE_FAR
#undef PRE_ANZIO_FAR
#undef PRE_MP7_FAR
#undef PRE_MP7_SUP
#undef PRE_P90_FAR
#undef PRE_P90_SUP

class CfgSoundCurves
{
    class PredateBucaTailFalloff
    {
        // Match DayZ's tail gain at close range and taper the shared echo
        // before it overwhelms the direct report at a distance.
        points[] = {{0,1}, {0.004,0.6}, {0.01,0.45}, {0.02,0.4333},
            {0.05,0.25}, {0.1,0.13}, {0.2,0.075}, {0.3,0.05},
            {0.5,0.025}, {0.7,0.013}, {0.9,0.002}, {1,0}};
    };
};

#define PRE_BUCA_TAIL(Name, Base) class Name: Base { volumeCurve = "PredateBucaTailFalloff"; };

class CfgSoundSets
{
    class AK74_Tail_SoundSet;
    class AK74_silencerTail_SoundSet;
    class AKM_Tail_SoundSet;
    class AKM_silencerTail_SoundSet;
    class CZ61_Tail_SoundSet;
    class CZ61_silencerTail_SoundSet;
    class FAL_Tail_SoundSet;
    class FAL_silencerHomeMadeTail_SoundSet;
    class M4A1_Tail_SoundSet;
    class M4A1_silencerTail_SoundSet;
    class MP5K_Tail_SoundSet;
    class MP5K_silencerTail_SoundSet;
    class Mosin9130_Tail_SoundSet;
    class Mosin9130_silencerHomeMadeTail_SoundSet;
    class Mp133_Tail_SoundSet;
    class SV98_Tail_SoundSet;
    class SV98_silencerHomeMadeTail_SoundSet;
    class SVD_Tail_SoundSet;
    class SVD_silencerTail_SoundSet;
    class UMP45_Tail_SoundSet;
    class UMP45_silencerTail_SoundSet;
    class VSS_silencerTail_SoundSet;

    PRE_BUCA_TAIL(BC_Cal_9x19_Tail_SoundSet, MP5K_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_9x19_SilencerTail_SoundSet, MP5K_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_45ACP_Tail_SoundSet, UMP45_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_45ACP_SilencerTail_SoundSet, UMP45_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_46x30_Tail_SoundSet, CZ61_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_46x30_SilencerTail_SoundSet, CZ61_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_57x28_Tail_SoundSet, MP5K_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_57x28_SilencerTail_SoundSet, MP5K_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_556x45_Tail_SoundSet, M4A1_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_556x45_SilencerTail_SoundSet, M4A1_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_545x39_Tail_SoundSet, AK74_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_545x39_SilencerTail_SoundSet, AK74_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_762x39_Tail_SoundSet, AKM_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_762x39_SilencerTail_SoundSet, AKM_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_300BO_Tail_SoundSet, M4A1_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_300BO_SilencerTail_SoundSet, M4A1_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_50Beowulf_Tail_SoundSet, FAL_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_50Beowulf_SilencerTail_SoundSet, FAL_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_68x51_Tail_SoundSet, FAL_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_68x51_SilencerTail_SoundSet, FAL_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_308_Tail_SoundSet, FAL_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_308_SilencerTail_SoundSet, FAL_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_762x54_Tail_SoundSet, SVD_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_762x54_SilencerTail_SoundSet, SVD_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_792x57_Tail_SoundSet, Mosin9130_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_792x57_SilencerTail_SoundSet, Mosin9130_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_9x39_Tail_SoundSet, AKM_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_9x39_SilencerTail_SoundSet, VSS_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_127x55_Tail_SoundSet, SVD_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_127x55_SilencerTail_SoundSet, SVD_silencerTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_338_Tail_SoundSet, SV98_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_338_SilencerTail_SoundSet, SV98_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_408_Tail_SoundSet, SV98_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_408_SilencerTail_SoundSet, SV98_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_408AP_Tail_SoundSet, SV98_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_408AP_SilencerTail_SoundSet, SV98_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_50BMG_Tail_SoundSet, SV98_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_50BMG_SilencerTail_SoundSet, SV98_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_20mm_Tail_SoundSet, SV98_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_20mm_SilencerTail_SoundSet, SV98_silencerHomeMadeTail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_12ga_Tail_SoundSet, Mp133_Tail_SoundSet)
    PRE_BUCA_TAIL(BC_Cal_12ga_SilencerTail_SoundSet, Mosin9130_silencerHomeMadeTail_SoundSet)

    // Preserve the vanilla fallback's close, mid and mechanical layers.
    class FAL_silencerHomeMade_SoundSet;
    class SVD_silencer_SoundSet;
    class SVD_silencerHomeMade_SoundSet;
    class PredateBucaFALSuppressed: FAL_silencerHomeMade_SoundSet
    {
        soundShaders[] = {"FAL_silencerHomeMadeCloseShot_SoundShader", "FAL_silencerHomeMadeMidShot_SoundShader",
            "BC_M110_SilfarShot_SoundShader", "FAL_Mech_silencerHomeMade_SoundShader"};
    };
    class PredateBucaPSG1Suppressed: SVD_silencer_SoundSet
    {
        soundShaders[] = {"SVD_silencerCloseShot_SoundShader", "SVD_silencerMidShot_SoundShader",
            "BC_M110_SilfarShot_SoundShader", "SVD_Mech_silencerPro_SoundShader"};
    };
    class PredateBucaPSG1Improvised: SVD_silencerHomeMade_SoundSet
    {
        soundShaders[] = {"SVD_silencerHomeMadeCloseShot_SoundShader", "SVD_silencerHomeMadeMidShot_SoundShader",
            "BC_M110_SilfarShot_SoundShader", "SVD_Mech_silencerHomeMade_SoundShader"};
    };
    class PredateBucaPKMSuppressed: SVD_silencer_SoundSet
    {
        soundShaders[] = {"SVD_silencerCloseShot_SoundShader", "SVD_silencerMidShot_SoundShader",
            "PKP_SilfarShot_SoundShader", "SVD_Mech_silencerPro_SoundShader"};
    };
    class PredateBucaPKMImprovised: SVD_silencerHomeMade_SoundSet
    {
        soundShaders[] = {"SVD_silencerHomeMadeCloseShot_SoundShader", "SVD_silencerHomeMadeMidShot_SoundShader",
            "PKP_SilfarShot_SoundShader", "SVD_Mech_silencerHomeMade_SoundShader"};
    };
};

#undef PRE_BUCA_TAIL
