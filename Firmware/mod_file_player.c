    uint16_t counter = 0;

    uint32_t step = 0x50000;
    uint32_t pos = 0;
    uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;
    uint vol = 128;
    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) { 
        sine_wave_table[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN));
    }
    pocketmod_context pm_c;
    pocketmod_init(&pm_c, bananasplit, 19035*16, 44100);

    float renderTarget[SAMPLES_PER_BUFFER*16];
    while(true)
    {
        /**/
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        // 4 bytes per channel * 2 channels

        pocketmod_render(&pm_c, renderTarget, buffer->max_sample_count*2*4);
        
        for (uint i = 0; i < buffer->max_sample_count*2; i++) {
            samples[i] = 0x7fff*clip(renderTarget[i]);
        }
       // printf("sample value: %i\n", samples[0]);
        
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
