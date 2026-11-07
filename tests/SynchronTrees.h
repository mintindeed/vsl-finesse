#pragma once

// Articulation trees of Studio Special Edition presets, cut down from
// "Save instrument details" dumps of Synchron Player 1.1.0 (November 2026) to
// the fields the reader uses: the tree's titles, switches, selection and
// patches, each patch's velXFadeMode, and the preset's velXFadeEnabled.
namespace vslf::test::trees
{
// Studio Special Editions/01 Solo strings/01 VelXF sus - MOD/01 Violin I - VelXF sus
inline const char* violinVelXFSus =
    R"({"version":0,"data":{"currentPreset":"Studio Special Editions/01 Solo strings/01 VelXF sus - MOD/01 Violin I -)"
    R"( VelXF sus","parameters":{"velXFadeEnabled":1.0},"custom":{"sampler":{"rootNode":{"title":"","switchControl":2)"
    R"(5,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Short notes","switchControl":37,"whiteNote)"
    R"(Switch":true,"currentNodeIndex":1.0,"nodes":[{"title":"Staccato","patchName":"01 VI staccato","parameters":{"v)"
    R"(elXFadeMode":1}},{"title":"Detache","patchName":"03 VI detache","parameters":{"velXFadeMode":1}}]},{"title":"L)"
    R"(ong notes","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex":2.0,"nodes":[{"title":"Sustain","patc)"
    R"(hName":"04 VI sustain","parameters":{"velXFadeMode":0}},{"title":"Marcato","switchControl":257,"whiteNoteSwitc)"
    R"(h":false,"currentNodeIndex":0.97058,"nodes":[{"title":"Sustain","patchName":"04 VI sustain","parameters":{"vel)"
    R"(XFadeMode":0}},{"title":"Staccato","patchName":"01 VI staccato","parameters":{"velXFadeMode":1}}]},{"title":"X)"
    R"(F tremolo","switchControl":258,"whiteNoteSwitch":false,"currentNodeIndex":0.5,"nodes":[{"title":"Sustain","pat)"
    R"(chName":"04 VI sustain","parameters":{"velXFadeMode":0}},{"title":"Tremolo","patchName":"05 VI tremolo","param)"
    R"(eters":{"velXFadeMode":0}}]}]},{"title":"Legato","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex")"
    R"(:1.0,"nodes":[{"title":"Legato","patchName":"11 VI legato","parameters":{"velXFadeMode":0}},{"title":"Portamen)"
    R"(to","patchName":"12 VI portamento","parameters":{"velXFadeMode":0}}]},{"title":"Dynamics","switchControl":37,")"
    R"(whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Sforzato","patchName":"06 VI sfz","parameters)"
    R"(":{"velXFadeMode":0}},{"title":"Sfz XF tremolo","switchControl":258,"whiteNoteSwitch":false,"currentNodeIndex")"
    R"(:0.5,"nodes":[{"title":"Sforzato","patchName":"06 VI sfz","parameters":{"velXFadeMode":0}},{"title":"Tremolo",)"
    R"("patchName":"05 VI tremolo","parameters":{"velXFadeMode":0}}]}]},{"title":"Tremolo","switchControl":37,"whiteN)"
    R"(oteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Tremolo","patchName":"05 VI tremolo","parameters":{)"
    R"("velXFadeMode":0}},{"title":"Trem. marcato","switchControl":257,"whiteNoteSwitch":false,"currentNodeIndex":0.9)"
    R"(7058,"nodes":[{"title":"Tremolo","patchName":"05 VI tremolo","parameters":{"velXFadeMode":0}},{"title":"Stacca)"
    R"(to","patchName":"01 VI staccato","parameters":{"velXFadeMode":1}}]}]},{"title":"Pizzicato","patchName":"08 VI )"
    R"(pizz","parameters":{"velXFadeMode":1}},{"title":"Custom","switchControl":37,"whiteNoteSwitch":false,"currentNo)"
    R"(deIndex":2.0,"nodes":[{"title":"Custom 1","patchName":"","parameters":{"velXFadeMode":0}},{"title":"Custom 2",)"
    R"("patchName":"","parameters":{"velXFadeMode":0}},{"title":"Custom 3","patchName":"","parameters":{"velXFadeMode)"
    R"(":0}},{"title":"Custom 4","patchName":"","parameters":{"velXFadeMode":0}}]}]}}}}})"
    ;

// Studio Special Editions/01 Solo strings/03 Velocity/01 Violin I PLUS
inline const char* violinPlusVelocity =
    R"({"version":0,"data":{"currentPreset":"Studio Special Editions/01 Solo strings/03 Velocity/01 Violin I PLUS","p)"
    R"(arameters":{"velXFadeEnabled":0.0},"custom":{"sampler":{"rootNode":{"title":"","switchControl":25,"whiteNoteSw)"
    R"(itch":false,"currentNodeIndex":3.0,"nodes":[{"title":"Short notes","switchControl":37,"whiteNoteSwitch":false,)"
    R"("currentNodeIndex":1.0,"nodes":[{"title":"Staccato","patchName":"01 VI staccato","parameters":{"velXFadeMode":)"
    R"(0}},{"title":"Detache short","patchName":"02 VI detache short","parameters":{"velXFadeMode":0}},{"title":"Deta)"
    R"(che","patchName":"03 VI detache","parameters":{"velXFadeMode":0}},{"title":"Legato rep.","patchName":"21 VI pe)"
    R"(rf-rep legato","parameters":{"velXFadeMode":0}},{"title":"Spiccato rep.","patchName":"22 VI perf-rep spiccato")"
    R"(,"parameters":{"velXFadeMode":0}}]},{"title":"Long notes","switchControl":37,"whiteNoteSwitch":false,"currentN)"
    R"(odeIndex":1.0,"nodes":[{"title":"Sustain","patchName":"04 VI sustain","parameters":{"velXFadeMode":0}},{"title)"
    R"(":"Marcato","switchControl":257,"whiteNoteSwitch":false,"currentNodeIndex":0.97058,"nodes":[{"title":"Sustain")"
    R"(,"patchName":"04 VI sustain","parameters":{"velXFadeMode":0}},{"title":"Staccato","patchName":"01 VI staccato")"
    R"(,"parameters":{"velXFadeMode":0}}]},{"title":"XF tremolo","switchControl":258,"whiteNoteSwitch":false,"current)"
    R"(NodeIndex":0.5,"nodes":[{"title":"Sustain","patchName":"04 VI sustain","parameters":{"velXFadeMode":0}},{"titl)"
    R"(e":"Tremolo","patchName":"05 VI tremolo","parameters":{"velXFadeMode":0}}]}]},{"title":"Legato","switchControl)"
    R"(":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Legato","patchName":"11 VI legato","par)"
    R"(ameters":{"velXFadeMode":0}},{"title":"Portamento","patchName":"12 VI portamento","parameters":{"velXFadeMode")"
    R"(:0}}]},{"title":"Dynamics","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title")"
    R"(:"Sforzato","patchName":"06 VI sfz","parameters":{"velXFadeMode":0}},{"title":"Sfz XF tremolo","switchControl")"
    R"(:258,"whiteNoteSwitch":false,"currentNodeIndex":0.5,"nodes":[{"title":"Sforzato","patchName":"06 VI sfz","para)"
    R"(meters":{"velXFadeMode":0}},{"title":"Tremolo","patchName":"05 VI tremolo","parameters":{"velXFadeMode":0}}]},)"
    R"({"title":"Fortepiano","patchName":"07 VI fp","parameters":{"velXFadeMode":0}},{"title":"Fp XF tremolo","switch)"
    R"(Control":258,"whiteNoteSwitch":false,"currentNodeIndex":0.5,"nodes":[{"title":"Fortepiano","patchName":"07 VI )"
    R"(fp","parameters":{"velXFadeMode":0}},{"title":"Tremolo","patchName":"05 VI tremolo","parameters":{"velXFadeMod)"
    R"(e":0}}]}]},{"title":"Tremolo + Trills","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"node)"
    R"(s":[{"title":"Tremolo","patchName":"05 VI tremolo","parameters":{"velXFadeMode":0}},{"title":"Trem. marcato",")"
    R"(switchControl":257,"whiteNoteSwitch":false,"currentNodeIndex":0.97058,"nodes":[{"title":"Tremolo","patchName":)"
    R"("05 VI tremolo","parameters":{"velXFadeMode":0}},{"title":"Staccato","patchName":"01 VI staccato","parameters")"
    R"(:{"velXFadeMode":0}}]},{"title":"Half tone trill","patchName":"15 VI trill_1","parameters":{"velXFadeMode":0}})"
    R"(,{"title":"Whole tone trill","patchName":"16 VI trill_2","parameters":{"velXFadeMode":0}},{"title":"Fast rep. )"
    R"(150","patchName":"25 VI fast-rep BPM-150","parameters":{"velXFadeMode":0}},{"title":"Fast rep. 170","patchName)"
    R"(":"26 VI fast-rep BPM-170","parameters":{"velXFadeMode":0}},{"title":"Fast rep. 190","patchName":"27 VI fast-r)"
    R"(ep BPM-190","parameters":{"velXFadeMode":0}}]},{"title":"Pizzicato + Legno","switchControl":37,"whiteNoteSwitc)"
    R"(h":false,"currentNodeIndex":1.0,"nodes":[{"title":"Pizzicato","patchName":"08 VI pizz","parameters":{"velXFade)"
    R"(Mode":0}},{"title":"Snap pizzicato","patchName":"09 VI pizz-snap","parameters":{"velXFadeMode":0}},{"title":"C)"
    R"(ol legno","patchName":"10 VI col-legno","parameters":{"velXFadeMode":0}}]},{"title":"Harmonics","switchControl)"
    R"(":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Harmonics stac.","patchName":"31 VI har)"
    R"(m-art staccato","parameters":{"velXFadeMode":0}},{"title":"Harmonics","patchName":"32 VI harm-art sustain","pa)"
    R"(rameters":{"velXFadeMode":0}},{"title":"Harm. marcato","switchControl":257,"whiteNoteSwitch":false,"currentNod)"
    R"(eIndex":0.97058,"nodes":[{"title":"Harmonics","patchName":"32 VI harm-art sustain","parameters":{"velXFadeMode)"
    R"(":0}},{"title":"Harmonics stac.","patchName":"31 VI harm-art staccato","parameters":{"velXFadeMode":0}}]}]},{")"
    R"(title":"Sul ponticello","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"S)"
    R"(taccato","patchName":"41 VI pon staccato","parameters":{"velXFadeMode":0}},{"title":"Sustain","patchName":"42 )"
    R"(VI pon sustain","parameters":{"velXFadeMode":0}},{"title":"Marcato","switchControl":257,"whiteNoteSwitch":fals)"
    R"(e,"currentNodeIndex":0.97058,"nodes":[{"title":"Sustain","patchName":"42 VI pon sustain","parameters":{"velXFa)"
    R"(deMode":0}},{"title":"Staccato","patchName":"41 VI pon staccato","parameters":{"velXFadeMode":0}}]},{"title":")"
    R"(Tremolo","patchName":"43 VI pon tremolo","parameters":{"velXFadeMode":0}}]},{"title":"Custom","switchControl":)"
    R"(37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Custom 1","patchName":"","parameters":{"v)"
    R"(elXFadeMode":0}},{"title":"Custom 2","patchName":"","parameters":{"velXFadeMode":0}},{"title":"Custom 3","patc)"
    R"(hName":"","parameters":{"velXFadeMode":0}},{"title":"Custom 4","patchName":"","parameters":{"velXFadeMode":0}})"
    R"(]}]}}}}})"
    ;

// Studio Special Editions/03 Orchestral strings/02 VelXF - MOD/05 Double basses 6 - VelXF
inline const char* doubleBassesVelXF =
    R"({"version":0,"data":{"currentPreset":"Studio Special Editions/03 Orchestral strings/02 VelXF - MOD/05 Double b)"
    R"(asses 6 - VelXF","parameters":{"velXFadeEnabled":1.0},"custom":{"sampler":{"rootNode":{"title":"","switchContr)"
    R"(ol":85,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Short notes","switchControl":97,"whit)"
    R"(eNoteSwitch":true,"currentNodeIndex":0.0,"nodes":[{"title":"Staccato","patchName":"01 DB-6 staccato","paramete)"
    R"(rs":{"velXFadeMode":0}},{"title":"Detache","patchName":"03 DB-6 detache","parameters":{"velXFadeMode":0}}]},{")"
    R"(title":"Long notes","switchControl":97,"whiteNoteSwitch":false,"currentNodeIndex":0.0,"nodes":[{"title":"Susta)"
    R"(in","patchName":"04 DB-6 sustain","parameters":{"velXFadeMode":0}},{"title":"Marcato","switchControl":257,"whi)"
    R"(teNoteSwitch":false,"currentNodeIndex":0.97058,"nodes":[{"title":"Sustain","patchName":"04 DB-6 sustain","para)"
    R"(meters":{"velXFadeMode":0}},{"title":"Staccato","patchName":"01 DB-6 staccato","parameters":{"velXFadeMode":0})"
    R"(}]},{"title":"XF tremolo","switchControl":258,"whiteNoteSwitch":false,"currentNodeIndex":0.5,"nodes":[{"title")"
    R"(:"Sustain","patchName":"04 DB-6 sustain","parameters":{"velXFadeMode":0}},{"title":"Tremolo","patchName":"05 D)"
    R"(B-6 tremolo","parameters":{"velXFadeMode":0}}]}]},{"title":"Legato","switchControl":97,"whiteNoteSwitch":false)"
    R"(,"currentNodeIndex":0.0,"nodes":[{"title":"Legato","patchName":"11 DB-6 legato","parameters":{"velXFadeMode":0)"
    R"(}},{"title":"Portamento","patchName":"12 DB-6 portamento","parameters":{"velXFadeMode":0}}]},{"title":"Dynamic)"
    R"(s","switchControl":97,"whiteNoteSwitch":false,"currentNodeIndex":0.0,"nodes":[{"title":"Sforzato","patchName":)"
    R"("06 DB-6 sfz","parameters":{"velXFadeMode":0}},{"title":"Sfz XF tremolo","switchControl":258,"whiteNoteSwitch")"
    R"(:false,"currentNodeIndex":0.5,"nodes":[{"title":"Sforzato","patchName":"06 DB-6 sfz","parameters":{"velXFadeMo)"
    R"(de":0}},{"title":"Tremolo","patchName":"05 DB-6 tremolo","parameters":{"velXFadeMode":0}}]}]},{"title":"Tremol)"
    R"(o","switchControl":97,"whiteNoteSwitch":false,"currentNodeIndex":0.5,"nodes":[{"title":"Tremolo","patchName":")"
    R"(05 DB-6 tremolo","parameters":{"velXFadeMode":0}},{"title":"Trem. marcato","switchControl":257,"whiteNoteSwitc)"
    R"(h":false,"currentNodeIndex":0.97058,"nodes":[{"title":"Tremolo","patchName":"05 DB-6 tremolo","parameters":{"v)"
    R"(elXFadeMode":0}},{"title":"Staccato","patchName":"01 DB-6 staccato","parameters":{"velXFadeMode":0}}]}]},{"tit)"
    R"(le":"Pizzicato","patchName":"08 DB-6 pizz","parameters":{"velXFadeMode":0}},{"title":"Custom","switchControl":)"
    R"(97,"whiteNoteSwitch":false,"currentNodeIndex":0.0,"nodes":[{"title":"Custom 1","patchName":"","parameters":{"v)"
    R"(elXFadeMode":0}},{"title":"Custom 2","patchName":"","parameters":{"velXFadeMode":0}},{"title":"Custom 3","patc)"
    R"(hName":"","parameters":{"velXFadeMode":0}},{"title":"Custom 4","patchName":"","parameters":{"velXFadeMode":0}})"
    R"(]}]}}}}})"
    ;

// Studio Special Editions/31 Percussion/01 VelXF rolls - MOD/01 Timpani - VelXF rolls
inline const char* timpaniVelXFRolls =
    R"({"version":0,"data":{"currentPreset":"Studio Special Editions/31 Percussion/01 VelXF rolls - MOD/01 Timpani - )"
    R"(VelXF rolls","parameters":{"velXFadeEnabled":1.0},"custom":{"sampler":{"rootNode":{"title":"","switchControl":)"
    R"(25,"whiteNoteSwitch":false,"currentNodeIndex":0.0,"nodes":[{"title":"Single hits","patchName":"01 Timpani Sing)"
    R"(le-Hits","parameters":{"velXFadeMode":1}},{"title":"Secco","patchName":"02 Timpani Single-Hits secco","paramet)"
    R"(ers":{"velXFadeMode":1}},{"title":"Rolls","patchName":"03 Timpani  roll","parameters":{"velXFadeMode":0}}]}}}})"
    R"(})"
    ;

// Studio Special Editions/06 Plucked instruments/01 Harp I
inline const char* harp =
    R"({"version":0,"data":{"currentPreset":"Studio Special Editions/06 Plucked instruments/01 Harp I","parameters":{)"
    R"("velXFadeEnabled":0.0},"custom":{"sampler":{"rootNode":{"title":"","switchControl":22,"whiteNoteSwitch":false,)"
    R"("currentNodeIndex":1.0,"nodes":[{"title":"Regular","patchName":"01 HA regular","parameters":{"velXFadeMode":0})"
    R"(},{"title":"Harmonics","patchName":"02 HA harmonics","parameters":{"velXFadeMode":0}}]}}}}})"
    ;

} // namespace vslf::test::trees
