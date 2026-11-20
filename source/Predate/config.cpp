class CfgPatches
{
    class Predate_Prototype
    {
        units[]={"PredateWeaponModule","PredatePanoramicNVG","PredateOperatorNVG_Black","PredateOperatorNVG_Tan","PredateOperatorHeadstrap_Black","PredateOperatorHeadstrap_Tan","PredateChinos_Brown"}; weapons[]={}; requiredVersion=0.1;
        requiredAddons[]={"DZ_Data","DZ_Characters_Glasses","DZ_Characters_Pants","DZ_Weapons_Lights","DZ_Worlds_Chernarusplus_World","DayZExpansion_AI_Scripts"};
    };
};
class CfgVehicles
{
    class CanvasPants_Beige;
    class PredateChinos_Brown: CanvasPants_Beige
    {
        displayName="Brown Chinos";
        hiddenSelectionsTextures[]={
            "#(argb,8,8,3)color(0.38,0.26,0.16,1,co)",
            "#(argb,8,8,3)color(0.38,0.26,0.16,1,co)",
            "#(argb,8,8,3)color(0.38,0.26,0.16,1,co)"
        };
    };
    class NVGoggles;
    class PredatePanoramicNVG: NVGoggles
    {
        scope=2;
        displayName="Panoramic Night Vision Goggles";
        descriptionShort="White-phosphor panoramic view. Uses a 9V battery.";
    };
    class PredateOperatorNVG_Black: PredatePanoramicNVG
    {
        scope=1;
        hiddenSelections[]={"hide"};
        hiddenSelectionsTextures[]={"#(argb,8,8,3)color(0.09,0.09,0.09,1,co)"};
    };
    class PredateOperatorNVG_Tan: PredateOperatorNVG_Black
    {
        hiddenSelectionsTextures[]={"#(argb,8,8,3)color(0.49,0.37,0.23,1,co)"};
    };
    class NVGHeadstrap;
    class PredateOperatorHeadstrap_Black: NVGHeadstrap
    {
        scope=1;
        displayName="NVG Headstrap (Black)";
        hiddenSelections[]={"camoGround","camoMale","camoFemale"};
        hiddenSelectionsTextures[]={"#(argb,8,8,3)color(0.09,0.09,0.09,1,co)","#(argb,8,8,3)color(0.09,0.09,0.09,1,co)","#(argb,8,8,3)color(0.09,0.09,0.09,1,co)"};
    };
    class PredateOperatorHeadstrap_Tan: PredateOperatorHeadstrap_Black
    {
        displayName="NVG Headstrap (Tan)";
        hiddenSelectionsTextures[]={"#(argb,8,8,3)color(0.49,0.37,0.23,1,co)","#(argb,8,8,3)color(0.49,0.37,0.23,1,co)","#(argb,8,8,3)color(0.49,0.37,0.23,1,co)"};
    };
    class UniversalLight;
    class PredateWeaponModule: UniversalLight
    {
        scope=2;
        displayName="Multimode Weapon Light";
        descriptionShort="White light, visible laser, IR laser and IR illuminator. Uses a 9V battery.";
        inventorySlot[]={"weaponFlashlight"};
    };
};
class CfgWorlds
{
    class CAWorld;
    class ChernarusPlus: CAWorld
    {
        class Names
        {
            // Chernarus location names in Latin script. The terrain supplies the coordinates and map symbols.
            class Settlement_Chernogorsk { name="Chernogorsk"; };
            class Settlement_Novodmitrovsk { name="Novodmitrovsk"; };
            class Settlement_Novoselki { name="Novoselki"; };
            class Settlement_Dubovo { name="Dubovo"; };
            class Settlement_Vysotovo { name="Vysotovo"; };
            class Settlement_Zelenogorsk { name="Zelenogorsk"; };
            class Settlement_Berezino { name="Berezino"; };
            class Settlement_Elektrozavodsk { name="Elektrozavodsk"; };
            class Settlement_NovayaPetrovka { name="Novaya Petrovka"; };
            class Settlement_Gorka { name="Gorka"; };
            class Settlement_Solnechny { name="Solnechny"; };
            class Settlement_StarySobor { name="Stary Sobor"; };
            class Settlement_Vybor { name="Vybor"; };
            class Settlement_Severograd { name="Severograd"; };
            class Settlement_Bor { name="Bor"; };
            class Settlement_Svetloyarsk { name="Svetloyarsk"; };
            class Settlement_Krasnostav { name="Krasnostav"; };
            class Settlement_ChernayaPolyana { name="Chernaya Polyana"; };
            class Settlement_Polyana { name="Polyana"; };
            class Settlement_Tulga { name="Tulga"; };
            class Settlement_Msta { name="Msta"; };
            class Settlement_Staroye { name="Staroye"; };
            class Settlement_Shakhovka { name="Shakhovka"; };
            class Settlement_Dolina { name="Dolina"; };
            class Settlement_Orlovets { name="Orlovets"; };
            class Settlement_NovySobor { name="Novy Sobor"; };
            class Settlement_Kabanino { name="Kabanino"; };
            class Settlement_Mogilevka { name="Mogilevka"; };
            class Settlement_Nadezhdino { name="Nadezhdino"; };
            class Settlement_Guglovo { name="Guglovo"; };
            class Settlement_Kamyshovo { name="Kamyshovo"; };
            class Settlement_Pusta { name="Pusta"; };
            class Settlement_Dubrovka { name="Dubrovka"; };
            class Settlement_VyshnayaDubrovka { name="Verkhnyaya Dubrovka"; };
            class Settlement_Khelm { name="Khelm"; };
            class Settlement_Olsha { name="Olsha"; };
            class Settlement_Gvozdno { name="Gvozdno"; };
            class Settlement_Grishino { name="Grishino"; };
            class Settlement_Rogovo { name="Rogovo"; };
            class Settlement_Pogorevka { name="Pogorevka"; };
            class Settlement_Pustoshka { name="Pustoshka"; };
            class Settlement_Kozlovka { name="Kozlovka"; };
            class Settlement_Karmanovka { name="Karmanovka"; };
            class Settlement_Balota { name="Balota"; };
            class Settlement_Komarovo { name="Komarovo"; };
            class Settlement_Kamenka { name="Kamenka"; };
            class Settlement_Myshkino { name="Myshkino"; };
            class Settlement_Pavlovo { name="Pavlovo"; };
            class Settlement_Lopatino { name="Lopatino"; };
            class Settlement_Vyshnoye { name="Vyshnoye"; };
            class Settlement_Prigorodki { name="Prigorodki"; };
            class Settlement_Drozhino { name="Drozhino"; };
            class Settlement_Sosnovka { name="Sosnovka"; };
            class Settlement_Nizhneye { name="Nizhneye"; };
            class Settlement_Pulkovo { name="Pulkovo"; };
            class Settlement_Berezhki { name="Berezhki"; };
            class Settlement_Turovo { name="Turovo"; };
            class Settlement_BelayaPolyana { name="Belaya Polyana"; };
            class Settlement_Dobroye { name="Dobroye"; };
            class Settlement_Nagornoye { name="Nagornoye"; };
            class Settlement_Svergino { name="Svergino"; };
            class Settlement_Ratnoye { name="Ratnoye"; };
            class Settlement_Kamensk { name="Kamensk"; };
            class Settlement_Krasnoye { name="Krasnoye"; };
            class Settlement_StaryYar { name="Stary Yar"; };
            class Settlement_Polesovo { name="Polesovo"; };
            class Settlement_Tisy { name="Tisy"; };
            class Settlement_Topolniki { name="Topolniki"; };
            class Settlement_Zaprudnoye { name="Zaprudnoye"; };
            class Settlement_Sinystok { name="Sinystok"; };
            class Settlement_Vavilovo { name="Vavilovo"; };
            class Settlement_Kumyrna { name="Kumyrna"; };
            class Settlement_Kalinovka { name="Kalinovka"; };
            class Settlement_Bogatyrka { name="Bogatyrka"; };
            class Settlement_SvyatoyRomanSkiResort { name="St. Roman Ski Resort"; };
            class Settlement_SKVSChBiathlonArena { name="SKVSCh Biathlon Arena"; };
            class Settlement_Zvir { name="Zvir"; };
            class Settlement_Zabolotye { name="Zabolotye"; };
            class Settlement_Galkino { name="Galkino"; };
            class Camp_Arsenovo { name="Arsenevo"; };
            class Camp_Stroytel { name="Stroitel"; };
            class Camp_Romashka { name="Romashka"; };
            class Camp_Kometa { name="Kometa"; };
            class Camp_Druzhba { name="Druzhba"; };
            class Camp_Nadezhda { name="Nadezhda"; };
            class Camp_YouthPioneer { name="Young Pioneer Camp"; };
            class Camp_ProudChernarus { name="Proud Chernarus Camp"; };
            class Camp_Shkolnik { name="Shkolnik"; };
            class Camp_Pobeda { name="Pobeda"; };
            class Camp_Metalurg { name="Metallurg"; };
            class Hill_Zelenayagora { name="Zelenaya Gora"; };
            class Local_Dichina { name="Dishina"; };
            class Local_Novylug { name="Novyy Lug"; };
            class Local_Staryeluga { name="Starye Luga"; };
            class Hill_Vysota { name="Vysota"; };
            class Hill_Kopyto { name="Kopyto"; };
            class Local_Grubyeskaly { name="Grubye Skaly"; };
            class Local_Chernyles { name="Chernyy Les"; };
            class Hill_Altar { name="Altar"; };
            class Local_RadioZenit { name="Radio Zenit"; };
            class Hill_PikKozlova { name="Pik Kozlova"; };
            class Local_Pustoykhrebet { name="Pustoy Khrebet"; };
            class Hill_Bashnya { name="Bashnya"; };
            class Hill_Veresnik { name="Veresnik"; };
            class Hill_Kurgan { name="Kurgan"; };
            class Hill_Kustryk { name="Kustryk"; };
            class Hill_Vetryanayagora { name="Vetrenaya Gora"; };
            class Hill_Kalmyk { name="Kalmyk"; };
            class Hill_PopIvan { name="Pop Ivan"; };
            class Hill_Erbenka { name="Erbenka"; };
            class Local_Lesnoykhrebet { name="Lesnoy Khrebet"; };
            class Hill_Vysokiykamen { name="Vysokiy Kamen"; };
            class Hill_Dobry { name="Dobryy"; };
            class Hill_Baranchik { name="Baranchik"; };
            class Hill_Malinovka { name="Malinovka"; };
            class Hill_Dubina { name="Dubina"; };
            class Hill_Klen { name="Klen"; };
            class Hill_Chernayagora { name="Chernaya Gora"; };
            class Ruin_Zolotar { name="Zolotar"; };
            class Hill_Ostry { name="Ostryy"; };
            class Hill_Olsha { name="Olsha"; };
            class Marine_Tikhiyzaliv { name="Tikhiy Zaliv"; };
            class Marine_Mutnyizaliv { name="Mutnyy Zaliv"; };
            class Marine_Chernyzaliv { name="Chernyy Zaliv"; };
            class Marine_Zelenyzaliv { name="Zelenyy Zaliv"; };
            class Marine_Skalistyproliv { name="Skalisty Strait"; };
            class Marine_Nizhniyzaliv { name="Nizhniy Zaliv"; };
            class Marine_ZalivGuba { name="Zaliv Guba"; };
            class Marine_Rify { name="Rify"; };
            class Marine_Ivovoyeozero { name="Willow Lake"; };
            class Marine_Chernoyeozero { name="Black Lake"; };
            class Marine_PlotinaTopolka { name="Topolka Dam"; };
            class Marine_PlotinaPobeda { name="Pobeda Dam"; };
            class Marine_PlotinaTishina { name="Tishina Dam"; };
            class Marine_Ozerko { name="Ozerko"; };
            class Marine_Prud { name="Prud"; };
            class Ruin_Chortovzamok { name="Chertov Zamok"; };
            class Ruin_Zub { name="Zub"; };
            class Ruin_Rog { name="Rog"; };
            class Local_Grozovypereval { name="Grozovoy Pereval"; };
            class Local_Sosnovypereval { name="Sosnovyy Pereval"; };
            class Local_PerevalOreshka { name="Pereval Oreshka"; };
            class Local_Turovskiypereval { name="Turovskiy Pereval"; };
            class Local_Tridoliny { name="Tri Doliny"; };
            class Local_Grozovyedoly { name="Grozovye Doli"; };
            class Ruin_Klyuch { name="Klyuch"; };
            class Hill_Lysayagora { name="Lysaya Gora"; };
            class Marine_Glubokoyeozero { name="Deep Lake"; };
            class Local_Skalka { name="Skalka"; };
            class Local_Vidy { name="Vidy"; };
            class Hill_Tumannyverkh { name="Tumannyy Verkh"; };
            class Local_Adamovka { name="Adamovka"; };
            class Hill_Shishok { name="Shishok"; };
            class Settlement_Skalisty { name="Skalisty"; };
            class Ruin_Storozh { name="Storozh"; };
            class Local_MysGolova { name="Mys Golova"; };
            class Local_Drakon { name="Drakon"; };
            class Local_Otmel { name="Otmel"; };
            class Local_MysKrutoy { name="Mys Krutoy"; };
            class Hill_Tokarnya { name="Tokarnya"; };
            class Hill_Ostrog { name="Ostrog"; };
            class Local_Maryanka { name="Maryanka"; };
            class Local_Polonina { name="Polonina"; };
            class Local_Kalinka { name="Kalinka"; };
            class Hill_Kikimora { name="Kikimora"; };
            class Hill_BolshoyKotel { name="Bolshoy Kotel"; };
            class Hill_Simurg { name="Simurg"; };
            class Hill_Volchiypik { name="Volchiy Pik"; };
            class Ruin_Krona { name="Krona"; };
            class Local_TriKresta { name="Tri Kresta"; };
            class Local_Rostoki { name="Rostoki"; };
            class Marine_OrlyeOzero { name="Eagle Lake"; };
            class Local_Makosh { name="Makosh"; };
            class Local_Klenovyipereval { name="Klenovyy Pereval"; };
            class Local_Zmeinykhrebet { name="Zmeinyy Khrebet"; };
            class Hill_Sokol { name="Sokol"; };
            class Local_Krutyeskaly { name="Krutye Skaly"; };
            class Local_Bogat { name="Bogat"; };
            class Local_Dubnik { name="Dubnik"; };
            class Hill_Viselitsa { name="Viselitsa"; };
            class Local_Dazhbog { name="Dazhbog"; };
            class Marine_Verbnik { name="Verbnik"; };
            class Local_Medvezhilugi { name="Medvezhi Luga"; };
            class Ruin_Voron { name="Voron"; };
            class Ruin_Gnomovzamok { name="Gnomskiy Zamok"; };
            class Marine_Glaza { name="Glaza"; };
            class Local_KarerKrasnayaZarya { name="Krasnaya Zarya Quarry"; };
            class Local_Matveyevo { name="Matveevo"; };
            class Local_Kotka { name="Kotka"; };
            class Local_Chernyeskaly { name="Chernye Skaly"; };
            class Hill_SvyatoyRoman { name="Svyatoy Roman"; };
            class Hill_Koman { name="Koman"; };
            class Hill_Mayak { name="Mayak"; };
            class Local_MB_VMC { name="Vybor Military Base"; };
            class Local_MB_Tisy { name="Tisy Military Base"; };
            class Local_MB_Kamensk { name="Kamensk Military Base"; };
            class Local_MB_Zeleno { name="Zelenogorsk Military Base"; };
            class Local_MB_Pavlovo { name="Pavlovo Military Base"; };
            class Local_AF_Balota { name="Balota Airfield"; };
            class Local_AF_Krasno { name="Krasnostav Airfield"; };
            class Local_AF_Vybor { name="Northwest Airfield"; };
        };
    };
};
class CfgMods
{
    class Predate
    {
        dir="Predate"; name="PREDATE - First Hunt"; author="PREDATE contributors";
        type="mod"; inputs="Predate/inputs.xml";
        dependencies[]={"Game","World","Mission"};
        class defs
        {
            class gameScriptModule { value=""; files[]={"Predate/scripts/3_Game"}; };
            class worldScriptModule { value=""; files[]={"Predate/scripts/4_World"}; };
            class missionScriptModule { value=""; files[]={"Predate/scripts/5_Mission"}; };
        };
    };
};
