`openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -s tcl`
`cmake -G"NMake Makefiles" ..`
run vscode in developer / admistrator mode for access to nmake (developer command prompt for vs 2019)
check to make sure the "kit" is selected (`GCC for arm-none-eabi 10.2.1`)

see this as an option for installing the toolchain
https://github.com/ndabas/pico-setup-windows

`>C:\Python27\python.exe resources_compiler.py resources/resources.py`

note the forward slashes, it does some kind of replacement on the path to convert to a module name

rates 2x 3/2x 1x 3/4x (triplets) 1/2x 1/4x 1/8x

we want to count up to 0xffffff 8 times per second? at 120bpm

sixteents per second, thirty seconds per second

bpm to bps = 
120 * (1 / 60) * (1/4)

bps to 32nds p sec

bps * 1/8

beats per sample

44100 * 32nds p sec

    "1:1", 1
    
    "1:2", 2
    "2:2", 3 (2*1)
    
    "1:3", 4
    "2:3", 5 
    "1:3", 6 (3*2)

    "1:4", 7
    "2:4", 8
    "3:4", 9
    "4:4", 10 (5*2)
  e

2,756.25
q8          q4
(44100<<8)/(60<<4) = q4

60*32 = 1920

local valTable = {}

for i=2,8 do
  for j=1,i do
   table.insert(valTable, j)
   table.insert(valTable, i)
  end
end

print("const uint8_t *ConditionalEvery[".. #valTable .. "] = {\n" .. table.concat(valTable, ", ") .. "};")


        // simplify, lets load from a known good source
        uint32_t filesize = ((uint32_t*)AudioSampleSecondbloop)[0]&0xffffff;
        int16_t* wave = ((int16_t*)AudioSampleSecondbloop)+2;
        if(filesize == 0 || sampleSegment == SMP_COMPLETE)
        {
            memset(buffer, 0, SAMPLES_PER_BUFFER*2);
            return;
        }
        if(sampleOffset > filesize)
            sampleSegment = SMP_COMPLETE;
        if(sampleSegment == SMP_COMPLETE)
            return;
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            if(sampleOffset > filesize - 1)
            {
                sampleSegment = SMP_COMPLETE;
            }
            // int16_t wave;
            // ffs_seek(GetFilesystem(), file, sampleOffset*2);
            // ffs_read(GetFilesystem(), file, wave, 2);

            phase_ += phase_increment;
            sampleOffset+=(phase_>>25);
            phase_-=(phase_&(0xfe<<24));

            buffer[i] = wave[sampleOffset];
            
        }
        return;
