/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.8-dev */

#ifndef PB_VOICEDATAINTERNAL_PB_H_INCLUDED
#define PB_VOICEDATAINTERNAL_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Struct definitions */
typedef struct _VoiceDataInternal_Pattern {
    uint8_t rate;
    uint8_t length;
    uint8_t notes[64];
    uint8_t keys[64];
} VoiceDataInternal_Pattern;

typedef struct _VoiceDataInternal_EnvelopeData {
    uint8_t attack;
    uint8_t decay;
    uint8_t target;
    uint8_t depth;
} VoiceDataInternal_EnvelopeData;

typedef struct _VoiceDataInternal {
    uint32_t version;
    pb_callback_t locksForPattern;
    uint8_t instrumentType;
    uint8_t effectEditTarget;
    uint8_t delaySend;
    uint8_t reverbSend;
    bool has_env1;
    VoiceDataInternal_EnvelopeData env1;
    bool has_env2;
    VoiceDataInternal_EnvelopeData env2;
    uint8_t lfoRate;
    uint8_t lfoDepth;
    uint8_t lfoTarget;
    uint8_t lfoDelay;
    uint8_t sampleStart[16];
    uint8_t sampleLength[16];
    uint8_t color;
    uint8_t timbre;
    uint8_t conditionMode;
    uint8_t conditionData;
    uint8_t cutoff;
    uint8_t resonance;
    uint8_t volume;
    uint8_t pan;
    uint8_t retriggerSpeed;
    uint8_t retriggerLength;
    uint8_t retriggerFade;
    uint8_t retriggerTargetEnv;
    uint8_t octave;
    uint8_t portamento;
    uint8_t fineTune;
    pb_size_t which_extraTypeUnion;
    union {
        uint8_t synthShape;
        uint8_t samplerType;
        uint8_t midiChannel;
    } extraTypeUnion;
    uint8_t sampleAttack;
    uint8_t sampleDecay;
    /* these are per pattern */
    VoiceDataInternal_Pattern patterns[16];
} VoiceDataInternal;

typedef struct _VoiceDataInternal_LockPointer {
    uint8_t pattern;
    uint16_t pointer;
} VoiceDataInternal_LockPointer;


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define VoiceDataInternal_init_default           {0, {{NULL}, NULL}, 0, 0, 0, 0, false, VoiceDataInternal_EnvelopeData_init_default, false, VoiceDataInternal_EnvelopeData_init_default, 0, 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0}, 0, 0, {VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default, VoiceDataInternal_Pattern_init_default}}
#define VoiceDataInternal_Pattern_init_default   {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
#define VoiceDataInternal_EnvelopeData_init_default {0, 0, 0, 0}
#define VoiceDataInternal_LockPointer_init_default {0, 0}
#define VoiceDataInternal_init_zero              {0, {{NULL}, NULL}, 0, 0, 0, 0, false, VoiceDataInternal_EnvelopeData_init_zero, false, VoiceDataInternal_EnvelopeData_init_zero, 0, 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0}, 0, 0, {VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero, VoiceDataInternal_Pattern_init_zero}}
#define VoiceDataInternal_Pattern_init_zero      {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
#define VoiceDataInternal_EnvelopeData_init_zero {0, 0, 0, 0}
#define VoiceDataInternal_LockPointer_init_zero  {0, 0}

/* Field tags (for use in manual encoding/decoding) */
#define VoiceDataInternal_Pattern_rate_tag       1
#define VoiceDataInternal_Pattern_length_tag     2
#define VoiceDataInternal_Pattern_notes_tag      3
#define VoiceDataInternal_Pattern_keys_tag       4
#define VoiceDataInternal_EnvelopeData_attack_tag 1
#define VoiceDataInternal_EnvelopeData_decay_tag 2
#define VoiceDataInternal_EnvelopeData_target_tag 3
#define VoiceDataInternal_EnvelopeData_depth_tag 4
#define VoiceDataInternal_version_tag            1
#define VoiceDataInternal_locksForPattern_tag    2
#define VoiceDataInternal_instrumentType_tag     3
#define VoiceDataInternal_effectEditTarget_tag   6
#define VoiceDataInternal_delaySend_tag          7
#define VoiceDataInternal_reverbSend_tag         8
#define VoiceDataInternal_env1_tag               9
#define VoiceDataInternal_env2_tag               10
#define VoiceDataInternal_lfoRate_tag            11
#define VoiceDataInternal_lfoDepth_tag           12
#define VoiceDataInternal_lfoTarget_tag          13
#define VoiceDataInternal_lfoDelay_tag           14
#define VoiceDataInternal_sampleStart_tag        19
#define VoiceDataInternal_sampleLength_tag       20
#define VoiceDataInternal_color_tag              21
#define VoiceDataInternal_timbre_tag             22
#define VoiceDataInternal_conditionMode_tag      23
#define VoiceDataInternal_conditionData_tag      24
#define VoiceDataInternal_cutoff_tag             25
#define VoiceDataInternal_resonance_tag          26
#define VoiceDataInternal_volume_tag             27
#define VoiceDataInternal_pan_tag                28
#define VoiceDataInternal_retriggerSpeed_tag     29
#define VoiceDataInternal_retriggerLength_tag    30
#define VoiceDataInternal_retriggerFade_tag      31
#define VoiceDataInternal_retriggerTargetEnv_tag 32
#define VoiceDataInternal_octave_tag             33
#define VoiceDataInternal_portamento_tag         34
#define VoiceDataInternal_fineTune_tag           35
#define VoiceDataInternal_synthShape_tag         64
#define VoiceDataInternal_samplerType_tag        65
#define VoiceDataInternal_midiChannel_tag        66
#define VoiceDataInternal_sampleAttack_tag       67
#define VoiceDataInternal_sampleDecay_tag        68
#define VoiceDataInternal_patterns_tag           69
#define VoiceDataInternal_LockPointer_pattern_tag 1
#define VoiceDataInternal_LockPointer_pointer_tag 2

/* Struct field encoding specification for nanopb */
#define VoiceDataInternal_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   version,           1) \
X(a, CALLBACK, REPEATED, MESSAGE,  locksForPattern,   2) \
X(a, STATIC,   SINGULAR, UINT32,   instrumentType,    3) \
X(a, STATIC,   SINGULAR, UINT32,   effectEditTarget,   6) \
X(a, STATIC,   SINGULAR, UINT32,   delaySend,         7) \
X(a, STATIC,   SINGULAR, UINT32,   reverbSend,        8) \
X(a, STATIC,   OPTIONAL, MESSAGE,  env1,              9) \
X(a, STATIC,   OPTIONAL, MESSAGE,  env2,             10) \
X(a, STATIC,   SINGULAR, UINT32,   lfoRate,          11) \
X(a, STATIC,   SINGULAR, UINT32,   lfoDepth,         12) \
X(a, STATIC,   SINGULAR, UINT32,   lfoTarget,        13) \
X(a, STATIC,   SINGULAR, UINT32,   lfoDelay,         14) \
X(a, STATIC,   FIXARRAY, UINT32,   sampleStart,      19) \
X(a, STATIC,   FIXARRAY, UINT32,   sampleLength,     20) \
X(a, STATIC,   SINGULAR, UINT32,   color,            21) \
X(a, STATIC,   SINGULAR, UINT32,   timbre,           22) \
X(a, STATIC,   SINGULAR, UINT32,   conditionMode,    23) \
X(a, STATIC,   SINGULAR, UINT32,   conditionData,    24) \
X(a, STATIC,   SINGULAR, UINT32,   cutoff,           25) \
X(a, STATIC,   SINGULAR, UINT32,   resonance,        26) \
X(a, STATIC,   SINGULAR, UINT32,   volume,           27) \
X(a, STATIC,   SINGULAR, UINT32,   pan,              28) \
X(a, STATIC,   SINGULAR, UINT32,   retriggerSpeed,   29) \
X(a, STATIC,   SINGULAR, UINT32,   retriggerLength,  30) \
X(a, STATIC,   SINGULAR, UINT32,   retriggerFade,    31) \
X(a, STATIC,   SINGULAR, UINT32,   retriggerTargetEnv,  32) \
X(a, STATIC,   SINGULAR, UINT32,   octave,           33) \
X(a, STATIC,   SINGULAR, UINT32,   portamento,       34) \
X(a, STATIC,   SINGULAR, UINT32,   fineTune,         35) \
X(a, STATIC,   ONEOF,    UINT32,   (extraTypeUnion,synthShape,extraTypeUnion.synthShape),  64) \
X(a, STATIC,   ONEOF,    UINT32,   (extraTypeUnion,samplerType,extraTypeUnion.samplerType),  65) \
X(a, STATIC,   ONEOF,    UINT32,   (extraTypeUnion,midiChannel,extraTypeUnion.midiChannel),  66) \
X(a, STATIC,   SINGULAR, UINT32,   sampleAttack,     67) \
X(a, STATIC,   SINGULAR, UINT32,   sampleDecay,      68) \
X(a, STATIC,   FIXARRAY, MESSAGE,  patterns,         69)
#define VoiceDataInternal_CALLBACK pb_default_field_callback
#define VoiceDataInternal_DEFAULT NULL
#define VoiceDataInternal_locksForPattern_MSGTYPE VoiceDataInternal_LockPointer
#define VoiceDataInternal_env1_MSGTYPE VoiceDataInternal_EnvelopeData
#define VoiceDataInternal_env2_MSGTYPE VoiceDataInternal_EnvelopeData
#define VoiceDataInternal_patterns_MSGTYPE VoiceDataInternal_Pattern

#define VoiceDataInternal_Pattern_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   rate,              1) \
X(a, STATIC,   SINGULAR, UINT32,   length,            2) \
X(a, STATIC,   FIXARRAY, UINT32,   notes,             3) \
X(a, STATIC,   FIXARRAY, UINT32,   keys,              4)
#define VoiceDataInternal_Pattern_CALLBACK NULL
#define VoiceDataInternal_Pattern_DEFAULT NULL

#define VoiceDataInternal_EnvelopeData_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   attack,            1) \
X(a, STATIC,   SINGULAR, UINT32,   decay,             2) \
X(a, STATIC,   SINGULAR, UINT32,   target,            3) \
X(a, STATIC,   SINGULAR, UINT32,   depth,             4)
#define VoiceDataInternal_EnvelopeData_CALLBACK NULL
#define VoiceDataInternal_EnvelopeData_DEFAULT NULL

#define VoiceDataInternal_LockPointer_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   pattern,           1) \
X(a, STATIC,   SINGULAR, UINT32,   pointer,           2)
#define VoiceDataInternal_LockPointer_CALLBACK NULL
#define VoiceDataInternal_LockPointer_DEFAULT NULL

extern const pb_msgdesc_t VoiceDataInternal_msg;
extern const pb_msgdesc_t VoiceDataInternal_Pattern_msg;
extern const pb_msgdesc_t VoiceDataInternal_EnvelopeData_msg;
extern const pb_msgdesc_t VoiceDataInternal_LockPointer_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define VoiceDataInternal_fields &VoiceDataInternal_msg
#define VoiceDataInternal_Pattern_fields &VoiceDataInternal_Pattern_msg
#define VoiceDataInternal_EnvelopeData_fields &VoiceDataInternal_EnvelopeData_msg
#define VoiceDataInternal_LockPointer_fields &VoiceDataInternal_LockPointer_msg

/* Maximum encoded size of messages (where known) */
/* VoiceDataInternal_size depends on runtime parameters */
#define VoiceDataInternal_EnvelopeData_size      12
#define VoiceDataInternal_LockPointer_size       7
#define VoiceDataInternal_Pattern_size           390

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
