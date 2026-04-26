//MIDI CC control numbers
//These broadly follow standard CC assignments

// NRPN values

#define CCdco1_range      0x00
#define CCdco1_wave       0x01
#define CCdco1_tune       0x02
#define CCdco1_pitch_lfo  0x03
#define CCdco1_pitch_lfo_source 0x04
#define CCdco1_pitch_env  0x05
#define CCdco1_pitch_dyn  0x06
#define CCdco1_pitch_env_source 0x07

#define CCdco2_range      0x08
#define CCdco2_wave       0x09
#define CCdco2_tune       0x0A
#define CCdco2_pitch_lfo  0x0B
#define CCdco2_pitch_lfo_source  0x0C
#define CCdco2_pitch_env  0x0D
#define CCdco2_pitch_dyn  0x0E
#define CCdco2_pitch_env_source  0x0F

#define CCdco1_xmod       0x10
#define CCdco2_fine       0x11
#define CCvcf_hpf         0x12
#define CCchorus_sw       0x13

#define CCdco1_PW         0x14
#define CCdco1_PWM_env    0x15
#define CCdco1_PWM_lfo    0x16
#define CCdco1_PWM_lfo_source  0x17
#define CCdco1_PWM_dyn    0x18
#define CCdco1_PWM_env_source  0x19

#define CCdco2_PW         0x1A
#define CCdco2_PWM_env    0x1B
#define CCdco2_PWM_lfo    0x1C
#define CCdco2_PWM_lfo_source  0x1D
#define CCdco2_PWM_dyn    0x1E
#define CCdco2_PWM_env_source  0x1F

#define CCdco1_level      0x20
#define CCdco2_level      0x21
#define CCdco2_mod        0x22
#define CCdco_mix_dyn     0x23
#define CCdco_mix_env_source 0x24

#define CCvcf_cutoff    0x25
#define CCvcf_res       0x26
#define CCvcf_lfo1      0x27
#define CCvcf_lfo2      0x28
#define CCvcf_env       0x29
#define CCvcf_kb        0x2A
#define CCvcf_dyn       0x2B
#define CCvcf_env_source  0x2C

#define CCvca_mod       0x2D
#define CCvca_env_source  0x2E
#define CCvca_dyn       0x2F

#define CClfo1_wave   0x30
#define CClfo1_delay  0x31
#define CClfo1_rate   0x32
#define CClfo1_lfo2   0x33
#define CClfo1_sync   0x34

#define CClfo2_wave   0x35
#define CClfo2_delay  0x36
#define CClfo2_rate   0x37
#define CClfo2_lfo1   0x38
#define CClfo2_sync   0x39

#define CCtime1          0x3A
#define CClevel1         0x3B
#define CCtime2          0x3C
#define CClevel2         0x3D
#define CCtime3          0x3E
#define CClevel3         0x3F
#define CCtime4          0x40
#define CC5stage_mode    0x41

#define CC2time1         0x42
#define CC2level1        0x43
#define CC2time2         0x44
#define CC2level2        0x45
#define CC2time3         0x46
#define CC2level3        0x47
#define CC2time4         0x48
#define CC25stage_mode   0x49

#define CCattack         0x4A
#define CCdecay          0x4B
#define CCsustain        0x4C
#define CCrelease        0x4D
#define CCadsr_mode      0x4E

#define CC4attack         0x4F
#define CC4decay          0x50
#define CC4sustain        0x51
#define CC4release        0x52
#define CC4adsr_mode      0x53

// Not defined as NRPN or sysex

#define CCdualdetune      0x54
#define CCunisondetune    0x55
#define CCbalance         0x56
#define CCat_vib          0x57
#define CCat_bri          0x58
#define CCat_vol          0x59
#define CCenv5stage       0x5A
#define CCadsr            0x5B
#define CCeditMode        0x5E
#define CCvolume          0x5F
#define CCportamento      0x60
#define CCportamento_sw   0x61
#define CCbend_range      0x62
#define CCmod_lfo         0x63
#define CCoctave_down     0x64
#define CCoctave_up       0x65
#define CCdual_button     0x66
#define CCsplit_button    0x67
#define CCsingle_button   0x68
#define CCspecial_button  0x69
#define CCpoly_button     0x6A
#define CCmono_button     0x6B
#define CCunison_button   0x6C
#define CCkeyMode         0x6D
#define CCbend_enable     0x6E
#define CCat_vib_sw       0x6F
#define CCat_bri_sw       0x70
#define CCat_vol_sw       0x71

#define CCallnotesoff 123//Panic button
