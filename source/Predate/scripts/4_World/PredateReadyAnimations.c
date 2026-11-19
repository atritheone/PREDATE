// Root-motion distances derived from matching installed lowered-weapon clips.
// Keep animation timing and inputs while matching the lowered-weapon distances.
class PredateReadyAnimations
{
    static bool s_Applied;

    static void Apply()
    {
        if (s_Applied) return;
        s_Applied = true;
        int applied;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_runFL_ras.anm", "-1.58752483 0 1.58757328")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_runFR_ras.anm", "1.58754905 0 1.58754905")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_runF_ras.anm", "0 0 2.24513340")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_walkBL_ras.anm", "-1.35221736 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_walkBR_ras.anm", "1.34575464 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_walkB_ras.anm", "0 0 -1.32833862")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_walkFL_ras.anm", "-1.47325468 0 0.00002248")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_walkFR_ras.anm", "1.47528020 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_cro_walkF_ras.anm", "0 0 1.54275319")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_runBL_ras.anm", "-2.44180835 0 -0.00003726")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_runBR_ras.anm", "2.55828022 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_runB_ras.anm", "0.00003672 0 -2.40634258")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_runFL_ras.anm", "-2.55824118 0 0.00003904")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_runFR_ras.anm", "2.44180835 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_runF_ras.anm", "0 0 2.92213113")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_walkBL_ras.anm", "-4.98232150 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_walkBR_ras.anm", "4.97849728 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_walkB_ras.anm", "0 0 -4.02922776")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_walkFL_ras.anm", "-4.97849728 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_walkFR_ras.anm", "4.98232150 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/rifles/p_rfl_erc_walkF_ras.anm", "0 0 5.15189141")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_runFL_ras.anm", "-1.77341807 0 1.77341807")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_runFR_ras.anm", "1.77344545 0 1.77341839")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_runF_ras.anm", "0 0 2.50803010")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_walkBL_ras.anm", "-1.46340479 0 -0.00013411")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_walkBR_ras.anm", "1.40517059 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_walkB_ras.anm", "0.00006081 0 -1.32832907")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_walkFL_ras.anm", "-1.44775152 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_walkFR_ras.anm", "1.50775830 0 0.00011503")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_cro_walkF_ras.anm", "0 0 1.53855387")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_runBL_ras.anm", "-3.26682245 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_runBR_ras.anm", "3.26645697 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_runB_ras.anm", "0 0 -2.38539124")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_runFL_ras.anm", "-3.26645697 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_runFR_ras.anm", "3.26682245 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_runF_ras.anm", "0 0 3.26995050")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_walkBL_ras.anm", "-5.52608461 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_walkBR_ras.anm", "5.52352050 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_walkB_ras.anm", "-0.00007224 0 -4.73455949")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_walkFL_ras.anm", "-5.52352050 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_walkFR_ras.anm", "5.52608461 0 0")) applied++;
        if (DayZPlayerUtils.DebugOverrideAnimationTranslation("dz/anims/anm/player/moves/pistols/p_pst_erc_walkF_ras.anm", "0 0 5.71350163")) applied++;
        Print("[Predate] Ready movement root motion: " + applied + "/42 clips matched to lowered-weapon travel speed.");
    }
}
